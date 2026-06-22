//
//  AGenUIBaseTest.swift
//  AGenUITests
//
//  AGenUI 集成测试基类
//
//  封装通用能力：
//  - SurfaceManager 初始化与清理
//  - 异步等待：sendAndWaitForSurface + waitForMainThread
//  - Surface 销毁等待：sendAndWaitForDeleteSurface
//
//  异步机制说明：
//  receiveTextChunk() 立即返回，但 Surface 创建与组件填充通过
//  NotificationCenter 异步回调完成。测试必须等待回调后再断言。
//

import XCTest
@testable import Playground
@testable import AGenUI

class AGenUIBaseTest: XCTestCase {

    static let timeoutSeconds: TimeInterval = 5.0

    /// 被测 SurfaceManager，每个测试方法独立创建/释放
    var surfaceManager: SurfaceManager!

    override func setUpWithError() throws {
        try super.setUpWithError()
        // 每个测试用例独立创建 SurfaceManager 实例（独立渲染上下文）
        surfaceManager = SurfaceManager()
    }

    override func tearDownWithError() throws {
        surfaceManager = nil
        try super.tearDownWithError()
    }

    // MARK: - 异步等待工具

    /// 以 beginTextStream / receiveTextChunk / endTextStream 发送完整 JSON，
    /// 并阻塞等待指定 surfaceId 的 onCreateSurface 回调触发。
    ///
    /// - Parameters:
    ///   - json: 符合 A2UI 协议的完整 JSON 字符串
    ///   - surfaceId: 期待创建的 surfaceId
    /// - Returns: 创建完成的 Surface 实例
    @discardableResult
    func sendAndWaitForSurface(_ json: String, surfaceId: String) -> Surface? {
        let expectation = XCTestExpectation(description: "Surface created: \(surfaceId)")
        var capturedSurface: Surface?

        let listener = SurfaceCreatedListener(targetSurfaceId: surfaceId) { surface in
            capturedSurface = surface
            expectation.fulfill()
        }
        surfaceManager.addListener(listener)

        surfaceManager.beginTextStream()
        surfaceManager.receiveTextChunk(json)
        surfaceManager.endTextStream()

        let result = XCTWaiter().wait(for: [expectation], timeout: AGenUIBaseTest.timeoutSeconds)
        surfaceManager.removeListener(listener)

        if result != .completed {
            XCTFail("Surface 创建超时（surfaceId=\(surfaceId)）")
            return nil
        }
        return capturedSurface
    }

    /// 发送含 deleteSurface 消息的 JSON，并等待 onDeleteSurface 回调触发。
    ///
    /// - Parameters:
    ///   - json: 含 deleteSurface 的 JSON 字符串
    ///   - surfaceId: 期待销毁的 surfaceId
    func sendAndWaitForDeleteSurface(_ json: String, surfaceId: String) {
        let expectation = XCTestExpectation(description: "Surface deleted: \(surfaceId)")

        let listener = SurfaceDeletedListener(targetSurfaceId: surfaceId) {
            expectation.fulfill()
        }
        surfaceManager.addListener(listener)

        surfaceManager.beginTextStream()
        surfaceManager.receiveTextChunk(json)
        surfaceManager.endTextStream()

        let result = XCTWaiter().wait(for: [expectation], timeout: AGenUIBaseTest.timeoutSeconds)
        surfaceManager.removeListener(listener)

        if result != .completed {
            XCTFail("Surface 销毁超时（surfaceId=\(surfaceId)）")
        }
    }

    /// 等待主线程所有 pending 任务执行完毕，确保组件已写入 componentTree。
    func waitForMainThread() {
        let expectation = XCTestExpectation(description: "Main thread flush")
        DispatchQueue.main.async {
            expectation.fulfill()
        }
        XCTWaiter().wait(for: [expectation], timeout: AGenUIBaseTest.timeoutSeconds)
    }

