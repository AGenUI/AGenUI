#pragma once

#include "agenui_data_value_base.h"
#include <string>
#include <vector>
#include <memory>

namespace agenui {

class IDataModel;
class IDataChangedObserver;
class CheckRuleDataValue;

/**
 * @brief Checks data value
 * @remark Represents a set of validation check rules with implicit AND logic
 */
class ChecksDataValue : public DataValue {
public:
    ChecksDataValue(IDataValueContext* context,
                    const std::vector<std::shared_ptr<CheckRuleDataValue>>& rules);

    DataType getDataType() const override;
    DataBindingStatus getDataBindingStatus() const override;
    SerializableData getValueData() const override;
    void bind(IDataChangedObserver* observer) override;
    void unbind() override;
    std::shared_ptr<DataValue> cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const override;

private:
    std::vector<std::shared_ptr<CheckRuleDataValue>> _checks;
};

}  // namespace agenui