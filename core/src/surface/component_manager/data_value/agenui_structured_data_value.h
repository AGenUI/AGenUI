#pragma once

#include "agenui_data_value_base.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace agenui {

class IDataChangedObserver;

/**
 * @brief Container data value that preserves a JSON object/array skeleton whose
 *        leaves are themselves DataValues
 * @remark Lets data bindings and function calls nested inside an otherwise opaque
 *         structure take part in binding, resolution and template cloning. Nested
 *         objects/arrays are represented by child StructuredDataValue instances,
 *         so recursion falls out of the DataValue polymorphism.
 */
class StructuredDataValue : public DataValue {
public:
    /// Which JSON container this node represents. Serves as the type tag that
    /// replaces RTTI (see agent-context/rules/cpp-no-rtti.md).
    enum class NodeKind {
        Object,
        Array
    };

    StructuredDataValue(IDataValueContext* context,
                        const std::map<std::string, std::shared_ptr<DataValue>>& fields);
    StructuredDataValue(IDataValueContext* context,
                        const std::vector<std::shared_ptr<DataValue>>& elements);
    ~StructuredDataValue() override;

    DataType getDataType() const override;
    DataBindingStatus getDataBindingStatus() const override;
    SerializableData getValueData() const override;
    void bind(IDataChangedObserver* observer) override;
    void unbind() override;
    std::shared_ptr<DataValue> cloneAsTemplate(IDataValueContext* context,
                                               const std::string& rootDataPath) const override;

    NodeKind getNodeKind() const;

private:
    NodeKind _kind;
    std::map<std::string, std::shared_ptr<DataValue>> _fields;  // Populated when _kind == Object
    std::vector<std::shared_ptr<DataValue>> _elements;          // Populated when _kind == Array
};

}  // namespace agenui
