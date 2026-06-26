import Testing
@testable import AGenUI

// =============================================================================
// CSSPropertyRegistry — Registration, Lookup, and Initialization
// =============================================================================

// Ensure registry is initialized before static config(for:) calls
private func ensureInitialized() {
    _ = CSSPropertyRegistry.shared
}

// MARK: - config(for:) on known properties

@Test func cssPropertyRegistry_configForOverflow_returnsConfig() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "overflow")
    #expect(config != nil)
    #expect(config?.name == "overflow")
    #expect(config?.valueType == .keyword)
    #expect(config?.priority == 45)
}

@Test func cssPropertyRegistry_configForVisibility_returnsConfig() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "visibility")
    #expect(config != nil)
    #expect(config?.name == "visibility")
    #expect(config?.defaultValue == .keyword("visible"))
}

@Test func cssPropertyRegistry_configForDisplay_lowerPriority() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "display")
    #expect(config != nil)
    #expect(config?.priority == 44)
    #expect(config?.validValues?.contains("none") == true)
    #expect(config?.validValues?.contains("flex") == true)
}

@Test func cssPropertyRegistry_configForBackground_colorType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "background")
    #expect(config != nil)
    #expect(config?.valueType == .color)
    #expect(config?.priority == 40)
}

@Test func cssPropertyRegistry_configForBackgroundColor_colorType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "background-color")
    #expect(config != nil)
    #expect(config?.valueType == .color)
}

@Test func cssPropertyRegistry_configForBorderRadius_dimensionType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "border-radius")
    #expect(config != nil)
    #expect(config?.valueType == .dimension)
    #expect(config?.defaultValue == .number(0))
}

@Test func cssPropertyRegistry_configForOpacity_opacityType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "opacity")
    #expect(config != nil)
    #expect(config?.valueType == .opacity)
    #expect(config?.defaultValue == .number(1.0))
}

@Test func cssPropertyRegistry_configForBackgroundImage_urlType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "background-image")
    #expect(config != nil)
    #expect(config?.valueType == .url)
    #expect(config?.priority == 39)
}

@Test func cssPropertyRegistry_configForFilter_shadowType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "filter")
    #expect(config != nil)
    #expect(config?.valueType == .shadow)
}

@Test func cssPropertyRegistry_configForBoxShadow_shadowType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "box-shadow")
    #expect(config != nil)
    #expect(config?.valueType == .shadow)
}

@Test func cssPropertyRegistry_configForBorderWidth_dimensionType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "border-width")
    #expect(config != nil)
    #expect(config?.valueType == .dimension)
    #expect(config?.defaultValue == .number(0))
}

@Test func cssPropertyRegistry_configForBorderStyle_keywordType() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "border-style")
    #expect(config != nil)
    #expect(config?.valueType == .keyword)
    #expect(config?.defaultValue == .keyword("solid"))
}

// MARK: - config(for:) on unknown property

@Test func cssPropertyRegistry_configForUnknown_returnsNil() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "non-existent-property")
    #expect(config == nil)
}

@Test func cssPropertyRegistry_configForEmptyString_returnsNil() {
    ensureInitialized()
    let config = CSSPropertyRegistry.config(for: "")
    #expect(config == nil)
}

// MARK: - getAllPropertyNames

@Test func cssPropertyRegistry_getAllPropertyNames_containsKnownProperties() {
    let names = CSSPropertyRegistry.shared.getAllPropertyNames()
    #expect(names.contains("overflow"))
    #expect(names.contains("visibility"))
    #expect(names.contains("display"))
    #expect(names.contains("opacity"))
    #expect(names.contains("background-image"))
}

@Test func cssPropertyRegistry_getAllPropertyNames_nonEmpty() {
    let names = CSSPropertyRegistry.shared.getAllPropertyNames()
    #expect(names.count >= 12) // At least all the registered properties
}

// MARK: - Priority ordering consistency

@Test func cssPropertyRegistry_backgroundImage_lowerPriority_thanBackgroundColor() {
    ensureInitialized()
    let bgColor = CSSPropertyRegistry.config(for: "background-color")
    let bgImage = CSSPropertyRegistry.config(for: "background-image")
    #expect(bgImage!.priority < bgColor!.priority) // lower number = lower priority
}

@Test func cssPropertyRegistry_display_lowerPriority_thanVisibility() {
    ensureInitialized()
    let display = CSSPropertyRegistry.config(for: "display")
    let visibility = CSSPropertyRegistry.config(for: "visibility")
    #expect(display!.priority < visibility!.priority)
}
