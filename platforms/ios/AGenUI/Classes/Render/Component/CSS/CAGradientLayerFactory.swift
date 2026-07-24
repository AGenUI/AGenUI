//
//  CAGradientLayerFactory.swift
//  AGenUI
//
//  Configures CAGradientLayer with linear / radial / conic gradient geometry.
//  Cross-platform counterpart of Android's GradientDrawableFactory.
//

import UIKit

enum CAGradientLayerFactory {

    /// Configure an existing CAGradientLayer. Returns false if < 2 color stops.
    static func configure(_ info: AGUIGradientInfo, on layer: CAGradientLayer, bounds: CGRect) -> Bool {
        guard let stops = collectStops(info.colorStops,
                                       forSweep: info.gradientType == .conic) else {
            return false
        }

        layer.colors = stops.colors.map { $0 as Any }
        layer.locations = stops.positions.map { NSNumber(value: Float($0)) }

        switch info.gradientType {
        case .linear:
            configureLinear(layer, info: info, bounds: bounds)
        case .radial:
            configureRadial(layer, info: info, bounds: bounds)
        case .conic:
            configureConic(layer, info: info, bounds: bounds)
        }

        return true
    }

    // MARK: - Linear

    private static func configureLinear(_ layer: CAGradientLayer,
                                        info: AGUIGradientInfo,
                                        bounds: CGRect) {
        layer.type = .axial

        let w = bounds.width
        let h = bounds.height
        guard w > 0, h > 0 else {
            layer.startPoint = .zero
            layer.endPoint = CGPoint(x: 1, y: 1)
            return
        }

        // CSS 0deg = "to top"; CAGradientLayer y-axis points down.
        let angle = Double(info.linear?.angle ?? 180.0)
        let rad = (angle - 90.0) * .pi / 180.0
        let dx = cos(rad)
        let dy = sin(rad)

        // Same length formula as Android's GradientDrawableFactory.buildLinear.
        let angleRad = angle * .pi / 180.0
        let len = abs(Double(w) * sin(angleRad)) + abs(Double(h) * cos(angleRad))

        let cx = Double(w) / 2.0
        let cy = Double(h) / 2.0
        let startX = cx - dx * len / 2.0
        let startY = cy - dy * len / 2.0
        let endX = cx + dx * len / 2.0
        let endY = cy + dy * len / 2.0

        layer.startPoint = CGPoint(x: startX / Double(w), y: startY / Double(h))
        layer.endPoint = CGPoint(x: endX / Double(w), y: endY / Double(h))
    }

    // MARK: - Radial

    private static func configureRadial(_ layer: CAGradientLayer,
                                        info: AGUIGradientInfo,
                                        bounds: CGRect) {
        layer.type = .radial

        let w = bounds.width
        let h = bounds.height
        guard w > 0, h > 0, let rp = info.radial else {
            layer.startPoint = CGPoint(x: 0.5, y: 0.5)
            layer.endPoint = CGPoint(x: 1, y: 1)
            return
        }

        let cxPx = rp.centerXIsPx ? a2uiToPt(CGFloat(rp.centerX)) : CGFloat(rp.centerX) * w
        let cyPx = rp.centerYIsPx ? a2uiToPt(CGFloat(rp.centerY)) : CGFloat(rp.centerY) * h

        var rx: CGFloat
        var ry: CGFloat
        if rp.hasExplicitSize {
            rx = rp.radiusXIsPercent ? CGFloat(rp.radiusX) * w : a2uiToPt(CGFloat(rp.radiusX))
            ry = rp.radiusYIsPercent ? CGFloat(rp.radiusY) * h : a2uiToPt(CGFloat(rp.radiusY))
        } else {
            let sides = sidesFromCenter(cx: cxPx, cy: cyPx, w: w, h: h)
            let kw = keywordSize(size: rp.size, shape: rp.shape, sides: sides)
            rx = kw.0
            ry = kw.1
        }
        if rx <= 0 { rx = 1 }
        if ry <= 0 { ry = 1 }

        layer.startPoint = CGPoint(x: cxPx / w, y: cyPx / h)
        // CAGradientLayer .radial draws an ellipse from startPoint to endPoint; the
        // endPoint is the corner of the bounding box, so add (rx/w, ry/h).
        layer.endPoint = CGPoint(x: (cxPx + rx) / w, y: (cyPx + ry) / h)
    }

