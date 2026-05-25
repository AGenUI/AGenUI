#pragma once

#include "agenui_data_value_base.h"
#include <string>
#include <map>
#include <memory>

namespace agenui {

class IDataModel;
class IDataChangedObserver;

/**
 * @brief Function-call data value
 * @remark Represents an invocation of a registered function call (built-in or
 *         platform-registered), holding the function name together with its
 *         resolved argument map. Evaluating this DataValue triggers the
 *         function via FunctionCallManager and yields the call's return
 *         value as a SerializableData.
 */
class FunctionCallDataValue : public DataValue {
public:
    FunctionCallDataValue(IDataValueContext* context, const std::string& functionName, const std::map<std::string, std::shared_ptr<DataValue>>& args);

    DataType getDataType() const override;
    DataBindingStatus getDataBindingStatus() const override;
    SerializableData getValueData() const override;
    void bind(IDataChangedObserver* observer) override;
    void unbind() override;
    std::shared_ptr<DataValue> cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const override;

    std::string getFunctionName() const;
    std::map<std::string, std::shared_ptr<DataValue>> getArgs() const;

private:
    std::string _functionName;
    std::map<std::string, std::shared_ptr<DataValue>> _args;
};

}  // namespace agenui
