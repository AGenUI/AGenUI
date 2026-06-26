import XCTest
@testable import Playground
@testable import AGenUI

final class SDKRiskProbeFunctionUnregisterRaceTest: AGenUIBaseTest {

    private let functionName = "toast"
    private let roundDuration: TimeInterval = 5.0

    func testSDKRISK03_functionExecuteVsUnregisterRace() throws {
        let fixturePath = "function_call/action_toast.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadMessagesAsString(fixturePath)

        let surface = sendAndWaitForRender(json, surfaceId: surfaceId)
        XCTAssertNotNil(surface, "Surface should render for race probe")

        guard let button = surface?.getComponent(componentId: "toast-btn") else {
            XCTFail("toast-btn should exist")
            return
        }

        let stop = LockedFlag()
        let executeCount = LockedCounter()
        let registerCount = LockedCounter()

        let worker = DispatchQueue(label: "sdk-risk.unregister-race", qos: .userInitiated)
        let done = expectation(description: "register loop finished")

        worker.async {
            while !stop.get() {
                autoreleasepool {
                    let fn = SlowToastFunction(executeCount: executeCount)
                    AGenUISDK.registerFunction(fn)
                    registerCount.increment()
                    AGenUISDK.unregisterFunction(self.functionName)
                }
            }
            done.fulfill()
        }

        let deadline = Date().addingTimeInterval(roundDuration)
        while Date() < deadline {
            DispatchQueue.main.sync {
                button.triggerAction()
            }
            RunLoop.current.run(until: Date().addingTimeInterval(0.001))
        }

        stop.set(true)
        wait(for: [done], timeout: 3.0)
        AGenUISDK.unregisterFunction(functionName)

        XCTAssertGreaterThan(registerCount.get(), 0, "Expected at least one register/unregister iteration")
    }
}

private final class SlowToastFunction: NSObject, Function {
    let functionConfig = FunctionConfig(name: "toast")
    private let executeCount: LockedCounter

    init(executeCount: LockedCounter) {
        self.executeCount = executeCount
    }

    func execute(context: FunctionCallContext, params: String) -> FunctionResult {
        executeCount.increment()
        Thread.sleep(forTimeInterval: 0.002)
        return FunctionResult.success(value: "{\"result\":\"ok\"}")
    }
}

private final class LockedFlag {
    private let lock = NSLock()
    private var value = false

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

private final class LockedCounter {
    private let lock = NSLock()
    private var value = 0

    func increment() {
        lock.lock()
        value += 1
        lock.unlock()
    }

    func get() -> Int {
        lock.lock()
        defer { lock.unlock() }
        return value
    }
}