    // MARK: - Conic

    private static func configureConic(_ layer: CAGradientLayer,
                                       info: AGUIGradientInfo,
                                       bounds: CGRect) {
        layer.type = .conic

        let w = bounds.width
        let h = bounds.height
        guard w > 0, h > 0 else {
            layer.startPoint = CGPoint(x: 0.5, y: 0.5)
            layer.endPoint = CGPoint(x: 1, y: 0.5)
            return
        }

        var cxNorm: CGFloat = 0.5
        var cyNorm: CGFloat = 0.5
        if let cp = info.conic {
            let cxPx = cp.centerXIsPx ? a2uiToPt(CGFloat(cp.centerX)) : CGFloat(cp.centerX) * w
            let cyPx = cp.centerYIsPx ? a2uiToPt(CGFloat(cp.centerY)) : CGFloat(cp.centerY) * h
            // Sensible default when parser left both at 0 px (first paint, no layout yet).
            if cp.centerXIsPx && cp.centerX == 0 && cp.centerY == 0 {
                cxNorm = 0.5
                cyNorm = 0.5
            } else {
                cxNorm = cxPx / w
                cyNorm = cyPx / h
            }
        }
        layer.startPoint = CGPoint(x: cxNorm, y: cyNorm)

        // CSS conic 0deg = 12 o'clock; CAGradientLayer .conic 0 deg = direction
        // (startPoint -> endPoint). For startAngle=0 we want endPoint to point
        // straight up. Setting endPoint = center + (sin a, -cos a) keeps the colors
        // sweeping clockwise from that direction.
        let startAngle = Double(info.conic?.startAngle ?? 0)
        let rad = startAngle * .pi / 180.0
        // Use radius 0.5 in normalized space so endPoint stays inside the unit box.
        let dx = sin(rad) * 0.5
        let dy = -cos(rad) * 0.5
        layer.endPoint = CGPoint(x: cxNorm + dx, y: cyNorm + dy)
    }

    // MARK: - Color stops (mirrors GradientDrawableFactory.collectStops)

    private struct Stops {
        let colors: [CGColor]
        let positions: [CGFloat]
    }

    private static func collectStops(_ raw: [AGUIColorStop], forSweep: Bool) -> Stops? {
        let kept = raw.filter { !$0.isHint }
        guard kept.count >= 2 else { return nil }

        var colors: [CGColor] = []
        var positions: [CGFloat] = []
        var anyExplicit = false
        for cs in kept {
            colors.append(cgColor(fromArgb: cs.color))
            if cs.hasPosition {
                positions.append(normalizePosition(cs, forSweep: forSweep))
                anyExplicit = true
            } else {
                positions.append(-1) // sentinel: fill later
            }
        }

        let n = positions.count
        if !anyExplicit {
            for i in 0..<n {
                positions[i] = CGFloat(i) / CGFloat(n - 1)
            }
        } else {
            if positions[0] < 0 { positions[0] = 0 }
            if positions[n - 1] < 0 { positions[n - 1] = 1 }
            var i = 0
            while i < n {
                if positions[i] >= 0 { i += 1; continue }
                let gapStart = i - 1
                var gapEnd = i
                while gapEnd < n && positions[gapEnd] < 0 { gapEnd += 1 }
                let a = positions[gapStart]
                let b = gapEnd < n ? positions[gapEnd] : 1
                let slots = gapEnd - gapStart
                for k in 1..<slots {
                    positions[gapStart + k] = a + (b - a) * CGFloat(k) / CGFloat(slots)
                }
                i = gapEnd
            }
        }

        // Monotonic non-decreasing in [0,1].
        for i in 0..<n {
            if positions[i] < 0 { positions[i] = 0 }
            if positions[i] > 1 { positions[i] = 1 }
            if i > 0 && positions[i] < positions[i - 1] {
                positions[i] = positions[i - 1]
            }
        }

        return Stops(colors: colors, positions: positions)
    }

