import Testing
@testable import AGenUI

// MARK: - CSSPropertyParser Unit Tests
// Tests for pure-logic parsing methods that do NOT depend on native bridge

// ============================================================================
// parseDimension — Normal Path
// ============================================================================

@Test func parseDimension_unitlessNumber_returnsNumber() {
    let result = CSSPropertyParser.parseDimension("100")
    #expect(result == .number(100.0))
}

@Test func parseDimension_pxUnit_appliesScale() {
    // 100px * BS_POINT_SCALE(0.5) = 50.0
    let result = CSSPropertyParser.parseDimension("100px")
    #expect(result == .number(50.0))
}

@Test func parseDimension_ptUnit_returnsDirectValue() {
    // pt passes through without scaling
    let result = CSSPropertyParser.parseDimension("20pt")
    #expect(result == .number(20.0))
}

@Test func parseDimension_percentage_returnsPercentage() {
    // 50% -> 0.5
    let result = CSSPropertyParser.parseDimension("50%")
    #expect(result == .percentage(0.5))
}

@Test func parseDimension_autoKeyword_returnsKeyword() {
    let result = CSSPropertyParser.parseDimension("auto")
    #expect(result == .keyword("auto"))
}

@Test func parseDimension_autoUpperCase_returnsKeyword() {
    let result = CSSPropertyParser.parseDimension("AUTO")
    #expect(result == .keyword("auto"))
}

@Test func parseDimension_zeroValue_returnsNumber() {
    let result = CSSPropertyParser.parseDimension("0")
    #expect(result == .number(0.0))
}

@Test func parseDimension_decimalValue_returnsNumber() {
    let result = CSSPropertyParser.parseDimension("3.14")
    #expect(result == .number(3.14))
}

@Test func parseDimension_negativeValue_returnsNumber() {
    let result = CSSPropertyParser.parseDimension("-10")
    #expect(result == .number(-10.0))
}

// ============================================================================
// parseDimension — Boundary Values
// ============================================================================

@Test func parseDimension_zeroPx_returnsZero() {
    let result = CSSPropertyParser.parseDimension("0px")
    #expect(result == .number(0.0))
}

@Test func parseDimension_zeroPercent_returnsZeroPercentage() {
    let result = CSSPropertyParser.parseDimension("0%")
    #expect(result == .percentage(0.0))
}

@Test func parseDimension_100Percent_returnsOne() {
    let result = CSSPropertyParser.parseDimension("100%")
    #expect(result == .percentage(1.0))
}

@Test func parseDimension_veryLargePx_calculatesCorrectly() {
    // 10000px * 0.5 = 5000
    let result = CSSPropertyParser.parseDimension("10000px")
    #expect(result == .number(5000.0))
}

@Test func parseDimension_negativePx_appliesScale() {
    // -20px * 0.5 = -10
    let result = CSSPropertyParser.parseDimension("-20px")
    #expect(result == .number(-10.0))
}

// ============================================================================
// parseDimension — Error Path
// ============================================================================

@Test func parseDimension_invalidText_returnsInvalid() {
    let result = CSSPropertyParser.parseDimension("abc")
    #expect(result == .invalid)
}

@Test func parseDimension_emptyString_returnsInvalid() {
    let result = CSSPropertyParser.parseDimension("")
    #expect(result == .invalid)
}

@Test func parseDimension_onlyPxSuffix_returnsInvalid() {
    let result = CSSPropertyParser.parseDimension("px")
    #expect(result == .invalid)
}

@Test func parseDimension_onlyPercentSign_returnsInvalid() {
    let result = CSSPropertyParser.parseDimension("%")
    #expect(result == .invalid)
}

// ============================================================================
// parseKeyword — Normal Path
// ============================================================================

@Test func parseKeyword_validKeyword_returnsKeyword() {
    let result = CSSPropertyParser.parseKeyword("hidden", validValues: ["visible", "hidden", "scroll"])
    #expect(result == .keyword("hidden"))
}

@Test func parseKeyword_validKeywordUpperCase_returnsLowercased() {
    let result = CSSPropertyParser.parseKeyword("HIDDEN", validValues: ["visible", "hidden", "scroll"])
    #expect(result == .keyword("hidden"))
}

@Test func parseKeyword_firstValidValue_returnsKeyword() {
    let result = CSSPropertyParser.parseKeyword("visible", validValues: ["visible", "hidden"])
    #expect(result == .keyword("visible"))
}

// ============================================================================
// parseKeyword — Error Path
// ============================================================================

@Test func parseKeyword_invalidKeyword_returnsInvalid() {
    let result = CSSPropertyParser.parseKeyword("invalid", validValues: ["visible", "hidden"])
    #expect(result == .invalid)
}

@Test func parseKeyword_emptyValidValues_returnsInvalid() {
    let result = CSSPropertyParser.parseKeyword("anything", validValues: [])
    #expect(result == .invalid)
}

// ============================================================================
// parseOffset — delegates to parseDimension
// ============================================================================

@Test func parseOffset_pxValue_appliesScale() {
    let result = CSSPropertyParser.parseOffset("10px")
    #expect(result == .number(5.0))
}

@Test func parseOffset_percentage_returnsPercentage() {
    let result = CSSPropertyParser.parseOffset("50%")
    #expect(result == .percentage(0.5))
}

