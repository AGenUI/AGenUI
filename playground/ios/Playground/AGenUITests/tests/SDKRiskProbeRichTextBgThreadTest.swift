import XCTest
@testable import Playground
@testable import AGenUI

// MARK: - RISK44: RichText HTML Parsing on Background Thread (Main-Thread Violation)

/// RISK44 iOS-specific: `RichTextComponent.measure()` calls
/// `NSAttributedString(data:options:[.documentType: .html])` from the C++ engine's
/// background layout thread.
///
/// Root cause (iOS platform-specific):
///   Apple explicitly documents that NSAttributedString with `.html` document type
///   MUST be called from the main thread only. It uses WebKit internally for HTML
///   parsing, which requires the main thread run loop.
///
///   When called from a background thread:
///   1. WebKit tries to synchronize with the main run loop
///   2. This can DEADLOCK (blocks indefinitely) or CRASH (EXC_BAD_ACCESS in WebThread)
///   3. On some iOS versions, produces incorrect/nil results silently
///
///   The SDK's measurement callback is invoked from the C++ engine's layout thread
///   (documented in Component.swift: "This method is called on the engine's background
///   thread"). When a RichText component needs measurement, the call chain is:
///     C++ Yoga layout (worker thread)
///       -> IOSBridgeMeasurement::measure()
///       -> measureCallback
///       -> SurfaceManager.measureComponent()
///       -> RichTextComponent.measure()
///       -> buildAttributedString(htmlString:styles:)
///       -> NSAttributedString(data:options:[.documentType: .html])  <- CRASH/DEADLOCK
///
///   Android uses Spanned/Html.fromHtml() which is thread-safe.
///   HarmonyOS ArkTS is single-threaded by design.
///   Only iOS has this vulnerability due to WebKit's main-thread requirement.
///
/// Detection:
///   - Process crash (EXC_BAD_ACCESS in WebKit/WebThread)
///   - Or: test hangs/deadlocks (timeout)
///   - Or: test runner reports "lost connection to test process"
final class SDKRiskProbeRichTextBgThreadTest: XCTestCase {

    // MARK: - Helper: Listener that provides surface size

    /// A SurfaceManagerListener that provides a non-zero surface size so Yoga
    /// will actually perform layout and call measure on leaf components.
    private class SizeProvider: NSObject, SurfaceManagerListener {
        func surfaceSize(for surfaceId: String) -> CGSize {
            return CGSize(width: 375, height: 667) // iPhone SE size
        }
    }

    // MARK: - Test Data

    /// Build a complete A2UI protocol payload with createSurface + updateComponents
    /// containing a RichText component with HTML content.
    private static func richTextPayload(surfaceId: String, html: String = "<p>This is <strong>bold</strong> and <em>italic</em> text with <a href='https://test.com'>a link</a>.</p>") -> String {
        """
        {"version":"v0.9","createSurface":{"surfaceId":"\(surfaceId)","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"},"updateComponents":{"surfaceId":"\(surfaceId)","components":[{"id":"root","component":"RichText","text":"\(html)","styles":{"fontSize":16}}]}}
        """
    }

    /// Complex HTML payload with deeply nested structures
    private static func complexRichTextPayload(surfaceId: String) -> String {
        let html = "<div><h1>Title</h1><p>Paragraph with <b>bold</b>, <i>italic</i>, <u>underline</u></p><ul><li>Item 1</li><li>Item 2</li><li>Item 3</li></ul><ol><li>First</li><li>Second</li></ol><blockquote>A quoted passage with <a href='http://example.com'>link</a></blockquote></div>"
        return """
        {"version":"v0.9","createSurface":{"surfaceId":"\(surfaceId)","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"},"updateComponents":{"surfaceId":"\(surfaceId)","components":[{"id":"rt1","component":"RichText","text":"\(html)","styles":{"fontSize":14,"color":"#333333"}}]}}
        """
    }

    // MARK: - RISK44a: RichText measure from explicit background thread

