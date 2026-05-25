#pragma once
#include <string>

namespace agenui {
class IDataModel;

/**
 * @brief Surface context interface
 *
 * Provides access to surface-level context information (instance ID, surface ID, data model).
 * Implemented by Surface to pass context down to ComponentManager and ComponentModel.
 */
class ISurfaceContext {
public:
    virtual ~ISurfaceContext() = default;
    virtual int getInstanceId() const = 0;
    virtual std::string getSurfaceId() const = 0;
    virtual IDataModel* getDataModel() const = 0;
};
} // namespace agenui
