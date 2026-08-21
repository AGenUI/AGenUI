import Testing
import UIKit
@testable import AGenUI

// MARK: - CSSPropertyApplier Integration Tests
// Behavior tests at the `apply(properties:to:)` seam. Observe outcomes on a plain
// UIView (isHidden, borderWidth, cornerRadius, clipsToBounds) — never reach into
// applier internals. These lock the behaviors introduced/changed by the registry
// removal: ordered application (display overrides visibility), border-style invalid
// reset, and the unitless-dimension ×0.5 fix.

// ============================================================================
// Application order: display overrides visibility (both write isHidden)
// ============================================================================

@Test @MainActor func apply_displayFlex_overridesVisibilityHidden() {
    let view = UIView()
    CSSPropertyApplier.apply(properties: ["visibility": "hidden", "display": "flex"], to: view)
    // display applied after visibility → wins
    #expect(view.isHidden == false)
}

@Test @MainActor func apply_displayNone_overridesVisibilityVisible() {
    let view = UIView()
    CSSPropertyApplier.apply(properties: ["visibility": "visible", "display": "none"], to: view)
    // display:none wins
    #expect(view.isHidden == true)
}

@Test @MainActor func apply_visibilityHidden_alone_setsHidden() {
    let view = UIView()
    CSSPropertyApplier.apply(properties: ["visibility": "hidden"], to: view)
    #expect(view.isHidden == true)
}

@Test @MainActor func apply_displayNone_alone_setsHidden() {
    let view = UIView()
    CSSPropertyApplier.apply(properties: ["display": "none"], to: view)
    #expect(view.isHidden == true)
}

// ============================================================================
// border-style: invalid value resets border-width (formerly dead code, now live)
// ============================================================================

@Test @MainActor func apply_borderStyleInvalid_resetsBorderWidth() {
    let view = UIView()
    view.layer.borderWidth = 5.0
    CSSPropertyApplier.apply(properties: ["border-style": "dashed"], to: view)
    // unsupported style → clear border
    #expect(view.layer.borderWidth == 0)
}

@Test @MainActor func apply_borderStyleSolid_withUnitlessWidth_appliesScale() {
    let view = UIView()
    CSSPropertyApplier.apply(properties: ["border-width": "5", "border-style": "solid"], to: view)
    // border-width:5 → 5 * 0.5 = 2.5 (unitless dimension fix); border-style:solid → no-op
    #expect(view.layer.borderWidth == 2.5)
}

// ============================================================================
// Unitless dimension × BS_POINT_SCALE fix (applied via border-radius)
// ============================================================================

@Test @MainActor func apply_borderRadiusUnitless_appliesScale() {
    let view = UIView()
    CSSPropertyApplier.apply(properties: ["border-radius": "8"], to: view)
    // 8 * 0.5 = 4.0 (plain view → layer.cornerRadius)
    #expect(view.layer.cornerRadius == 4.0)
}

// ============================================================================
// Clip decision: overflow + border-radius (applyClipDecision, runs after the loop)
// ============================================================================

@Test @MainActor func apply_overflowHidden_setsClipsToBounds() {
    let view = UIView()
    CSSPropertyApplier.apply(properties: ["overflow": "hidden"], to: view)
    #expect(view.clipsToBounds == true)
}

@Test @MainActor func apply_overflowVisible_clearsClipsToBounds() {
    let view = UIView()
    view.clipsToBounds = true
    CSSPropertyApplier.apply(properties: ["overflow": "visible"], to: view)
    #expect(view.clipsToBounds == false)
}

@Test @MainActor func apply_borderRadiusPositive_setsClipsToBounds() {
    let view = UIView()
    CSSPropertyApplier.apply(properties: ["border-radius": "10"], to: view)
    // radius 10*0.5=5 > 0 → clips on
    #expect(view.clipsToBounds == true)
}

// ============================================================================
// Unknown property: no effect
// ============================================================================

@Test @MainActor func apply_unknownProperty_noEffect() {
    let view = UIView()
    view.isHidden = false
    CSSPropertyApplier.apply(properties: ["not-a-real-property": "whatever"], to: view)
    #expect(view.isHidden == false)
}
