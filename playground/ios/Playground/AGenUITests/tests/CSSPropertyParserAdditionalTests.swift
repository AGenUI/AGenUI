import Testing
import UIKit
@testable import AGenUI

// MARK: - CSSPropertyParser Additional Tests
// Covers parse() branches not tested in CSSPropertyParserTests:
// - parseNumber (ratio expressions, standard numbers)
// - parseOpacity (clamping behavior)
// - parse(value:config:) dispatch

// ============================================================================
// parseNumber via parse() — Standard Numbers
// ============================================================================

@Test func parseNumber_standardInteger_returnsNumber() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "3", config: config)
    #expect(result == .number(3.0))
}

@Test func parseNumber_standardDecimal_returnsNumber() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "0.5", config: config)
    #expect(result == .number(0.5))
}

@Test func parseNumber_negativeNumber_returnsNumber() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "-2.5", config: config)
    #expect(result == .number(-2.5))
}

@Test func parseNumber_zero_returnsNumber() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "0", config: config)
    #expect(result == .number(0.0))
}

// ============================================================================
// parseNumber via parse() — Ratio Expressions
// ============================================================================

@Test func parseNumber_ratioExpression_3over2_returnsCorrectValue() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "3 / 2", config: config)
    #expect(result == .number(1.5))
}

@Test func parseNumber_ratioExpression_16over9_returnsCorrectValue() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "16/9", config: config)
    // 16/9 ≈ 1.7778
    if case .number(let value) = result {
        #expect(abs(value - 16.0/9.0) < 0.0001)
    } else {
        #expect(Bool(false), "Expected .number, got \(result)")
    }
}

@Test func parseNumber_ratioExpression_1over1_returnsOne() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "1 / 1", config: config)
    #expect(result == .number(1.0))
}

@Test func parseNumber_ratioExpression_zeroDenominator_returnsInvalid() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "3 / 0", config: config)
    #expect(result == .invalid)
}

@Test func parseNumber_ratioExpression_invalidNumerator_returnsInvalid() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "abc / 2", config: config)
    #expect(result == .invalid)
}

@Test func parseNumber_invalidText_returnsInvalid() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "abc", config: config)
    #expect(result == .invalid)
}

@Test func parseNumber_emptyString_returnsInvalid() {
    let config = CSSPropertyConfig(name: "aspect-ratio", valueType: .number, priority: 10)
    let result = CSSPropertyParser.parse(value: "", config: config)
    #expect(result == .invalid)
}

// ============================================================================
// parseOpacity via parse() — Normal Range
// ============================================================================

@Test func parseOpacity_validHalf_returnsNumber() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "0.5", config: config)
    #expect(result == .number(0.5))
}

@Test func parseOpacity_validOne_returnsNumber() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "1", config: config)
    #expect(result == .number(1.0))
}

@Test func parseOpacity_validZero_returnsNumber() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "0", config: config)
    #expect(result == .number(0.0))
}

@Test func parseOpacity_validQuarter_returnsNumber() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "0.25", config: config)
    #expect(result == .number(0.25))
}

// ============================================================================
// parseOpacity via parse() — Clamping Behavior
// ============================================================================

@Test func parseOpacity_aboveOne_clampsToOne() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "1.5", config: config)
    #expect(result == .number(1.0))
}

@Test func parseOpacity_belowZero_clampsToZero() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "-0.5", config: config)
    #expect(result == .number(0.0))
}

@Test func parseOpacity_veryLarge_clampsToOne() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "100", config: config)
    #expect(result == .number(1.0))
}

@Test func parseOpacity_veryNegative_clampsToZero() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "-100", config: config)
    #expect(result == .number(0.0))
}

// ============================================================================
// parseOpacity via parse() — Error Cases
// ============================================================================

@Test func parseOpacity_invalidText_returnsInvalid() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "half", config: config)
    #expect(result == .invalid)
}

@Test func parseOpacity_emptyString_returnsInvalid() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "", config: config)
    #expect(result == .invalid)
}

// ============================================================================
// parse(value:config:) — Dispatch to Keyword with validValues
// ============================================================================

@Test func parseKeywordViaConfig_validOverflow_returnsKeyword() {
    let config = CSSPropertyConfig(
        name: "overflow",
        valueType: .keyword,
        priority: 45,
        validValues: ["visible", "hidden"]
    )
    let result = CSSPropertyParser.parse(value: "hidden", config: config)
    #expect(result == .keyword("hidden"))
}

@Test func parseKeywordViaConfig_invalidOverflow_returnsInvalid() {
    let config = CSSPropertyConfig(
        name: "overflow",
        valueType: .keyword,
        priority: 45,
        validValues: ["visible", "hidden"]
    )
    let result = CSSPropertyParser.parse(value: "scroll", config: config)
    #expect(result == .invalid)
}

// ============================================================================
// parse(value:config:) — Whitespace Trimming
// ============================================================================

@Test func parse_valueWithWhitespace_trims() {
    let config = CSSPropertyConfig(name: "opacity", valueType: .opacity, priority: 40)
    let result = CSSPropertyParser.parse(value: "  0.8  ", config: config)
    #expect(result == .number(0.8))
}

@Test func parse_dimensionWithWhitespace_trims() {
    let config = CSSPropertyConfig(name: "width", valueType: .dimension, priority: 50)
    let result = CSSPropertyParser.parse(value: "  100px  ", config: config)
    #expect(result == .number(50.0))
}
