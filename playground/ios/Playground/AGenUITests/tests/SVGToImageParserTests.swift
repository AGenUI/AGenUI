import Testing
import UIKit
@testable import AGenUI

// MARK: - SVGXMLParser Unit Tests
// Black-box tests via the public init(data:)/parse() entry to exercise the
// internally private parseSVGColor / colorFromHex / parseLineCap /
// parseLineJoin / parsePathData / tokenize logic, plus dimension parsing
// and global style inheritance.

// ============================================================================
// Helpers
// ============================================================================

private func parseSVG(_ xml: String) -> (parser: SVGXMLParser, shapes: [SVGShape]) {
    let data = xml.data(using: .utf8)!
    let parser = SVGXMLParser(data: data)
    let shapes = parser.parse()
    return (parser, shapes)
}

private func rgba(_ color: UIColor) -> (CGFloat, CGFloat, CGFloat, CGFloat) {
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    color.getRed(&r, green: &g, blue: &b, alpha: &a)
    return (r, g, b, a)
}

// ============================================================================
// dimensions: viewBox / width / height
// ============================================================================

@Test func svgParser_viewBox_fourNumbers_isParsed() {
    let xml = #"<svg viewBox="0 0 24 24"><path d="M0 0 L1 1"/></svg>"#
    let (parser, _) = parseSVG(xml)
    #expect(parser.viewBox == CGRect(x: 0, y: 0, width: 24, height: 24))
}

@Test func svgParser_viewBox_nonZeroOrigin_isParsed() {
    let xml = #"<svg viewBox="-5 10 100 200"><path d="M0 0 L1 1"/></svg>"#
    let (parser, _) = parseSVG(xml)
    #expect(parser.viewBox == CGRect(x: -5, y: 10, width: 100, height: 200))
}

@Test func svgParser_widthHeight_areParsedAsCGFloat() {
    let xml = #"<svg width="48" height="36" viewBox="0 0 48 36"><path d="M0 0"/></svg>"#
    let (parser, _) = parseSVG(xml)
    #expect(parser.svgWidth == 48)
    #expect(parser.svgHeight == 36)
}

@Test func svgParser_missingViewBox_fallsBackToWidthHeight() {
    let xml = #"<svg width="100" height="200"><path d="M0 0 L1 1"/></svg>"#
    let (parser, _) = parseSVG(xml)
    #expect(parser.viewBox == CGRect(x: 0, y: 0, width: 100, height: 200))
}

@Test func svgParser_missingViewBoxAndDimensions_viewBoxIsNil() {
    let xml = #"<svg><path d="M0 0"/></svg>"#
    let (parser, _) = parseSVG(xml)
    #expect(parser.viewBox == nil)
    #expect(parser.svgWidth == nil)
    #expect(parser.svgHeight == nil)
}

@Test func svgParser_invalidViewBoxNumberCount_isIgnored() {
    let xml = #"<svg viewBox="0 0 24"><path d="M0 0"/></svg>"#
    let (parser, _) = parseSVG(xml)
    #expect(parser.viewBox == nil)
}

// ============================================================================
// element handling: path / rect / circle / ellipse / line
// ============================================================================

