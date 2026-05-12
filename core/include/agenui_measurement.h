#pragma once
#include <string>
#include <memory>

namespace agenui {

/**
 * @brief Calculation type for component measurement results
 *
 * Sync  — Text, style sizes etc. that can return results immediately in the measureFunction callback
 * Async — Images, Lottie, Chart, Markdown etc. that require async loading/rendering to determine size.
 *         Async components should return {0, 0} first, then trigger recalculation via markDirty after loading completes.
 */
enum class CalcType {
    Sync,   ///< Sync: measureFunction returns real dimensions
    Async,  ///< Async: measureFunction returns {0,0} first, markDirty after loading completes
};

struct MeasureModeInfo {
    float maxValue = 0.0f;  ///< Max available value (from Yoga width/height)
    int   mode     = 0;     ///< Measure mode (YGMeasureMode: 0=Undefined 1=Exactly 2=AtMost)
};

struct MeasureModes {
    MeasureModeInfo width;
    MeasureModeInfo height;
};

struct MeasureResult {
    CalcType calcType    = CalcType::Sync;  ///< Whether async recalculation is needed
    float    width       = 0.0f;            ///< Width in a2ui logical units (pv * 2)
    float    height      = 0.0f;            ///< Height in a2ui logical units (pv * 2)
    int      countOfLines = 0;              ///< Text line count (0 for non-text components)
};

/**
 * @brief Component measurement interface
 *
 * Each component type has a corresponding IMeasurement implementation that converts
 * component attribute JSON into the size result required by Yoga.
 *
 * Lifecycle:
 *   Instances are held by MeasurementManagerImpl via shared_ptr, passed in at registration.
 *   Call unregisterMeasurement before Surface destruction; shared_ptr ref count drops to zero and destructs.
 */
class IMeasurement {
public:
    virtual ~IMeasurement() = default;

    /**
     * @brief Calculate the intrinsic size of a component
     * @param paramJson Component attribute JSON string (constructed by VirtualDOMNode)
     * @param modes     Width/height constraints from Yoga
     * @return          Measurement result; width/height are 0 when CalcType::Async
     */
    virtual MeasureResult measure(const std::string& paramJson,
                                  const MeasureModes& modes) = 0;

    /**
     * @brief Whether this component allows measureFunc to be set even when Yoga child nodes exist
     *
     * Default returns false (Yoga rule: child nodes should not have measureFunc).
     * Tabs and similar container components override to return true: although Yoga child nodes
     * are each tab's VirtualDOMNode, the Tabs own height (tabBar + selected tab content)
     * needs to be self-calculated via measureFunc.
     */
    virtual bool allowsMeasureWithChildren() const { return false; }
};

/**
 * @brief Component measurement manager interface
 *
 * Maintains a type -> IMeasurement registry, exposing a unified measure entry point.
 * Held by AGenUIEngine (unique_ptr<MeasurementManagerImpl>) as an engine-level singleton.
 * Surface and VirtualDOMNode only hold raw pointer references.
 */
class IMeasurementManager {
public:
    virtual ~IMeasurementManager() = default;

    /**
     * @brief Register a measurement implementation for a component type
     * @param type Component type string (e.g. "Text", "Image")
     * @param impl shared_ptr wrapping the IMeasurement implementation
     * @note Thread-safe (internal unique_lock). Duplicate registration triggers assert.
     */
    virtual void registerMeasurement(const std::string& type,
                                     std::shared_ptr<IMeasurement> impl) = 0;

    /**
     * @brief Unregister the measurement implementation for a component type
     * @param type Component type string
     * @note Thread-safe (internal unique_lock). In-flight measure() calls are unaffected after unregistration.
     */
    virtual void unregisterMeasurement(const std::string& type) = 0;

    /**
     * @brief Look up a measurement implementation by type
     * @param type Component type string
     * @return Corresponding shared_ptr, or nullptr if not registered
     */
    virtual std::shared_ptr<IMeasurement> getMeasurement(const std::string& type) = 0;

    /**
     * @brief Unified measurement entry point
     * @param type      Component type
     * @param paramJson Component attribute JSON
     * @param modes     Width/height constraints from Yoga
     * @return Measurement result; returns {CalcType::Sync, 0, 0} if not registered
     * @note Thread-safe (internal shared_lock, impl->measure called outside lock)
     */
    virtual MeasureResult measure(const std::string& type,
                                  const std::string& paramJson,
                                  const MeasureModes& modes) = 0;
};

}  // namespace agenui
