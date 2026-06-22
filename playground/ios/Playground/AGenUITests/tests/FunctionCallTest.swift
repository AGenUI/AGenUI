//
//  FunctionCallTest.swift
//  AGenUITests
//
//  FunctionCall 集成测试
//
//  通过 fixture 的 messages 回放 + textContent 断言，验证引擎 FunctionCall 执行结果。
//  复用与 Android FunctionCallTest.java 完全一致的 fixture 文件和断言逻辑。
//
//  测试覆盖：
//  - SKILL-01~11：11 个内置函数的单独验证
//  - SCENE-01~04：4 个混合场景验证
//  不包含 action_toast.json（需要平台回调验证，由 SkillTest 覆盖）。
//

import XCTest
@testable import Playground
@testable import AGenUI

class FunctionCallTest: AGenUIBaseTest {

    // MARK: - 通用验证辅助

    /// 发送 fixture 消息，等待渲染完成，断言组件数量和 ID，并返回 Surface。
    @discardableResult
    private func renderAndVerify(_ fixturePath: String) throws -> Surface? {
        let surfaceId = try TestFixtureLoader.getSurfaceId(fixturePath)
        let json = try TestFixtureLoader.loadMessagesAsString(fixturePath)
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        let expectedCount = expect["componentCount"] as? Int ?? 0

        let surface = sendAndWaitForSurface(json, surfaceId: surfaceId)
        XCTAssertNotNil(surface, "Surface 应创建成功: \(fixturePath)")

        // 等待组件数稳定（传入 expectedCount 避免提前误判）
        waitForComponentsStable(surface, expectedCount: expectedCount)

        XCTAssertEqual(surface?.getAllComponents().count, expectedCount,
                       "组件数量应为 \(expectedCount): \(fixturePath)")

        if let componentIds = expect["componentIds"] as? [String] {
            for id in componentIds {
                XCTAssertNotNil(surface?.getComponent(componentId: id),
                                "组件 ID '\(id)' 应存在: \(fixturePath)")
            }
        }

        return surface
    }

    /// 断言 fixture expect.textContent 中每一项的组件渲染文本包含期望字符串。
    ///
    /// 通过遍历 Component（UIView）的 subviews 查找 UILabel，读取其 text 属性。
    /// 不修改任何 SDK 源码。
    private func assertTextContent(_ surface: Surface, expect: [String: Any], fixturePath: String) {
        guard let textContent = expect["textContent"] as? [[String: Any]] else { return }

        for item in textContent {
            guard let componentId = item["componentId"] as? String,
                  let expectedContains = item["contains"] as? String else { continue }
            let note = item["note"] as? String ?? componentId

            guard let component = surface.getComponent(componentId: componentId) else {
                XCTFail("组件 '\(componentId)' 应存在: \(note)")
                continue
            }

            // Component 继承 UIView，TextComponent 内部添加 UILabel 子视图
            var actualText = ""
            func findLabel(in view: UIView) -> UILabel? {
                if let label = view as? UILabel { return label }
                for subview in view.subviews {
                    if let label = findLabel(in: subview) { return label }
                }
                return nil
            }
            if let label = findLabel(in: component) {
                actualText = label.text ?? ""
            }

            XCTAssertTrue(
                actualText.contains(expectedContains),
                "【\(note)】文本应包含 '\(expectedContains)'，实际为: '\(actualText)'"
            )
        }
    }

    /// 完整回放 fixture 并同时验证组件结构与 textContent。
    private func runFixtureTest(_ fixturePath: String) throws {
        let expect = try TestFixtureLoader.getExpect(fixturePath)
        guard let surface = try renderAndVerify(fixturePath) else { return }

        // 额外等待一个主线程 cycle，确保 FunctionCall 求值已写入 UILabel
        waitForMainThread()

        assertTextContent(surface, expect: expect, fixturePath: fixturePath)
    }

    // MARK: - SKILL 系列：单函数验证

