#pragma once

#include "agenui_data_value_base.h"
#include <string>
#include <memory>

namespace agenui {

class IDataModel;
class IDataChangedObserver;

/**
 * @brief Single check rule data value
 * @remark Represents a single validation rule: {condition: DynamicBoolean, message: string}
 */
class CheckRuleDataValue : public DataValue {
public:
    CheckRuleDataValue(IDataModel* dataModel,
                       std::shared_ptr<DataValue> condition,
                       const std::string& message);

    DataType getDataType() const override;
    DataBindingStatus getDataBindingStatus() const override;
    SerializableData getValueData() const override;
    void bind(IDataChangedObserver* observer) override;
    void unbind() override;
    std::shared_ptr<DataValue> cloneAsTemplate(const std::string& rootDataPath) const override;

    const std::string& getMessage() const;

private:
    std::shared_ptr<DataValue> _condition;
    std::string _message;
};

}  // namespace agenui
