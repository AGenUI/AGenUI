import Testing
import UIKit
@testable import AGenUI

// MARK: - CSSPropertyParser Additional Tests
// Covers parse() branches not tested in CSSPropertyParserTests:
// - parseOpacity (clamping behavior)
// parse(value:valueType:validValues:) dispatch

// ============================================================================
// parseOpacity via parse() — Normal Range
// ============================================================================

@Test func parseOpacity_validHalf_returnsNumber() {
    let result = CSSPropertyParser.parse(value: "0.5", valueType: .opacity)
    #expect(result == .number(0.5))
}

@Test func parseOpacity_validOne_returnsNumber() {
    let result = CSSPropertyParser.parse(value: "1", valueType: .opacity)
    #expect(result == .number(1.0))
}

@Test func parseOpacity_validZero_returnsNumber() {
    let result = CSSPropertyParser.parse(value: "0", valueType: .opacity)
    #expect(result == .number(0.0))
}

@Test func parseOpacity_validQuarter_returnsNumber() {
    let result = CSSPropertyParser.parse(value: "0.25", valueType: .opacity)
    #expect(result == .number(0.25))
}

// ============================================================================
// parseOpacity via parse() — Clamping Behavior
// ============================================================================

@Test func parseOpacity_aboveOne_clampsToOne() {
    let result = CSSPropertyParser.parse(value: "1.5", valueType: .opacity)
    #expect(result == .number(1.0))
}

@Test func parseOpacity_belowZero_clampsToZero() {
    let result = CSSPropertyParser.parse(value: "-0.5", valueType: .opacity)
    #expect(result == .number(0.0))
}

@Test func parseOpacity_veryLarge_clampsToOne() {
    let result = CSSPropertyParser.parse(value: "100", valueType: .opacity)
    #expect(result == .number(1.0))
}

@Test func parseOpacity_veryNegative_clampsToZero() {
    let result = CSSPropertyParser.parse(value: "-100", valueType: .opacity)
    #expect(result == .number(0.0))
}

// ============================================================================
// parseOpacity via parse() — Error Cases
// ============================================================================

@Test func parseOpacity_invalidText_returnsInvalid() {
    let result = CSSPropertyParser.parse(value: "half", valueType: .opacity)
    #expect(result == .invalid)
}

@Test func parseOpacity_emptyString_returnsInvalid() {
    let result = CSSPropertyParser.parse(value: "", valueType: .opacity)
    #expect(result == .invalid)
}

// ============================================================================
// parse(value:valueType:validValues:) — Dispatch to Keyword with validValues
// ============================================================================

@Test func parseKeyword_validOverflow_returnsKeyword() {
    let result = CSSPropertyParser.parse(value: "hidden", valueType: .keyword, validValues: ["visible", "hidden"])
    #expect(result == .keyword("hidden"))
}

@Test func parseKeyword_invalidOverflow_returnsInvalid() {
    let result = CSSPropertyParser.parse(value: "scroll", valueType: .keyword, validValues: ["visible", "hidden"])
    #expect(result == .invalid)
}

// ============================================================================
// parse(value:valueType:) — Whitespace Trimming
// ============================================================================

@Test func parse_valueWithWhitespace_trims() {
    let result = CSSPropertyParser.parse(value: "  0.8  ", valueType: .opacity)
    #expect(result == .number(0.8))
}

@Test func parse_dimensionWithWhitespace_trims() {
    let result = CSSPropertyParser.parse(value: "  100px  ", valueType: .dimension)
    #expect(result == .number(50.0))
}
