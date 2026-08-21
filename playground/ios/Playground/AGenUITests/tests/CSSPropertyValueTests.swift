import Testing
@testable import AGenUI
import UIKit

// =============================================================================
// CSSPropertyValue — Equatable
// =============================================================================

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
