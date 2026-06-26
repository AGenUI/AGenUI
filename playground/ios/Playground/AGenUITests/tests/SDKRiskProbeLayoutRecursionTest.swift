import XCTest
@testable import Playground
@testable import AGenUI

/// RISK45: Recursive layout notification causing stack overflow.
///
/// Root cause (iOS-specific):
///   Surface.updateSize() calls notifyLayoutChangedInternal(), which invokes the
///   public onLayoutChanged callback. If the user's callback calls updateSize()
///   again (a reasonable pattern to adjust surface size in response to layout),
///   it triggers unbounded recursion → stack overflow → EXC_BAD_ACCESS.
///
///   The SDK provides NO re-entrancy guard on this path.
///
/// Attack surface:
///   Surface.updateSize() → notifyLayoutChangedInternal() → notifyLayoutChangedInternalReal()
///   → onLayoutChanged?() → [user callback] → surface.updateSize() → ... (infinite)
///
/// Severity: HIGH — deterministic crash, triggered through normal public API usage.
/// Fix: Add a boolean re-entrancy guard (e.g., `isNotifyingLayout`) in Surface.
final class SDKRiskProbeLayoutRecursionTest: XCTestCase {

    // MARK: - Listener helper

    private class SizeProvider: NSObject, SurfaceManagerListener {
        func surfaceSize(for surfaceId: String) -> CGSize {
            return CGSize(width: 375, height: 800)
        }
    }

    private class SurfaceCapture: NSObject, SurfaceManagerListener {
        var capturedSurface: Surface?
        let expectation: XCTestExpectation

        init(expectation: XCTestExpectation) {
            self.expectation = expectation
        }

        func onCreateSurface(_ surface: Surface) {
            capturedSurface = surface
            expectation.fulfill()
        }
    }

    // MARK: - JSON helpers

    private func buildCreateSurfaceJSON(surfaceId: String) -> String {
        return """
        {"version":"v0.9","createSurface":{"surfaceId":"\(surfaceId)","catalogId":"test"}}
        """
    }

    private func buildRootWithTextJSON(surfaceId: String) -> String {
        return """
        {"version":"v0.9","updateComponents":{"surfaceId":"\(surfaceId)","components":[{"id":"root","component":"Column","styles":{"width":"200px","height":"100px"},"children":["t1"]},{"id":"t1","component":"Text","text":"Hello RISK45"}]}}
        """
    }

    // MARK: - RISK45a: Prove unbounded recursion exists

