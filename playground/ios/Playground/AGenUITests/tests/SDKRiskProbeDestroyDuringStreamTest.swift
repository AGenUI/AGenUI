import XCTest
@testable import Playground
@testable import AGenUI

// MARK: - RISK42: Concurrent Function Registration Dictionary Race

/// RISK42 iOS-specific: Concurrent mutation of `AGenUISDK.registeredFunctions` static
/// Dictionary from multiple threads causes COW (Copy-on-Write) data race → EXC_BAD_ACCESS.
///
/// Root cause (iOS platform-specific):
///   `AGenUISDK` maintains a `private static var registeredFunctions: [String: Function]`
///   with ZERO synchronization. Swift's Dictionary uses COW which is NOT thread-safe for
///   concurrent mutations. When two threads mutate the dictionary simultaneously:
///   1. Both threads see refcount > 1 for the buffer → both try to COW-detach
///   2. One thread frees the old buffer while the other is still reading from it
///   3. Or: both threads write to the same buffer without coordination → corruption
///   → EXC_BAD_ACCESS (SIGSEGV)
///
///   Android uses `synchronized` blocks + Java's strong memory model.
///   HarmonyOS ArkTS is single-threaded by design.
///   Only iOS has this vulnerability due to unprotected Swift value-type Dictionary.
///
/// Detection:
///   - Process crash (SIGSEGV / EXC_BAD_ACCESS) during concurrent reg/unreg
///   - Or: test runner reports "lost connection to test process"
final class SDKRiskProbeConcurrentFuncRegTest: XCTestCase {

    // MARK: - Test Function Implementation

    /// Minimal Function implementation for testing
    private class TestFunction: NSObject, Function {
        let functionConfig: FunctionConfig

        init(name: String) {
            self.functionConfig = FunctionConfig(name: name)
        }

        func execute(context: FunctionCallContext, params: String) -> FunctionResult {
            return FunctionResult.success(value: "{\"result\":\"ok\"}")
        }
    }

    // MARK: - RISK42a: Concurrent registerFunction from multiple threads

    /// Multiple threads simultaneously call AGenUISDK.registerFunction() with different
    /// function names. The static `registeredFunctions` dictionary is mutated concurrently
    /// without synchronization → COW race → EXC_BAD_ACCESS.
    func testRISK42a_iOS_concurrentFunctionRegistration() {
        let concurrency = 8
        let iterationsPerThread = 100
        let group = DispatchGroup()

        for threadIdx in 0..<concurrency {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                for i in 0..<iterationsPerThread {
                    let name = "risk42a_t\(threadIdx)_f\(i)"
                    let function = TestFunction(name: name)
                    AGenUISDK.registerFunction(function)
                }
                group.leave()
            }
        }

        let result = group.wait(timeout: .now() + 30)
        XCTAssertEqual(result, .success, "All threads should complete within timeout")

        // Cleanup
        for threadIdx in 0..<concurrency {
            for i in 0..<iterationsPerThread {
                AGenUISDK.unregisterFunction("risk42a_t\(threadIdx)_f\(i)")
            }
        }
    }

    // MARK: - RISK42b: Concurrent register + unregister interleaved

    /// One set of threads registers functions while another set unregisters them
    /// simultaneously. The dictionary sees concurrent insert + remove → COW corruption.
    func testRISK42b_iOS_concurrentRegisterAndUnregister() {
        let concurrency = 6
        let iterations = 150
        let group = DispatchGroup()

        // Pre-register some functions
        for i in 0..<iterations {
            let function = TestFunction(name: "risk42b_f\(i)")
            AGenUISDK.registerFunction(function)
        }

        // Half threads register new functions, half unregister existing ones
        for threadIdx in 0..<concurrency {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                if threadIdx % 2 == 0 {
                    // Register new functions
                    for i in 0..<iterations {
                        let name = "risk42b_new_t\(threadIdx)_f\(i)"
                        let function = TestFunction(name: name)
                        AGenUISDK.registerFunction(function)
                    }
                } else {
                    // Unregister existing functions
                    for i in 0..<iterations {
                        AGenUISDK.unregisterFunction("risk42b_f\(i)")
                    }
                }
                group.leave()
            }
        }

        let result = group.wait(timeout: .now() + 30)
        XCTAssertEqual(result, .success, "All threads should complete within timeout")

        // Cleanup remaining
        for threadIdx in stride(from: 0, to: concurrency, by: 2) {
            for i in 0..<iterations {
                AGenUISDK.unregisterFunction("risk42b_new_t\(threadIdx)_f\(i)")
            }
        }
    }

    // MARK: - RISK42c: Register same name from multiple threads (overwrite race)

    /// Multiple threads register functions with the SAME name simultaneously.
    /// This exercises the `registeredFunctions[name] = function` overwrite path
    /// where one thread's read-check and another's write can corrupt the buffer.
    func testRISK42c_iOS_concurrentSameNameRegistration() {
        let concurrency = 8
        let iterations = 200
        let group = DispatchGroup()

        for threadIdx in 0..<concurrency {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                for i in 0..<iterations {
                    // All threads fight over the same 10 function names
                    let name = "risk42c_shared_\(i % 10)"
                    let function = TestFunction(name: name)
                    AGenUISDK.registerFunction(function)
                }
                group.leave()
            }
        }

        let result = group.wait(timeout: .now() + 30)
        XCTAssertEqual(result, .success, "All threads should complete within timeout")

        // Cleanup
        for i in 0..<10 {
            AGenUISDK.unregisterFunction("risk42c_shared_\(i)")
        }
    }
}

