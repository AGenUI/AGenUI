//
//  MultiSurfaceTest.swift
//  AGenUITests
//
//  MULTI 模块集成测试 — 同一 SM 内多 surfaceId 隔离性
//
//  覆盖用例：
//  - MULTI-01：同一 SM 依次创建 3 个 Surface，onCreateSurface 触发 3 次，各 surfaceId 独立
//  - MULTI-02：surfaceId-A 和 surfaceId-B 组件树内容互不影响
//  - MULTI-03：销毁其中一个 surfaceId，另一个组件树完整
//

import XCTest
@testable import Playground
@testable import AGenUI

class MultiSurfaceTest: AGenUIBaseTest {

    // MARK: - MULTI-01

    /// MULTI-01：同一 SM 依次创建 3 个 Surface，onCreateSurface 触发 3 次
    func testMULTI01_createThreeSurfacesCallbackFiredThreeTimes() {
        let surfaceIds = [
            "test-surf-multi-01-a",
            "test-surf-multi-01-b",
            "test-surf-multi-01-c"
        ]

        let expectation = XCTestExpectation(description: "3 Surfaces created")
        expectation.expectedFulfillmentCount = 3
        var callbackCount = 0
        let lock = NSLock()

        class MultiListener: NSObject, SurfaceManagerListener {
            let targets: [String]
            let expectation: XCTestExpectation
            var count: Int = 0
            let lock: NSLock

            init(targets: [String], expectation: XCTestExpectation, lock: NSLock) {
                self.targets = targets
                self.expectation = expectation
                self.lock = lock
            }

            func onCreateSurface(_ surface: Surface) {
                if targets.contains(surface.surfaceId) {
                    lock.lock()
                    count += 1
                    lock.unlock()
                    expectation.fulfill()
                }
            }
        }

        let listener = MultiListener(targets: surfaceIds, expectation: expectation, lock: lock)
        surfaceManager.addListener(listener)

        // 依次发送 3 个 createSurface 消息
        for sid in surfaceIds {
            let json = "{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"\(sid)\",\"catalogId\":\"https://a2ui.org/specification/v0_9/standard_catalog.json\",\"animated\":false}}"
            surfaceManager.beginTextStream()
            surfaceManager.receiveTextChunk(json)
            surfaceManager.endTextStream()
        }

        let result = XCTWaiter().wait(for: [expectation], timeout: AGenUIBaseTest.timeoutSeconds)
        surfaceManager.removeListener(listener)
        callbackCount = listener.count

        XCTAssertEqual(result, .completed, "MULTI-01: 3 个 Surface 创建超时")
        XCTAssertEqual(callbackCount, 3, "MULTI-01: onCreateSurface 应触发 3 次")
    }

    // MARK: - MULTI-02

    /// MULTI-02：surfaceId-A（Button）与 surfaceId-B（Text）组件树相互独立
    func testMULTI02_twoSurfacesComponentTreesAreIsolated() throws {
        let fixPathA = "multi_surface/surface_a.json"
        let fixPathB = "multi_surface/surface_b.json"

        let surfaceIdA = try TestFixtureLoader.getSurfaceId(fixPathA)
        let surfaceIdB = try TestFixtureLoader.getSurfaceId(fixPathB)
        let jsonA = try TestFixtureLoader.loadMessagesAsString(fixPathA)
        let jsonB = try TestFixtureLoader.loadMessagesAsString(fixPathB)
        let expectA = try TestFixtureLoader.getExpect(fixPathA)
        let expectB = try TestFixtureLoader.getExpect(fixPathB)
        let expectedCountA = expectA["componentCount"] as? Int ?? 0
        let expectedCountB = expectB["componentCount"] as? Int ?? 0

        // 依次创建两个 Surface 并渲染
        let surfaceA = sendAndWaitForRender(jsonA, surfaceId: surfaceIdA)
        let surfaceB = sendAndWaitForRender(jsonB, surfaceId: surfaceIdB)

        XCTAssertNotNil(surfaceA, "MULTI-02: Surface A 应创建成功")
        XCTAssertNotNil(surfaceB, "MULTI-02: Surface B 应创建成功")

        // 验证组件数量
        XCTAssertEqual(surfaceA?.getAllComponents().count, expectedCountA,
                       "MULTI-02: Surface A 组件数应为 \(expectedCountA)")
        XCTAssertEqual(surfaceB?.getAllComponents().count, expectedCountB,
                       "MULTI-02: Surface B 组件数应为 \(expectedCountB)")

        // 验证隔离性：A 中不应包含 B 的组件，反之亦然
        XCTAssertNil(surfaceA?.getComponent(componentId: "text-b1"),
                     "MULTI-02: Surface A 不应包含 Surface B 的 text-b1 组件")
        XCTAssertNil(surfaceB?.getComponent(componentId: "btn-a"),
                     "MULTI-02: Surface B 不应包含 Surface A 的 btn-a 组件")

        // 验证各自的特有组件存在
        XCTAssertNotNil(surfaceA?.getComponent(componentId: "btn-a"),
                        "MULTI-02: Surface A 应包含 btn-a 组件")
        XCTAssertNotNil(surfaceB?.getComponent(componentId: "text-b1"),
                        "MULTI-02: Surface B 应包含 text-b1 组件")
    }

    // MARK: - MULTI-03

    /// MULTI-03：销毁 surfaceId-A 后，surfaceId-B 组件树完整
    func testMULTI03_deleteSurfaceADoesNotAffectSurfaceB() throws {
        let fixPathA = "multi_surface/surface_a.json"
        let fixPathB = "multi_surface/surface_b.json"

        let surfaceIdA = try TestFixtureLoader.getSurfaceId(fixPathA)
        let surfaceIdB = try TestFixtureLoader.getSurfaceId(fixPathB)
        let expectB = try TestFixtureLoader.getExpect(fixPathB)
        let expectedCountB = expectB["componentCount"] as? Int ?? 0

        // 1. 创建两个 Surface
        let surfaceA = sendAndWaitForRender(try TestFixtureLoader.loadMessagesAsString(fixPathA), surfaceId: surfaceIdA)
        let surfaceB = sendAndWaitForRender(try TestFixtureLoader.loadMessagesAsString(fixPathB), surfaceId: surfaceIdB)
        XCTAssertNotNil(surfaceA, "前置条件：Surface A 应已创建")
        XCTAssertNotNil(surfaceB, "前置条件：Surface B 应已创建")

        // 2. 从 fixture 加载 deleteSurface JSON
        let deleteJsonA = try TestFixtureLoader.loadMessagesAsString("multi_surface/delete_surface_a.json")
        sendAndWaitForDeleteSurface(deleteJsonA, surfaceId: surfaceIdA)
        waitForMainThread()

        // 3. Surface A 删除回调已在 sendAndWaitForDeleteSurface 中验证
        // SurfaceManager 无 state 属性，回调触发即证明 A 已销毁

        // 4. Surface B 组件树应完整（未受 A 删除影响）
        XCTAssertEqual(surfaceB?.getAllComponents().count, expectedCountB,
                       "MULTI-03: Surface B 组件数仍应为 \(expectedCountB)")
        XCTAssertNotNil(surfaceB?.getComponent(componentId: "text-b1"),
                        "MULTI-03: Surface B 中 text-b1 组件仍应存在")
    }
}
