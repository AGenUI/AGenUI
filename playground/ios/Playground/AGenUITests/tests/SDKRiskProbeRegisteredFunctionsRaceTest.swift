import XCTest
@testable import Playground
@testable import AGenUI

/// RISK42: AGenUISDK.registeredFunctions concurrent mutation crash (iOS-specific)
///
/// Root cause: `AGenUISDK.registeredFunctions` is a Swift static `[String: Function]`
/// dictionary with ZERO thread synchronization. Swift Dictionary uses Copy-on-Write;
/// concurrent writes from multiple threads corrupt the internal buffer → EXC_BAD_ACCESS.
///
/// Why this is iOS-specific:
///   - Android: Java HashMap access is wrapped in `synchronized` blocks
///   - The ObjC bridge layer has `sFunctionMutex` (for C++ map) and
///     `@synchronized(_functionCallCallbacks)` (for OC dictionary)
///   - But the Swift-level `registeredFunctions` property has NO lock/queue protection
///
/// Attack surface: Any SDK user who calls registerFunction / unregisterFunction
/// from different threads (e.g., main thread + background callback) can trigger this.
///
/// Detection: EXC_BAD_ACCESS (SIGSEGV or SIGBUS) during dictionary mutation.
/// The crash typically manifests within the first few hundred iterations.
final class SDKRiskProbeRegisteredFunctionsRaceTest: XCTestCase {

    private let iterationsPerThread = 500
    private let threadCount = 4
    private let testDuration: TimeInterval = 10.0

    // MARK: - RISK42: Concurrent registerFunction race

    /// Multiple threads simultaneously call registerFunction with different names.
    /// Swift Dictionary concurrent writes → CoW buffer corruption → EXC_BAD_ACCESS.
    func testRISK42_concurrentRegisterFunctionRace() throws {
        let group = DispatchGroup()
        let startBarrier = DispatchSemaphore(value: 0)
        let crashDetected = AtomicBool(initial: false)

        // Spawn multiple threads, each registering functions concurrently
        for threadIdx in 0..<threadCount {
            group.enter()
            Thread.detachNewThread {
                // Wait for all threads to be ready
                startBarrier.wait()

                for i in 0..<self.iterationsPerThread {
                    autoreleasepool {
                        let name = "raceFunc_t\(threadIdx)_i\(i)"
                        let fn = DummyRaceFunction(name: name)
                        AGenUISDK.registerFunction(fn)
                    }
                }
                group.leave()
            }
        }

        // Release all threads simultaneously to maximize contention
        for _ in 0..<threadCount {
            startBarrier.signal()
        }

        // Wait for completion or crash (test process will die on EXC_BAD_ACCESS)
        let result = group.wait(timeout: .now() + testDuration)

        // Cleanup: unregister all functions we created
        for threadIdx in 0..<threadCount {
            for i in 0..<iterationsPerThread {
                AGenUISDK.unregisterFunction("raceFunc_t\(threadIdx)_i\(i)")
            }
        }

        // If we reach here without crash, the race didn't manifest this time
        // But with high thread count and iterations, it should crash most runs
        if result == .timedOut {
            NSLog("[RISK42] Test timed out — threads may be deadlocked or crashed")
            XCTFail("RISK42: Test timed out, possible deadlock or crash")
        } else {
            NSLog("[RISK42] All threads completed without crash (this run)")
            NSLog("[RISK42] Note: Race may not manifest every run; increase iterations if needed")
        }
    }

    // MARK: - RISK42b: Concurrent register + unregister interleaved

