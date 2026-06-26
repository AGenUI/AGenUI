import XCTest
@testable import Playground
@testable import AGenUI

/// RISK22/23 iOS equivalent: FunctionCallManager re-entrant deadlock.
///
/// Root cause (shared core/):
///   FunctionCallManager._mutex is std::mutex (non-recursive).
///   executeFunctionCallSync() holds the lock and calls platform function
///   directly on the same thread (AGenUIEngineFunction::callSync → ObjC callback).
///   If the callback calls registerFunction/unregisterFunction, the same thread
///   tries to re-acquire the mutex → permanent deadlock.
///
/// Detection: After the deadlock, subsequent SM operations (posted to the same
/// worker thread) never complete — we verify this via a timeout.
final class SDKRiskProbeReentrantDeadlockTest: XCTestCase {

    private let setupTimeout: TimeInterval = 10.0
    private let deadlockCheckTimeout: TimeInterval = 5.0

    // MARK: - JSON builders

    private func buildCreateSurfaceJSON(surfaceId: String) -> String {
        return "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"\(surfaceId)\",\"catalogId\":\"https://a2ui.org/specification/v0_9/basic_catalog.json\"}}"
    }

    private func buildUpdateWithFunctionCallBinding(surfaceId: String, funcName: String) -> String {
        return "{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"\(surfaceId)\",\"components\":[{\"id\":\"root\",\"component\":\"Column\",\"children\":[\"txt1\"]},{\"id\":\"txt1\",\"component\":\"Text\",\"text\":{\"call\":\"\(funcName)\",\"args\":{\"input\":\"test\"}}}]}}"
    }

    // MARK: - Helper

    /// Check if the worker thread responds to a new create-surface operation.
    /// Returns true if onCreateSurface fires within the timeout.
    private func isWorkerThreadAlive(sm: SurfaceManager, surfaceId: String) -> Bool {
        let expectation = XCTestExpectation(description: "Worker thread alive check: \(surfaceId)")
        let listener = WorkerAliveListener(targetSurfaceId: surfaceId) {
            expectation.fulfill()
        }
        sm.addListener(listener)
        sm.beginTextStream()
        sm.receiveTextChunk(buildCreateSurfaceJSON(surfaceId: surfaceId))
        sm.endTextStream()
        let result = XCTWaiter().wait(for: [expectation], timeout: deadlockCheckTimeout)
        sm.removeListener(listener)
        return result == .completed
    }

    // MARK: - RISK22: Re-entrant deadlock via registerFunction

    /// Platform function execute() calls AGenUISDK.registerFunction() while
    /// executeFunctionCallSync holds _mutex on the same thread.
    /// Expected: permanent deadlock on worker thread.
    func testRISK22_iOS_reentrantDeadlockViaRegisterFunction() throws {
        let sm = SurfaceManager()
        var functionCallCount = 0
        var deadlockDetected = false

        // Register a function whose execute() re-enters via registerFunction
        let reEntrantFunc = ReEntrantRegisterFunction(onExecute: { count in
            functionCallCount = count
        })
        AGenUISDK.registerFunction(reEntrantFunc)

        // Stream content with function call data binding
        let surfaceId = "deadlock-ios-s1"
        let stream = buildCreateSurfaceJSON(surfaceId: surfaceId)
            + buildUpdateWithFunctionCallBinding(surfaceId: surfaceId, funcName: "reEntrantFuncIOS")

        sm.beginTextStream()
        sm.receiveTextChunk(stream)
        sm.endTextStream()

        // Wait for the deadlock to take effect
        Thread.sleep(forTimeInterval: 3.0)

        // Check if worker thread is alive
        let alive = isWorkerThreadAlive(sm: sm, surfaceId: "probe-ios-alive-1")
        if !alive && functionCallCount > 0 {
            deadlockDetected = true
            NSLog("[RiskProbe-Deadlock] *** iOS DEADLOCK DETECTED (RISK22: registerFunction) ***")
            NSLog("[RiskProbe-Deadlock] Function called %d time(s), worker thread frozen", functionCallCount)
        }

        NSLog("[RiskProbe-Deadlock] === RISK22 iOS RESULT ===")
        NSLog("[RiskProbe-Deadlock] Deadlock detected: %@", deadlockDetected ? "true" : "false")
        NSLog("[RiskProbe-Deadlock] Function call count: %d", functionCallCount)

        if deadlockDetected {
            NSLog("[RiskProbe-Deadlock] ROOT CAUSE: Same as Android — FunctionCallManager._mutex is non-recursive std::mutex")
            NSLog("[RiskProbe-Deadlock] iOS callSync path: executeFunctionCallSync → AGenUIEngineFunction::callSync → ObjC callback (same thread)")
        }

        // Do NOT call unregisterFunction — would block on the deadlocked mutex
        // SurfaceManager will leak but test can still complete

        XCTAssertTrue(deadlockDetected,
            "RISK22-iOS: Deadlock should be detected (FunctionCallManager mutex is non-recursive, same thread re-entry)")
    }

    // MARK: - RISK23: Re-entrant deadlock via unregisterFunction

