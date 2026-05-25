#pragma once
#include "agenui_measurement.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace agenui {

/**
 * @brief Internal context for deciding whether to register a Yoga measureFunc.
 *
 * Populated by the layout engine and evaluated by
 * MeasurementManagerImpl::shouldUseMeasureFunc. Internal type — NOT exposed
 * through the public IMeasurementManager interface.
 */
struct MeasureDecisionContext {
    bool        hasChildren        = false;  ///< Yoga node currently has > 0 children
    bool        platformSizeLocked = false;  ///< notifyRenderFinish has supplied a concrete size
    std::string widthStyle;                  ///< raw style string for "width"
    std::string heightStyle;                 ///< raw style string for "height"
    std::string aspectRatioStyle;            ///< raw style string for "aspect-ratio"
};

/**
 * @brief Internal three-state decision from MeasurementManagerImpl::shouldUseMeasureFunc.
 * Internal type — NOT exposed through the public IMeasurementManager interface.
 */
enum class MeasureDecision {
    Register,   ///< Register (or keep) measureFunc on the node
    Clear,      ///< Explicitly clear any existing measureFunc (Yoga can solve on its own)
    Skip,       ///< No-op (no measurement impl for this type, and nothing to clear)
};

/**
 * @brief Thread-safe IMeasurementManager implementation
 *
 * Threading model:
 *   - registerMeasurement / unregisterMeasurement called on main thread (ETS registration)
 *   - getMeasurement / measure called on Yoga worker thread
 *   - Uses a plain std::mutex for all read/write access. The registry is tiny
 *     (typically a dozen entries) and accesses are short; contention is
 *     negligible compared to the lifecycle-risk simplification of dropping
 *     shared_mutex's reader/writer split.
 *
 * Lifecycle:
 *   - Held by AGenUIEngine (unique_ptr), engine-level singleton
 *   - Obtained via IAGenUIEngine::getMeasurementManager() as raw pointer
 *   - Surface and VirtualDOMNode hold raw pointer references
 */
class MeasurementManagerImpl : public IMeasurementManager {
public:
    MeasurementManagerImpl()  = default;
    ~MeasurementManagerImpl() = default;

    // Non-copyable
    MeasurementManagerImpl(const MeasurementManagerImpl&) = delete;
    MeasurementManagerImpl& operator=(const MeasurementManagerImpl&) = delete;

    void registerMeasurement(const std::string& type,
                             std::shared_ptr<IMeasurement> impl) override;

    void unregisterMeasurement(const std::string& type) override;

    std::shared_ptr<IMeasurement> getMeasurement(const std::string& type) override;

    MeasureResult measure(const std::string& type,
                          const std::string& paramJson,
                          const MeasureModes& modes) override;

    /**
     * @brief Internal API consumed by YogaLayoutEngine; NOT part of the
     *        public IMeasurementManager interface.
     */
    MeasureDecision shouldUseMeasureFunc(const std::string& type,
                                         const MeasureDecisionContext& ctx);

private:
    mutable std::mutex                                   _mutex;
    std::map<std::string, std::shared_ptr<IMeasurement>> _registry;
};

}  // namespace agenui