    /// Explicitly call receiveTextChunk from a background thread to guarantee
    /// the C++ processing (and thus Yoga measurement) happens off main thread.
    /// If the engine processes data synchronously on the calling thread, the
    /// HTML parsing will occur on our background thread -> crash/deadlock.
    func testRISK44a_richTextMeasureFromBackgroundThread() {
        let expectation = self.expectation(description: "Background processing completes")

        DispatchQueue.global(qos: .userInitiated).async {
            let sm = SurfaceManager()
            let sizeProvider = SizeProvider()
            sm.addListener(sizeProvider)
            sm.beginTextStream()
            sm.receiveTextChunk(Self.richTextPayload(surfaceId: "risk44a"))
            sm.endTextStream()

            // The engine processes data asynchronously on its worker thread.
            // We must keep the SurfaceManager alive while the engine does Yoga layout
            // (which calls RichTextComponent.measure -> NSAttributedString(.html)).
            // If this deadlocks, the thread will be blocked here.
            Thread.sleep(forTimeInterval: 3.0)

            // Keep sm alive through the sleep
            _ = sm
            expectation.fulfill()
        }

        // If NSAttributedString(.html) deadlocks on background thread, this will timeout
        waitForExpectations(timeout: 10.0, handler: nil)
    }

    // MARK: - RISK44b: Multiple RichText components measured concurrently

    /// Feed multiple RichText components from several background threads simultaneously.
    /// Even if single-threaded HTML parsing doesn't crash, concurrent access to WebKit's
    /// internal state from multiple threads may corrupt it.
    func testRISK44b_concurrentRichTextMeasure() {
        let group = DispatchGroup()
        let threadCount = 4
        var managers: [SurfaceManager] = []
        let lock = NSLock()

        for i in 0..<threadCount {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                let sm = SurfaceManager()
                let sizeProvider = SizeProvider()
                sm.addListener(sizeProvider)
                lock.lock()
                managers.append(sm)
                lock.unlock()

                sm.beginTextStream()
                sm.receiveTextChunk(Self.richTextPayload(surfaceId: "risk44b_\(i)"))
                sm.endTextStream()

                // Wait for engine processing
                Thread.sleep(forTimeInterval: 3.0)
                group.leave()
            }
        }

        let result = group.wait(timeout: .now() + 15.0)
        if result == .timedOut {
            XCTFail("RISK44b: Deadlock detected - RichText HTML parsing blocked on background thread")
        }
        _ = managers // prevent early dealloc
    }

    // MARK: - RISK44c: Rapid repeated RichText measurement from background

    /// Rapid-fire RichText payloads from background thread to stress WebKit's
    /// internal HTML parser. High iteration count increases the chance of hitting
    /// the race window between WebKit's main-thread sync and our background call.
    func testRISK44c_rapidRichTextFromBackground() {
        let iterations = 20
        let expectation = self.expectation(description: "Rapid RichText processing completes")

        DispatchQueue.global(qos: .userInitiated).async {
            var managers: [SurfaceManager] = []
            for i in 0..<iterations {
                let sm = SurfaceManager()
                managers.append(sm)
                sm.beginTextStream()
                sm.receiveTextChunk(Self.richTextPayload(surfaceId: "risk44c_\(i)"))
                sm.endTextStream()
            }

            // Wait for all engine workers to finish processing
            Thread.sleep(forTimeInterval: 5.0)
            _ = managers // prevent early dealloc
            expectation.fulfill()
        }

        waitForExpectations(timeout: 30.0, handler: nil)
    }

    // MARK: - RISK44d: Complex HTML content measurement from background

    /// Use deeply nested/complex HTML to maximize WebKit processing time,
    /// increasing the window for threading issues to manifest.
    func testRISK44d_complexHTMLMeasureFromBackground() {
        let expectation = self.expectation(description: "Complex HTML processing completes")

        DispatchQueue.global(qos: .userInitiated).async {
            let sm = SurfaceManager()
            let sizeProvider = SizeProvider()
            sm.addListener(sizeProvider)
            sm.beginTextStream()
            sm.receiveTextChunk(Self.complexRichTextPayload(surfaceId: "risk44d"))
            sm.endTextStream()

            // Wait for engine processing
            Thread.sleep(forTimeInterval: 3.0)
            _ = sm
            expectation.fulfill()
        }

        waitForExpectations(timeout: 10.0, handler: nil)
    }
}
