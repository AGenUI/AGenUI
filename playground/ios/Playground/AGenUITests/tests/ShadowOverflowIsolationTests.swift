//
//  ShadowOverflowIsolationTests.swift
//  AGenUITests
//
//  AJX-style shadow isolation tests (see docs/prds/agenui-ios-shadow-ajx-scheme.md):
//  the shadow lives in a SIBLING view inside the parent, directly below the body —
//  never in the body's own layer. Grown slice by slice (TDD), one seam per slice.
//

import Testing
import UIKit
@testable import AGenUI

// MARK: - Slice 1 / S1: sibling mounting

/// A component with a drop-shadow filter, mounted in a parent, must get a shadow
/// SIBLING view in the parent — directly below the body in z-order, interaction
/// disabled — while the body's own layer carries no shadow.
@Test @MainActor
func shadow_dropShadow_mountedAsSiblingInParent_directlyBelowSelf() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)

    child.updateProperties(DiffValue.from([
        "styles": [
            "width": "200px",
            "height": "100px",
            "filter": "drop-shadow(0px 8px 16px rgba(0,0,0,0.5))",
        ]
    ]))

    #expect(child.shadowView != nil, "shadow style must create a shadow view")
    guard let shadow = child.shadowView else { return }

    #expect(shadow.superview === parent, "shadow view must live in the parent, not inside the body")
    #expect(shadow.isUserInteractionEnabled == false, "shadow view must never intercept touches")

    let bodyIndex = parent.subviews.firstIndex(of: child) ?? -1
    let shadowIndex = parent.subviews.firstIndex(of: shadow) ?? -1
    #expect(bodyIndex > 0, "body must be mounted in the parent")
    #expect(shadowIndex == bodyIndex - 1, "shadow must sit directly below its body in z-order")

    #expect(child.layer.shadowOpacity == 0, "body layer must not carry the shadow itself")
    #expect(shadow.layer.shadowOpacity == 1.0)
    #expect(shadow.layer.shadowRadius == 8.0, "blur 16px -> radius 8 (CSS blur is a diameter)")
    #expect(shadow.layer.shadowOffset == CGSize(width: 0, height: 4), "offset 8px -> 4 (px are halved)")
}

// MARK: - Shared scene helpers

/// White host big enough to show shadow halos; the component is mounted straight
/// onto it (shadow mounting only needs a superview — AJX parity).
@MainActor
private func renderComponent(styles: [String: Any]) -> (component: Component, host: UIView) {
    let component = Component(componentId: "t-shadow", componentType: "column",
                              properties: ["styles": styles])
    let host = UIView(frame: CGRect(x: 0, y: 0, width: 300, height: 200))
    host.backgroundColor = .white
    host.addSubview(component)
    component.createView()
    return (component, host)
}

// MARK: - Slice 2 / S2: self-clip isolation (structural)

/// Rounded + clipped body with a shadow: the clip lands on the body itself, the
/// shadow lives OUTSIDE it — so the body can never clip its own shadow.
@Test @MainActor
func shadow_selfClip_borderRadiusOverflowHidden_shadowLivesOutsideBody() {
    let (component, host) = renderComponent(styles: [
        "x": 10, "y": 10, "width": 200, "height": 100,
        "background-color": "#FF3B30",
        "border-radius": "24px",
        "overflow": "hidden",
        "filter": "drop-shadow(0px 8px 16px rgba(0,0,0,0.5))",
    ])
    defer { withExtendedLifetime(host) {} }

    // The clip decision still clips — on the body itself.
    #expect(component.clipsToBounds == true)
    #expect(component.layer.cornerRadius == 12.0, "24px -> 12pt")

    // The body's own layer carries no shadow...
    #expect(component.layer.shadowOpacity == 0)

    // ...the shadow is a sibling in the host, below the body, unclipped.
    guard let shadow = component.shadowView else {
        #expect(Bool(false), "shadow view missing")
        return
    }
    #expect(shadow.superview === host)
    #expect(shadow.clipsToBounds == false)
    #expect(shadow.isUserInteractionEnabled == false)
    #expect(shadow.layer.shadowOpacity == 1.0)
    let bodyIndex = host.subviews.firstIndex(of: component) ?? -1
    let shadowIndex = host.subviews.firstIndex(of: shadow) ?? -1
    #expect(shadowIndex == bodyIndex - 1)
    #expect(shadow.frame == component.frame, "shadow frame mirrors the body frame")
}

