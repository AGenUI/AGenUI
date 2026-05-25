//
//  CSSPaddingResolver.swift
//  AGenUI
//
//  Shared CSS `padding` resolver used by all leaf-style components
//  (Text / Image / Button / ...).
//
//  Background:
//    The C++ Yoga engine has already accounted for `padding` when sizing the
//    leaf node's borderBox layout, but the native UIKit subview (UILabel,
//    UIImageView, button content, ...) draws its content over the entire
//    parent frame. Translating CSS padding into the four edge-anchor
//    constants on the inner subview shrinks the rendered content into the
//    contentBox, mirroring Android `View.setPadding(...)` and Harmony
//    `BaseNode.setPadding(...)`. This is NOT a double-count with Yoga since
//    the outer view's frame is unchanged.
//
//  Parsing rules (kept in lockstep with Android / Harmony):
//    All shorthand and per-edge string parsing is delegated to the canonical
//    C++ `EdgeInsetsParser` via `AGenUIEdgeInsetsBridge`. Only `px` (and
//    unitless, which the parser normalizes to px) is honored on the platform
//    layer; every other CSS unit collapses to 0. The render layer
//    intentionally stays px-only so the three platforms render identically
//    without dragging viewport / font-baseline / physical-unit math into
//    platform code.
//    `padding-top/right/bottom/left` always override the shorthand.
//    Negative values are clamped to 0.
//    Numeric tokens (NSNumber / Int / Double / CGFloat) are accepted and
//    treated as raw px (legacy contract used by some style sources).
//
//  Output: four iOS points, scaled by `pointScale = 0.5` to match the same
//  px -> pt scaling used by font-size and line-height across the codebase.

import UIKit

/// Resolved CSS padding in iOS points. Edges are clamped to >= 0.
struct CSSPadding {
    let top: CGFloat
    let right: CGFloat
    let bottom: CGFloat
    let left: CGFloat

    static let zero = CSSPadding(top: 0, right: 0, bottom: 0, left: 0)

    var hasAny: Bool {
        return top > 0 || right > 0 || bottom > 0 || left > 0
    }
}

enum CSSPaddingResolver {

    /// Same px -> pt scaling factor used by font-size and line-height
    /// elsewhere in the iOS render layer (see `TextComponent.BS_POINT_SCALE`).
    static let pointScale: CGFloat = 0.5

    /// Resolve a CSS `styles` dictionary into a 4-edge padding tuple.
    /// Returns `CSSPadding.zero` when no padding-* key is present so callers
    /// can rely on a non-nil result and decide whether to apply or skip.
    static func resolve(_ styles: [String: Any]) -> CSSPadding {
        var top: CGFloat = 0
        var right: CGFloat = 0
        var bottom: CGFloat = 0
        var left: CGFloat = 0

        // Shorthand: `padding`
        if let raw = styles["padding"], let shorthand = resolveShorthand(raw) {
            top = shorthand.top
            right = shorthand.right
            bottom = shorthand.bottom
            left = shorthand.left
        }

        // Per-edge overrides
        if let v = parseLengthPt(styles["padding-top"]) { top = v }
        if let v = parseLengthPt(styles["padding-right"]) { right = v }
        if let v = parseLengthPt(styles["padding-bottom"]) { bottom = v }
        if let v = parseLengthPt(styles["padding-left"]) { left = v }

        return CSSPadding(
            top: max(0, top),
            right: max(0, right),
            bottom: max(0, bottom),
            left: max(0, left)
        )
    }

    /// Returns true when the styles object carries any `padding*` key.
    /// Used by callers that want to skip touching edge constraints when the
    /// caller never asked for padding (avoids overwriting prior values).
    static func hasAnyPaddingKey(_ styles: [String: Any]) -> Bool {
        return styles["padding"] != nil
            || styles["padding-top"] != nil
            || styles["padding-right"] != nil
            || styles["padding-bottom"] != nil
            || styles["padding-left"] != nil
    }

    /// Parse a single CSS length token into iOS points. Accepts String (any
    /// CSS length unit, parsed via the canonical C++ parser), NSNumber, Int,
    /// Double, CGFloat. Numeric tokens are treated as raw px and scaled by
    /// `pointScale`. Returns nil for unparseable input.
    static func parseLengthPt(_ value: Any?) -> CGFloat? {
        guard let value = value else { return nil }
        if let s = value as? String {
            let trimmed = s.trimmingCharacters(in: .whitespaces)
            if trimmed.isEmpty { return nil }
            // Reuse the shorthand parser by treating a single token as a
            // 1-value shorthand, then take any side (all four are equal).
            guard let insets = AGenUIEdgeInsetsBridge.parse(trimmed) else { return nil }
            return resolveSidePt(insets.top)
        }
        if let n = value as? NSNumber {
            return CGFloat(n.doubleValue) * pointScale
        }
        if let d = value as? Double {
            return CGFloat(d) * pointScale
        }
        if let i = value as? Int {
            return CGFloat(i) * pointScale
        }
        if let f = value as? CGFloat {
            return f * pointScale
        }
        return nil
    }

    // MARK: - Private

    /// Parse `padding` shorthand value into a 4-edge tuple. Strings are
    /// routed through the canonical C++ parser; bare numbers are treated as
    /// uniform px on all four edges.
    private static func resolveShorthand(_ raw: Any) -> CSSPadding? {
        if let str = raw as? String {
            let trimmed = str.trimmingCharacters(in: .whitespaces)
            if trimmed.isEmpty { return nil }
            guard let insets = AGenUIEdgeInsetsBridge.parse(trimmed) else { return nil }
            return CSSPadding(
                top:    resolveSidePt(insets.top),
                right:  resolveSidePt(insets.right),
                bottom: resolveSidePt(insets.bottom),
                left:   resolveSidePt(insets.left)
            )
        }
        if let n = parseLengthPt(raw) {
            return CSSPadding(top: n, right: n, bottom: n, left: n)
        }
        return nil
    }

    /// Convert a single parsed edge value into iOS points. Only `px` (and
    /// unitless, which the C++ parser normalizes to px) is honored; every
    /// other CSS unit collapses to 0 so the three platforms stay aligned.
    private static func resolveSidePt(_ side: AGUIEdgeInsetSide) -> CGFloat {
        if side.isCalc { return 0 }
        if side.unit == .px {
            return CGFloat(side.value) * pointScale
        }
        return 0
    }
}
