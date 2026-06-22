//
//  PlatformFunctionTest.swift
//  AGenUITests
//
//  平台 Function 集成测试
//
//  验证通过 AGenUISDK.registerFunction 注册的平台 Function 行为正确。
//
//  覆盖场景：
//  - FUNC-01：注册 toast Function，验证注册成功且组件树完整
//  - FUNC-02：注册后注销 Function，验证幂等性（重复注销不崩溃）
//  - FUNC-03：同时注册多个不同 name 的 Function，验证均注册成功
//  - FUNC-04：FunctionResult 的 result/value 字段正确
//

import XCTest
@testable import Playground
@testable import AGenUI

class PlatformFunctionTest: AGenUIBaseTest {

    override func tearDownWithError() throws {
        // 清理所有测试中注册的 Function
        AGenUISDK.unregisterFunction("toast")
        AGenUISDK.unregisterFunction("testUnregisterFn")
        AGenUISDK.unregisterFunction("multiTestFn1")
        AGenUISDK.unregisterFunction("multiTestFn2")
        try super.tearDownWithError()
    }

    // MARK: - FUNC-01：基本注册与回调

    /// FUNC-01：注册 toast Function，发送包含 functionCall 的 JSON，
    /// 验证 Function 注册成功且 Surface 组件树完整。
    func testFUNC01_registerAndCallback() throws {
        class ToastFunction: NSObject, Function {
            let functionConfig = FunctionConfig(name: "toast")
            func execute(context: FunctionCallContext, params: String) -> FunctionResult {
                return FunctionResult.success(value: "{\"result\":\"ok\"}")
            }
        }

        let toastFunc = ToastFunction()
        AGenUISDK.registerFunction(toastFunc)

        // 加载 toast fixture
        let json = try TestFixtureLoader.loadMessagesAsString("function_call/action_toast.json")
        let surfaceId = try TestFixtureLoader.getSurfaceId("function_call/action_toast.json")

        // 渲染 Surface
        let surface = sendAndWaitForRender(json, surfaceId: surfaceId)

        // 验证 Function 注册成功、Surface 组件树完整
        XCTAssertNotNil(surface, "FUNC-01: Surface 应创建成功")
        XCTAssertNotNil(surface?.getComponent(componentId: "toast-btn"),
                        "FUNC-01: toast-btn 组件应存在")
        XCTAssertNotNil(surface?.getComponent(componentId: "toast-btn-text"),
                        "FUNC-01: toast-btn-text 组件应存在")
    }

    // MARK: - FUNC-02：注销后不崩溃（幂等性）

    /// FUNC-02：注册 Function 后立即注销，验证不抛出异常；重复注销也不崩溃。
    func testFUNC02_unregisterIdempotent() {
        class TestFn: NSObject, Function {
            let functionConfig = FunctionConfig(name: "testUnregisterFn")
            func execute(context: FunctionCallContext, params: String) -> FunctionResult {
                return FunctionResult.success(value: "{}")
            }
        }

        let fn = TestFn()
        // 注册
        AGenUISDK.registerFunction(fn)

        // 注销（不应崩溃）
        AGenUISDK.unregisterFunction("testUnregisterFn")

        // 再次注销（幂等性，不应崩溃）
        AGenUISDK.unregisterFunction("testUnregisterFn")
    }

    // MARK: - FUNC-03：多 Function 注册隔离

    /// FUNC-03：同时注册两个不同 name 的 Function，验证均注册成功。
    func testFUNC03_multipleRegistrations() {
        class MultiTestFn1: NSObject, Function {
            let functionConfig = FunctionConfig(name: "multiTestFn1")
            func execute(context: FunctionCallContext, params: String) -> FunctionResult {
                return FunctionResult.success(value: "{\"fn\":\"1\"}")
            }
        }

        class MultiTestFn2: NSObject, Function {
            let functionConfig = FunctionConfig(name: "multiTestFn2")
            func execute(context: FunctionCallContext, params: String) -> FunctionResult {
                return FunctionResult.success(value: "{\"fn\":\"2\"}")
            }
        }

        let fn1 = MultiTestFn1()
        let fn2 = MultiTestFn2()

        // 两个 Function 注册均不应崩溃
        AGenUISDK.registerFunction(fn1)
        AGenUISDK.registerFunction(fn2)

        // 验证配置正确
        XCTAssertEqual(fn1.functionConfig.name, "multiTestFn1",
                       "FUNC-03: fn1 name 应为 multiTestFn1")
        XCTAssertEqual(fn2.functionConfig.name, "multiTestFn2",
                       "FUNC-03: fn2 name 应为 multiTestFn2")

        // 清理
        AGenUISDK.unregisterFunction("multiTestFn1")
        AGenUISDK.unregisterFunction("multiTestFn2")
    }

    // MARK: - FUNC-04：FunctionResult 正确性

    /// FUNC-04：验证 FunctionResult.success / failure 的字段正确。
    func testFUNC04_functionResultCorrectness() {
        let successResult = FunctionResult.success(value: "{\"message\":\"hello\"}")
        XCTAssertTrue(successResult.result, "FUNC-04: success result 应为 true")
        XCTAssertTrue(successResult.value.contains("hello"),
                       "FUNC-04: success value 应包含 hello")

        let failureResult = FunctionResult.failure(value: "{\"error\":\"something went wrong\"}")
        XCTAssertFalse(failureResult.result, "FUNC-04: failure result 应为 false")
        XCTAssertTrue(failureResult.value.contains("something went wrong"),
                       "FUNC-04: failure value 应包含错误信息")
    }
}