    /// 组合发送 JSON + 等待 Surface 创建 + 等待组件渲染完成
    ///
    /// C++ 引擎在后台线程解析 JSON，通过 dispatch_async(main_queue) 分别派发
    /// onCreateSurface 和 onUpdateComponents。两者的 dispatch 时间差不确定，
    /// 因此使用 RunLoop polling 等待组件树稳定（连续多次采样 count 不变）。
    ///
    /// - Parameters:
    ///   - json: A2UI 协议 JSON 字符串
    ///   - surfaceId: 期待创建的 surfaceId
    /// - Returns: 已完成组件渲染的 Surface
    @discardableResult
    func sendAndWaitForRender(_ json: String, surfaceId: String) -> Surface? {
        let surface = sendAndWaitForSurface(json, surfaceId: surfaceId)
        waitForComponentsStable(surface)
        return surface
    }

    /// 等待 Surface 的组件树稳定（连续 stableThreshold 次采样 count 不变）
    ///
    /// 当含数据绑定的组件（如 `${user.name}`）需要等 updateDataModel 后才会
    /// 通过 checkCanDisplay 派发到 iOS 桥接层时，组件会分多批异步到达。
    /// 提供 expectedCount 可避免在中间批次误判为 "已稳定"。
    ///
    /// - Parameters:
    ///   - surface: 目标 Surface
    ///   - expectedCount: 期望的最终组件数（可选）；提供时先等 count 达标再判稳定
    ///   - stableThreshold: 连续相同 count 的采样次数，默认 3
    func waitForComponentsStable(_ surface: Surface?, expectedCount: Int? = nil, stableThreshold: Int = 3) {
        guard surface != nil else { return }
        let deadline = Date().addingTimeInterval(AGenUIBaseTest.timeoutSeconds)

        // Phase 1: 若提供了 expectedCount，先等组件数达标
        if let expected = expectedCount {
            while (surface?.getAllComponents().count ?? 0) < expected && Date() < deadline {
                RunLoop.current.run(until: Date().addingTimeInterval(0.02))
            }
        }

        // Phase 2: 等组件树稳定（连续 stableThreshold 次采样 count 不变）
        var lastCount = -1
        var stableCount = 0
        while stableCount < stableThreshold && Date() < deadline {
            // 推进 RunLoop，处理主队列 pending 的 dispatch block
            RunLoop.current.run(until: Date().addingTimeInterval(0.02))
            let currentCount = surface?.getAllComponents().count ?? 0
            if currentCount == lastCount {
                stableCount += 1
            } else {
                lastCount = currentCount
                stableCount = 0
            }
        }
    }

    // MARK: - 流式分片工具

    /// 将完整 JSON 按 chunkSize 分片发送
    ///
    /// - Parameters:
    ///   - sm: 目标 SurfaceManager
    ///   - json: 完整 JSON 字符串
    ///   - chunkSize: 每片字符数
    func streamJson(to sm: SurfaceManager, json: String, chunkSize: Int) {
        sm.beginTextStream()
        var index = json.startIndex
        while index < json.endIndex {
            let end = json.index(index, offsetBy: chunkSize, limitedBy: json.endIndex) ?? json.endIndex
            sm.receiveTextChunk(String(json[index..<end]))
            index = end
        }
        sm.endTextStream()
    }
}

// MARK: - 内部辅助 Listener 类

/// 监听 Surface 创建事件的辅助类
private class SurfaceCreatedListener: NSObject, SurfaceManagerListener {
    private let targetSurfaceId: String
    private let callback: (Surface) -> Void

    init(targetSurfaceId: String, callback: @escaping (Surface) -> Void) {
        self.targetSurfaceId = targetSurfaceId
        self.callback = callback
    }

    func onCreateSurface(_ surface: Surface) {
        if surface.surfaceId == targetSurfaceId {
            callback(surface)
        }
    }
}

/// 监听 Surface 销毁事件的辅助类
private class SurfaceDeletedListener: NSObject, SurfaceManagerListener {
    private let targetSurfaceId: String
    private let callback: () -> Void

    init(targetSurfaceId: String, callback: @escaping () -> Void) {
        self.targetSurfaceId = targetSurfaceId
        self.callback = callback
    }

    func onDeleteSurface(_ surface: Surface) {
        if surface.surfaceId == targetSurfaceId {
            callback()
        }
    }
}
