//
//  CSSPropertyApplier.swift
//  AGenUI
//
// Created on 2026/2/28.
//

import UIKit

/// CSS property applier
/// Responsible for applying parsed CSS properties to components and views
class CSSPropertyApplier {
    // MARK: - Main Application Methods

    /// Applies CSS properties to a UIView (Component as View mode).
    @MainActor static func apply(properties: [String: Any], to view: UIView) {
        applyStyles(properties: properties, to: view)
    }

    /// Applies CSS properties to a component (BaseA2UIComponent mode).
    @MainActor static func apply(properties: [String: Any], to component: Component, view: UIView) {
        applyStyles(properties: properties, to: view)
    }

    /// Applies all supported style properties in a fixed order.
    ///
    /// The call order IS the priority: `display` is called after `visibility` so
    /// it overrides `isHidden`; the rest write non-conflicting targets so their
    /// order only needs to be deterministic. Overflow is not applied here — it is
    /// resolved together with border-radius by `applyClipDecision` below.
    /// Order mirrors the former registry priorities (visibility 45 > display 44 >
    /// visual styles 40 > background-image 39).
    @MainActor private static func applyStyles(properties: [String: Any], to view: UIView) {
        // Nested helper captures `properties`/`view`; each call declares its own
        // value type (and valid-values for keywords) inline — no registry needed.
        // nil (key absent) and .invalid (parse failure) both fall back to the
        // caller-supplied defaultValue, so apply methods always receive a usable
        // value and never need a guard-let for the absent/invalid case.
        func ifPresent(
            _ key: String, valueType: CSSValueType, validValues: [String]? = nil,
            defaultValue: CSSPropertyValue,
            then apply: (CSSPropertyValue) -> Void
        ) {
            guard let raw = properties[key] else {
                apply(defaultValue)
                return
            }
            let valueStr = (raw as? String) ?? "\(raw)"
            let parsed = CSSPropertyParser.parse(value: valueStr, valueType: valueType, validValues: validValues)
            apply(parsed == .invalid ? defaultValue : parsed)
        }

        ifPresent("visibility",       valueType: .keyword,   validValues: ["visible", "hidden"], defaultValue: .keyword("visible")) { applyVisibility($0, to: view) }
        ifPresent("background",       valueType: .color,                                       defaultValue: .color(.clear))     { applyBackgroundColor($0, to: view) }
        ifPresent("background-color", valueType: .color,                                       defaultValue: .color(.clear))     { applyBackgroundColor($0, to: view) }
        ifPresent("border-radius",    valueType: .dimension,                                   defaultValue: .number(0))         { applyBorderRadius($0, to: view) }
        ifPresent("opacity",          valueType: .opacity,                                     defaultValue: .number(1.0))       { applyOpacity($0, to: view) }
        ifPresent("border-color",     valueType: .color,                                       defaultValue: .color(.clear))     { applyBorderColor($0, to: view) }
        ifPresent("border-style",     valueType: .keyword,   validValues: ["solid"],            defaultValue: .keyword("solid"))  { applyBorderStyle($0, to: view) }
        ifPresent("border-width",     valueType: .dimension,                                   defaultValue: .number(0))         { applyBorderWidth($0, to: view) }
        ifPresent("filter",           valueType: .shadow,                                      defaultValue: .invalid)           { applyFilter($0, to: view) }
        ifPresent("background-image", valueType: .url,                                          defaultValue: .url(""))           { applyBackgroundImage($0, to: view) }

        // overflow + border-radius clip decision (depends on both, runs after the loop)
        applyClipDecision(properties: properties, to: view)
    }

    // MARK: - Clip Decision (border-radius + overflow)

    /// Outcome of the combined `border-radius` + `overflow` clip decision.
    private enum ClipDecision {
        /// Leave `clipsToBounds` exactly as it is.
        case unspecified
        case on
        case off
    }

