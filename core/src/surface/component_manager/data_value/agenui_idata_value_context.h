#pragma once
#include <string>

namespace agenui {
class IDataModel;

/**
 * @brief Data value context interface
 *
 * Provides execution context for DataValue instances, including engine/surface identification
 * and access to the data model. Implemented by ComponentModel.
 */
class IDataValueContext {
public:
    virtual ~IDataValueContext() = default;
    virtual int getInstanceId() const = 0;
    virtual std::string getSurfaceId() const = 0;
    virtual IDataModel* getDataModel() const = 0;
};
} // namespace agenui