/// setShadow(nil) clears the sibling shadow view — the API mirrors setGradient,
/// which also takes an optional and clears on nil.
@Test @MainActor
func shadow_setShadow_nilClearsSiblingView() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 200, "height": 100,
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    #expect(parent.subviews.contains(shadow))

    child.setShadow(nil)
    #expect(child.shadowView == nil)
    #expect(parent.subviews.contains(shadow) == false, "cleared shadow must leave the hierarchy")
}

/// The shadow shapes itself from the body's cornerRadius. When the radius
/// arrives AFTER the shadow (separate engine batches, or same batch where the
/// equal-priority keys land in that order), the shadowPath must be recomputed —
/// a stale rectangular path would leak square halo corners.
/// Probe: (1,1) on a 100x50 shape is inside the rectangular path but outside a
/// 12pt-rounded one.
@Test @MainActor
func shadow_radiusArrivesAfterShadow_shadowPathFollowsRadius() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    // Geometry first (engine layout channel: x/y/width/height inside styles —
    // applyLayoutFromStyles overwrites the frame from the styles dict on EVERY
    // updateProperties, so layout and shadow must be separate batches), then a
    // shadow-only batch. Equal-priority keys inside one batch apply in
    // unspecified order, so separate batches are the deterministic way to say
    // "shadow before radius".
    child.updateProperties(DiffValue.from(["styles": ["x": 0, "y": 0, "width": 200, "height": 100]]))
    child.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 200, "height": 100,   // engine batches always carry layout
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    // No radius yet: path is a plain rect, corner point inside.
    #expect(shadow.layer.shadowPath?.contains(CGPoint(x: 1, y: 1)) == true)

    // Radius arrives in a LATER batch.
    child.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 200, "height": 100,
        "border-radius": "24px",
    ]]))
    #expect(child.layer.cornerRadius == 12.0)
    #expect(shadow.layer.shadowPath?.contains(CGPoint(x: 1, y: 1)) == false,
            "shadowPath must follow the later-arriving radius")
}

/// Same-batch radius + shadow: both keys share priority 40, so either may be
/// applied first — the final shadowPath must be rounded regardless.
@Test @MainActor
func shadow_sameBatch_radiusAndShadow_pathIsRoundedEitherOrder() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 200, "height": 100,
        "border-radius": "24px",
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    #expect(child.layer.cornerRadius == 12.0)
    #expect(shadow.layer.shadowPath?.contains(CGPoint(x: 1, y: 1)) == false,
            "same-batch radius must shape the shadow regardless of apply order")
}

// MARK: - Pixel sampling infrastructure
//
// Default capture goes through an OFFSCREEN UIGraphicsImageRenderer +
// drawHierarchy: UIKit's own compositor renders CALayer shadows, unlike
// layer.render(in:) which drops them, and unlike the simulator render-server
// path which is unreliable headless (stale frames / dropped clips).
// `inProcess` (layer.render) is the CLIP-evidence channel: it always honours
// clipsToBounds and never draws shadows, so clip assertions use it in scenes
// deliberately built without a halo — with a shadow present the halo darkens
// the same pixels and clip and halo cannot be told apart by color alone.
// Channel order is self-calibrated per capture with a random-color probe pixel
// (the renderer hands back both BGRA and RGBA; bitmap flags do not track it).
// The format is forced to 8-bit sRGB: the simulator intermittently yields a
// 16-bit/channel wide-gamut format, and byte sampling silently reads garbage
// halves of those pixels.

private struct PixelCanvas {
    let pixels: Data
    let width: Int
    let height: Int
    let bpp: Int
    let bpr: Int
    let scale: CGFloat
    let offsets: [Int]   // [redOffset, greenOffset, blueOffset] in memory bytes

    /// channel: 0 = red, 1 = green, 2 = blue.
    func channel(_ channel: Int, at point: CGPoint) -> CGFloat {
        let x = max(0, min(width - 1, Int(point.x * scale)))
        let y = max(0, min(height - 1, Int(point.y * scale)))
        return CGFloat(pixels[y * bpr + x * bpp + offsets[channel]]) / 255.0
    }
}

