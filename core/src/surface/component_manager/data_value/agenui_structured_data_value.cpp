#include "agenui_structured_data_value.h"
#include "surface/agenui_serializable_data_impl.h"

namespace agenui {

StructuredDataValue::StructuredDataValue(IDataValueContext* context,
                                        const std::map<std::string, std::shared_ptr<DataValue>>& fields)
    : DataValue(context), _kind(NodeKind::Object), _fields(fields) {
}

StructuredDataValue::StructuredDataValue(IDataValueContext* context,
                                        const std::vector<std::shared_ptr<DataValue>>& elements)
    : DataValue(context), _kind(NodeKind::Array), _elements(elements) {
}

StructuredDataValue::~StructuredDataValue() {
    unbind();
}

DataType StructuredDataValue::getDataType() const {
    return DataType::StructuredData;
}

StructuredDataValue::NodeKind StructuredDataValue::getNodeKind() const {
    return _kind;
}

DataBindingStatus StructuredDataValue::getDataBindingStatus() const {
    std::vector<DataBindingStatus> statuses;

    if (_kind == NodeKind::Object) {
        for (const auto& pair : _fields) {
            if (pair.second) {
                statuses.emplace_back(pair.second->getDataBindingStatus());
            }
        }
    } else {
        for (const auto& element : _elements) {
            if (element) {
                statuses.emplace_back(element->getDataBindingStatus());
            }
        }
    }

    return aggregateBindingStatus(statuses);
}

SerializableData StructuredDataValue::getValueData() const {
    if (_kind == NodeKind::Object) {
        auto impl = SerializableData::Impl::createObject();

        for (const auto& pair : _fields) {
            if (!pair.second) {
                continue;
            }
            auto valueData = pair.second->getValueData();
            // Mirror StylesDataValue / ComponentSnapshot::stringify: an unresolved
            // field is omitted rather than emitted as null.
            if (valueData.isValid()) {
                impl->set(pair.first, valueData);
            }
        }

        return SerializableData(impl);
    }

    auto impl = SerializableData::Impl::createArray();

    for (const auto& element : _elements) {
        if (!element) {
            continue;
        }
        // Appended unconditionally: skipping an unresolved element would shift the
        // indices of every element after it. Impl::append writes JSON null for an
        // invalid value.
        impl->append(element->getValueData());
    }

    return SerializableData(impl);
}

void StructuredDataValue::bind(IDataChangedObserver* observer) {
    if (_kind == NodeKind::Object) {
        for (auto& pair : _fields) {
            if (pair.second) {
                pair.second->bind(observer);
            }
        }
        return;
    }

    for (auto& element : _elements) {
        if (element) {
            element->bind(observer);
        }
    }
}

void StructuredDataValue::unbind() {
    if (_kind == NodeKind::Object) {
        for (auto& pair : _fields) {
            if (pair.second) {
                pair.second->unbind();
            }
        }
        return;
    }

    for (auto& element : _elements) {
        if (element) {
            element->unbind();
        }
    }
}

std::shared_ptr<DataValue> StructuredDataValue::cloneAsTemplate(IDataValueContext* context,
                                                               const std::string& rootDataPath) const {
    // rootDataPath must reach every leaf: a relative binding nested anywhere in
    // this structure is only resolvable after being rewritten against the current
    // list item scope.
    if (_kind == NodeKind::Object) {
        std::map<std::string, std::shared_ptr<DataValue>> clonedFields;

        for (const auto& pair : _fields) {
            if (pair.second) {
                clonedFields[pair.first] = pair.second->cloneAsTemplate(context, rootDataPath);
            }
        }

        return std::make_shared<StructuredDataValue>(context, clonedFields);
    }

    std::vector<std::shared_ptr<DataValue>> clonedElements;
    clonedElements.reserve(_elements.size());

    for (const auto& element : _elements) {
        if (element) {
            clonedElements.emplace_back(element->cloneAsTemplate(context, rootDataPath));
        }
    }

    return std::make_shared<StructuredDataValue>(context, clonedElements);
}

}  // namespace agenui