@Test func svgParser_pathElement_producesOneShape() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0 L10 10"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_rectElement_producesOneShape() {
    let xml = #"<svg viewBox="0 0 10 10"><rect x="1" y="2" width="6" height="4"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_rectElement_withCornerRadius_producesOneShape() {
    let xml = #"<svg viewBox="0 0 10 10"><rect x="0" y="0" width="6" height="4" rx="1" ry="2"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_circleElement_producesOneShape() {
    let xml = #"<svg viewBox="0 0 10 10"><circle cx="5" cy="5" r="3"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_ellipseElement_producesOneShape() {
    let xml = #"<svg viewBox="0 0 10 10"><ellipse cx="5" cy="5" rx="4" ry="2"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_lineElement_producesOneShape() {
    let xml = #"<svg viewBox="0 0 10 10"><line x1="0" y1="0" x2="10" y2="10"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_multipleElements_produceCorrectCount() {
    let xml = #"""
    <svg viewBox="0 0 10 10">
      <path d="M0 0 L1 1"/>
      <rect x="0" y="0" width="2" height="2"/>
      <circle cx="5" cy="5" r="1"/>
    </svg>
    """#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 3)
}

@Test func svgParser_unknownElement_isIgnored() {
    let xml = #"<svg viewBox="0 0 10 10"><polygon points="0,0 1,1"/><path d="M0 0 L1 1"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    // Only path is recognized; polygon is ignored
    #expect(shapes.count == 1)
}

// ============================================================================
// parseSVGColor: hex / named / none / currentColor
// ============================================================================

@Test func svgParser_fillNone_producesClearFillColor() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0 L1 1" fill="none"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].fillColor == UIColor.clear)
}

@Test func svgParser_fillCurrentColor_producesNilFillColor() {
    // currentColor is mapped to nil so caller can substitute tintColor
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0 L1 1" fill="currentColor"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].fillColor == nil)
}

@Test func svgParser_fillHex6_redIsParsedCorrectly() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0 L1 1" fill="#FF0000"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (r, g, b, a) = rgba(shapes[0].fillColor!)
    #expect(r == 1.0)
    #expect(g == 0.0)
    #expect(b == 0.0)
    #expect(a == 1.0)
}

@Test func svgParser_fillHex6_lowerCaseIsParsed() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0 L1 1" fill="#00ff00"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (r, g, b, _) = rgba(shapes[0].fillColor!)
    #expect(r == 0.0)
    #expect(g == 1.0)
    #expect(b == 0.0)
}

@Test func svgParser_fillHex3_isParsed() {
    // #F00 ≈ #FF0000
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0 L1 1" fill="#F00"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (r, g, b, a) = rgba(shapes[0].fillColor!)
    #expect(r == 1.0)
    #expect(g == 0.0)
    #expect(b == 0.0)
    #expect(a == 1.0)
}

@Test func svgParser_fillHex8_includesAlpha() {
    // #RRGGBBAA: 80 alpha = 128/255
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0 L1 1" fill="#FF000080"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (r, _, _, a) = rgba(shapes[0].fillColor!)
    #expect(r == 1.0)
    #expect(abs(a - 128.0/255.0) < 0.01)
}

@Test func svgParser_fillHexInvalidLength_returnsNil() {
    // 5-digit hex has no case → nil
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0 L1 1" fill="#FFFFF"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].fillColor == nil)
}

@Test func svgParser_fillNamedBlack_isParsed() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" fill="black"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (r, g, b, _) = rgba(shapes[0].fillColor!)
    #expect(r == 0.0); #expect(g == 0.0); #expect(b == 0.0)
}

@Test func svgParser_fillNamedWhite_isParsed() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" fill="white"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (r, g, b, _) = rgba(shapes[0].fillColor!)
    #expect(r == 1.0); #expect(g == 1.0); #expect(b == 1.0)
}

@Test func svgParser_fillNamedColors_areAllRecognized() {
    let names = ["red", "green", "blue", "yellow", "gray", "grey", "orange", "purple"]
    for name in names {
        let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" fill=""# + name + #""/></svg>"#
        let (_, shapes) = parseSVG(xml)
        #expect(shapes[0].fillColor != nil, "color name \(name) should be recognized")
    }
}

@Test func svgParser_fillUnknownName_returnsNil() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" fill="rebeccapurple"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].fillColor == nil)
}

@Test func svgParser_strokeColor_isParsedSeparatelyFromFill() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" fill="red" stroke="#0000FF"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (sr, sg, sb, _) = rgba(shapes[0].strokeColor!)
    #expect(sr == 0.0); #expect(sg == 0.0); #expect(sb == 1.0)
}

// ============================================================================
// parseLineCap / parseLineJoin
// ============================================================================

@Test func svgParser_lineCapRound_isParsed() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" stroke="black" stroke-linecap="round"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].lineCap == .round)
}

@Test func svgParser_lineCapSquare_isParsed() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" stroke="black" stroke-linecap="square"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].lineCap == .square)
}

@Test func svgParser_lineCapUnknown_defaultsToButt() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" stroke="black" stroke-linecap="weird"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].lineCap == .butt)
}

@Test func svgParser_lineJoinRound_isParsed() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" stroke="black" stroke-linejoin="round"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].lineJoin == .round)
}

@Test func svgParser_lineJoinBevel_isParsed() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" stroke="black" stroke-linejoin="bevel"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].lineJoin == .bevel)
}

@Test func svgParser_lineJoinUnknown_defaultsToMiter() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" stroke="black" stroke-linejoin="weird"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].lineJoin == .miter)
}

@Test func svgParser_strokeWidth_isParsedAsCGFloat() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" stroke="black" stroke-width="3.5"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].strokeWidth == 3.5)
}

@Test func svgParser_strokeWidthMissing_isNil() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0 0" stroke="black"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].strokeWidth == nil)
}

// ============================================================================
// global style inheritance from <svg>
// ============================================================================

@Test func svgParser_globalFill_isInheritedByChildWithoutFill() {
    let xml = #"<svg viewBox="0 0 10 10" fill="#FF0000"><path d="M0 0"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (r, _, _, _) = rgba(shapes[0].fillColor!)
    #expect(r == 1.0)
}

@Test func svgParser_childFill_overridesGlobalFill() {
    let xml = #"<svg viewBox="0 0 10 10" fill="#FF0000"><path d="M0 0" fill="#00FF00"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (r, g, _, _) = rgba(shapes[0].fillColor!)
    #expect(r == 0.0)
    #expect(g == 1.0)
}

@Test func svgParser_globalStroke_isInheritedByChild() {
    let xml = #"<svg viewBox="0 0 10 10" stroke="#0000FF"><path d="M0 0"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    let (_, _, b, _) = rgba(shapes[0].strokeColor!)
    #expect(b == 1.0)
}

@Test func svgParser_globalStrokeWidth_isInherited() {
    let xml = #"<svg viewBox="0 0 10 10" stroke-width="2.5"><path d="M0 0" stroke="black"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].strokeWidth == 2.5)
}

@Test func svgParser_globalLineCap_isInherited() {
    let xml = #"<svg viewBox="0 0 10 10" stroke-linecap="round"><path d="M0 0" stroke="black"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].lineCap == .round)
}

@Test func svgParser_globalLineJoin_isInherited() {
    let xml = #"<svg viewBox="0 0 10 10" stroke-linejoin="bevel"><path d="M0 0" stroke="black"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes[0].lineJoin == .bevel)
}

// ============================================================================
// parsePathData: M / L / H / V / C / S / Q / T / A / Z
// (Black-box: assert successful shape creation with non-empty path)
// ============================================================================

@Test func svgParser_pathMoveLine_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 10 L90 90"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathHorizontalVertical_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 10 H90 V90"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathCubicBezier_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 10 C20 20 80 80 90 90"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathSmoothCubic_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 10 C20 20 30 30 40 40 S60 60 70 70"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathQuadratic_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 10 Q50 50 90 10"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathSmoothQuadratic_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 10 Q30 30 50 10 T90 10"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathArc_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 50 A20 20 0 0 1 90 50"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathClose_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 10 L90 10 L50 90 Z"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathRelativeCommands_areParsed() {
    // Lowercase letters are relative
    let xml = #"<svg viewBox="0 0 100 100"><path d="m10 10 l20 20 h10 v10"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathImplicitRepeatedMoveTo_treatedAsLine() {
    // "M 10 10 20 20" => move to (10,10) then implicit lineTo (20,20)
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10 10 20 20 30 30"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathWithCommas_isParsed() {
    let xml = #"<svg viewBox="0 0 100 100"><path d="M10,10 L20,20 C30,30 40,40 50,50"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathWithNegativeNumbers_isParsed() {
    // tokenize must handle inline negative signs without separators
    let xml = #"<svg viewBox="-50 -50 100 100"><path d="M-10-10L-20-20"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathWithDecimalNumbers_isParsed() {
    let xml = #"<svg viewBox="0 0 10 10"><path d="M0.5 0.5 L9.5 9.5"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathWithExponentialNumbers_isParsed() {
    // Scientific notation tokens
    let xml = #"<svg viewBox="0 0 100 100"><path d="M1e1 1e1 L9e1 9e1"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(!shapes[0].path.cgPath.isEmpty)
}

@Test func svgParser_pathEmptyData_producesEmptyPath() {
    let xml = #"<svg viewBox="0 0 10 10"><path d=""/></svg>"#
    let (_, shapes) = parseSVG(xml)
    // Element is still recognized but path has no segments
    #expect(shapes.count == 1)
    #expect(shapes[0].path.cgPath.isEmpty)
}

// ============================================================================
// edge: malformed input should not crash
// ============================================================================

@Test func svgParser_emptyInput_returnsEmptyShapes() {
    let xml = ""
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.isEmpty)
}

@Test func svgParser_nonSVGXML_returnsEmptyShapes() {
    let xml = "<root><foo/></root>"
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.isEmpty)
}

@Test func svgParser_rectWithoutDimensions_usesZeroDefaults() {
    // Missing width/height → 0, still creates shape (empty rect)
    let xml = #"<svg viewBox="0 0 10 10"><rect/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
}

@Test func svgParser_circleWithoutRadius_usesZeroDefault() {
    let xml = #"<svg viewBox="0 0 10 10"><circle cx="5" cy="5"/></svg>"#
    let (_, shapes) = parseSVG(xml)
    #expect(shapes.count == 1)
}