// MARK: - RISK41 Tests (Destroy During Stream)

/// RISK41 iOS-specific: SurfaceManager destroy during active C++ stream processing.
///
/// Root cause (iOS platform-specific):
///   ARC's deterministic deallocation means setting the last strong reference to nil
///   immediately triggers `deinit → teardown → destroySurfaceManager`. If the C++
///   worker thread is still processing a previously-enqueued chunk (layout, event
///   dispatch, getSurfaceSize callback), it accesses the freed ISurfaceManager.
///
///   On Android, Java GC is non-deterministic, so the native object lives longer and
///   the race window is negligible. On iOS, ARC + `unsafe_unretained` C++ pointers
///   make this trivially triggerable.
///
/// Attack surface:
///   1. `receiveTextChunk()` enqueues work to the C++ worker thread and returns immediately
///   2. `sm = nil` fires `deinit` → `teardown` → `destroySurfaceManager` on the calling thread
///   3. Worker thread accesses freed ISurfaceManager → EXC_BAD_ACCESS (SIGSEGV)
///
/// Detection:
///   - Process crash (SIGSEGV / EXC_BAD_ACCESS) during the rapid-cycle loop
///   - Or: test runner reports "lost connection to test process"
final class SDKRiskProbeDestroyDuringStreamTest: XCTestCase {

    // MARK: - JSON payloads

    /// Minimal createSurface + updateComponents payload that triggers layout
    private func buildLayoutTriggeringJSON(surfaceId: String) -> String {
        return """
        {"version":"v0.9","createSurface":{"surfaceId":"\(surfaceId)","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"},"updateComponents":{"surfaceId":"\(surfaceId)","components":[{"id":"root","component":"Column","children":["t1","t2","t3","t4","t5"]},{"id":"t1","component":"Text","text":"Hello 1"},{"id":"t2","component":"Text","text":"Hello 2"},{"id":"t3","component":"Text","text":"Hello 3"},{"id":"t4","component":"Text","text":"Hello 4"},{"id":"t5","component":"Text","text":"Hello 5"}]}}
        """
    }

    /// Larger payload with more components to increase worker thread processing time
    private func buildHeavyPayload(surfaceId: String, componentCount: Int) -> String {
        var children: [String] = []
        var components: [String] = []
        for i in 0..<componentCount {
            let cid = "c_\(i)"
            children.append("\"\(cid)\"")
            components.append("{\"id\":\"\(cid)\",\"component\":\"Text\",\"text\":\"Item \(i) with some longer text content to increase parsing time\"}")
        }
        let childrenStr = children.joined(separator: ",")
        let componentsStr = components.joined(separator: ",")
        return """
        {"version":"v0.9","createSurface":{"surfaceId":"\(surfaceId)","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"},"updateComponents":{"surfaceId":"\(surfaceId)","components":[{"id":"root","component":"Column","children":[\(childrenStr)],\"style\":{\"width\":\"300px\",\"padding\":\"10px\"}},\(componentsStr)]}}
        """
    }

    // MARK: - RISK41a: Rapid create-stream-destroy cycle

    /// Rapidly create SurfaceManagers, feed them data, and immediately release them.
    /// The C++ worker thread may still be processing when teardown fires.
    ///
    /// Expected: EXC_BAD_ACCESS or test runner crash within ~100 iterations.
    func testRISK41a_iOS_destroyDuringActiveStreaming() {
        let iterations = 200

        for i in 0..<iterations {
            autoreleasepool {
                let sm = SurfaceManager()
                let surfaceId = "risk41a_\(i)"
                let json = buildHeavyPayload(surfaceId: surfaceId, componentCount: 50)

                // Begin streaming and feed data — this enqueues work to C++ worker thread
                sm.beginTextStream()
                sm.receiveTextChunk(json)
                sm.endTextStream()

                // sm goes out of scope here → ARC dealloc → deinit → teardown
                // → destroySurfaceManager while worker thread is still processing
            }
            // No sleep — maximize race pressure
        }

        // If we reach here without crash, the race didn't trigger this run.
        // The test is designed to crash the process on hit.
        XCTAssert(true, "Completed \(iterations) iterations without crash (race not triggered this run)")
    }

