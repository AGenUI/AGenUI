import Testing
import UIKit
@testable import AGenUI

// MARK: - UIColor+Hex Unit Tests
// Tests for the UIColor hex string parsing extension

// ============================================================================
// init?(hexString:) — Valid 6-digit Hex (#RRGGBB)
// ============================================================================

@Test func uiColorHex_valid6DigitRed_createsCorrectColor() {
    let color = UIColor(hexString: "#FF0000")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 1.0)
    #expect(g == 0.0)
    #expect(b == 0.0)
    #expect(a == 1.0)
}

@Test func uiColorHex_valid6DigitGreen_createsCorrectColor() {
    let color = UIColor(hexString: "#00FF00")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 0.0)
    #expect(g == 1.0)
    #expect(b == 0.0)
    #expect(a == 1.0)
}

@Test func uiColorHex_valid6DigitBlue_createsCorrectColor() {
    let color = UIColor(hexString: "#0000FF")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 0.0)
    #expect(g == 0.0)
    #expect(b == 1.0)
    #expect(a == 1.0)
}

@Test func uiColorHex_valid6DigitBlack_createsCorrectColor() {
    let color = UIColor(hexString: "#000000")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 0.0)
    #expect(g == 0.0)
    #expect(b == 0.0)
    #expect(a == 1.0)
}

@Test func uiColorHex_valid6DigitWhite_createsCorrectColor() {
    let color = UIColor(hexString: "#FFFFFF")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 1.0)
    #expect(g == 1.0)
    #expect(b == 1.0)
    #expect(a == 1.0)
}

@Test func uiColorHex_valid6DigitLowerCase_createsCorrectColor() {
    let color = UIColor(hexString: "#ff8800")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 1.0)
    #expect(abs(g - 136.0/255.0) < 0.001)
    #expect(b == 0.0)
}

// ============================================================================
// init?(hexString:) — Valid 8-digit Hex (#RRGGBBAA)
// ============================================================================

@Test func uiColorHex_valid8Digit_fullOpacity_createsCorrectColor() {
    let color = UIColor(hexString: "#FF0000FF")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 1.0)
    #expect(g == 0.0)
    #expect(b == 0.0)
    #expect(a == 1.0)
}

@Test func uiColorHex_valid8Digit_halfOpacity_createsCorrectColor() {
    let color = UIColor(hexString: "#FF000080")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 1.0)
    #expect(g == 0.0)
    #expect(b == 0.0)
    #expect(abs(a - 128.0/255.0) < 0.001)
}

@Test func uiColorHex_valid8Digit_zeroOpacity_createsCorrectColor() {
    let color = UIColor(hexString: "#FF000000")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 1.0)
    #expect(g == 0.0)
    #expect(b == 0.0)
    #expect(a == 0.0)
}

// ============================================================================
// init?(hexString:) — Without Hash Prefix
// ============================================================================

@Test func uiColorHex_withoutHashPrefix_createsCorrectColor() {
    let color = UIColor(hexString: "FF0000")
    #expect(color != nil)
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color?.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(r == 1.0)
    #expect(g == 0.0)
    #expect(b == 0.0)
}

// ============================================================================
// init?(hexString:) — Error Cases
// ============================================================================

@Test func uiColorHex_3DigitHex_returnsNil() {
    // Only 6 and 8 digit hex are supported
    let color = UIColor(hexString: "#F00")
    #expect(color == nil)
}

@Test func uiColorHex_invalidChars_returnsNil() {
    let color = UIColor(hexString: "#GGGGGG")
    #expect(color == nil)
}

@Test func uiColorHex_emptyString_returnsNil() {
    let color = UIColor(hexString: "")
    #expect(color == nil)
}

@Test func uiColorHex_onlyHash_returnsNil() {
    let color = UIColor(hexString: "#")
    #expect(color == nil)
}

@Test func uiColorHex_tooShort_returnsNil() {
    let color = UIColor(hexString: "#FFF")
    #expect(color == nil)
}

@Test func uiColorHex_tooLong_returnsNil() {
    let color = UIColor(hexString: "#FFFFFFFFFF")
    #expect(color == nil)
}

@Test func uiColorHex_withWhitespace_handlesCorrectly() {
    // Implementation trims whitespace
    let color = UIColor(hexString: "  #FF0000  ")
    #expect(color != nil)
}

// ============================================================================
// from(hexString:defaultColor:) — Non-optional Variant
// ============================================================================

@Test func uiColorHex_from_validHex_returnsColor() {
    let color = UIColor.from(hexString: "#00FF00")
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color.getRed(&r, green: &g, blue: &b, alpha: &a)
    #expect(g == 1.0)
}

@Test func uiColorHex_from_invalidHex_returnsDefault() {
    let defaultColor = UIColor.red
    let color = UIColor.from(hexString: "invalid", defaultColor: defaultColor)
    #expect(color == defaultColor)
}

@Test func uiColorHex_from_emptyString_returnsDefault() {
    let defaultColor = UIColor.blue
    let color = UIColor.from(hexString: "", defaultColor: defaultColor)
    #expect(color == defaultColor)
}
