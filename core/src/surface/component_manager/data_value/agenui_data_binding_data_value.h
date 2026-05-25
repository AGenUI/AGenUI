#pragma once

#include <string>
#include <memory>
#include "agenui_data_value_base.h"

namespace agenui {

class IDataModel;
class IDataChangedObserver;

/**
 * @brief Data-binding data value
 * @remark Represents a dynamic value bound to a path in the data model.
 *         Resolving this DataValue reads the latest value at `bindingPath`
 *         from the surface's IDataModel.
 */
class DataBindingDataValue : public DataValue {
public:
    DataBindingDataValue(IDataValueContext* context, const std::string& bindingPath);
    ~DataBindingDataValue() override;

    DataType getDataType() const override;
    DataBindingStatus getDataBindingStatus() const override;
    SerializableData getValueData() const override;
    void bind(IDataChangedObserver* observer) override;
    void unbind() override;
    std::shared_ptr<DataValue> cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const override;

    void setContext(IDataValueContext* context);
    std::string getBindingPath() const;
    void syncBindingValue(const std::string& value);

private:
    std::string _bindingPath;
    IDataChangedObserver* _observer;
};

}  // namespace agenui
