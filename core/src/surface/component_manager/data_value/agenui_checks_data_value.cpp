#include "agenui_checks_data_value.h"
#include "agenui_check_rule_data_value.h"
#include "surface/agenui_serializable_data_impl.h"

namespace agenui {

ChecksDataValue::ChecksDataValue(IDataValueContext* context,
                                 const std::vector<std::shared_ptr<CheckRuleDataValue>>& rules)
    : DataValue(context), _checks(rules) {
}

DataType ChecksDataValue::getDataType() const {
    return DataType::ChecksData;
}

DataBindingStatus ChecksDataValue::getDataBindingStatus() const {
    std::vector<DataBindingStatus> statuses;
    for (const auto& check : _checks) {
        if (check) {
            statuses.emplace_back(check->getDataBindingStatus());
        }
    }
    return aggregateBindingStatus(statuses);
}

SerializableData ChecksDataValue::getValueData() const {
    for (const auto& check : _checks) {
        if (!check) {
            continue;
        }

        auto result = check->getValueData();
        if (!(result.isBool() && result.asBool())) {
            auto impl = SerializableData::Impl::createObject();
            impl->set("result", false);
            impl->set("message", check->getMessage());
            return SerializableData(impl);
        }
    }

    auto impl = SerializableData::Impl::createObject();
    impl->set("result", true);
    return SerializableData(impl);
}

void ChecksDataValue::bind(IDataChangedObserver* observer) {
    for (auto& check : _checks) {
        if (check) {
            check->bind(observer);
        }
    }
}

void ChecksDataValue::unbind() {
    for (auto& check : _checks) {
        if (check) {
            check->unbind();
        }
    }
}

std::shared_ptr<DataValue> ChecksDataValue::cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const {
    std::vector<std::shared_ptr<CheckRuleDataValue>> clonedChecks;
    clonedChecks.reserve(_checks.size());

    for (const auto& check : _checks) {
        if (check) {
            auto clonedCheck = std::static_pointer_cast<CheckRuleDataValue>(check->cloneAsTemplate(context, rootDataPath));
            if (clonedCheck) {
                clonedChecks.emplace_back(clonedCheck);
            }
        }
    }

    return std::make_shared<ChecksDataValue>(context, clonedChecks);
}

}  // namespace agenui
