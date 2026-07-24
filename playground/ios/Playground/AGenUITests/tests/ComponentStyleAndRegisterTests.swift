import Testing
import UIKit
@testable import AGenUI

// ═══════════════════════════════════════════════════════════════════════════════════
// ComponentStyleConfigManager Tests
// ═══════════════════════════════════════════════════════════════════════════════════

// --- parseSize ---

@Test func styleConfigManager_parseSize_validPixelValue_returnsHalfPoint() {
    let result = ComponentStyleConfigManager.parseSize("48px")
    #expect(result == 24.0)
}

@Test func styleConfigManager_parseSize_zeroPixels_returnsZero() {
    let result = ComponentStyleConfigManager.parseSize("0px")
    #expect(result == 0.0)
}

@Test func styleConfigManager_parseSize_decimalPixels_returnsHalf() {
    let result = ComponentStyleConfigManager.parseSize("33px")
    #expect(result == 16.5)
}

@Test func styleConfigManager_parseSize_invalidString_returnsNil() {
    let result = ComponentStyleConfigManager.parseSize("abc")
    #expect(result == nil)
}

@Test func styleConfigManager_parseSize_emptyString_returnsNil() {
    let result = ComponentStyleConfigManager.parseSize("")
    #expect(result == nil)
}

@Test func styleConfigManager_parseSize_numericWithSpaces_returnsValue() {
    let result = ComponentStyleConfigManager.parseSize("  100px  ")
    #expect(result == 50.0)
}

// --- parseFontWeight ---

@Test func styleConfigManager_parseFontWeight_bold_returnsBold() {
    let result = ComponentStyleConfigManager.parseFontWeight("bold")
    #expect(result == .bold)
}

@Test func styleConfigManager_parseFontWeight_normal_returnsRegular() {
    let result = ComponentStyleConfigManager.parseFontWeight("normal")
    #expect(result == .regular)
}

@Test func styleConfigManager_parseFontWeight_numericBold_returnsBold() {
    let result = ComponentStyleConfigManager.parseFontWeight("700")
    #expect(result == .bold)
}

@Test func styleConfigManager_parseFontWeight_numeric500_returnsBold() {
    let result = ComponentStyleConfigManager.parseFontWeight("500")
    #expect(result == .bold)
}

@Test func styleConfigManager_parseFontWeight_numeric400_returnsRegular() {
    let result = ComponentStyleConfigManager.parseFontWeight("400")
    #expect(result == .regular)
}

@Test func styleConfigManager_parseFontWeight_uppercase_returnsBold() {
    let result = ComponentStyleConfigManager.parseFontWeight("BOLD")
    #expect(result == .bold)
}

@Test func styleConfigManager_parseFontWeight_unknown_returnsRegular() {
    let result = ComponentStyleConfigManager.parseFontWeight("unknown")
    #expect(result == .regular)
}

// --- parseContentMode ---

@Test func styleConfigManager_parseContentMode_fill_returnsAspectFill() {
    let result = ComponentStyleConfigManager.parseContentMode("fill")
    #expect(result == .scaleAspectFill)
}

@Test func styleConfigManager_parseContentMode_fit_returnsAspectFit() {
    let result = ComponentStyleConfigManager.parseContentMode("fit")
    #expect(result == .scaleAspectFit)
}

@Test func styleConfigManager_parseContentMode_scaleToFill_returnsScaleToFill() {
    let result = ComponentStyleConfigManager.parseContentMode("scaletofill")
    #expect(result == .scaleToFill)
}

@Test func styleConfigManager_parseContentMode_unknown_defaultsToAspectFill() {
    let result = ComponentStyleConfigManager.parseContentMode("unknown")
    #expect(result == .scaleAspectFill)
}

@Test func styleConfigManager_parseContentMode_caseInsensitive_works() {
    let result = ComponentStyleConfigManager.parseContentMode("FIT")
    #expect(result == .scaleAspectFit)
}

// --- parseTime ---

@Test func styleConfigManager_parseTime_validMs_returnsSeconds() {
    let result = ComponentStyleConfigManager.parseTime("300ms")
    #expect(result == 0.3)
}

@Test func styleConfigManager_parseTime_zeroMs_returnsZero() {
    let result = ComponentStyleConfigManager.parseTime("0ms")
    #expect(result == 0.0)
}

@Test func styleConfigManager_parseTime_invalidString_returnsNil() {
    let result = ComponentStyleConfigManager.parseTime("abc")
    #expect(result == nil)
}

// --- getConfig ---

@Test func styleConfigManager_getConfig_tabs_returnsNonNil() {
    let config = ComponentStyleConfigManager.shared.getConfig(for: "Tabs")
    #expect(config != nil)
    #expect(config?["tab-mode"] as? String == "fixed")
}

@Test func styleConfigManager_getConfig_button_returnsNonNil() {
    let config = ComponentStyleConfigManager.shared.getConfig(for: "Button")
    #expect(config != nil)
    #expect(config?["disabled-opacity"] as? String == "0.4")
}

@Test func styleConfigManager_getConfig_unknown_returnsNil() {
    let config = ComponentStyleConfigManager.shared.getConfig(for: "UnknownWidget")
    #expect(config == nil)
}

@Test func styleConfigManager_getConfig_slider_returnsExpectedKeys() {
    let config = ComponentStyleConfigManager.shared.getConfig(for: "Slider")
    #expect(config != nil)
    #expect(config?["slider-height"] as? String == "48px")
    #expect(config?["track-height"] as? String == "4px")
}