@MainActor
private func capturePixels(of parent: UIView, inProcess: Bool = false,
                            requiresRedContent: Bool = true,
                            validate: ((PixelCanvas) -> Bool)? = nil) -> PixelCanvas? {
    var lastCanvas: PixelCanvas?
    var failReason = "no-attempt"
    for _ in 0..<3 {
        // Freshness sentinel + channel calibration: one probe pixel with a random
        // permutation of three intensities; the captured probe must match
        // slot-for-slot (±2). Re-created every attempt so a cached frame from an
        // unchanged hierarchy both invalidates and becomes detectable.
        let palette = [UInt8(210), UInt8(130), UInt8(60)].shuffled()
        let (rVal, gVal, bVal) = (palette[0], palette[1], palette[2])
        let probe = UIView(frame: CGRect(x: parent.bounds.width - 4, y: parent.bounds.height - 4, width: 2, height: 2))
        probe.backgroundColor = UIColor(red: CGFloat(rVal) / 255.0,
                                        green: CGFloat(gVal) / 255.0,
                                        blue: CGFloat(bVal) / 255.0, alpha: 1)
        parent.addSubview(probe)

        CATransaction.flush()
        RunLoop.main.run(until: Date().addingTimeInterval(0.05))

        let fmt = UIGraphicsImageRendererFormat.default()
        fmt.preferredRange = .standard
        let renderer = UIGraphicsImageRenderer(bounds: parent.bounds, format: fmt)
        let image = renderer.image { ctx in
            if inProcess {
                parent.layer.render(in: ctx.cgContext)
            } else {
                parent.drawHierarchy(in: parent.bounds, afterScreenUpdates: true)
            }
        }
        guard let cgImage = image.cgImage, let data = cgImage.dataProvider?.data else {
            failReason = "no-cgimage"; probe.removeFromSuperview(); continue
        }
        guard cgImage.bitsPerPixel == 32 else {
            failReason = "unexpected-bpp-\(cgImage.bitsPerPixel)"; probe.removeFromSuperview(); continue
        }
        let pixels = data as Data
        let width = cgImage.width
        let height = cgImage.height
        let bpp = cgImage.bitsPerPixel / 8
        let bpr = cgImage.bytesPerRow

        let px = max(0, min(width - 1, Int(probe.frame.midX * image.scale)))
        let py = max(0, min(height - 1, Int(probe.frame.midY * image.scale)))
        let base = py * bpr + px * bpp
        let bytes = (0..<min(bpp, 3)).map { Int(pixels[base + $0]) }
        func slot(of value: UInt8, excluding taken: Set<Int>) -> Int? {
            bytes.indices.first { !taken.contains($0) && abs(bytes[$0] - Int(value)) <= 2 }
        }
        guard let rOff = slot(of: rVal, excluding: []),
              let gOff = slot(of: gVal, excluding: [rOff]),
              let bOff = slot(of: bVal, excluding: [rOff, gOff]) else {
            failReason = "probe-mismatch bytes=\(bytes) expected=(\(rVal),\(gVal),\(bVal))"
            probe.removeFromSuperview(); continue
        }

        // Content sentinel (optional): frames that committed only the background
        // pass the probe check. Scenes with a pure-red element require several
        // clearly red pixels on a coarse grid (skipping the probe corner).
        if requiresRedContent {
            var redCount = 0
            let step = max(1, width / 120)
            var yy = 0
            while yy < height && redCount < 3 {
                var xx = 0
                while xx < width && redCount < 3 {
                    if xx < width - 12 && yy < height - 12 {
                        let p = yy * bpr + xx * bpp
                        if Int(pixels[p + rOff]) > 150 && Int(pixels[p + gOff]) < 100 && Int(pixels[p + bOff]) < 100 {
                            redCount += 1
                        }
                    }
                    xx += step
                }
                yy += step
            }
            guard redCount >= 3 else { failReason = "no-red-content count=\(redCount)"; probe.removeFromSuperview(); continue }
        }

        let candidate = PixelCanvas(pixels: pixels, width: width, height: height, bpp: bpp,
                                    bpr: bpr, scale: image.scale, offsets: [rOff, gOff, bOff])
        if let validate, !validate(candidate) { failReason = "scene-validate-failed"; probe.removeFromSuperview(); continue }

        lastCanvas = candidate
        probe.removeFromSuperview()
        break
    }
    if lastCanvas == nil {
        #expect(Bool(false), "capture never validated: \(failReason)")
    }
    return lastCanvas
}

