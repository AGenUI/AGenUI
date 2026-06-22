//
//  StreamTest.swift
//  AGenUITests
//
//  流式数据集成测试
//
//  验证 A2UI 协议在不同 chunk 分片策略下，最终 Surface 组件树与完整 JSON 一次性发送结果一致。
//
//  覆盖场景：
//  - STREAM-01：小 chunk（chunkSize=10）流式发送 Button 场景
//  - STREAM-02：大 chunk（chunkSize=500）流式发送 Button 场景
//  - STREAM-03：单字符（chunkSize=1）流式发送 Button 场景
//  - STREAM-04：一次性发送（整体）Button 场景
//  - STREAM-05：复杂嵌套布局（nested_layout）大 chunk 流式发送
//  - STREAM-06：含 dataModel 场景（chunkSize=20）
//  - STREAM-07：静态预分片（key 边界截断，chunk_01.txt + chunk_02.txt）
//  - STREAM-08：中途 reset（beginTextStream 后再次 beginTextStream），验证旧 Surface 不产生
//

import XCTest
@testable import Playground
@testable import AGenUI

class StreamTest: AGenUIBaseTest {

    // MARK: - 辅助方法

    /// 以指定 chunkSize 分片发送完整 JSON，等待 surfaceId 对应的 Surface 创建并完成渲染。
    ///
    /// - Parameters:
    ///   - fullJson: 完整 A2UI JSON 字符串
    ///   - surfaceId: 期待创建的 surfaceId
    ///   - chunkSize: 每片字符数（0 表示整体发送）
    /// - Returns: 渲染完成的 Surface（超时则返回 nil）
    private func streamAndWaitForRender(_ fullJson: String, surfaceId: String, chunkSize: Int, expectedCount: Int? = nil) -> Surface? {
        let expectation = XCTestExpectation(description: "Stream surface created: \(surfaceId)")
        var capturedSurface: Surface?

        class StreamListener: NSObject, SurfaceManagerListener {
            let targetId: String
            let expectation: XCTestExpectation
            var surface: Surface?
            init(_ targetId: String, _ expectation: XCTestExpectation) {
                self.targetId = targetId
                self.expectation = expectation
            }
            func onCreateSurface(_ surface: Surface) {
                if surface.surfaceId == targetId {
                    self.surface = surface
                    expectation.fulfill()
                }
            }
        }

        let listener = StreamListener(surfaceId, expectation)
        surfaceManager.addListener(listener)

        let chunks = TestFixtureLoader.splitIntoChunks(fullJson, chunkSize: chunkSize)
        surfaceManager.beginTextStream()
        for chunk in chunks {
            surfaceManager.receiveTextChunk(chunk)
        }
        surfaceManager.endTextStream()

        let result = XCTWaiter().wait(for: [expectation], timeout: AGenUIBaseTest.timeoutSeconds)
        surfaceManager.removeListener(listener)
        capturedSurface = listener.surface

        if result != .completed {
            XCTFail("流式 Surface 创建超时（surfaceId=\(surfaceId), chunkSize=\(chunkSize)）")
            return nil
        }

        // 等待组件树稳定
        waitForComponentsStable(capturedSurface, expectedCount: expectedCount)

        return capturedSurface
    }

    /// 获取参照组件数（以完整 JSON 一次性发送为基准）
    private func getReferenceComponentCount(_ fixturePath: String) throws -> Int {
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        // 用不同的 surfaceId 前缀避免与流式用例冲突（创建独立 SurfaceManager）
        let referenceSM = SurfaceManager()
        let json = try TestFixtureLoader.loadPayloadAsString(fixturePath)

        let exp = XCTestExpectation(description: "reference surface created")

        class RefListener: NSObject, SurfaceManagerListener {
            let targetId: String
            let exp: XCTestExpectation
            var surface: Surface?
            init(_ targetId: String, _ exp: XCTestExpectation) {
                self.targetId = targetId
                self.exp = exp
            }
            func onCreateSurface(_ surface: Surface) {
                if surface.surfaceId == targetId {
                    self.surface = surface
                    exp.fulfill()
                }
            }
        }

        let refListener = RefListener(surfaceId, exp)
        referenceSM.addListener(refListener)
        referenceSM.beginTextStream()
        referenceSM.receiveTextChunk(json)
        referenceSM.endTextStream()

        XCTWaiter().wait(for: [exp], timeout: AGenUIBaseTest.timeoutSeconds)
        referenceSM.removeListener(refListener)

        // 等待组件渲染完成（polling 稳定）
        waitForComponentsStable(refListener.surface)

        return refListener.surface?.getAllComponents().count ?? 0
    }