// ═══════════════════════════════════════════════════════════════════════════════════
// ComponentRegister Tests
// ═══════════════════════════════════════════════════════════════════════════════════

@Test func componentRegister_createComponent_textType_returnsNonNil() {
    let component = ComponentRegister.shared.createComponent("Text", id: "t1", properties: [:])
    #expect(component != nil)
}

@Test func componentRegister_createComponent_unknownType_returnsNil() {
    let component = ComponentRegister.shared.createComponent("UnknownXYZ", id: "u1", properties: [:])
    #expect(component == nil)
}

@Test func componentRegister_classForType_text_returnsTextComponent() {
    let cls = ComponentRegister.shared.classForType("Text")
    #expect(cls != nil)
    #expect(cls == TextComponent.self)
}

@Test func componentRegister_classForType_unknown_returnsNil() {
    let cls = ComponentRegister.shared.classForType("UnknownXYZ")
    #expect(cls == nil)
}

@Test func componentRegister_createComponent_row_returnsRowComponent() {
    let component = ComponentRegister.shared.createComponent("Row", id: "r1", properties: [:])
    #expect(component != nil)
    #expect(component is RowComponent)
}

@Test func componentRegister_createComponent_column_returnsColumnComponent() {
    let component = ComponentRegister.shared.createComponent("Column", id: "c1", properties: [:])
    #expect(component != nil)
    #expect(component is ColumnComponent)
}

@Test func componentRegister_createComponent_button_returnsButtonComponent() {
    let component = ComponentRegister.shared.createComponent("Button", id: "b1", properties: [:])
    #expect(component != nil)
    #expect(component is ButtonComponent)
}

@Test func componentRegister_createComponent_image_returnsImageComponent() {
    let component = ComponentRegister.shared.createComponent("Image", id: "i1", properties: [:])
    #expect(component != nil)
    #expect(component is ImageComponent)
}

// ═══════════════════════════════════════════════════════════════════════════════════
// CAGradientLayerFactory Tests
// ═══════════════════════════════════════════════════════════════════════════════════

@Test func gradientLayerFactory_configure_lessThanTwoStops_returnsFalse() {
    var info = AGUIGradientInfo()
    info.gradientType = .linear
    info.colorStops = [AGUIColorStop(color: 0xFFFF0000, position: 0, unit: .percent, hasPosition: true, isHint: false)]
    let layer = CAGradientLayer()
    let result = CAGradientLayerFactory.configure(info, on: layer, bounds: CGRect(x: 0, y: 0, width: 100, height: 100))
    #expect(result == false)
}

@Test func gradientLayerFactory_configure_twoStopsLinear_returnsTrue() {
    var info = AGUIGradientInfo()
    info.gradientType = .linear
    info.colorStops = [
        AGUIColorStop(color: 0xFFFF0000, position: 0, unit: .percent, hasPosition: true, isHint: false),
        AGUIColorStop(color: 0xFF0000FF, position: 1, unit: .percent, hasPosition: true, isHint: false)
    ]
    info.linear = AGUILinearGradientParams(angle: 180)
    let layer = CAGradientLayer()
    let result = CAGradientLayerFactory.configure(info, on: layer, bounds: CGRect(x: 0, y: 0, width: 100, height: 100))
    #expect(result == true)
    #expect(layer.type == .axial)
}

@Test func gradientLayerFactory_configure_zeroBounds_returnsTrue() {
    var info = AGUIGradientInfo()
    info.gradientType = .linear
    info.colorStops = [
        AGUIColorStop(color: 0xFFFF0000, position: 0, unit: .percent, hasPosition: true, isHint: false),
        AGUIColorStop(color: 0xFF0000FF, position: 1, unit: .percent, hasPosition: true, isHint: false)
    ]
    info.linear = AGUILinearGradientParams(angle: 0)
    let layer = CAGradientLayer()
    let result = CAGradientLayerFactory.configure(info, on: layer, bounds: .zero)
    #expect(result == true)
}

@Test func gradientLayerFactory_configure_radialType_returnsRadialLayer() {
    var info = AGUIGradientInfo()
    info.gradientType = .radial
    info.colorStops = [
        AGUIColorStop(color: 0xFFFF0000, position: 0, unit: .percent, hasPosition: true, isHint: false),
        AGUIColorStop(color: 0xFF00FF00, position: 1, unit: .percent, hasPosition: true, isHint: false)
    ]
    info.radial = AGUIRadialGradientParams()
    let layer = CAGradientLayer()
    let result = CAGradientLayerFactory.configure(info, on: layer, bounds: CGRect(x: 0, y: 0, width: 200, height: 200))
    #expect(result == true)
    #expect(layer.type == .radial)
}

@Test func gradientLayerFactory_configure_conicType_returnsConicLayer() {
    var info = AGUIGradientInfo()
    info.gradientType = .conic
    info.colorStops = [
        AGUIColorStop(color: 0xFFFF0000, position: 0, unit: .percent, hasPosition: true, isHint: false),
        AGUIColorStop(color: 0xFF00FF00, position: 1, unit: .percent, hasPosition: true, isHint: false)
    ]
    info.conic = AGUIConicGradientParams()
    let layer = CAGradientLayer()
    let result = CAGradientLayerFactory.configure(info, on: layer, bounds: CGRect(x: 0, y: 0, width: 200, height: 200))
    #expect(result == true)
    #expect(layer.type == .conic)
}