// MARK: - Slice 2 / S2: self-clip isolation (pixel-level)

/// Clipped red card + strong drop shadow: the halo must be visible OUTSIDE the
/// card — the body's own clip cannot eat a shadow living in a sibling view.
/// (Corner-clip evidence is asserted separately without a halo, below.)
@Test @MainActor
func shadow_renderedPixels_selfClipShadowVisibleOutsideClippedCard() {
    let (component, host) = renderComponent(styles: [
        "x": 0, "y": 0, "width": 200, "height": 100,     // 100x50 pt card
        "background-color": "#FF3B30",
        "border-radius": "24px",                          // 12 pt
        "overflow": "hidden",
        "filter": "drop-shadow(0px 12px 30px rgba(0,0,0,0.8))",  // offset 6, radius 15
    ])
    defer { withExtendedLifetime(host) {} }

    // Validate on a NON-asserted card-interior point.
    let canvas = capturePixels(of: host,
                               validate: { $0.channel(0, at: CGPoint(x: 20, y: 20)) > 0.8 })

    // Halo below the card (y=62): offset 6 + radius 15 reaches well past y=50;
    // the white host must be darkened even though the body itself clips.
    let haloGreens = (10...90).filter { $0 % 10 == 0 }.map {
        canvas?.channel(1, at: CGPoint(x: CGFloat($0), y: 62)) ?? 1
    }
    #expect(haloGreens.contains { $0 < 0.95 }, "shadow halo must darken the host below the clipped card")

    // Card body interior stays red.
    let centerRed = canvas?.channel(0, at: CGPoint(x: 50, y: 25)) ?? 0
    #expect(centerRed > 0.9)
}

/// Corner-radius clip evidence WITHOUT a shadow (halo would pollute the corner
/// pixels): a red child filling the card must be clipped to host white at the
/// corner arc. Combined clip decision (restored from 6ed50ce6, aligned with
/// Android/HarmonyOS): a declared positive radius clips on its own — no
/// overflow keyword needed. This test pins that semantic.
@Test @MainActor
func shadow_renderedPixels_borderRadiusStillClipsChildren() {
    let (component, host) = renderComponent(styles: [
        "x": 0, "y": 0, "width": 200, "height": 100,
        "background-color": "#E5E5EA",
        "border-radius": "24px",   // positive radius alone -> clip decision .on
    ])
    let child = Component(componentId: "t-radius-child", componentType: "column",
                          properties: ["styles": [
                              "x": 0, "y": 0, "width": 200, "height": 100,
                              "background-color": "#FF3B30",
                          ]])
    component.properties["children"] = ["t-radius-child"]
    component.addChild(child)
    defer { withExtendedLifetime(host) {} }

    // Card (0,0,100,50) pt, radius 12pt. (2,2) is ~14pt from the arc centre ->
    // outside the arc: clipped to host white (green 1.0, not child red's 0.23).
    // Clip evidence via the in-process channel (always honours clipsToBounds).
    let canvas = capturePixels(of: host, inProcess: true)
    let cornerGreen = canvas?.channel(1, at: CGPoint(x: 2, y: 2)) ?? 0
    let centerGreen = canvas?.channel(1, at: CGPoint(x: 50, y: 25)) ?? 0
    #expect(cornerGreen > 0.9, "corner arc must clip the child red away")
    #expect(centerGreen < 0.5, "card interior must keep the child red")
}