    private static func normalizePosition(_ cs: AGUIColorStop, forSweep: Bool) -> CGFloat {
        switch cs.unit {
        case .percent:
            return CGFloat(cs.position) // already 0~1
        case .deg:
            return forSweep ? CGFloat(cs.position) / 360.0 : CGFloat(cs.position)
        case .grad:
            return forSweep ? CGFloat(cs.position) / 400.0 : CGFloat(cs.position)
        case .rad:
            return forSweep ? CGFloat(cs.position) / CGFloat(2.0 * .pi) : CGFloat(cs.position)
        case .turn:
            return CGFloat(cs.position)
        case .px:
            // Same fallback as Android: without the gradient line length we cannot
            // map exactly; return the sentinel and let the even-distribution path
            // smooth it out.
            return -1
        }
    }

    // MARK: - Geometry helpers (mirrors GradientDrawableFactory)

    /// Distances {l, r, t, b} from (cx, cy) to view sides.
    private static func sidesFromCenter(cx: CGFloat, cy: CGFloat,
                                        w: CGFloat, h: CGFloat)
        -> (l: CGFloat, r: CGFloat, t: CGFloat, b: CGFloat)
    {
        return (cx, max(0, w - cx), cy, max(0, h - cy))
    }

    private static func keywordSize(size: AGUIRadialSize,
                                    shape: AGUIRadialShape,
                                    sides: (l: CGFloat, r: CGFloat, t: CGFloat, b: CGFloat))
        -> (CGFloat, CGFloat)
    {
        let l = sides.l, r = sides.r, t = sides.t, b = sides.b
        let isCircle = shape == .circle
        switch size {
        case .closestSide:
            let rx = min(l, r)
            let ry = min(t, b)
            return isCircle ? (min(rx, ry), min(rx, ry)) : (rx, ry)
        case .farthestSide:
            let rx = max(l, r)
            let ry = max(t, b)
            return isCircle ? (max(rx, ry), max(rx, ry)) : (rx, ry)
        case .closestCorner:
            let dx = min(l, r)
            let dy = min(t, b)
            let rad = sqrt(dx * dx + dy * dy)
            if isCircle { return (rad, rad) }
            let k = sqrt(2.0)
            return (min(l, r) * CGFloat(k), min(t, b) * CGFloat(k))
        case .farthestCorner:
            let dx = max(l, r)
            let dy = max(t, b)
            let rad = sqrt(dx * dx + dy * dy)
            if isCircle { return (rad, rad) }
            let k = sqrt(2.0)
            return (max(l, r) * CGFloat(k), max(t, b) * CGFloat(k))
        }
    }

    // MARK: - Unit conversion

    /// a2ui units are double-resolution (a2ui/2 = pt). Same convention as
    /// CSSPropertyParser.BS_POINT_SCALE = 0.5.
    private static func a2uiToPt(_ value: CGFloat) -> CGFloat {
        return value * 0.5
    }

    private static func cgColor(fromArgb argb: UInt32) -> CGColor {
        let a = CGFloat((argb >> 24) & 0xFF) / 255.0
        let r = CGFloat((argb >> 16) & 0xFF) / 255.0
        let g = CGFloat((argb >> 8) & 0xFF) / 255.0
        let b = CGFloat(argb & 0xFF) / 255.0
        return UIColor(red: r, green: g, blue: b, alpha: a).cgColor
    }
}
