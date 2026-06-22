//
//  InitializationTest.swift
//  AGenUITests
//
//  INIT 模块集成测试
//
//  覆盖用例：
//  - INIT-01：SDK 已完成初始化（AGenUIEngineBridge 单例在首次 SurfaceManager 创建时初始化）
//  - INIT-02：多次创建 SurfaceManager 不崩溃
//  - INIT-03：SurfaceManager 实例创建成功
//

import XCTest
@testable import Playground
@testable import AGenUI

class InitializationTest: AGenUIBaseTest {

    // MARK: - INIT-01

    /// INIT-01：SDK 已完成初始化
    ///
    /// iOS AGenUI SDK 在首次创建 SurfaceManager 时自动初始化（惰性单例），
    /// 通过 AGenUISDK.getVersion() 非空验证引擎已就绪。
    func testINIT01_sdkInitialized() {
        // AGenUIBaseTest.setUp() 已创建 SurfaceManager，引擎已自动初始化
        // 通过 getVersion() 验证 SDK 已就绪
        let version = AGenUISDK.getVersion()
        XCTAssertFalse(version.isEmpty, "INIT-01: AGenUI SDK 版本号不应为空，说明引擎已初始化")
    }

    /// INIT-01（扩展）：SurfaceManager 初始化后可正常工作
    func testINIT01_surfaceManagerFunctional() {
        // AGenUIBaseTest.setUp() 已创建 surfaceManager
        XCTAssertNotNil(surfaceManager, "INIT-01: SurfaceManager 应已成功创建")
    }

    // MARK: - INIT-02

    /// INIT-02：重复创建 SurfaceManager 不崩溃
    ///
    /// 多次创建 SurfaceManager 实例，每个实例独立持有一个 C++ ISurfaceManager，
    /// 验证不会崩溃。
    func testINIT02_multipleManagersNotCrash() {
        var managers: [SurfaceManager] = []
        for _ in 0..<3 {
            let sm = SurfaceManager()
            XCTAssertNotNil(sm, "INIT-02: SurfaceManager 应可重复创建")
            managers.append(sm)
        }
        // managers 在测试结束时自动释放
        XCTAssertEqual(managers.count, 3, "INIT-02: 应成功创建 3 个 SurfaceManager 实例")
    }

    // MARK: - INIT-03

    /// INIT-03：SurfaceManager 可正常创建（引擎已就绪）
    ///
    /// AGenUIBaseTest.setUp() 中创建的 surfaceManager 非 nil 即表明引擎已就绪。
    func testINIT03_surfaceManagerCreatedSuccessfully() {
        XCTAssertNotNil(surfaceManager, "INIT-03: SurfaceManager 应创建成功（非 nil）")
    }

    /// INIT-03（扩展）：SurfaceManager 创建后可添加 Listener
    func testINIT03_listenerCanBeAdded() {
        class NoopListener: NSObject, SurfaceManagerListener {}

        let listener = NoopListener()
        // 不应崩溃
        surfaceManager.addListener(listener)
        surfaceManager.removeListener(listener)
    }
}
