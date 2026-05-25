#include "agenui_measurement_manager.h"
#include "agenui_logger_internal.h"
#include <cassert>

namespace agenui {

void MeasurementManagerImpl::registerMeasurement(
        const std::string& type, std::shared_ptr<IMeasurement> impl) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _registry.find(type);
    if (it != _registry.end()) {
        AGENUI_LOG("[MeasurementManager][WARN] duplicate registration for type='%s'; "
                   "removing previous entry and registering the new one (last-write-wins)",
                   type.c_str());
        _registry.erase(it);
    }
    _registry.emplace(type, std::move(impl));
}

void MeasurementManagerImpl::unregisterMeasurement(const std::string& type) {
    std::lock_guard<std::mutex> lock(_mutex);
    _registry.erase(type);
}

std::shared_ptr<IMeasurement> MeasurementManagerImpl::getMeasurement(
        const std::string& type) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _registry.find(type);
    return it != _registry.end() ? it->second : nullptr;
}

MeasureResult MeasurementManagerImpl::measure(
        const std::string& type,
        const std::string& paramJson,
        const MeasureModes& modes) {
    // Take shared_ptr under lock (ref count +1), release lock, then call measure.
    // This ensures impl is not freed even if ETS side unregisters concurrently.
    std::shared_ptr<IMeasurement> impl = getMeasurement(type);
    if (!impl) {
        return MeasureResult{CalcType::Sync, 0.0f, 0.0f, 0};
    }
    return impl->measure(paramJson, modes);  // call outside lock to avoid deadlock
}

// ===================== measureFunc decision policy =====================
// Evaluated in priority order; first matching rule wins.
//
// Mapping against Yoga source reality:
//   * Yoga itself does NOT decide whether to invoke a measureFunc — once set,
//     leaf evaluation will always call it.
//   * Yoga enforces exactly one rule internally: a node with a measureFunc
//     cannot have children (YGNodeInsertChild asserts !YGNodeHasMeasureFunc).
//   * All other rules below are business optimisations: registering measureFunc
//     would otherwise SHADOW style-based size (explicit px / aspect-ratio).
// =======================================================================

namespace {

inline bool isExplicitPx(const std::string& val) {
    if (val.empty() || val == "auto") return false;
    if (val.back() == '%') return false;
    return true;  // bare number, or number + px/vp unit suffix
}

inline bool isPercent(const std::string& val) {
    return !val.empty() && val.back() == '%';
}

inline bool isValidAspectRatio(const std::string& val) {
    if (val.empty() || val == "auto" || val == "0") return false;
    return true;
}

}  // namespace

MeasureDecision MeasurementManagerImpl::shouldUseMeasureFunc(
        const std::string& type,
        const MeasureDecisionContext& ctx) {
    // [Rule 1] No measurement implementation registered for this type.
    //          Callers should still Clear any residual measureFunc to avoid drift.
    std::shared_ptr<IMeasurement> impl = getMeasurement(type);
    if (!impl) {
        return MeasureDecision::Clear;
    }

    // [Rule 2] Platform has reported a concrete size — measureFunc must yield
    //          so Yoga trusts the platform value (no oscillation).
    if (ctx.platformSizeLocked) {
        return MeasureDecision::Clear;
    }

    // [Rule 3] Yoga hard rule: measureFunc + children are mutually exclusive,
    //          unless the component explicitly opts in (e.g. Tabs).
    if (ctx.hasChildren && !impl->allowsMeasureWithChildren()) {
        return MeasureDecision::Clear;
    }

    // [Rule 4] Both axes are explicit pixels: Yoga can size the node directly
    //          from styles; a measureFunc would only add cost and risk shadowing.
    if (isExplicitPx(ctx.widthStyle) && isExplicitPx(ctx.heightStyle)) {
        return MeasureDecision::Clear;
    }

    // [Rule 5] aspect-ratio set AND at least one axis resolvable (explicit px
    //          or percentage): Yoga can derive the other axis via aspect-ratio.
    //          If both axes are auto, aspect-ratio alone cannot solve size, so we
    //          still fall through to register measureFunc for the intrinsic size.
    if (isValidAspectRatio(ctx.aspectRatioStyle)) {
        const bool anySolvable =
            isExplicitPx(ctx.widthStyle)  || isPercent(ctx.widthStyle) ||
            isExplicitPx(ctx.heightStyle) || isPercent(ctx.heightStyle);
        if (anySolvable) {
            return MeasureDecision::Clear;
        }
        // fall through: both axes auto, Yoga needs measureFunc to get intrinsic
    }

    // [Rule 7] Register measureFunc: Yoga has no other way to size this node.
    return MeasureDecision::Register;
}

}  // namespace agenui
