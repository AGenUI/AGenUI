#include "agenui_function_call_data_value.h"
#include "agenui_idata_value_context.h"
#include "surface/agenui_serializable_data_impl.h"
#include "agenui_platform_function.h"
#include "function_call/agenui_functioncall_manager.h"
#include "function_call/agenui_functioncall_resolution.h"
#include "agenui_engine_context.h"
#include "agenui_logger_internal.h"

namespace agenui {

FunctionCallDataValue::FunctionCallDataValue(IDataValueContext* context, const std::string& functionName, const std::map<std::string, std::shared_ptr<DataValue>>& args) : DataValue(context), _functionName(functionName), _args(args) {
}

DataType FunctionCallDataValue::getDataType() const {
    return DataType::FunctionCallData;
}

DataBindingStatus FunctionCallDataValue::getDataBindingStatus() const {
    std::vector<DataBindingStatus> statuses;
    for (const auto& pair : _args) {
        if (pair.second) {
            statuses.emplace_back(pair.second->getDataBindingStatus());
        }
    }
    return aggregateBindingStatus(statuses);
}

SerializableData FunctionCallDataValue::getValueData() const {
    // Each arg has already been resolved recursively at parse time: nested
    // bindings / function calls are stored as their own DataValue and produce
    // a fully-resolved SerializableData via getValueData(). We just need to
    // collect them into a JSON object and invoke the platform function call
    // directly via FunctionCallManager.
    nlohmann::json jsonArgs = nlohmann::json::object();
    for (const auto& pair : _args) {
        if (!pair.second) {
            continue;
        }

        auto valueData = pair.second->getValueData();
        if (!valueData.isValid()) {
            continue;
        }

        const auto& impl = valueData.getImpl();
        if (!impl || impl->node == nullptr) {
            continue;
        }
        jsonArgs[pair.first] = *(impl->node);
    }

    auto* engineContext = getEngineContext();
    if (engineContext == nullptr) {
        AGENUI_LOG("error: EngineContext is null, name=%s", _functionName.c_str());
        return SerializableData();
    }

    auto* functionCallManager = engineContext->getFunctionCallManager();
    if (functionCallManager == nullptr) {
        AGENUI_LOG("error: FunctionCallManager is null, name=%s", _functionName.c_str());
        return SerializableData();
    }

    FunctionCallContext fcContext{
        _context ? _context->getInstanceId() : 0,
        _context ? _context->getSurfaceId() : ""
    };

    AGENUI_LOG("calling FunctionCallManager.executeFunctionCallSync: name=%s, args=%s",
               _functionName.c_str(), jsonArgs.dump().c_str());
    FunctionCallResolution resolution = functionCallManager->executeFunctionCallSync(
        _functionName, fcContext, jsonArgs);

    if (resolution.getStatus() != FunctionCallStatus::Success) {
        AGENUI_LOG("error: name=%s, error=%s", _functionName.c_str(), resolution.getError().c_str());
        return SerializableData();
    }

    return SerializableData(SerializableData::Impl::create(resolution.getValue()));
}

std::string FunctionCallDataValue::getFunctionName() const {
    return _functionName;
}

std::map<std::string, std::shared_ptr<DataValue>> FunctionCallDataValue::getArgs() const {
    return _args;
}

void FunctionCallDataValue::bind(IDataChangedObserver* observer) {
    for (auto& pair : _args) {
        if (pair.second) {
            pair.second->bind(observer);
        }
    }
}

void FunctionCallDataValue::unbind() {
    for (auto& pair : _args) {
        if (pair.second) {
            pair.second->unbind();
        }
    }
}

std::shared_ptr<DataValue> FunctionCallDataValue::cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const {
    // Recursively clone all arguments
    std::map<std::string, std::shared_ptr<DataValue>> clonedArgs;
    for (const auto& pair : _args) {
        if (pair.second) {
            clonedArgs[pair.first] = pair.second->cloneAsTemplate(context, rootDataPath);
        }
    }

    return std::make_shared<FunctionCallDataValue>(context, _functionName, clonedArgs);
}

}  // namespace agenui
