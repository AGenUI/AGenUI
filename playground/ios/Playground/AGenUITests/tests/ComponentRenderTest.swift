//
//  ComponentRenderTest.swift
//  AGenUITests
//
//  组件渲染验证测试
//
//  覆盖各类 fixture 的组件数量、类型、ID 的全面断言：
//  - 01_text_only：Column + 3×Text
//  - 02_button_with_action：Column + Button + Text，Button 类型与 ID 正确
//  - 03_nested_column：外层 Column + 2×内层 Column + 4×Text
//  - 04_card_complex：Column + Card + Column + 2×Text + Button + Text
//  - 05_modal_with_trigger：Column + Button + Text + Modal + Text
//

import XCTest
@testable import Playground
@testable import AGenUI

class ComponentRenderTest: AGenUIBaseTest {

    // MARK: - 通用验证辅助

    /// 读取 fixture，发送并等待渲染完成，然后验证组件数量和所有组件 ID 是否存在。
    @discardableResult
    private func renderAndVerifyBasic(_ fixturePath: String) throws -> Surface? {
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadMessagesAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)

        let surface = sendAndWaitForRender(json, surfaceId: surfaceId)
        XCTAssertNotNil(surface, "Surface 应创建成功: \(fixturePath)")

        // 验证组件数量
        let expectedCount = expect["componentCount"] as? Int ?? 0
        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "组件数量应为 \(expectedCount): \(fixturePath)")

        // 验证所有组件 ID 均可通过 getComponent 获取
        if let componentIds = expect["componentIds"] as? [String] {
            for id in componentIds {
                XCTAssertNotNil(surface?.getComponent(componentId: id),
                                "组件 ID '\(id)' 应存在于组件树中: \(fixturePath)")
            }
        }

        return surface
    }

    // MARK: - 测试用例

    /// 01_text_only：Column + 3 个 Text，共 4 个组件
    func testRender_01_textOnly() throws {
        let surface = try renderAndVerifyBasic("components/01_text_only.json")

        // 验证根组件类型
        XCTAssertNotNil(surface?.getComponent(componentId: "root"), "root 组件不应为 nil")
        XCTAssertEqual(surface?.getComponent(componentId: "root")?.componentType, "Column",
                       "root 类型应为 Column")

        // 验证 Text 组件类型
        XCTAssertEqual(surface?.getComponent(componentId: "text-title")?.componentType, "Text",
                       "text-title 类型应为 Text")
        XCTAssertEqual(surface?.getComponent(componentId: "text-body")?.componentType, "Text",
                       "text-body 类型应为 Text")
        XCTAssertEqual(surface?.getComponent(componentId: "text-caption")?.componentType, "Text",
                       "text-caption 类型应为 Text")
    }

    /// 02_button_with_action：Column + Button + Text，共 3 个组件
    func testRender_02_buttonWithAction() throws {
        let surface = try renderAndVerifyBasic("components/02_button_with_action.json")

        // 验证 Button 类型与 ID
        XCTAssertNotNil(surface?.getComponent(componentId: "btn-submit"),
                        "btn-submit 不应为 nil")
        XCTAssertEqual(surface?.getComponent(componentId: "btn-submit")?.componentType, "Button",
                       "btn-submit 类型应为 Button")

        // 验证 Button 子 Text
        XCTAssertNotNil(surface?.getComponent(componentId: "btn-label"),
                        "btn-label 不应为 nil")
        XCTAssertEqual(surface?.getComponent(componentId: "btn-label")?.componentType, "Text",
                       "btn-label 类型应为 Text")
    }

    /// 03_nested_column：外层 Column + 2×内层 Column + 4×Text，共 7 个组件
    func testRender_03_nestedColumn() throws {
        let surface = try renderAndVerifyBasic("components/03_nested_column.json")

        // 验证所有 Column 和 Text 类型
        XCTAssertEqual(surface?.getComponent(componentId: "col-a")?.componentType, "Column",
                       "col-a 类型应为 Column")
        XCTAssertEqual(surface?.getComponent(componentId: "col-b")?.componentType, "Column",
                       "col-b 类型应为 Column")
        XCTAssertEqual(surface?.getComponent(componentId: "text-a1")?.componentType, "Text",
                       "text-a1 类型应为 Text")
        XCTAssertEqual(surface?.getComponent(componentId: "text-b2")?.componentType, "Text",
                       "text-b2 类型应为 Text")
    }

    /// 04_card_complex：Column + Card + Column + 2×Text + Button + Text，共 7 个组件
    func testRender_04_cardComplex() throws {
        let surface = try renderAndVerifyBasic("components/04_card_complex.json")

        // 验证 Card 类型
        XCTAssertEqual(surface?.getComponent(componentId: "card-wrapper")?.componentType, "Card",
                       "card-wrapper 类型应为 Card")

        // 验证 Button 存在且类型正确
        XCTAssertNotNil(surface?.getComponent(componentId: "card-btn"),
                        "card-btn 不应为 nil")
        XCTAssertEqual(surface?.getComponent(componentId: "card-btn")?.componentType, "Button",
                       "card-btn 类型应为 Button")
    }

    /// 05_modal_with_trigger：Column + Button + Text + Modal + Text，共 5 个组件
    func testRender_05_modalWithTrigger() throws {
        let surface = try renderAndVerifyBasic("components/05_modal_with_trigger.json")

        // 验证 Modal 类型
        XCTAssertNotNil(surface?.getComponent(componentId: "modal-dialog"),
                        "modal-dialog 不应为 nil")
        XCTAssertEqual(surface?.getComponent(componentId: "modal-dialog")?.componentType, "Modal",
                       "modal-dialog 类型应为 Modal")

        // 验证 Modal 的 trigger Button 存在
        XCTAssertNotNil(surface?.getComponent(componentId: "trigger-btn"),
                        "trigger-btn 不应为 nil")
    }

    /// 完整组件树验证：通过 getAllComponents() 确认所有组件均在树中
    func testRender_componentTreeContainsAllComponents() throws {
        let fixturePath = "components/03_nested_column.json"
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadMessagesAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)

        let surface = sendAndWaitForRender(json, surfaceId: surfaceId)

        guard let allComponents = surface?.getAllComponents() else {
            XCTFail("getAllComponents() 不应返回 nil")
            return
        }
        let allComponentIds = Set(allComponents.map { $0.componentId })

        if let componentIds = expect["componentIds"] as? [String] {
            for id in componentIds {
                XCTAssertTrue(allComponentIds.contains(id),
                              "getAllComponents() 应包含组件 ID: \(id)")
            }
        }
    }
}