    /// Resolves `border-radius` + `overflow` into ONE clip decision, kept condition for condition
    /// identical to Android `StyleHelper.resolveClipDecision` and the HarmonyOS `NODE_CLIP` block:
    ///
    ///     radius > 0 || overflow == hidden || overflow == scroll  -> on
    ///     hasRadiusKey || overflow == visible                     -> off
    ///     otherwise                                               -> unspecified
    ///
    /// Three things this encodes, each of which iOS used to get wrong by handling the two keys in
    /// separate per-property appliers:
    ///
    /// - A positive radius is a PEER of the clipping keywords, not a fallback consulted only when
    ///   `overflow` is absent. It clips even against an explicit `overflow: visible`. iOS
    ///   previously set only `layer.cornerRadius` for a radius and never touched `clipsToBounds`,
    ///   so a rounded container did not clip its content at all.
    /// - A `border-radius` key that is present but resolves to zero is the unclip reset path.
    /// - `auto`, and any unknown keyword, is `unspecified` — a no-op that preserves the current
    ///   state rather than resetting to `visible`.
    ///
    /// The radius is parsed from the DECLARED style, never read back off `view.layer.cornerRadius`:
    /// the layer also carries radii components gave themselves in `init` and the payload never
    /// asked for — `VideoComponent` and `ModalComponent` hardcode 8, `ChoicePickerComponent` sets
    /// its own. Treating those as declared would force `clipsToBounds` on for components with no
    /// radius in their payload, breaking the "only an explicitly declared key can flip those"
    /// guarantee on `applyClipDecision`. Android (`StyleHelper.resolveClipDecision`) and HarmonyOS
    /// (`A2UIComponent::applyClipDecision`) parse their style maps too, so all three share one input.
    @MainActor private static func resolveClipDecision(properties: [String: Any]) -> ClipDecision {
        let overflow = (properties["overflow"] as? String)?
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased() ?? ""
        let hasRadiusKey = properties["border-radius"] != nil

        // parseDimension is what the per-property loop already feeds applyBorderRadius:
        // border-radius is registered as .dimension, and CSSPropertyParser.parse routes
        // .dimension straight here. Same call, so this cannot drift from the applied radius.
        var radius: CGFloat = 0
        if let raw = properties["border-radius"] {
            let valueStr = (raw as? String) ?? "\(raw)"
            let trimmed = valueStr.trimmingCharacters(in: .whitespacesAndNewlines)
            if case .number(let parsed) = CSSPropertyParser.parseDimension(trimmed) {
                radius = parsed
            }
        }

        if radius > 0 || overflow == "hidden" || overflow == "scroll" {
            return .on
        }
        if hasRadiusKey || overflow == "visible" {
            return .off
        }
        return .unspecified
    }

    /// Sole writer of `clipsToBounds` from CSS, so the radius and the keyword can never disagree.
    /// Runs after the per-property loop, which is why the loop order is irrelevant to the outcome.
    ///
    /// SCOPE DIFFERENCE, worth knowing before trusting this to be "the same as Android": the three
    /// platforms do not apply this rule to the same set of components. Android reaches
    /// `StyleHelper.applyOverflow` only for a `ViewGroup`, via `A2UILayoutComponent`, so containers
    /// only; HarmonyOS covers Column / Row / RichText; this entry point runs for EVERY component.
    /// Components that manage `clipsToBounds` themselves are therefore now also subject to it —
    /// `ChoicePickerComponent` and `VideoComponent` enable it in `init`, and `TextComponent`
    /// deliberately disables it so glyphs can overflow. Only an explicitly declared key can flip
    /// those: a declared `border-radius: 0` (or `overflow: visible`) resolves to `.off`, and a
    /// positive radius to `.on`. Narrow this to container components if that turns out to be
    /// unwanted for replaced elements.
    @MainActor private static func applyClipDecision(properties: [String: Any], to view: UIView) {
        switch resolveClipDecision(properties: properties) {
        case .on:
            view.clipsToBounds = true
        case .off:
            view.clipsToBounds = false
        case .unspecified:
            break
        }
    }

    // MARK: - Style Properties
    
    /// Applies background color (solid or gradient).
    @MainActor private static func applyBackgroundColor(_ value: CSSPropertyValue, to view: UIView) {
        switch value {
        case .color(let color):
            view.backgroundColor = color
            (view as? Component)?.setGradient(nil)
        case .gradient(let info):
            view.backgroundColor = .clear
            (view as? Component)?.setGradient(info)
        default:
            view.backgroundColor = .clear
            (view as? Component)?.setGradient(nil)
        }
    }
    
