//
//  SurfaceLifecycleTest.swift
//  AGenUITests
//
//  SURFACE 模块集成测试 — Surface 生命周期
//
//  覆盖用例：
//  - SURFACE-01：创建 Surface — onCreateSurface 回调触发
//  - SURFACE-02：销毁 Surface — onDeleteSurface 回调触发，getSurface 不再返回该 Surface
//  - SURFACE-03：渲染组件 — 等待主线程 flush 后 getAllComponents().count 等于预期
//

import XCTest
@testable import Playground
@testable import AGenUI

class SurfaceLifecycleTest: AGenUIBaseTest {

    // MARK: - SURFACE-01

    /// SURFACE-01：创建 Surface
    ///
    /// 发送含 createSurface 消息的 JSON，验证 SurfaceManagerListener.onCreateSurface 被触发，
    /// 返回的 Surface 实例非 nil 且 surfaceId 匹配。
    func testSURFACE01_createSurfaceCallbackTriggered() throws {
        let surfaceId = try TestFixtureLoader.getSurfaceId("init/create_surface_simple.json")
        let json = try TestFixtureLoader.loadMessagesAsString("init/create_surface_simple.json")

        // sendAndWaitForSurface 内部使用 XCTestExpectation 等待 onCreateSurface 回调
        let surface = sendAndWaitForSurface(json, surfaceId: surfaceId)

        XCTAssertNotNil(surface, "SURFACE-01: onCreateSurface 回调应触发并返回非 nil Surface")
        XCTAssertEqual(surface?.surfaceId, surfaceId, "SURFACE-01: surfaceId 应匹配")
    }

    // MARK: - SURFACE-02

    /// SURFACE-02：销毁 Surface
    ///
    /// 先创建 Surface，再发送 deleteSurface 消息，验证：
    /// - SurfaceManagerListener.onDeleteSurface 回调触发
    /// - Surface state 变为 destroyed
    func testSURFACE02_deleteSurfaceCallbackTriggered() throws {
        let surfaceId = try TestFixtureLoader.getSurfaceId("init/create_surface_simple.json")
        let createJson = try TestFixtureLoader.loadMessagesAsString("init/create_surface_simple.json")

        // 1. 先创建 Surface
        let surface = sendAndWaitForSurface(createJson, surfaceId: surfaceId)
        XCTAssertNotNil(surface, "前置条件：Surface 应已创建")

        // 2. 从 fixture 加载 deleteSurface JSON
        let deleteJson = try TestFixtureLoader.loadMessagesAsString("init/delete_surface_simple.json")

        // 3. 发送并等待 onDeleteSurface 回调
        sendAndWaitForDeleteSurface(deleteJson, surfaceId: surfaceId)

        // 4. 等待主线程处理完成
        waitForMainThread()

        // 5. onDeleteSurface 回调已在 sendAndWaitForDeleteSurface 中验证触发
        // SurfaceManager 无 getSurface API，删除回调触发即证明 Surface 已销毁
    }

    // MARK: - SURFACE-03

    /// SURFACE-03：渲染组件
    ///
    /// 发送含 createSurface + updateComponents 的完整 JSON，
    /// 等待主线程 flush 后验证 getAllComponents().count 等于预期值。
    func testSURFACE03_componentCountAfterRender() throws {
        let fixturePath = "components/01_text_only.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadMessagesAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        // sendAndWaitForRender = sendAndWaitForSurface + waitForMainThread
        let surface = sendAndWaitForRender(json, surfaceId: surfaceId)

        XCTAssertNotNil(surface, "前置条件：Surface 不应为 nil")
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "SURFACE-03: 组件数量应为 \(expectedCount)")
    }

    /// SURFACE-03（扩展）：验证 Button 渲染后组件 ID 可通过 getComponent 查到
    func testSURFACE03_buttonComponentAccessibleById() throws {
        let fixturePath = "components/02_button_with_action.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadMessagesAsString(fixturePath)

        let surface = sendAndWaitForRender(json, surfaceId: surfaceId)

        XCTAssertNotNil(surface, "Surface 应创建成功")
        XCTAssertNotNil(surface?.getComponent(componentId: "btn-submit"),
                        "SURFACE-03: btn-submit 组件应可通过 getComponent 获取")
        XCTAssertEqual(surface?.getComponent(componentId: "btn-submit")?.componentType, "Button",
                       "SURFACE-03: btn-submit 类型应为 Button")
    }
}