    /// One set of threads registers functions while another set unregisters them.
    /// This maximizes the chance of CoW buffer corruption during resize/shrink.
    func testRISK42b_concurrentRegisterUnregisterInterleaved() throws {
        let group = DispatchGroup()
        let startBarrier = DispatchSemaphore(value: 0)
        let totalThreads = threadCount * 2  // half register, half unregister

        // Pre-register some functions so unregister threads have work to do
        let sharedNames = (0..<200).map { "shared_\($0)" }
        for name in sharedNames {
            let fn = DummyRaceFunction(name: name)
            AGenUISDK.registerFunction(fn)
        }

        // Register threads
        for threadIdx in 0..<threadCount {
            group.enter()
            Thread.detachNewThread {
                startBarrier.wait()
                for i in 0..<self.iterationsPerThread {
                    autoreleasepool {
                        let name = "reg_t\(threadIdx)_i\(i)"
                        let fn = DummyRaceFunction(name: name)
                        AGenUISDK.registerFunction(fn)
                    }
                }
                group.leave()
            }
        }

        // Unregister threads — remove shared names and re-add them in a loop
        for threadIdx in 0..<threadCount {
            group.enter()
            Thread.detachNewThread {
                startBarrier.wait()
                for i in 0..<self.iterationsPerThread {
                    autoreleasepool {
                        let nameIdx = i % sharedNames.count
                        let name = sharedNames[nameIdx]
                        AGenUISDK.unregisterFunction(name)
                        // Re-register immediately to keep contention high
                        let fn = DummyRaceFunction(name: name)
                        AGenUISDK.registerFunction(fn)
                    }
                }
                group.leave()
            }
        }

        // Release all threads at once
        for _ in 0..<totalThreads {
            startBarrier.signal()
        }

        let result = group.wait(timeout: .now() + testDuration)

        // Cleanup
        for name in sharedNames {
            AGenUISDK.unregisterFunction(name)
        }
        for threadIdx in 0..<threadCount {
            for i in 0..<iterationsPerThread {
                AGenUISDK.unregisterFunction("reg_t\(threadIdx)_i\(i)")
            }
        }

        if result == .timedOut {
            NSLog("[RISK42b] Test timed out — possible deadlock or crash")
            XCTFail("RISK42b: Test timed out")
        } else {
            NSLog("[RISK42b] Completed without crash this run")
        }
    }

    // MARK: - RISK42c: Rapid register/unregister same name from multiple threads

    /// Multiple threads fight over the same function name — register and unregister
    /// the exact same key concurrently. This targets the dictionary's internal
    /// hash-table bucket manipulation (insert vs delete on same slot).
    func testRISK42c_sameNameConcurrentRegisterUnregister() throws {
        let group = DispatchGroup()
        let startBarrier = DispatchSemaphore(value: 0)
        let sharedName = "contested_function"
        let iterations = 1000

        for threadIdx in 0..<(threadCount * 2) {
            group.enter()
            Thread.detachNewThread {
                startBarrier.wait()
                for _ in 0..<iterations {
                    autoreleasepool {
                        if threadIdx % 2 == 0 {
                            let fn = DummyRaceFunction(name: sharedName)
                            AGenUISDK.registerFunction(fn)
                        } else {
                            AGenUISDK.unregisterFunction(sharedName)
                        }
                    }
                }
                group.leave()
            }
        }

        for _ in 0..<(threadCount * 2) {
            startBarrier.signal()
        }

        let result = group.wait(timeout: .now() + testDuration)

        AGenUISDK.unregisterFunction(sharedName)

        if result == .timedOut {
            NSLog("[RISK42c] Test timed out — possible deadlock or crash")
            XCTFail("RISK42c: Test timed out")
        } else {
            NSLog("[RISK42c] Completed without crash this run")
        }
    }
}

// MARK: - Helper Types

/// Minimal Function implementation for race testing
private final class DummyRaceFunction: NSObject, Function {
    let functionConfig: FunctionConfig

    init(name: String) {
        self.functionConfig = FunctionConfig(name: name)
    }

    func execute(context: FunctionCallContext, params: String) -> FunctionResult {
        return FunctionResult.success(value: "{\"result\":\"dummy\"}")
    }
}

/// Simple atomic boolean for cross-thread flag
private final class AtomicBool {
    private let lock = NSLock()
    private var value: Bool

    init(initial: Bool) {
        self.value = initial
    }

    func get() -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return value
    }

    func set(_ newValue: Bool) {
        lock.lock()
        value = newValue
        lock.unlock()
    }
}
