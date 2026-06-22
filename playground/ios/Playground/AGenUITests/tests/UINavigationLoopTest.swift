//
//  UINavigationLoopTest.swift
//  AGenUITests
//
//  UI自动化循环测试：冷启动 -> 导航菜单 -> A2UI Show -> Gallery -> 等待 -> 杀掉App
//  循环执行10次
//

import XCTest

/// UI导航循环测试
/// 
/// 测试流程：
/// 1. 冷启动App
/// 2. 点击左上角菜单按钮
/// 3. 点击A2UI Show
/// 4. 点击Gallery
/// 5. 等待3秒渲染
/// 6. 截取屏幕
/// 7. 杀掉App
/// 8. 重复10次
class UINavigationLoopTest: XCTestCase {
    
    let app = XCUIApplication()
    
    // 测试配置
    let loopCount = 10
    let waitSeconds = 3
    
    override func setUpWithError() throws {
        continueAfterFailure = false
        app.launchArguments = ["-uiTesting", "true"]
    }
    
    override func tearDownWithError() throws {
        // 清理
        app.terminate()
    }
    
    /// 主测试：循环执行导航流程
    func testNavigationLoop() throws {
        print("=========================================")
        print("开始UI导航循环测试")
        print("循环次数: \(loopCount)")
        print("等待时间: \(waitSeconds)秒")
        print("=========================================")
        
        var successCount = 0
        var failCount = 0
        
        for i in 1...loopCount {
            print("\n-----------------------------------------")
            print("开始第 \(i)/\(loopCount) 次循环")
            print("-----------------------------------------")
            
            do {
                try runSingleIteration(iteration: i)
                print("✅ 第 \(i) 次循环完成")
                successCount += 1
            } catch {
                print("❌ 第 \(i) 次循环失败: \(error.localizedDescription)")
                failCount += 1
                
                // 确保杀掉App
                app.terminate()
            }
            
            // 循环间等待
            sleep(2)
        }
        
        // 打印总结
        print("\n=========================================")
        print("测试完成总结")
        print("=========================================")
        print("成功: \(successCount) 次")
        print("失败: \(failCount) 次")
        print("=========================================")
        
        // 如果有失败，标记测试失败
        XCTAssert(failCount == 0, "有 \(failCount) 次循环失败")
    }
    
    /// 执行单次循环
    private func runSingleIteration(iteration: Int) throws {
        // 1. 冷启动App
        try coldStartApp()
        
        // 2. 点击菜单按钮
        try tapMenuButton()
        
        // 3. 点击A2UI Show
        try tapA2UIShow()
        
        // 4. 点击Gallery
        try tapGallery()
        
        // 5. 等待渲染
        waitForRender()
        
        // 6. 截图
        captureScreenshot(iteration: iteration)
        
        // 7. 杀掉App
        app.terminate()
        sleep(1)
    }
    
    /// 冷启动App
    private func coldStartApp() throws {
        print("冷启动App...")
        
        // 确保App完全停止
        app.terminate()
        sleep(1)
        
        // 启动App
        app.launch()
        sleep(2)
        
        // 验证App已启动
        XCTAssertTrue(app.wait(for: .runningForeground, timeout: 10), "App启动失败")
    }
    
    /// 点击左上角菜单按钮
    private func tapMenuButton() throws {
        print("点击菜单按钮...")
        
        // 方法1: 通过accessibility identifier查找
        if app.navigationBars.buttons["menuButton"].exists {
            app.navigationBars.buttons["menuButton"].tap()
        }
        // 方法2: 通过图片名称查找
        else if app.navigationBars.buttons["line.3.horizontal"].exists {
            app.navigationBars.buttons["line.3.horizontal"].tap()
        }
        // 方法3: 通过第一个按钮（通常是菜单）
        else if app.navigationBars.buttons.firstMatch.exists {
            app.navigationBars.buttons.firstMatch.tap()
        }
        else {
            throw TestError.elementNotFound("菜单按钮")
        }
        
        sleep(1)
    }
    
    /// 点击A2UI Show菜单项
    private func tapA2UIShow() throws {
        print("点击A2UI Show...")
        
        // 在表格中查找A2UI Show
        let a2uiShowCell = app.tables.cells.containing(.staticText, identifier: "A2UI Show").firstMatch
        
        if a2uiShowCell.exists {
            a2uiShowCell.tap()
        } else {
            // 尝试通过文本查找
            let a2uiShowText = app.staticTexts["A2UI Show"].firstMatch
            if a2uiShowText.exists {
                a2uiShowText.tap()
            } else {
                throw TestError.elementNotFound("A2UI Show菜单项")
            }
        }
        
        sleep(1)
    }
    
    /// 点击Gallery菜单项
    private func tapGallery() throws {
        print("点击Gallery...")
        
        // 在表格中查找Gallery
        let galleryCell = app.tables.cells.containing(.staticText, identifier: "Gallery").firstMatch
        
        if galleryCell.exists {
            galleryCell.tap()
        } else {
            // 尝试通过文本查找
            let galleryText = app.staticTexts["Gallery"].firstMatch
            if galleryText.exists {
                galleryText.tap()
            } else {
                throw TestError.elementNotFound("Gallery菜单项")
            }
        }
        
        sleep(1)
    }
    
    /// 等待渲染完成
    private func waitForRender() {
        print("等待\(waitSeconds)秒渲染...")
        sleep(UInt32(waitSeconds))
    }
    
    /// 截取屏幕截图
    private func captureScreenshot(iteration: Int) {
        print("截取屏幕截图...")
        
        let screenshot = app.screenshot()
        let attachment = XCTAttachment(screenshot: screenshot)
        attachment.name = "iteration_\(iteration)"
        attachment.lifetime = .keepAlways
        add(attachment)
    }
}

// MARK: - 自定义错误类型

enum TestError: Error, LocalizedError {
    case elementNotFound(String)
    
    var errorDescription: String? {
        switch self {
        case .elementNotFound(let element):
            return "未找到UI元素: \(element)"
        }
    }
}

// MARK: - 性能测试变体

extension UINavigationLoopTest {
    
    /// 性能测试：记录每次循环的时间
    func testNavigationLoopPerformance() throws {
        measure {
            try? runSingleIteration(iteration: 1)
        }
    }
    
    /// 压力测试：增加循环次数
    func testNavigationLoopStress() throws {
        let stressLoopCount = 50
        print("开始压力测试：\(stressLoopCount)次循环")
        
        for i in 1...stressLoopCount {
            try runSingleIteration(iteration: i)
            
            // 每10次打印进度
            if i % 10 == 0 {
                print("已完成 \(i)/\(stressLoopCount) 次循环")
            }
        }
    }
}
