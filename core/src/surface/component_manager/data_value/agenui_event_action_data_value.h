#pragma once

#include "agenui_data_value_base.h"
#include <map>
#include <memory>
#include <string>

namespace agenui {
class EventDispatcher;
}

namespace agenui {

class IDataModel;
class IDataChangedObserver;

/**
 * @brief Event action data value
 * @remark Represents an event action as defined by the Action.event structure in the A2UI v0.9 spec
 */
class EventActionDataValue : public DataValue {
public:
    EventActionDataValue(IDataValueContext* context, const std::string& eventName, const std::map<std::string, std::shared_ptr<DataValue>>& context_data);
    virtual ~EventActionDataValue();

    DataType getDataType() const override;
    DataBindingStatus getDataBindingStatus() const override;
    SerializableData getValueData() const override;
    void bind(IDataChangedObserver* observer) override;
    void unbind() override;
    std::shared_ptr<DataValue> cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const override;

    std::string getEventName() const;
    std::map<std::string, std::shared_ptr<DataValue>> getContextData() const;

    /**
     * @brief Execute the event action
     * @param surfaceId Surface ID
     * @param sourceComponentId ID of the component that triggered the event
     * @param dispatcher Event dispatcher
     */
    void execute(const std::string& surfaceId, const std::string& sourceComponentId, EventDispatcher* dispatcher) const;

private:
    std::string _eventName;
    std::map<std::string, std::shared_ptr<DataValue>> _contextData;
};

}  // namespace agenui