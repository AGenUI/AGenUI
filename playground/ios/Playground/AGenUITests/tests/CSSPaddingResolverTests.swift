import Testing
import UIKit
@testable import AGenUI

// MARK: - CSSPaddingResolver Unit Tests
// Tests for pure logic in CSSPaddingResolver that does NOT depend on native bridge.
// String-based padding parsing requires AGenUIEdgeInsetsBridge and is NOT tested here.

// ============================================================================
// CSSPadding Struct — Properties
// ============================================================================

@Test func cssPadding_zero_hasAnyReturnsFalse() {
    let padding = CSSPadding.zero
    #expect(padding.hasAny == false)
}

@Test func cssPadding_withTopValue_hasAnyReturnsTrue() {
    let padding = CSSPadding(top: 10, right: 0, bottom: 0, left: 0)
    #expect(padding.hasAny == true)
}

@Test func cssPadding_withRightValue_hasAnyReturnsTrue() {
    let padding = CSSPadding(top: 0, right: 5, bottom: 0, left: 0)
    #expect(padding.hasAny == true)
}

@Test func cssPadding_withBottomValue_hasAnyReturnsTrue() {
    let padding = CSSPadding(top: 0, right: 0, bottom: 8, left: 0)
    #expect(padding.hasAny == true)
}

@Test func cssPadding_withLeftValue_hasAnyReturnsTrue() {
    let padding = CSSPadding(top: 0, right: 0, bottom: 0, left: 3)
    #expect(padding.hasAny == true)
}

@Test func cssPadding_allValues_hasAnyReturnsTrue() {
    let padding = CSSPadding(top: 1, right: 2, bottom: 3, left: 4)
    #expect(padding.hasAny == true)
}

@Test func cssPadding_zero_valuesAreAllZero() {
    let padding = CSSPadding.zero
    #expect(padding.top == 0)
    #expect(padding.right == 0)
    #expect(padding.bottom == 0)
    #expect(padding.left == 0)
}

// ============================================================================
// hasAnyPaddingKey — Detection Logic
// ============================================================================

@Test func hasAnyPaddingKey_emptyDict_returnsFalse() {
    let styles: [String: Any] = [:]
    #expect(CSSPaddingResolver.hasAnyPaddingKey(styles) == false)
}

@Test func hasAnyPaddingKey_unrelatedKeys_returnsFalse() {
    let styles: [String: Any] = ["color": "red", "font-size": "14px", "width": "100"]
    #expect(CSSPaddingResolver.hasAnyPaddingKey(styles) == false)
}

@Test func hasAnyPaddingKey_paddingShorthand_returnsTrue() {
    let styles: [String: Any] = ["padding": "10px"]
    #expect(CSSPaddingResolver.hasAnyPaddingKey(styles) == true)
}

@Test func hasAnyPaddingKey_paddingTop_returnsTrue() {
    let styles: [String: Any] = ["padding-top": "5px"]
    #expect(CSSPaddingResolver.hasAnyPaddingKey(styles) == true)
}

@Test func hasAnyPaddingKey_paddingRight_returnsTrue() {
    let styles: [String: Any] = ["padding-right": "8"]
    #expect(CSSPaddingResolver.hasAnyPaddingKey(styles) == true)
}

@Test func hasAnyPaddingKey_paddingBottom_returnsTrue() {
    let styles: [String: Any] = ["padding-bottom": 12]
    #expect(CSSPaddingResolver.hasAnyPaddingKey(styles) == true)
}

@Test func hasAnyPaddingKey_paddingLeft_returnsTrue() {
    let styles: [String: Any] = ["padding-left": 6.0]
    #expect(CSSPaddingResolver.hasAnyPaddingKey(styles) == true)
}

@Test func hasAnyPaddingKey_multipleKeys_returnsTrue() {
    let styles: [String: Any] = ["padding-top": "5", "padding-bottom": "10", "color": "blue"]
    #expect(CSSPaddingResolver.hasAnyPaddingKey(styles) == true)
}

// ============================================================================
// parseLengthPt — Numeric Types (no bridge dependency)
// ============================================================================

@Test func parseLengthPt_nil_returnsNil() {
    let result = CSSPaddingResolver.parseLengthPt(nil)
    #expect(result == nil)
}

@Test func parseLengthPt_intValue_appliesScale() {
    // Int 10 * pointScale(0.5) = 5.0
    let result = CSSPaddingResolver.parseLengthPt(10 as Int)
    #expect(result == 5.0)
}