/// overflow:hidden clip evidence + shadow on the SAME parent: the overflowing
/// red child is clipped (in-process channel), and the halo below still renders
/// (drawHierarchy channel). Parent clip reaching child shadows is EXPECTED
/// under the AJX scheme and deliberately NOT defended here.
@Test @MainActor
func shadow_renderedPixels_overflowHiddenStillClipsChildren() {
    let (component, host) = renderComponent(styles: [
        "x": 0, "y": 0, "width": 200, "height": 100,
        "background-color": "#E5E5EA",
        "overflow": "hidden",
        "filter": "drop-shadow(0px 12px 30px rgba(0,0,0,0.8))",
    ])
    let child = Component(componentId: "t-clip-child", componentType: "column",
                          properties: ["styles": [
                              "x": 180, "y": 20, "width": 80, "height": 40,   // overflows right
                              "background-color": "#FF3B30",
                          ]])
    component.properties["children"] = ["t-clip-child"]
    component.addChild(child)
    defer { withExtendedLifetime(host) {} }

    // Clip evidence: child's visible strip stays red; far past the right edge
    // (and beyond halo reach) must be host white.
    let clipCanvas = capturePixels(of: host, inProcess: true)
    let insideChildGreen = clipCanvas?.channel(1, at: CGPoint(x: 95, y: 25)) ?? 1
    let overflowGreen = clipCanvas?.channel(1, at: CGPoint(x: 150, y: 25)) ?? 0
    #expect(insideChildGreen < 0.5, "child's visible strip must stay red")
    #expect(overflowGreen > 0.9, "overflowing child must be clipped away")

    // Halo evidence: validate on the child's visible strip so content-missing
    // frames get retried.
    let haloCanvas = capturePixels(of: host,
                                   validate: { $0.channel(1, at: CGPoint(x: 92, y: 25)) < 0.5 })
    let haloGreen = haloCanvas?.channel(1, at: CGPoint(x: 40, y: 62)) ?? 1
    #expect(haloGreen < 0.95, "shadow halo must darken the host below the clipped parent")
}

// MARK: - Slice 3 / S3: anchor insertion keeps z-order with shadow siblings

/// Children with shadows (each occupying an extra subview slot in the parent)
/// arrive out of order: the physical z-order must still match the logical
/// children order, each shadow pinned directly below its own body.
@Test @MainActor
func shadow_anchorInsertion_outOfOrderChildren_zOrderMatchesLogicalOrder() {
    // Logical order declared A, B, C — but they arrive C, A, B.
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["a", "b", "c"]])

    func makeChild(_ id: String, shadowed: Bool) -> Component {
        let child = Component(componentId: id, componentType: "card")
        parent.addChild(child)
        var styles: [String: Any] = ["width": "100px", "height": "100px"]
        if shadowed { styles["filter"] = "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))" }
        child.updateProperties(DiffValue.from(["styles": styles]))
        return child
    }

    let c = makeChild("c", shadowed: true)
    let a = makeChild("a", shadowed: true)
    let b = makeChild("b", shadowed: false)

    // Logical order sanity.
    #expect(parent.children.map(\.componentId) == ["a", "b", "c"])

    // Physical z-order: [A.shadow, A, B, C.shadow, C] — earlier children sit
    // further back, every shadow directly under its own body.
    let ids = parent.subviews.map { view -> String in
        if view === a.shadowView { return "a-shadow" }
        if view === c.shadowView { return "c-shadow" }
        if view === a { return "a" }
        if view === b { return "b" }
        if view === c { return "c" }
        return "unknown"
    }
    #expect(ids == ["a-shadow", "a", "b", "c-shadow", "c"],
            "physical z-order must mirror the logical order, got \(ids)")
}

/// Mid-list insertion: a later-arriving middle child must land between its
/// logical neighbours even though shadow slots shift every numeric index.
@Test @MainActor
func shadow_anchorInsertion_middleChildArrivesLast_landsBetweenNeighbours() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["a", "b", "c"]])

    func makeChild(_ id: String) -> Component {
        let child = Component(componentId: id, componentType: "card")
        parent.addChild(child)
        child.updateProperties(DiffValue.from(["styles": [
            "width": "100px", "height": "100px",
            "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
        ]]))
        return child
    }

    let a = makeChild("a")
    let c = makeChild("c")
    let b = makeChild("b")   // arrives last, belongs in the middle

    #expect(parent.children.map(\.componentId) == ["a", "b", "c"])
    let bodyIds = parent.subviews.compactMap { view -> String? in
        if view === a { return "a" }
        if view === b { return "b" }
        if view === c { return "c" }
        return nil
    }
    #expect(bodyIds == ["a", "b", "c"], "bodies must keep logical order, got \(bodyIds)")
    for child in [a, b, c] {
        guard let shadow = child.shadowView else { continue }
        let bodyIndex = parent.subviews.firstIndex(of: child) ?? -1
        let shadowIndex = parent.subviews.firstIndex(of: shadow) ?? -1
        #expect(shadowIndex == bodyIndex - 1, "\(child.componentId) shadow must sit directly below its body")
    }
}

