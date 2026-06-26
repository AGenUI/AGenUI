import Testing
@testable import AGenUI
import UIKit

// =============================================================================
// CSSPropertyValue — Convenience Properties & Equatable
// =============================================================================

// MARK: - isValid

@Test func cssPropertyValue_number_isValid() {
    let value = CSSPropertyValue.number(42.0)
    #expect(value.isValid == true)
}

@Test func cssPropertyValue_percentage_isValid() {
    let value = CSSPropertyValue.percentage(0.5)
    #expect(value.isValid == true)
}

@Test func cssPropertyValue_color_isValid() {
    let value = CSSPropertyValue.color(.red)
    #expect(value.isValid == true)
}

@Test func cssPropertyValue_keyword_isValid() {
    let value = CSSPropertyValue.keyword("center")
    #expect(value.isValid == true)
}

@Test func cssPropertyValue_url_isValid() {
    let value = CSSPropertyValue.url("https://example.com/img.png")
    #expect(value.isValid == true)
}

@Test func cssPropertyValue_invalid_isNotValid() {
    let value = CSSPropertyValue.invalid
    #expect(value.isValid == false)
}

// MARK: - Typed accessor: numberValue

@Test func cssPropertyValue_numberValue_returnsValue() {
    let value = CSSPropertyValue.number(123.5)
    #expect(value.numberValue == 123.5)
}

@Test func cssPropertyValue_numberValue_nil_forNonNumber() {
    let value = CSSPropertyValue.keyword("flex")
    #expect(value.numberValue == nil)
}

// MARK: - Typed accessor: percentageValue

@Test func cssPropertyValue_percentageValue_returnsValue() {
    let value = CSSPropertyValue.percentage(0.75)
    #expect(value.percentageValue == 0.75)
}

@Test func cssPropertyValue_percentageValue_nil_forNonPercentage() {
    let value = CSSPropertyValue.number(50)
    #expect(value.percentageValue == nil)
}

// MARK: - Typed accessor: colorValue

@Test func cssPropertyValue_colorValue_returnsColor() {
    let value = CSSPropertyValue.color(.blue)
    #expect(value.colorValue == UIColor.blue)
}

@Test func cssPropertyValue_colorValue_nil_forNonColor() {
    let value = CSSPropertyValue.invalid
    #expect(value.colorValue == nil)
}

// MARK: - Typed accessor: keywordValue

@Test func cssPropertyValue_keywordValue_returnsKeyword() {
    let value = CSSPropertyValue.keyword("hidden")
    #expect(value.keywordValue == "hidden")
}

@Test func cssPropertyValue_keywordValue_nil_forNonKeyword() {
    let value = CSSPropertyValue.number(0)
    #expect(value.keywordValue == nil)
}

// MARK: - Typed accessor: urlValue

@Test func cssPropertyValue_urlValue_returnsUrl() {
    let value = CSSPropertyValue.url("https://img.com/a.png")
    #expect(value.urlValue == "https://img.com/a.png")
}

@Test func cssPropertyValue_urlValue_nil_forNonUrl() {
    let value = CSSPropertyValue.keyword("none")
    #expect(value.urlValue == nil)
}

// MARK: - Typed accessor: shadowValue

@Test func cssPropertyValue_shadowValue_returnsShadow() {
    let shadow = CSSShadow(offsetX: 1, offsetY: 2, blur: 3, color: .black)
    let value = CSSPropertyValue.shadow(shadow)
    #expect(value.shadowValue == shadow)
}

@Test func cssPropertyValue_shadowValue_nil_forNonShadow() {
    let value = CSSPropertyValue.number(10)
    #expect(value.shadowValue == nil)
}

// MARK: - Equatable: same type, same value

@Test func cssPropertyValue_equatable_numberEqual() {
    #expect(CSSPropertyValue.number(10) == CSSPropertyValue.number(10))
}

@Test func cssPropertyValue_equatable_numberNotEqual() {
    #expect(CSSPropertyValue.number(10) != CSSPropertyValue.number(20))
}

@Test func cssPropertyValue_equatable_percentageEqual() {
    #expect(CSSPropertyValue.percentage(0.5) == CSSPropertyValue.percentage(0.5))
}

@Test func cssPropertyValue_equatable_colorEqual() {
    #expect(CSSPropertyValue.color(.red) == CSSPropertyValue.color(.red))
}

@Test func cssPropertyValue_equatable_keywordEqual() {
    #expect(CSSPropertyValue.keyword("flex") == CSSPropertyValue.keyword("flex"))
}

@Test func cssPropertyValue_equatable_urlEqual() {
    #expect(CSSPropertyValue.url("a.png") == CSSPropertyValue.url("a.png"))
}

@Test func cssPropertyValue_equatable_invalidEqual() {
    #expect(CSSPropertyValue.invalid == CSSPropertyValue.invalid)
}

// MARK: - Equatable: different types are never equal

@Test func cssPropertyValue_equatable_differentTypes_notEqual() {
    #expect(CSSPropertyValue.number(50) != CSSPropertyValue.percentage(50))
}

@Test func cssPropertyValue_equatable_numberVsKeyword_notEqual() {
    #expect(CSSPropertyValue.number(0) != CSSPropertyValue.keyword("0"))
}

// MARK: - CSSShadow Equatable

@Test func cssShadow_equatable_sameValues() {
    let a = CSSShadow(offsetX: 1, offsetY: 2, blur: 3, color: .black)
    let b = CSSShadow(offsetX: 1, offsetY: 2, blur: 3, color: .black)
    #expect(a == b)
}

@Test func cssShadow_equatable_differentBlur() {
    let a = CSSShadow(offsetX: 1, offsetY: 2, blur: 3, color: .black)
    let b = CSSShadow(offsetX: 1, offsetY: 2, blur: 5, color: .black)
    #expect(a != b)
}

@Test func cssShadow_withSpread_equatable() {
    let a = CSSShadow(offsetX: 0, offsetY: 2, blur: 4, spread: 1, color: .red)
    let b = CSSShadow(offsetX: 0, offsetY: 2, blur: 4, spread: 1, color: .red)
    #expect(a == b)
}

@Test func cssShadow_withSpread_differentSpread_notEqual() {
    let a = CSSShadow(offsetX: 0, offsetY: 2, blur: 4, spread: 1, color: .red)
    let b = CSSShadow(offsetX: 0, offsetY: 2, blur: 4, spread: 2, color: .red)
    #expect(a != b)
}