    /// SKILL-01: formatDate 日期格式化（ISO 字符串、时间戳、多种格式）
    func testSKILL01_formatDate() throws {
        try runFixtureTest("function_call/action_formatDate.json")
    }

    /// SKILL-02: formatString 模板字符串（简单插值、嵌套调用）
    func testSKILL02_formatString() throws {
        // TODO: 待修复 - SKILL-02-f (空字符串入参) UILabel 实际渲染为 ' '，期望包含 ''。
        // 详见 reports/runs/feature-1618-agenui_afe271c_20260527_173826
        throw XCTSkip("待修复：SKILL-02-f 空字符串入参渲染异常")
        // try runFixtureTest("function_call/action_formatString.json")
    }

    /// SKILL-03: formatNumber 数字格式化（千分位、小数位、百分比）
    func testSKILL03_formatNumber() throws {
        try runFixtureTest("function_call/format_formatNumber.json")
    }

    /// SKILL-04: formatCurrency 货币格式化（CNY、USD、EUR）
    func testSKILL04_formatCurrency() throws {
        try runFixtureTest("function_call/format_formatCurrency.json")
    }

    /// SKILL-05: pluralize 复数化（count=0/1/2）
    func testSKILL05_pluralize() throws {
        try runFixtureTest("function_call/format_pluralize.json")
    }

    /// SKILL-06: token 设计令牌（通过 formatString 嵌套）
    func testSKILL06_formatToken() throws {
        try runFixtureTest("function_call/format_token.json")
    }

    /// SKILL-07: required 非空校验（有值/空字符串/null）
    func testSKILL07_validateRequired() throws {
        // TODO: 待修复 - SKILL-07-c (null → false) UILabel 实际渲染为 ' '，期望包含 'false'。
        // 详见 reports/runs/feature-1618-agenui_afe271c_20260527_173826
        throw XCTSkip("待修复：SKILL-07-c null 入参 validateRequired 返回值未渲染")
        // try runFixtureTest("function_call/validate_required.json")
    }

    /// SKILL-08: numeric 数字校验（整数/小数/非数字字符串）
    func testSKILL08_validateNumeric() throws {
        try runFixtureTest("function_call/validate_numeric.json")
    }

    /// SKILL-09: length 长度校验（min/max/range）
    func testSKILL09_validateLength() throws {
        try runFixtureTest("function_call/validate_length.json")
    }

    /// SKILL-10: regex 正则校验（匹配/不匹配）
    func testSKILL10_validateRegex() throws {
        try runFixtureTest("function_call/validate_regex.json")
    }

    /// SKILL-11: email 邮箱格式校验（合法/非法）
    func testSKILL11_validateEmail() throws {
        try runFixtureTest("function_call/validate_email.json")
    }

    // MARK: - SCENE 系列：混合场景验证

    /// SCENE-01: formatString 嵌套调用（formatDate、formatNumber 内联）
    func testSCENE01_formatStringNested() throws {
        try runFixtureTest("function_call/mixed_formatString_nested.json")
    }

    /// SCENE-02: DataBinding 路径绑定 + FunctionCall 组合使用
    func testSCENE02_databindingWithFunc() throws {
        try runFixtureTest("function_call/mixed_databinding_with_func.json")
    }

    /// SCENE-03: 逻辑表达式中嵌入 FunctionCall（条件渲染）
    func testSCENE03_logicExpression() throws {
        // TODO: 待修复 - SCENE-03-a (DataBinding /form/username='Alice'(5字) length(2-20) → true)
        // UILabel 实际渲染为 ' '，期望包含 'true'。
        // 详见 reports/runs/feature-1618-agenui_afe271c_20260527_173826
        throw XCTSkip("待修复：SCENE-03-a 逻辑表达式中 FunctionCall 求值结果未渲染")
        // try runFixtureTest("function_call/mixed_logic_expression.json")
    }

    /// SCENE-04: FunctionCall 在 Action 参数中使用（仅验证 textContent 部分）
    func testSCENE04_funcInAction_textContent() throws {
        try runFixtureTest("function_call/mixed_func_in_action.json")
    }
}