    /// Applies background image
    /// Supports URL formats:
    /// - Network URL: url("https://example.com/image.png")
    /// - Local resource: url("res://icon") or url(paper.gif)
    /// - Local file: url("file:///path/to/image.png")
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    @MainActor private static func applyBackgroundImage(_ value: CSSPropertyValue, to view: UIView) {
        guard case .url(let urlString) = value else {
            view.layer.contents = nil
            return
        }
        
        if urlString.isEmpty {
            // Clear background image
            view.layer.contents = nil
            return
        }
        
        // Determine URL type and load
        if urlString.hasPrefix("http://") || urlString.hasPrefix("https://") {
            // Network image - async load
            loadBackgroundImage(from: urlString, for: view)
        } else if urlString.hasPrefix("res://") {
            // Local resource
            let resName = String(urlString.dropFirst(6))
            if let image = UIImage(named: resName) {
                setBackgroundImage(image, for: view)
            }
        } else if urlString.hasPrefix("file://") {
            // Local file
            let filePath = String(urlString.dropFirst(7))
            if let image = UIImage(contentsOfFile: filePath) {
                setBackgroundImage(image, for: view)
            }
        } else {
            // Load as resource name
            if let image = UIImage(named: urlString) {
                setBackgroundImage(image, for: view)
            }
        }
    }
    
    /// Sets background image to view layer
    /// - Parameters:
    ///   - image: Image
    ///   - view: Target view
    @MainActor private static func setBackgroundImage(_ image: UIImage, for view: UIView) {
        view.layer.contents = image.cgImage
        view.layer.contentsGravity = .resizeAspectFill
    }
    
    /// Asynchronously loads network background image
    /// - Parameters:
    ///   - urlString: Image URL
    ///   - view: Target view
    @MainActor private static func loadBackgroundImage(from urlString: String, for view: UIView) {
        guard let url = URL(string: urlString) else { return }
        
        URLSession.shared.dataTask(with: url) { data, _, error in
            guard let data = data,
                  let image = UIImage(data: data),
                  error == nil else {
                return
            }
            DispatchQueue.main.async {
                setBackgroundImage(image, for: view)
            }
        }.resume()
    }
    
    /// Applies border radius.
    ///
    /// Components route through `setBorderRadius` — the single radius seam,
    /// which also reshapes the sibling shadow so a later-arriving radius never
    /// leaves a stale shadowPath, and stays overridable for subclasses that
    /// mirror the radius onto inner subviews. Subtree clipping is handled
    /// separately by `applyClipDecision`. Plain views get the direct write.
    ///
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    @MainActor private static func applyBorderRadius(_ value: CSSPropertyValue, to view: UIView) {
        guard case .number(let radius) = value else { return }
        if let component = view as? Component {
            component.setBorderRadius(radius)
        } else {
            view.layer.cornerRadius = radius
        }
    }
    
    /// Applies opacity
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    @MainActor private static func applyOpacity(_ value: CSSPropertyValue, to view: UIView) {
        guard case .number(let opacity) = value else { return }
        // Ensure opacity is between 0.0 and 1.0
        view.alpha = max(0.0, min(1.0, opacity))
    }
    

    
    // MARK: - P1 Border Properties
    
    /// Applies border color
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    @MainActor private static func applyBorderColor(_ value: CSSPropertyValue, to view: UIView) {
        guard case .color(let color) = value else { return }
        view.layer.borderColor = color.cgColor
    }
    
    /// Applies border width
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    @MainActor private static func applyBorderWidth(_ value: CSSPropertyValue, to view: UIView) {
        guard case .number(let width) = value else { return }
        view.layer.borderWidth = width
    }
    

    
    // MARK: - Requirement 9: Display Control and Visual Effects Properties
    
    /// Applies border style (iOS only supports solid)
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    @MainActor private static func applyBorderStyle(_ value: CSSPropertyValue, to view: UIView) {
        guard case .keyword = value else {
            // invalid (incl. unsupported styles like dashed) → clear border
            view.layer.borderWidth = 0
            return
        }
        // .keyword("solid") — iOS default is solid, nothing to apply
    }
    
    /// Applies visibility control
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    @MainActor private static func applyVisibility(_ value: CSSPropertyValue, to view: UIView) {
        guard case .keyword(let visibility) = value else { return }
        
        switch visibility {
        case "hidden":
            view.isHidden = true
            // visibility:hidden still takes space, does not affect FlexLayout
        case "visible":
            view.isHidden = false
        default:
            #if DEBUG
            Logger.shared.debug("Unknown visibility value: \(visibility)")
            #endif
        }
    }
    
    /// Applies filter property (only supports drop-shadow)
    /// - Parameters:
    ///   - value: CSS property value
    ///   - view: Target view
    @MainActor private static func applyFilter(_ value: CSSPropertyValue, to view: UIView) {
        guard case .shadow(let shadow) = value else {
            // Default (filter absent or invalid): clear shadow
            (view as? Component)?.setShadow(nil)
            return
        }
        // Mirrors the gradient routing: the shadow lives on the Component as a
        // sibling view; non-component views carry no shadow.
        (view as? Component)?.setShadow(shadow)
    }
        


}