// MARK: - Slice 4 / S4: manual sync (frame / visibility / removal / reparent)

/// AJX parity: the shadow is an external sibling, so every body state change
/// must drag it along. Frame change -> shadow frame + path rebuilt.
@Test @MainActor
func shadow_sync_frameChange_shadowFollowsBodyGeometry() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 100, "height": 100,
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))",
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    #expect(shadow.frame == child.frame)

    // Engine-driven frame mutation (post-layout).
    child.frame = CGRect(x: 10, y: 20, width: 40, height: 20)
    #expect(shadow.frame == child.frame, "shadow frame must follow the body frame")
    #expect(shadow.layer.shadowPath != nil)
    let pathBounds = shadow.layer.shadowPath?.boundingBox ?? .null
    #expect(pathBounds == child.bounds,
            "shadowPath must track the new bounds")
}

/// hidden / alpha land on the body; the sibling shadow must follow both.
@Test @MainActor
func shadow_sync_hiddenAndAlpha_followBody() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "width": "100px", "height": "100px",
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }

    child.isHidden = true
    #expect(shadow.isHidden == true, "hidden must cover the shadow too")
    child.isHidden = false
    #expect(shadow.isHidden == false)

    child.alpha = 0.3
    #expect(abs(shadow.alpha - 0.3) < 0.001, "alpha must cover the shadow too")
}

/// Removing the body must remove its shadow from the hierarchy too.
@Test @MainActor
func shadow_sync_removeFromSuperview_shadowLeavesHierarchy() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "width": "100px", "height": "100px",
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    #expect(shadow.superview === parent)

    parent.removeChild(child)
    #expect(child.superview == nil)
    #expect(shadow.superview == nil, "removing the body must remove its shadow")
}

/// Re-parenting: the shadow must follow its body into the new parent and sit
/// directly below it again.
@Test @MainActor
func shadow_sync_reparent_shadowFollowsIntoNewParent() {
    let parentA = Component(componentId: "pa", componentType: "column",
                            properties: ["children": ["c"]])
    let parentB = Component(componentId: "pb", componentType: "column",
                            properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parentA.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "width": "100px", "height": "100px",
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    #expect(shadow.superview === parentA)

    parentA.removeChild(child)
    parentB.addChild(child)
    #expect(child.superview === parentB)
    #expect(shadow.superview === parentB, "shadow must follow the body into the new parent")
    let bodyIndex = parentB.subviews.firstIndex(of: child) ?? -1
    let shadowIndex = parentB.subviews.firstIndex(of: shadow) ?? -1
    #expect(shadowIndex == bodyIndex - 1, "shadow must sit directly below the body again")
}

// MARK: - Slice 5 / S5: hollowMask (shadow only outside the shape)

/// AJX hollowMask parity: the shadow must appear ONLY in the halo region —
/// nothing under the shape itself. Transparent body + centred shadow: without
/// the mask the body area would darken through; with it, the interior stays
/// host-white while the halo outside the edges still darkens. The mask is a
/// CAShapeLayer with two subpaths (outer rect + reversed shadow shape).
@Test @MainActor
func shadow_hollowMask_interiorClear_haloOutsideOnly() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    parent.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 600, "height": 400,   // 300x200 pt
        "background-color": "#FFFFFF",                  // white host behind the scene
    ]]))
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "filter": "drop-shadow(0px 0px 30px rgba(0,0,0,0.5))", // centred, blur 30 -> radius 15
    ]]))
    // Engine-style frame delivery (x/y inside styles are engine layout OUTPUTS
    // and unreliable when hand-fed; the frame setter is the real channel).
    child.frame = CGRect(x: 40, y: 40, width: 100, height: 50)
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }

    // Structural: mask is a CAShapeLayer whose path holds the outer rect AND
    // the reversed shape (2 subpaths -> hollow).
    let mask = shadow.layer.mask as? CAShapeLayer
    #expect(mask != nil, "shadow view must carry a shape-layer mask")
    #expect(mask?.path?.contains(CGPoint(x: 50, y: 25)) == false,
            "the hollow interior must be cut out of the mask")

    // Pixel: body is transparent, so an unmasked shadow would darken the
    // interior through the body. Interior must stay host-white...
    let canvas = capturePixels(of: parent, requiresRedContent: false,
                               validate: { $0.channel(1, at: CGPoint(x: 5, y: 5)) > 0.95 })
    let interiorGreen = canvas?.channel(1, at: CGPoint(x: 90, y: 65)) ?? 0
    #expect(interiorGreen > 0.9, "no shadow may bleed under the shape itself")

    // ...while the halo right outside the shape still darkens the host.
    // Sample close to the edge: the Gaussian blur tail fades fast (radius 15),
    // and the sample row also sweeps outward to catch the peak wherever it sits.
    let haloGreens = (92...104).filter { $0 % 2 == 0 }.map {
        canvas?.channel(1, at: CGPoint(x: 90, y: CGFloat($0))) ?? 1
    }
    #expect(haloGreens.contains { $0 < 0.95 }, "the halo outside the shape must still render, got \(haloGreens)")
}

