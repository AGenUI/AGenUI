#include "agenui_check_rule_data_value.h"
#include "surface/agenui_serializable_data_impl.h"

namespace agenui {

CheckRuleDataValue::CheckRuleDataValue(IDataModel* dataModel,
                                       std::shared_ptr<DataValue> condition,
                                       const std::string& message)
    : DataValue(dataModel), _condition(std::move(condition)), _message(message) {
}

DataType CheckRuleDataValue::getDataType() const {
    return DataType::CheckRuleData;
}

DataBindingStatus CheckRuleDataValue::getDataBindingStatus() const {
    if (_condition) {
        return _condition->getDataBindingStatus();
    }
    return DataBindingStatus::NotDependent;
}

SerializableData CheckRuleDataValue::getValueData() const {
    if (!_condition) {
        return SerializableData(SerializableData::Impl::create(false));
    }

    auto result = _condition->getValueData();
    if (result.isBool()) {
        return SerializableData(SerializableData::Impl::create(result.asBool()));
    }

    return SerializableData(SerializableData::Impl::create(false));
}

void CheckRuleDataValue::bind(IDataChangedObserver* observer) {
    if (_condition) {
        _condition->bind(observer);
    }
}

void CheckRuleDataValue::unbind() {
    if (_condition) {
        _condition->unbind();
    }
}

std::shared_ptr<DataValue> CheckRuleDataValue::cloneAsTemplate(const std::string& rootDataPath) const {
    std::shared_ptr<DataValue> clonedCondition;
    if (_condition) {
        clonedCondition = _condition->cloneAsTemplate(rootDataPath);
    }

    auto cloned = std::make_shared<CheckRuleDataValue>(_dataModel, clonedCondition, _message);
    cloned->_extensions = _extensions;
    return cloned;
}

const std::string& CheckRuleDataValue::getMessage() const {
    return _message;
}

}  // namespace agenui