@Test func parseLengthPt_doubleValue_appliesScale() {
    // Double 20.0 * pointScale(0.5) = 10.0
    let result = CSSPaddingResolver.parseLengthPt(20.0 as Double)
    #expect(result == 10.0)
}

@Test func parseLengthPt_cgfloatValue_appliesScale() {
    // CGFloat 8.0 * pointScale(0.5) = 4.0
    let result = CSSPaddingResolver.parseLengthPt(CGFloat(8.0))
    #expect(result == 4.0)
}

@Test func parseLengthPt_nsNumberValue_appliesScale() {
    // NSNumber 16 * pointScale(0.5) = 8.0
    let result = CSSPaddingResolver.parseLengthPt(NSNumber(value: 16))
    #expect(result == 8.0)
}

@Test func parseLengthPt_zeroInt_returnsZero() {
    let result = CSSPaddingResolver.parseLengthPt(0 as Int)
    #expect(result == 0.0)
}

@Test func parseLengthPt_negativeDouble_returnsNegative() {
    // Double -10.0 * 0.5 = -5.0
    let result = CSSPaddingResolver.parseLengthPt(-10.0 as Double)
    #expect(result == -5.0)
}

@Test func parseLengthPt_nonNumericType_returnsNil() {
    // Array is not a supported type
    let result = CSSPaddingResolver.parseLengthPt([1, 2, 3])
    #expect(result == nil)
}

@Test func parseLengthPt_emptyString_returnsNil() {
    // Empty string should return nil
    let result = CSSPaddingResolver.parseLengthPt("")
    #expect(result == nil)
}

// ============================================================================
// resolve() — Numeric Per-Edge Values (no bridge dependency)
// ============================================================================

@Test func resolve_emptyStyles_returnsZero() {
    let styles: [String: Any] = [:]
    let result = CSSPaddingResolver.resolve(styles)
    #expect(result.top == 0)
    #expect(result.right == 0)
    #expect(result.bottom == 0)
    #expect(result.left == 0)
}

@Test func resolve_numericPaddingTop_appliesScale() {
    // Int 20 * 0.5 = 10.0
    let styles: [String: Any] = ["padding-top": 20 as Int]
    let result = CSSPaddingResolver.resolve(styles)
    #expect(result.top == 10.0)
    #expect(result.right == 0)
    #expect(result.bottom == 0)
    #expect(result.left == 0)
}

@Test func resolve_numericAllEdges_appliesScale() {
    let styles: [String: Any] = [
        "padding-top": 10 as Int,
        "padding-right": 20.0 as Double,
        "padding-bottom": NSNumber(value: 30),
        "padding-left": CGFloat(40)
    ]
    let result = CSSPaddingResolver.resolve(styles)
    #expect(result.top == 5.0)     // 10 * 0.5
    #expect(result.right == 10.0)  // 20 * 0.5
    #expect(result.bottom == 15.0) // 30 * 0.5
    #expect(result.left == 20.0)   // 40 * 0.5
}

@Test func resolve_negativePadding_clampsToZero() {
    let styles: [String: Any] = ["padding-top": -10 as Int]
    let result = CSSPaddingResolver.resolve(styles)
    // -10 * 0.5 = -5.0, clamped to 0
    #expect(result.top == 0)
}

@Test func resolve_numericShorthand_appliesUniform() {
    // Numeric shorthand value applies to all 4 sides
    let styles: [String: Any] = ["padding": 12 as Int]
    let result = CSSPaddingResolver.resolve(styles)
    // 12 * 0.5 = 6.0 for all sides
    #expect(result.top == 6.0)
    #expect(result.right == 6.0)
    #expect(result.bottom == 6.0)
    #expect(result.left == 6.0)
}

@Test func resolve_perEdgeOverridesShorthand() {
    // Per-edge values should override shorthand
    let styles: [String: Any] = [
        "padding": 10 as Int,      // shorthand: all 4 sides = 5.0
        "padding-top": 20 as Int   // override top = 10.0
    ]
    let result = CSSPaddingResolver.resolve(styles)
    #expect(result.top == 10.0)    // 20 * 0.5 = override
    #expect(result.right == 5.0)   // 10 * 0.5 = shorthand
    #expect(result.bottom == 5.0)  // 10 * 0.5 = shorthand
    #expect(result.left == 5.0)    // 10 * 0.5 = shorthand
}

// ============================================================================
// pointScale Constant
// ============================================================================

@Test func pointScale_isHalf() {
    #expect(CSSPaddingResolver.pointScale == 0.5)
}