@Test func parseOffset_auto_returnsKeyword() {
    let result = CSSPropertyParser.parseOffset("auto")
    #expect(result == .keyword("auto"))
}

@Test func parseOffset_negativeValue_parsesCorrectly() {
    // -10px * 0.5 = -5.0
    let result = CSSPropertyParser.parseOffset("-10px")
    #expect(result == .number(-5.0))
}

@Test func parseOffset_withWhitespace_trims() {
    let result = CSSPropertyParser.parseOffset("  20pt  ")
    #expect(result == .number(20.0))
}

// ============================================================================
// extractStringValue
// ============================================================================

@Test func extractStringValue_stringInput_returnsString() {
    let result = CSSPropertyParser.extractStringValue("hello")
    #expect(result == "hello")
}

@Test func extractStringValue_intInput_returnsStringRepresentation() {
    let result = CSSPropertyParser.extractStringValue(42)
    #expect(result == "42")
}

@Test func extractStringValue_doubleInput_returnsStringRepresentation() {
    let result = CSSPropertyParser.extractStringValue(3.14)
    #expect(result.contains("3.14"))
}

// ============================================================================
// extractNumberValue
// ============================================================================

@Test func extractNumberValue_doubleInput_returnsDouble() {
    let result = CSSPropertyParser.extractNumberValue(3.14)
    #expect(result == 3.14)
}

@Test func extractNumberValue_intInput_returnsDouble() {
    let result = CSSPropertyParser.extractNumberValue(42)
    #expect(result == 42.0)
}

@Test func extractNumberValue_stringInput_returnsNil() {
    let result = CSSPropertyParser.extractNumberValue("not a number")
    #expect(result == nil)
}

// ============================================================================
// extractBooleanValue
// ============================================================================

@Test func extractBooleanValue_trueBoolean_returnsTrue() {
    let result = CSSPropertyParser.extractBooleanValue(true)
    #expect(result == true)
}

@Test func extractBooleanValue_falseBoolean_returnsFalse() {
    let result = CSSPropertyParser.extractBooleanValue(false)
    #expect(result == false)
}

@Test func extractBooleanValue_trueString_returnsTrue() {
    let result = CSSPropertyParser.extractBooleanValue("true")
    #expect(result == true)
}

@Test func extractBooleanValue_TRUEString_returnsTrue() {
    let result = CSSPropertyParser.extractBooleanValue("TRUE")
    #expect(result == true)
}

@Test func extractBooleanValue_falseString_returnsFalse() {
    let result = CSSPropertyParser.extractBooleanValue("false")
    #expect(result == false)
}

@Test func extractBooleanValue_otherString_returnsFalse() {
    let result = CSSPropertyParser.extractBooleanValue("anything")
    #expect(result == false)
}

// ============================================================================
// CSSPropertyValue — Computed Properties
// ============================================================================

@Test func cssPropertyValue_numberValue_returnsCorrectly() {
    let value = CSSPropertyValue.number(42.0)
    #expect(value.numberValue == 42.0)
    #expect(value.percentageValue == nil)
    #expect(value.keywordValue == nil)
    #expect(value.isValid == true)
}

@Test func cssPropertyValue_percentageValue_returnsCorrectly() {
    let value = CSSPropertyValue.percentage(0.5)
    #expect(value.percentageValue == 0.5)
    #expect(value.numberValue == nil)
    #expect(value.isValid == true)
}

@Test func cssPropertyValue_keywordValue_returnsCorrectly() {
    let value = CSSPropertyValue.keyword("auto")
    #expect(value.keywordValue == "auto")
    #expect(value.numberValue == nil)
    #expect(value.isValid == true)
}

@Test func cssPropertyValue_invalidValue_isNotValid() {
    let value = CSSPropertyValue.invalid
    #expect(value.isValid == false)
    #expect(value.numberValue == nil)
    #expect(value.percentageValue == nil)
    #expect(value.keywordValue == nil)
}

@Test func cssPropertyValue_urlValue_returnsCorrectly() {
    let value = CSSPropertyValue.url("https://example.com/img.png")
    #expect(value.urlValue == "https://example.com/img.png")
    #expect(value.isValid == true)
}

// ============================================================================
// CSSPropertyValue — Equality
// ============================================================================

@Test func cssPropertyValue_equalNumbers_areEqual() {
    #expect(CSSPropertyValue.number(10.0) == CSSPropertyValue.number(10.0))
}

@Test func cssPropertyValue_differentNumbers_areNotEqual() {
    #expect(CSSPropertyValue.number(10.0) != CSSPropertyValue.number(20.0))
}

@Test func cssPropertyValue_numberAndPercentage_areNotEqual() {
    #expect(CSSPropertyValue.number(0.5) != CSSPropertyValue.percentage(0.5))
}

@Test func cssPropertyValue_invalidAndInvalid_areEqual() {
    #expect(CSSPropertyValue.invalid == CSSPropertyValue.invalid)
}

@Test func cssPropertyValue_sameKeywords_areEqual() {
    #expect(CSSPropertyValue.keyword("auto") == CSSPropertyValue.keyword("auto"))
}

@Test func cssPropertyValue_differentKeywords_areNotEqual() {
    #expect(CSSPropertyValue.keyword("auto") != CSSPropertyValue.keyword("hidden"))
}
