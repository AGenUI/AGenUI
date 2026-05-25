#include "gradient_applier.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "a2ui/utils/a2ui_unit_utils.h"
#include "log/a2ui_capi_log.h"

extern ArkUI_NativeNodeAPI_1* g_nodeAPI;

namespace a2ui {

namespace {

constexpr const char* kTag = "GradientApplier";

struct ResolvedStops {
    std::vector<uint32_t> colors;
    std::vector<float> positions;  // in [0, 1]
};

float normalizePosition(const agenui::ColorStop& cs, bool forSweep) {
    using agenui::StopUnit;
    switch (cs.unit) {
        case StopUnit::Percent: return cs.position;  // already 0~1
        case StopUnit::Deg:     return forSweep ? (cs.position / 360.0f) : cs.position;
        case StopUnit::Grad:    return forSweep ? (cs.position / 400.0f) : cs.position;
        case StopUnit::Rad:     return forSweep ? static_cast<float>(cs.position / (2.0 * M_PI)) : cs.position;
        case StopUnit::Turn:    return cs.position;
        case StopUnit::Px:
        default:
            // Px positions in a linear-gradient cannot be resolved without the
            // gradient line length; fall back to the even-distribution sentinel
            // (mirrors GradientDrawableFactory.normalizePosition).
            return -1.0f;
    }
}

/**
 * Mirror of GradientDrawableFactory.collectStops in Android: drop hint stops,
 * fill in missing positions via linear interpolation, clamp to [0,1] and enforce
 * monotonic non-decreasing order.
 *
 * Returns false if fewer than two usable stops exist.
 */
bool collectStops(const std::vector<agenui::ColorStop>& raw,
                  bool forSweep,
                  ResolvedStops& out) {
    out.colors.clear();
    out.positions.clear();
    if (raw.empty()) return false;

    std::vector<const agenui::ColorStop*> kept;
    kept.reserve(raw.size());
    for (const auto& cs : raw) {
        if (cs.isHint) continue;
        kept.push_back(&cs);
    }
    const size_t n = kept.size();
    if (n < 2) return false;

    out.colors.reserve(n);
    out.positions.reserve(n);
    bool anyExplicit = false;
    for (size_t i = 0; i < n; ++i) {
        const agenui::ColorStop& cs = *kept[i];
        out.colors.push_back(cs.color);
        if (!cs.hasPosition) {
            out.positions.push_back(-1.0f);  // sentinel
        } else {
            out.positions.push_back(normalizePosition(cs, forSweep));
            anyExplicit = true;
        }
    }

    if (!anyExplicit) {
        for (size_t i = 0; i < n; ++i) {
            out.positions[i] = static_cast<float>(i) / static_cast<float>(n - 1);
        }
    } else {
        if (out.positions[0] < 0.0f)     out.positions[0]     = 0.0f;
        if (out.positions[n - 1] < 0.0f) out.positions[n - 1] = 1.0f;
        size_t i = 0;
        while (i < n) {
            if (out.positions[i] >= 0.0f) { ++i; continue; }
            size_t gapStart = i - 1;
            size_t gapEnd = i;
            while (gapEnd < n && out.positions[gapEnd] < 0.0f) ++gapEnd;
            float a = out.positions[gapStart];
            float b = (gapEnd < n) ? out.positions[gapEnd] : 1.0f;
            size_t slots = gapEnd - gapStart;
            for (size_t k = 1; k < slots; ++k) {
                out.positions[gapStart + k] = a + (b - a) * static_cast<float>(k) / static_cast<float>(slots);
            }
            i = gapEnd;
        }
    }

    // Monotonic non-decreasing in [0,1].
    for (size_t i = 0; i < n; ++i) {
        if (out.positions[i] < 0.0f) out.positions[i] = 0.0f;
        if (out.positions[i] > 1.0f) out.positions[i] = 1.0f;
        if (i > 0 && out.positions[i] < out.positions[i - 1]) {
            out.positions[i] = out.positions[i - 1];
        }
    }
    return true;
}

/** {l, r, t, b} distances from (cx, cy) to view sides (in vp). */
struct Sides { float l, r, t, b; };
Sides sidesFromCenter(float cx, float cy, float w, float h) {
    return {cx, std::max(0.0f, w - cx), cy, std::max(0.0f, h - cy)};
}

struct RxRy { float rx, ry; };

/**
 * Mirror of GradientDrawableFactory.keywordSize. ArkUI radial only supports a
 * single radius, so for ellipse shapes we collapse to max(rx, ry); the caller
 * is expected to log this approximation.
 */
RxRy keywordSize(agenui::RadialSize sz, agenui::RadialShape shape, const Sides& s) {
    using agenui::RadialSize;
    using agenui::RadialShape;
    const bool circle = shape == RadialShape::Circle;
    switch (sz) {
        case RadialSize::ClosestSide: {
            float rx = std::min(s.l, s.r);
            float ry = std::min(s.t, s.b);
            float v  = std::min(rx, ry);
            return circle ? RxRy{v, v} : RxRy{rx, ry};
        }
        case RadialSize::FarthestSide: {
            float rx = std::max(s.l, s.r);
            float ry = std::max(s.t, s.b);
            float v  = std::max(rx, ry);
            return circle ? RxRy{v, v} : RxRy{rx, ry};
        }
        case RadialSize::ClosestCorner: {
            float dx = std::min(s.l, s.r);
            float dy = std::min(s.t, s.b);
            float r  = std::hypot(dx, dy);
            if (circle) return RxRy{r, r};
            const float k = std::sqrt(2.0f);
            return RxRy{std::min(s.l, s.r) * k, std::min(s.t, s.b) * k};
        }
        case RadialSize::FarthestCorner:
        default: {
            float dx = std::max(s.l, s.r);
            float dy = std::max(s.t, s.b);
            float r  = std::hypot(dx, dy);
            if (circle) return RxRy{r, r};
            const float k = std::sqrt(2.0f);
            return RxRy{std::max(s.l, s.r) * k, std::max(s.t, s.b) * k};
        }
    }
}

void writeLinear(ArkUI_NodeHandle node, const agenui::GradientInfo& info) {
    ResolvedStops stops;
    if (!collectStops(info.colorStops, /*forSweep=*/false, stops)) {
        HM_LOGW("linear gradient skipped: <2 usable color stops");
        return;
    }
    if (info.linear.angleIsCalc) {
        HM_LOGW("linear gradient angle is calc(), using literal: %s",
                info.linear.angleCalcExpr.c_str());
    }

    ArkUI_ColorStop cs;
    cs.colors = stops.colors.data();
    cs.stops  = stops.positions.data();
    cs.size   = static_cast<int>(stops.colors.size());

    ArkUI_NumberValue gv[3];
    gv[0].f32 = info.linear.angle;
    gv[1].i32 = ARKUI_LINEAR_GRADIENT_DIRECTION_CUSTOM;
    gv[2].i32 = info.isRepeating ? 1 : 0;
    ArkUI_AttributeItem item = {gv, 3, nullptr, &cs};
    g_nodeAPI->setAttribute(node, NODE_LINEAR_GRADIENT, &item);
}

void writeRadial(ArkUI_NodeHandle node,
                 const agenui::GradientInfo& info,
                 float viewWidthA2ui,
                 float viewHeightA2ui) {
    ResolvedStops stops;
    if (!collectStops(info.colorStops, /*forSweep=*/false, stops)) {
        HM_LOGW("radial gradient skipped: <2 usable color stops");
        return;
    }
    const agenui::RadialGradientParams& rp = info.radial;
    const float wVp = UnitConverter::a2uiToVp(viewWidthA2ui);
    const float hVp = UnitConverter::a2uiToVp(viewHeightA2ui);

    const float cxVp = rp.centerXIsPx ? UnitConverter::a2uiToVp(rp.centerX)
                                      : (rp.centerX * wVp);
    const float cyVp = rp.centerYIsPx ? UnitConverter::a2uiToVp(rp.centerY)
                                      : (rp.centerY * hVp);

    float rxVp, ryVp;
    if (rp.hasExplicitSize) {
        if (rp.radiusXIsCalc || rp.radiusYIsCalc) {
            HM_LOGW("radial gradient radius calc() not supported; using literal");
        }
        rxVp = rp.radiusXIsPercent ? rp.radiusX * wVp : UnitConverter::a2uiToVp(rp.radiusX);
        ryVp = rp.radiusYIsPercent ? rp.radiusY * hVp : UnitConverter::a2uiToVp(rp.radiusY);
    } else {
        Sides s = sidesFromCenter(cxVp, cyVp, wVp, hVp);
        RxRy kw = keywordSize(rp.size, rp.shape, s);
        rxVp = kw.rx;
        ryVp = kw.ry;
    }
    if (std::fabs(rxVp - ryVp) > 0.5f) {
        HM_LOGW("radial gradient asymmetric radius (%.1f x %.1f vp); ArkUI NDK only "
                "supports a single radius, falling back to max",
                rxVp, ryVp);
    }
    float radiusVp = std::max(rxVp, ryVp);
    if (radiusVp <= 0.0f) radiusVp = 1.0f;

    ArkUI_ColorStop cs;
    cs.colors = stops.colors.data();
    cs.stops  = stops.positions.data();
    cs.size   = static_cast<int>(stops.colors.size());

    ArkUI_NumberValue rv[4];
    rv[0].f32 = cxVp;
    rv[1].f32 = cyVp;
    rv[2].f32 = radiusVp;
    rv[3].i32 = info.isRepeating ? 1 : 0;
    ArkUI_AttributeItem item = {rv, 4, nullptr, &cs};
    g_nodeAPI->setAttribute(node, NODE_RADIAL_GRADIENT, &item);
}

void writeConic(ArkUI_NodeHandle node,
                const agenui::GradientInfo& info,
                float viewWidthA2ui,
                float viewHeightA2ui) {
    ResolvedStops stops;
    if (!collectStops(info.colorStops, /*forSweep=*/true, stops)) {
        HM_LOGW("conic gradient skipped: <2 usable color stops");
        return;
    }
    const agenui::ConicGradientParams& cp = info.conic;
    const float wVp = UnitConverter::a2uiToVp(viewWidthA2ui);
    const float hVp = UnitConverter::a2uiToVp(viewHeightA2ui);

    float cxVp = cp.centerXIsPx ? UnitConverter::a2uiToVp(cp.centerX) : cp.centerX * wVp;
    float cyVp = cp.centerYIsPx ? UnitConverter::a2uiToVp(cp.centerY) : cp.centerY * hVp;
    if (cp.centerXIsPx && cp.centerX == 0.0f && cp.centerY == 0.0f) {
        // Sensible default when parser left both unset (first paint, no layout yet).
        cxVp = wVp / 2.0f;
        cyVp = hVp / 2.0f;
    }

    if (cp.startAngleIsCalc) {
        HM_LOGW("conic gradient startAngle is calc(), defaulting to 0: %s",
                cp.startAngleCalcExpr.c_str());
    }

    ArkUI_ColorStop cs;
    cs.colors = stops.colors.data();
    cs.stops  = stops.positions.data();
    cs.size   = static_cast<int>(stops.colors.size());

    // Sweep over a full revolution; rotate so 0deg of the gradient lines up with
    // the requested startAngle (CSS conic 0deg = 12 o'clock).
    ArkUI_NumberValue sv[6];
    sv[0].f32 = cxVp;
    sv[1].f32 = cyVp;
    sv[2].f32 = 0.0f;                          // sweep start
    sv[3].f32 = 360.0f;                        // sweep end (full circle)
    sv[4].f32 = cp.startAngleIsCalc ? 0.0f : cp.startAngle;  // rotation
    sv[5].i32 = info.isRepeating ? 1 : 0;
    ArkUI_AttributeItem item = {sv, 6, nullptr, &cs};
    g_nodeAPI->setAttribute(node, NODE_SWEEP_GRADIENT, &item);
}

}  // namespace

void GradientApplier::apply(ArkUI_NodeHandle node,
                            const agenui::GradientInfo& info,
                            float viewWidthA2ui,
                            float viewHeightA2ui) {
    if (!node || !g_nodeAPI) return;

    // Always reset other gradient kinds before installing a new one — switching
    // from linear → radial without resetting can leave both bound on the node.
    reset(node);

    switch (info.type) {
        case agenui::GradientType::Linear:
            writeLinear(node, info);
            break;
        case agenui::GradientType::Radial:
            writeRadial(node, info, viewWidthA2ui, viewHeightA2ui);
            break;
        case agenui::GradientType::Conic:
            writeConic(node, info, viewWidthA2ui, viewHeightA2ui);
            break;
    }
}

void GradientApplier::reset(ArkUI_NodeHandle node) {
    if (!node || !g_nodeAPI) return;
    g_nodeAPI->resetAttribute(node, NODE_LINEAR_GRADIENT);
    g_nodeAPI->resetAttribute(node, NODE_RADIAL_GRADIENT);
    g_nodeAPI->resetAttribute(node, NODE_SWEEP_GRADIENT);
}

}  // namespace a2ui
