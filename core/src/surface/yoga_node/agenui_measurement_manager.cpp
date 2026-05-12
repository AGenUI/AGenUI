#include "agenui_measurement_manager.h"
#include "agenui_logger_internal.h"
#include <cassert>

namespace agenui {

void MeasurementManagerImpl::registerMeasurement(
        const std::string& type, std::shared_ptr<IMeasurement> impl) {
    std::unique_lock<std::shared_mutex> lock(_mutex);  // exclusive write lock
    auto it = _registry.find(type);
    if (it != _registry.end()) {
        AGENUI_LOG("MeasurementManager: duplicate registration for type='%s', overwriting", type.c_str());
        assert(false && "MeasurementManager: duplicate registration detected");
    }
    _registry[type] = std::move(impl);
}

void MeasurementManagerImpl::unregisterMeasurement(const std::string& type) {
    std::unique_lock<std::shared_mutex> lock(_mutex);  // exclusive write lock
    _registry.erase(type);
}

std::shared_ptr<IMeasurement> MeasurementManagerImpl::getMeasurement(
        const std::string& type) {
    std::shared_lock<std::shared_mutex> lock(_mutex);  // shared read lock
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

}  // namespace agenui
