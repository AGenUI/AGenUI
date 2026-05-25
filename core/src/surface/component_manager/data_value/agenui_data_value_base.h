#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "surface/agenui_serializable_data.h"
#include "surface/virtual_dom/agenui_component_snapshot.h"
#include "agenui_idata_value_context.h"

namespace agenui {

class EventDispatcher;
class IDataModel;
class IDataChangedObserver;

/**
 * @brief Data type enum
 */
enum class DataType {
    StaticData,                    // Static data
    DataBindingData,               // Data binding data
    FunctionCallData,              // Function call data
    InterpolationExpressionData,   // Composite of multiple interpolation expression segments
    CheckRuleData,                 // Single check rule data
    ChecksData,                    // Conditional check data
    StylesData,                    // Style data
    TabsData,                      // Tabs data
    EventActionData,               // Event action data
    FunctionCallActionData         // Function-call action data
};

/**
 * @brief Data value interface
 * @remark Represents the data value of a component attribute; supports static, data-binding, and parseable values
 */
class DataValue {
public:
    virtual ~DataValue() = default;

    /**
     * @brief Get the data type
     * @return Data type
     */
    virtual DataType getDataType() const = 0;

    /**
     * @brief Get the data binding status
     * @return Data binding status
     * @remark Indicates whether the binding path can be resolved in the DataModel
     */
    virtual DataBindingStatus getDataBindingStatus() const = 0;

    /**
     * @brief Aggregate multiple data binding statuses
     * @param statuses Array of data binding statuses
     * @return Aggregated data binding status
     */
    static DataBindingStatus aggregateBindingStatus(const std::vector<DataBindingStatus>& statuses);

    /**
     * @brief Get the data value
     * @return Serializable data representation
     */
    virtual SerializableData getValueData() const = 0;

    /**
     * @brief Bind an observer
     * @param observer Observer pointer
     * @remark Registers the observer on the data path; it will be notified when the data changes
     */
    virtual void bind(IDataChangedObserver* observer) = 0;

    /**
     * @brief Unbind the observer
     */
    virtual void unbind() = 0;


    /**
     * @brief Clone the data value for template generation
     * @param rootDataPath Root data path used for relative path conversion
     * @return Cloned data value
     * @remark Used when generating components from templates; supports path substitution
     */
    virtual std::shared_ptr<DataValue> cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const = 0;

protected:
    /**
     * @brief Constructor
     * @param context Data value context pointer
     */
    explicit DataValue(IDataValueContext* context);

    DataValue();

    IDataValueContext* _context;                     // Data value context pointer
};

}  // namespace agenui
