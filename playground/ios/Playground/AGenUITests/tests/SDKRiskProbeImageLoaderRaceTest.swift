import XCTest
@testable import Playground
@testable import AGenUI

// MARK: - RISK43: Concurrent ImageLoader Registration Race

/// RISK43 iOS-specific: Concurrent mutation of `ImageLoaderConfiguration.shared.loader`
/// property from multiple threads causes ARC reference counting race → double-free
/// → EXC_BAD_ACCESS / SIGABRT.
///
/// Root cause (iOS platform-specific):
///   `ImageLoaderConfiguration.shared.loader` is a plain `var` stored property with
///   ZERO synchronization. Swift stored property setters execute a non-atomic sequence:
///     1. Load old value from storage
///     2. Store new value
///     3. Release old value
///   When two threads execute the setter simultaneously:
///     - Both load the SAME old value
///     - Both store their new values (one overwrites the other)
///     - Both release the same old value → DOUBLE-FREE → crash
///   Additionally, a concurrent reader (loadImage call) may load a partially-written
///   pointer → use-after-free → crash.
///
///   Android: Java GC manages references atomically, no double-free possible
///   HarmonyOS: ArkTS single-threaded, no concurrent access
///   Only iOS is vulnerable due to non-atomic Swift property + ARC deterministic release.
///
/// Detection:
///   - Process crash (SIGSEGV / EXC_BAD_ACCESS / SIGABRT) during concurrent property writes
///   - Or: test runner reports "unexpected exit, crash, or test timeout"
final class SDKRiskProbeImageLoaderRaceTest: XCTestCase {

    // MARK: - Test ImageLoader Implementations

    /// Minimal ImageLoader implementation for testing
    private class DummyImageLoader: NSObject, ImageLoader {
        let loaderId: Int

        init(id: Int) {
            self.loaderId = id
            super.init()
        }

        func loadImage(from url: URL, options: [String: Any]?, completion: @escaping (UIImage?, Bool, Error?) -> Void) -> String {
            // Return a unique task ID but don't actually load anything
            return "dummy_task_\(loaderId)_\(UUID().uuidString)"
        }

        func cancel(for taskId: String) {
            // No-op
        }

        func clearMemory() {
            // No-op
        }

        func clearDisk() {
            // No-op
        }
    }

    // MARK: - RISK43a: Concurrent registerImageLoader from multiple threads

    /// Multiple threads simultaneously call AGenUISDK.registerImageLoader() with different
    /// loader instances. The underlying `ImageLoaderConfiguration.shared.loader` property
    /// is written concurrently without synchronization → ARC double-free → crash.
    func testRISK43a_iOS_concurrentImageLoaderRegistration() {
        let concurrency = 8
        let iterationsPerThread = 200
        let group = DispatchGroup()

        for threadIdx in 0..<concurrency {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                for i in 0..<iterationsPerThread {
                    let loader = DummyImageLoader(id: threadIdx * 10000 + i)
                    AGenUISDK.registerImageLoader(loader)
                }
                group.leave()
            }
        }

        let result = group.wait(timeout: .now() + 30)
        XCTAssertEqual(result, .success, "All threads should complete within timeout")
    }

    // MARK: - RISK43b: Concurrent registerImageLoader + image load reads

    /// One set of threads registers new image loaders while another set reads the
    /// current loader (simulating ImageComponent triggering image loads).
    /// The property read/write race can cause:
    ///   - Reader loads partially-written (torn) pointer → use-after-free
    ///   - Writer's release frees object while reader holds dangling pointer → crash
    func testRISK43b_iOS_concurrentLoaderRegAndRead() {
        let writerThreads = 4
        let readerThreads = 4
        let iterations = 300
        let group = DispatchGroup()

        // Writer threads: rapidly replace the image loader
        for threadIdx in 0..<writerThreads {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                for i in 0..<iterations {
                    let loader = DummyImageLoader(id: threadIdx * 10000 + i)
                    AGenUISDK.registerImageLoader(loader)
                }
                group.leave()
            }
        }

        // Reader threads: rapidly read the current loader and call a method on it
        for threadIdx in 0..<readerThreads {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                for _ in 0..<iterations {
                    // Access the loader property and call cancel (safe no-op but exercises the ref)
                    let loader = ImageLoaderConfiguration.shared.loader
                    loader.cancel(for: "nonexistent_\(threadIdx)")
                }
                group.leave()
            }
        }

        let result = group.wait(timeout: .now() + 30)
        XCTAssertEqual(result, .success, "All threads should complete within timeout")
    }

    // MARK: - RISK43c: Rapid register/deregister cycle with object deallocation pressure

    /// Rapidly create short-lived loader instances and register them.
    /// The previous loader's refcount drops to 0 immediately on replacement,
    /// maximizing the chance of ARC release racing with another thread's read/write.
    func testRISK43c_iOS_rapidLoaderReplacementDealloc() {
        let concurrency = 8
        let iterations = 500
        let group = DispatchGroup()

        for threadIdx in 0..<concurrency {
            group.enter()
            DispatchQueue.global(qos: .userInitiated).async {
                for i in 0..<iterations {
                    autoreleasepool {
                        let loader = DummyImageLoader(id: threadIdx * 10000 + i)
                        AGenUISDK.registerImageLoader(loader)
                        // loader goes out of scope immediately after register
                        // The ONLY strong ref is now in ImageLoaderConfiguration.shared.loader
                        // Next iteration's register will release it → if another thread
                        // is simultaneously reading the property, it may access freed memory
                    }
                }
                group.leave()
            }
        }

        let result = group.wait(timeout: .now() + 30)
        XCTAssertEqual(result, .success, "All threads should complete within timeout")
    }
}