    /// Platform function execute() calls AGenUISDK.unregisterFunction() (one-shot pattern)
    /// while executeFunctionCallSync holds _mutex on the same thread.
    /// Expected: permanent deadlock on worker thread.
    func testRISK23_iOS_reentrantDeadlockViaUnregisterFunction() throws {
        let sm = SurfaceManager()
        var callCount = 0
        var deadlockDetected = false

        // Register a "one-shot" function that unregisters itself
        let selfUnregFunc = SelfUnregisterFunction(onExecute: { count in
            callCount = count
        })
        AGenUISDK.registerFunction(selfUnregFunc)

        // Stream content with function call data binding
        let surfaceId = "deadlock-ios-unreg-s1"
        let stream = buildCreateSurfaceJSON(surfaceId: surfaceId)
            + buildUpdateWithFunctionCallBinding(surfaceId: surfaceId, funcName: "selfUnregFuncIOS")

        sm.beginTextStream()
        sm.receiveTextChunk(stream)
        sm.endTextStream()

        // Wait for deadlock
        Thread.sleep(forTimeInterval: 3.0)

        // Check worker thread liveness
        let alive = isWorkerThreadAlive(sm: sm, surfaceId: "probe-ios-unreg-1")
        if !alive && callCount > 0 {
            deadlockDetected = true
            NSLog("[RiskProbe-Deadlock] *** iOS DEADLOCK DETECTED (RISK23: unregisterFunction) ***")
        }

        NSLog("[RiskProbe-Deadlock] === RISK23 iOS RESULT ===")
        NSLog("[RiskProbe-Deadlock] Deadlock detected: %@", deadlockDetected ? "true" : "false")
        NSLog("[RiskProbe-Deadlock] Function call count: %d", callCount)

        if deadlockDetected {
            NSLog("[RiskProbe-Deadlock] One-shot pattern (unregister self) triggers same mutex deadlock on iOS")
        }

        XCTAssertTrue(deadlockDetected,
            "RISK23-iOS: Deadlock should be detected (one-shot unregister self)")
    }
}

// MARK: - Function implementations

/// Function that re-enters via registerFunction during execute()
private final class ReEntrantRegisterFunction: NSObject, Function {
    var functionConfig: FunctionConfig { FunctionConfig(name: "reEntrantFuncIOS") }
    private let onExecute: (Int) -> Void
    private var count = 0

    init(onExecute: @escaping (Int) -> Void) {
        self.onExecute = onExecute
    }

    func execute(context: FunctionCallContext, params: String) -> FunctionResult {
        count += 1
        onExecute(count)
        NSLog("[RiskProbe-Deadlock] reEntrantFuncIOS called (count=%d), about to re-enter...", count)

        // RE-ENTRANT CALL: This runs on the worker thread while
        // executeFunctionCallSync holds FunctionCallManager._mutex.
        let innerFunc = SimpleFunction(name: "innerFunc_\(ProcessInfo.processInfo.systemUptime)")
        AGenUISDK.registerFunction(innerFunc)
        NSLog("[RiskProbe-Deadlock] reEntrantFuncIOS: registerFunction returned (no deadlock)")

        return FunctionResult.success(value: "{\"result\":\"done\"}")
    }
}

/// Function that unregisters itself during execute() (one-shot pattern)
private final class SelfUnregisterFunction: NSObject, Function {
    var functionConfig: FunctionConfig { FunctionConfig(name: "selfUnregFuncIOS") }
    private let onExecute: (Int) -> Void
    private var count = 0

    init(onExecute: @escaping (Int) -> Void) {
        self.onExecute = onExecute
    }

    func execute(context: FunctionCallContext, params: String) -> FunctionResult {
        count += 1
        onExecute(count)
        NSLog("[RiskProbe-Deadlock] selfUnregFuncIOS called (count=%d), about to unregister self...", count)

        // RE-ENTRANT CALL: unregister self while holding the mutex
        AGenUISDK.unregisterFunction("selfUnregFuncIOS")
        NSLog("[RiskProbe-Deadlock] selfUnregFuncIOS: unregisterFunction returned (no deadlock)")

        return FunctionResult.success(value: "{\"result\":\"one-shot done\"}")
    }
}

/// Simple function for inner registration
private final class SimpleFunction: NSObject, Function {
    let functionConfig: FunctionConfig
    init(name: String) {
        self.functionConfig = FunctionConfig(name: name)
    }
    func execute(context: FunctionCallContext, params: String) -> FunctionResult {
        return FunctionResult.success(value: "{\"result\":\"inner\"}")
    }
}

/// Listener that detects Surface creation for worker thread liveness check
private final class WorkerAliveListener: NSObject, SurfaceManagerListener {
    private let targetSurfaceId: String
    private let callback: () -> Void

    init(targetSurfaceId: String, callback: @escaping () -> Void) {
        self.targetSurfaceId = targetSurfaceId
        self.callback = callback
    }

    func onCreateSurface(_ surface: Surface) {
        if surface.surfaceId == targetSurfaceId {
            callback()
        }
    }
}