    // MARK: - RISK41b: Destroy with surfaceSizeProvider callback active

    /// Create SurfaceManager with a surfaceSize listener, stream data that triggers
    /// layout (which calls getSurfaceSize on worker thread), and release the SM.
    ///
    /// Race path:
    ///   Worker thread: getSurfaceSize() → block captures [weak self] → guard let self
    ///   Main thread: sm = nil → if worker has temp strong ref, deinit deferred
    ///   Worker thread: block returns → temp ref released → deinit fires ON WORKER THREAD
    ///   → teardown destroys ISurfaceManager that engine is still using for layout
    ///
    /// Expected: EXC_BAD_ACCESS when engine accesses freed SurfaceManager post-layout.
    func testRISK41b_iOS_destroyDuringSurfaceSizeCallback() {
        let iterations = 200

        for i in 0..<iterations {
            autoreleasepool {
                let sm = SurfaceManager()
                let surfaceId = "risk41b_\(i)"

                // Add a surfaceSize listener that returns a valid size
                // This forces the engine to call getSurfaceSize() during layout
                let sizeListener = SizeProviderListener(size: CGSize(width: 375, height: 667))
                sm.addListener(sizeListener)

                let json = buildHeavyPayload(surfaceId: surfaceId, componentCount: 30)

                sm.beginTextStream()
                sm.receiveTextChunk(json)
                sm.endTextStream()

                // Immediately remove listener and release SM
                sm.removeListener(sizeListener)
                // sm goes out of scope → dealloc races with worker thread layout
            }
        }

        XCTAssert(true, "Completed \(iterations) iterations without crash")
    }

    // MARK: - RISK41c: Multi-threaded create/destroy with concurrent streaming

    /// Multiple threads create and destroy SurfaceManagers while streaming data.
    /// Maximizes the race window between C++ worker thread processing and ARC dealloc.
    ///
    /// Expected: EXC_BAD_ACCESS from concurrent teardown vs worker thread access.
    func testRISK41c_iOS_concurrentCreateDestroyWithStreaming() {
        let iterations = 50
        let concurrency = 4
        let group = DispatchGroup()

        for threadIdx in 0..<concurrency {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                for i in 0..<iterations {
                    autoreleasepool {
                        let sm = SurfaceManager()
                        let surfaceId = "risk41c_t\(threadIdx)_\(i)"

                        let sizeListener = SizeProviderListener(size: CGSize(width: 320, height: 568))
                        sm.addListener(sizeListener)

                        let json = self.buildHeavyPayload(surfaceId: surfaceId, componentCount: 20)

                        sm.beginTextStream()
                        sm.receiveTextChunk(json)
                        sm.endTextStream()

                        // Immediately release — deinit fires on THIS background thread
                        // while C++ worker thread is processing
                    }
                }
                group.leave()
            }
        }

        let result = group.wait(timeout: .now() + 30)
        XCTAssertEqual(result, .success, "All threads should complete within timeout")
    }

    // MARK: - RISK41d: Interleaved stream operations during dealloc

    /// Start a stream, feed partial data, then release the SurfaceManager mid-stream.
    /// The C++ streaming parser holds internal state that may be accessed after free.
    ///
    /// Expected: EXC_BAD_ACCESS from accessing freed streaming parser state.
    func testRISK41d_iOS_destroyMidStreamPartialData() {
        let iterations = 300

        for i in 0..<iterations {
            autoreleasepool {
                let sm = SurfaceManager()
                let surfaceId = "risk41d_\(i)"
                let fullJson = buildHeavyPayload(surfaceId: surfaceId, componentCount: 40)

                sm.beginTextStream()

                // Feed partial chunks — the parser accumulates internal state
                let chunkSize = max(1, fullJson.count / 5)
                var index = fullJson.startIndex
                var chunksFed = 0
                while index < fullJson.endIndex && chunksFed < 3 {
                    let end = fullJson.index(index, offsetBy: chunkSize, limitedBy: fullJson.endIndex) ?? fullJson.endIndex
                    sm.receiveTextChunk(String(fullJson[index..<end]))
                    index = end
                    chunksFed += 1
                }

                // DO NOT call endTextStream — release mid-stream
                // sm goes out of scope → deinit while parser has partial state
                // and worker thread may be processing accumulated chunks
            }
        }

        XCTAssert(true, "Completed \(iterations) iterations without crash")
    }
}

// MARK: - Helper: SurfaceSize Provider Listener

/// Listener that provides a fixed surface size.
/// Forces the engine to call getSurfaceSize() during layout passes.
private class SizeProviderListener: NSObject, SurfaceManagerListener {
    private let size: CGSize

    init(size: CGSize) {
        self.size = size
    }

    func surfaceSize(for surfaceId: String) -> CGSize {
        return size
    }
}