// MARK: - Slice 3 extended: addChild under out-of-order arrivals

/// Helper: parent declaring the given logical order; children arrive one by one
/// via the supplied closure.
@MainActor
private func makeParent(logicalIds: [String]) -> Component {
    Component(componentId: "p", componentType: "column",
              properties: ["children": logicalIds])
}

@MainActor
private func mountChild(_ parent: Component, id: String, shadowed: Bool) -> Component {
    let child = Component(componentId: id, componentType: "card")
    parent.addChild(child)
    var styles: [String: Any] = ["width": "100px", "height": "100px"]
    if shadowed { styles["filter"] = "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))" }
    child.updateProperties(DiffValue.from(["styles": styles]))
    return child
}

/// Fully reversed arrival, every child shadowed — worst case for subview-slot
/// contamination. Bodies and shadows must both end in logical order.
@Test @MainActor
func shadow_addChild_reversedArrival_allShadowed_zOrderStillLogical() {
    let parent = makeParent(logicalIds: ["a", "b", "c"])
    let c = mountChild(parent, id: "c", shadowed: true)
    let b = mountChild(parent, id: "b", shadowed: true)
    let a = mountChild(parent, id: "a", shadowed: true)

    #expect(parent.children.map(\.componentId) == ["a", "b", "c"])
    let ids = parent.subviews.map { view -> String in
        if view === a.shadowView { return "a-shadow" }
        if view === b.shadowView { return "b-shadow" }
        if view === c.shadowView { return "c-shadow" }
        if view === a { return "a" }
        if view === b { return "b" }
        if view === c { return "c" }
        return "unknown"
    }
    #expect(ids == ["a-shadow", "a", "b-shadow", "b", "c-shadow", "c"],
            "reversed arrival must still produce logical z-order, got \(ids)")
}

/// Five children, shuffled arrival, mixed shadows: the FULL physical sequence
/// must mirror the logical order with each shadow pinned below its own body.
@Test @MainActor
func shadow_addChild_shuffledArrival_fiveChildren_fullZSequenceMatchesLogical() {
    let parent = makeParent(logicalIds: ["a", "b", "c", "d", "e"])
    // Arrival: c, a, e, b, d — shadows on a, c, e.
    let c = mountChild(parent, id: "c", shadowed: true)
    let a = mountChild(parent, id: "a", shadowed: true)
    let e = mountChild(parent, id: "e", shadowed: true)
    let b = mountChild(parent, id: "b", shadowed: false)
    let d = mountChild(parent, id: "d", shadowed: false)

    #expect(parent.children.map(\.componentId) == ["a", "b", "c", "d", "e"])
    let ids = parent.subviews.map { view -> String in
        if view === a.shadowView { return "a-shadow" }
        if view === c.shadowView { return "c-shadow" }
        if view === e.shadowView { return "e-shadow" }
        if view === a { return "a" }
        if view === b { return "b" }
        if view === c { return "c" }
        if view === d { return "d" }
        if view === e { return "e" }
        return "unknown"
    }
    #expect(ids == ["a-shadow", "a", "b", "c-shadow", "c", "d", "e-shadow", "e"],
            "shuffled arrival must still produce logical z-order, got \(ids)")
}

