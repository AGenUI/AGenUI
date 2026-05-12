#pragma once
#include "agenui_measurement.h"
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>

namespace agenui {

/**
 * @brief Thread-safe IMeasurementManager implementation
 *
 * Threading model:
 *   - registerMeasurement / unregisterMeasurement called on main thread (ETS registration)
 *   - getMeasurement / measure called on Yoga worker thread
 *   - Uses shared_mutex: concurrent reads (measure), exclusive writes (register/unregister)
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

private:
    mutable std::shared_mutex                            _mutex;
    std::map<std::string, std::shared_ptr<IMeasurement>> _registry;
};

}  // namespace agenui
