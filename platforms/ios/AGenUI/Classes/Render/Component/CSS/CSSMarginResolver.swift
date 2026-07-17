//
//  CSSMarginResolver.swift
//  AGenUI
//
//  Shared CSS `margin` resolver used by Surface to compute root content size.
//
//  Parsing rules (kept in lockstep with Android / Harmony):
//    All shorthand and per-edge string parsing is delegated to the canonical
//    C++ `EdgeInsetsParser` via `AGenUIEdgeInsetsBridge`. Only `px` (and
//    unitless, which the parser normalizes to px) is honored on the platform
//    layer; every other CSS unit collapses to 0. The render layer
//    intentionally stays px-only so the three platforms render identically
//    without dragging viewport / font-baseline / physical-unit math into
//    platform code.
//    `margin-top/right/bottom/left` always override the shorthand.
//    Negative values are clamped to 0.
//    Numeric tokens (NSNumber / Int / Double / CGFloat) are accepted and
//    treated as raw px (legacy contract used by some style sources).
//
//  Output: four iOS points, scaled by `pointScale = 0.5` to match the same
//  px -> pt scaling used by font-size and line-height across the codebase.

import UIKit

/// Resolved CSS margin in iOS points. Edges are clamped to >= 0.
public struct CSSMargin {
    public let top: CGFloat
    public let right: CGFloat
    public let bottom: CGFloat
    public let left: CGFloat

    public static let zero = CSSMargin(top: 0, right: 0, bottom: 0, left: 0)

    public var hasAny: Bool {
        return top > 0 || right > 0 || bottom > 0 || left > 0
    }
}

public enum CSSMarginResolver {

    /// Same px -> pt scaling factor used by font-size and line-height
    /// elsewhere in the iOS render layer (see `TextComponent.BS_POINT_SCALE`).
    public static let pointScale: CGFloat = 0.5

    /// Resolve a CSS `styles` dictionary into a 4-edge margin tuple.
    /// Returns `CSSMargin.zero` when no margin-* key is present so callers
    /// can rely on a non-nil result and decide whether to apply or skip.
    public static func resolve(_ styles: [String: Any]) -> CSSMargin {
        var top: CGFloat = 0
        var right: CGFloat = 0
        var bottom: CGFloat = 0
        var left: CGFloat = 0

        // Shorthand: `margin`
        if let raw = styles["margin"], let shorthand = resolveShorthand(raw) {
            top = shorthand.top
            right = shorthand.right
            bottom = shorthand.bottom
            left = shorthand.left
        }

        // Per-edge overrides
        if let v = parseLengthPt(styles["margin-top"]) { top = v }
        if let v = parseLengthPt(styles["margin-right"]) { right = v }
        if let v = parseLengthPt(styles["margin-bottom"]) { bottom = v }
        if let v = parseLengthPt(styles["margin-left"]) { left = v }

        return CSSMargin(
            top: max(0, top),
            right: max(0, right),
            bottom: max(0, bottom),
            left: max(0, left)
        )
    }

    /// Returns true when the styles object carries any `margin*` key.
    public static func hasAnyMarginKey(_ styles: [String: Any]) -> Bool {
        return styles["margin"] != nil
            || styles["margin-top"] != nil
            || styles["margin-right"] != nil
            || styles["margin-bottom"] != nil
            || styles["margin-left"] != nil
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

    /// Parse `margin` shorthand value into a 4-edge tuple. Strings are
    /// routed through the canonical C++ parser; bare numbers are treated as
    /// uniform px on all four edges.
    private static func resolveShorthand(_ raw: Any) -> CSSMargin? {
        if let str = raw as? String {
            let trimmed = str.trimmingCharacters(in: .whitespaces)
            if trimmed.isEmpty { return nil }
            guard let insets = AGenUIEdgeInsetsBridge.parse(trimmed) else { return nil }
            return CSSMargin(
                top:    resolveSidePt(insets.top),
                right:  resolveSidePt(insets.right),
                bottom: resolveSidePt(insets.bottom),
                left:   resolveSidePt(insets.left)
            )
        }
        if let n = parseLengthPt(raw) {
            return CSSMargin(top: n, right: n, bottom: n, left: n)
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