/// Duplicate addChild is a no-op (id-based), shadows included.
@Test @MainActor
func shadow_addChild_duplicate_isNoOp() {
    let parent = makeParent(logicalIds: ["c"])
    let c = mountChild(parent, id: "c", shadowed: true)
    let before = parent.subviews.count
    parent.addChild(c)
    #expect(parent.children.count == 1)
    #expect(parent.subviews.count == before, "duplicate addChild must not re-mount")
}

/// Baseline guard pinned by test: an id absent from properties["children"] is
/// not mounted into the tree (addChild returns before touching the view).
@Test @MainActor
func addChild_childIdNotDeclared_notMounted() {
    let parent = makeParent(logicalIds: ["a"])
    let stranger = Component(componentId: "x", componentType: "card")
    parent.addChild(stranger)
    #expect(parent.children.contains(where: { $0.componentId == "x" }) == false)
    #expect(stranger.superview == nil)
}

// MARK: - Slice 4 extended: geometry hooks (center / bounds / layoutSubviews)

/// ButtonComponent.layoutSubviews() recenters its child via `subview.center =`,
/// which used to bypass the frame setter and leave the sibling shadow stale.
/// The Component `center` override now replays the shadow, so layoutSubviews
/// keeps the shadow in sync. Regression for the "shadow偏左上 + 白色方块"
/// visual bug in Column_ButtonChildShadowCoverageGap.
@Test @MainActor
func shadow_buttonLayoutSubviews_recentersChild_shadowFollowsBody() {
    let button = ButtonComponent(componentId: "btn", properties: [
        "children": ["c"],
        "styles": ["x": 0, "y": 0, "width": 240, "height": 240]
    ])
    let child = Component(componentId: "c", componentType: "card")
    button.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 160, "height": 160,
        "filter": "drop-shadow(0px 0px 30px rgba(0,0,0,0.8))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    #expect(shadow.frame == child.frame, "pre-layoutSubviews shadow should be in sync")

    button.layoutSubviews()
    #expect(child.frame.origin != .zero, "layoutSubviews should have recentered the child")
    #expect(shadow.frame == child.frame,
            "shadow must follow body after layoutSubviews recenter, got shadow=\(shadow.frame) body=\(child.frame)")
}

/// Direct `center` write (bypassing `frame`) must still replay the sibling
/// shadow — the Component.center override ensures position changes drag the
/// shadow view along.
@Test @MainActor
func shadow_centerWrite_shadowFollowsBody() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 100, "height": 100,
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    #expect(shadow.frame == child.frame)

    child.center = CGPoint(x: 200, y: 150)
    #expect(shadow.frame == child.frame,
            "shadow must follow center write, got shadow=\(shadow.frame) body=\(child.frame)")
}

/// Direct `bounds` write (bypassing `frame`) must replay both gradient and
/// sibling shadow — the Component.bounds override ensures size changes update
/// the shadowPath and mask.
@Test @MainActor
func shadow_boundsWrite_shadowFollowsBody() {
    let parent = Component(componentId: "p", componentType: "column",
                           properties: ["children": ["c"]])
    let child = Component(componentId: "c", componentType: "card")
    parent.addChild(child)
    child.updateProperties(DiffValue.from(["styles": [
        "x": 0, "y": 0, "width": 200, "height": 100,
        "filter": "drop-shadow(0px 4px 8px rgba(0,0,0,0.5))"
    ]]))
    guard let shadow = child.shadowView else {
        #expect(Bool(false), "shadow view missing"); return
    }
    let oldPath = shadow.layer.shadowPath

    child.bounds = CGRect(x: 0, y: 0, width: 60, height: 30)
    #expect(shadow.frame == child.frame,
            "shadow frame must follow bounds write, got shadow=\(shadow.frame) body=\(child.frame)")
    let newPath = shadow.layer.shadowPath
    #expect(newPath != oldPath,
            "shadowPath must be rebuilt on bounds change")
}