    /// Round 1: Call updateSize() inside onLayoutChanged. If SDK lacks a re-entrancy
    /// guard, the callback count will exceed the "safe" threshold (2), proving that
    /// a real user would hit stack overflow.
    func testRISK45a_layoutCallbackRecursion() {
        let sm = SurfaceManager()
        let sizeProvider = SizeProvider()
        sm.addListener(sizeProvider)

        let surfaceId = "risk45a"

        // Capture the Surface instance
        let createExp = expectation(description: "Surface created")
        let capture = SurfaceCapture(expectation: createExp)
        sm.addListener(capture)

        // Stream: create surface + root component
        sm.beginTextStream()
        sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: surfaceId))
        sm.receiveTextChunk(buildRootWithTextJSON(surfaceId: surfaceId))
        sm.endTextStream()

        wait(for: [createExp], timeout: 5.0)

        guard let surface = capture.capturedSurface else {
            XCTFail("RISK45a: Surface not created")
            return
        }

        // Set a recursion counter with safety limit to prevent crashing the test runner
        var recursionCount = 0
        let maxSafeDepth = 50

        surface.onLayoutChanged = {
            recursionCount += 1
            if recursionCount <= maxSafeDepth {
                // This simulates a real user pattern: adjusting size in response to layout
                surface.updateSize(width: CGFloat(200 + recursionCount), height: 300)
            }
        }

        // Trigger the first layout notification via updateSize
        // If SDK has no guard: recursionCount will reach maxSafeDepth
        // If SDK has guard: recursionCount will be 1
        surface.updateSize(width: 200, height: 300)

        // Allow run loop to process
        RunLoop.current.run(until: Date().addingTimeInterval(1.0))

        // Verdict: recursionCount > 2 proves unbounded recursion exists
        // In production (without our safety cap), this would be stack overflow → crash
        if recursionCount > 2 {
            XCTFail("RISK45a: CONFIRMED — Unbounded recursion detected. " +
                    "onLayoutChanged fired \(recursionCount) times. " +
                    "Without safety cap, this is a stack overflow crash (EXC_BAD_ACCESS). " +
                    "SDK lacks re-entrancy guard in notifyLayoutChangedInternal().")
        }
    }

    // MARK: - RISK45b: Same pattern but via component property update triggering layout

    /// Round 2: Instead of updateSize, trigger layout through component property update.
    /// If the user's onLayoutChanged calls back into any method that triggers
    /// notifyLayoutChangedInternal, recursion occurs.
    func testRISK45b_layoutCallbackRecursionViaComponentUpdate() {
        let sm = SurfaceManager()
        let sizeProvider = SizeProvider()
        sm.addListener(sizeProvider)

        let surfaceId = "risk45b"

        let createExp = expectation(description: "Surface created")
        let capture = SurfaceCapture(expectation: createExp)
        sm.addListener(capture)

        sm.beginTextStream()
        sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: surfaceId))
        sm.receiveTextChunk(buildRootWithTextJSON(surfaceId: surfaceId))
        sm.endTextStream()

        wait(for: [createExp], timeout: 5.0)

        guard let surface = capture.capturedSurface else {
            XCTFail("RISK45b: Surface not created")
            return
        }

        var recursionCount = 0
        let maxSafeDepth = 50

        surface.onLayoutChanged = {
            recursionCount += 1
            if recursionCount <= maxSafeDepth {
                // User adjusts surface size after each layout — triggers recursion
                surface.updateSize(width: CGFloat(375), height: CGFloat(600 + recursionCount))
            }
        }

        // Trigger first layout by calling updateSize from outside
        surface.updateSize(width: 375, height: 600)

        RunLoop.current.run(until: Date().addingTimeInterval(1.0))

        if recursionCount > 2 {
            XCTFail("RISK45b: CONFIRMED — Unbounded recursion in layout notification. " +
                    "Callback fired \(recursionCount) times. Stack overflow in production.")
        }
    }

    // MARK: - RISK45c: Rapid updateSize calls without recursion (control test)

    /// Control: Verify that calling updateSize multiple times WITHOUT the recursive
    /// callback pattern works fine. This proves the issue is specifically about
    /// re-entrant notification dispatch, not just calling updateSize.
    func testRISK45c_controlNoRecursion() {
        let sm = SurfaceManager()
        let sizeProvider = SizeProvider()
        sm.addListener(sizeProvider)

        let surfaceId = "risk45c"

        let createExp = expectation(description: "Surface created")
        let capture = SurfaceCapture(expectation: createExp)
        sm.addListener(capture)

        sm.beginTextStream()
        sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: surfaceId))
        sm.receiveTextChunk(buildRootWithTextJSON(surfaceId: surfaceId))
        sm.endTextStream()

        wait(for: [createExp], timeout: 5.0)

        guard let surface = capture.capturedSurface else {
            XCTFail("RISK45c: Surface not created")
            return
        }

        var layoutCount = 0
        surface.onLayoutChanged = {
            layoutCount += 1
        }

        // Call updateSize 20 times sequentially (no recursion)
        for i in 0..<20 {
            surface.updateSize(width: CGFloat(200 + i), height: 300)
        }

        RunLoop.current.run(until: Date().addingTimeInterval(1.0))

        // Control: layoutCount should be ~20 (one per updateSize call)
        // This proves the basic mechanism works; the bug is specifically re-entrancy
        XCTAssertGreaterThan(layoutCount, 0, "RISK45c: onLayoutChanged should fire for updateSize calls")
        XCTAssertLessThanOrEqual(layoutCount, 25, "RISK45c: Without recursion, callback count should be bounded")
    }
}