    // MARK: - STREAM-01：小 chunk（chunkSize=10）

    /// STREAM-01：小 chunk 流式（chunkSize=10）发送 Button 场景，
    /// 最终组件数与完整 JSON 一次性发送一致。
    func testSTREAM01_smallChunkButtonScene() throws {
        let fixturePath = "stream/cases/01_button_simple.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadPayloadAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        let surface = streamAndWaitForRender(json, surfaceId: surfaceId, chunkSize: 10, expectedCount: expectedCount)

        XCTAssertNotNil(surface, "STREAM-01: Surface 应创建成功")
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "STREAM-01: 小 chunk 流式组件数（\(surface?.getAllComponents().count ?? -1)）应为 \(expectedCount)")
    }

    // MARK: - STREAM-02：大 chunk（chunkSize=500）

    /// STREAM-02：大 chunk 流式（chunkSize=500）发送 Button 场景，
    /// 最终组件数与完整 JSON 一次性发送一致。
    func testSTREAM02_largeChunkButtonScene() throws {
        let fixturePath = "stream/cases/01_button_simple.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadPayloadAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        let surface = streamAndWaitForRender(json, surfaceId: surfaceId, chunkSize: 500, expectedCount: expectedCount)

        XCTAssertNotNil(surface, "STREAM-02: Surface 应创建成功")
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "STREAM-02: 大 chunk 流式组件数应为 \(expectedCount)")
    }

    // MARK: - STREAM-03：单字符（chunkSize=1）

    /// STREAM-03：单字符流式（chunkSize=1）发送 Button 场景，
    /// 最终组件数与完整 JSON 一次性发送一致。
    func testSTREAM03_singleCharButtonScene() throws {
        let fixturePath = "stream/cases/01_button_simple.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadPayloadAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        let surface = streamAndWaitForRender(json, surfaceId: surfaceId, chunkSize: 1, expectedCount: expectedCount)

        XCTAssertNotNil(surface, "STREAM-03: Surface 应创建成功")
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "STREAM-03: 单字符流式组件数应为 \(expectedCount)")
    }

    // MARK: - STREAM-04：一次性发送

    /// STREAM-04：一次性发送整体 Button JSON，
    /// 最终组件数与预期一致。
    func testSTREAM04_fullJsonAtOnceButtonScene() throws {
        let fixturePath = "stream/cases/01_button_simple.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadPayloadAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        let surface = streamAndWaitForRender(json, surfaceId: surfaceId, chunkSize: 0, expectedCount: expectedCount) // 0 = 整体发送

        XCTAssertNotNil(surface, "STREAM-04: Surface 应创建成功")
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "STREAM-04: 一次性发送组件数应为 \(expectedCount)")
    }

    // MARK: - STREAM-05：复杂嵌套布局大 chunk

    /// STREAM-05：复杂嵌套布局（nested_layout）大 chunk 流式发送，
    /// 最终组件数与 fixture expect 一致。
    func testSTREAM05_nestedLayoutLargeChunk() throws {
        let fixturePath = "stream/cases/02_nested_layout.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadPayloadAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        let surface = streamAndWaitForRender(json, surfaceId: surfaceId, chunkSize: 200, expectedCount: expectedCount)

        XCTAssertNotNil(surface, "STREAM-05: Surface 应创建成功")
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "STREAM-05: 复杂嵌套布局流式组件数应为 \(expectedCount)")
    }

    // MARK: - STREAM-06：含 dataModel 场景

    /// STREAM-06：含 dataModel 场景（chunkSize=20）流式发送，
    /// 最终组件数与 fixture expect 一致。
    func testSTREAM06_withDataModelSmallChunk() throws {
        let fixturePath = "stream/cases/03_with_datamodel.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadPayloadAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        let surface = streamAndWaitForRender(json, surfaceId: surfaceId, chunkSize: 20, expectedCount: expectedCount)

        XCTAssertNotNil(surface, "STREAM-06: Surface 应创建成功")
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "STREAM-06: 含 dataModel 场景流式组件数应为 \(expectedCount)")
    }

    // MARK: - STREAM-07：静态预分片（key 边界截断）

    /// STREAM-07：发送预先在 key 边界处截断的两片静态分片（chunk_01.txt + chunk_02.txt），
    /// 验证引擎能正确拼接并创建完整 Surface。
    func testSTREAM07_staticChunksKeyBoundary() throws {
        let metaPath = "stream/static_chunks/01_split_at_key_boundary/meta.json"
        let meta = try TestFixtureLoader.loadFixture(metaPath)
        guard let surfaceId = meta["surfaceId"] as? String else {
            throw TestFixtureError.missingField("surfaceId", metaPath)
        }
        guard let expect = meta["expect"] as? [String: Any] else {
            throw TestFixtureError.missingField("expect", metaPath)
        }
        let expectedCount = expect["componentCount"] as? Int ?? 0

        // 读取两片静态分片（.txt 文件）
        let chunk1 = try TestFixtureLoader.readRawText(
            "stream/static_chunks/01_split_at_key_boundary/chunk_01.txt")
        let chunk2 = try TestFixtureLoader.readRawText(
            "stream/static_chunks/01_split_at_key_boundary/chunk_02.txt")

        let expectation = XCTestExpectation(description: "Static chunk surface created: \(surfaceId)")
        var capturedSurface: Surface?

        class ChunkListener: NSObject, SurfaceManagerListener {
            let targetId: String
            let expectation: XCTestExpectation
            var surface: Surface?
            init(_ targetId: String, _ expectation: XCTestExpectation) {
                self.targetId = targetId
                self.expectation = expectation
            }
            func onCreateSurface(_ surface: Surface) {
                if surface.surfaceId == targetId {
                    self.surface = surface
                    expectation.fulfill()
                }
            }
        }

        let listener = ChunkListener(surfaceId, expectation)
        surfaceManager.addListener(listener)

        surfaceManager.beginTextStream()
        surfaceManager.receiveTextChunk(chunk1)
        surfaceManager.receiveTextChunk(chunk2)
        surfaceManager.endTextStream()

        let result = XCTWaiter().wait(for: [expectation], timeout: AGenUIBaseTest.timeoutSeconds)
        surfaceManager.removeListener(listener)
        capturedSurface = listener.surface

        XCTAssertEqual(result, .completed, "STREAM-07: 静态分片 Surface 创建超时")
        XCTAssertNotNil(capturedSurface, "STREAM-07: Surface 应创建成功")

        // 等待组件树稳定
        waitForComponentsStable(capturedSurface, expectedCount: expectedCount)

        XCTAssertEqual(capturedSurface?.getAllComponents().count, expectedCount,
                       "STREAM-07: 静态分片组件数应为 \(expectedCount)")

        // 验证组件 ID
        if let componentIds = expect["componentIds"] as? [String] {
            for cid in componentIds {
                XCTAssertNotNil(capturedSurface?.getComponent(componentId: cid),
                                "STREAM-07: 组件 '\(cid)' 应存在")
            }
        }
    }

    // MARK: - STREAM-08：中途 reset（beginTextStream 后再次 beginTextStream）

    /// STREAM-08：流式传输过半后重置，验证新的完整流式结果正确
    ///
    /// 先发送一半 JSON（无效数据），再调用 beginTextStream 重置，
    /// 然后发送完整有效 JSON，验证最终组件树正确。
    func testSTREAM08_resetMidStreamProducesCorrectResult() throws {
        let fixturePath = "stream/cases/01_button_simple.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadPayloadAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        // 使用唯一的 surfaceId 避免冲突
        let resetSurfaceId = "test-surf-reset-\(Int.random(in: 1000...9999))"
        let resetJson = json.replacingOccurrences(of: surfaceId, with: resetSurfaceId)

        // 1. 发送无效的前半段
        surfaceManager.beginTextStream()
        let halfIndex = resetJson.index(resetJson.startIndex, offsetBy: resetJson.count / 2)
        surfaceManager.receiveTextChunk(String(resetJson[..<halfIndex]))

        // 2. 重置（再次 beginTextStream）
        surfaceManager.beginTextStream()

        // 3. 完整发送有效 JSON（分片）
        let surface = streamAndWaitForRender(resetJson, surfaceId: resetSurfaceId, chunkSize: 50, expectedCount: expectedCount)

        XCTAssertNotNil(surface, "STREAM-08: reset 后 Surface 应正确创建")
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "STREAM-08: reset 后组件数应为 \(expectedCount)")
    }
}
