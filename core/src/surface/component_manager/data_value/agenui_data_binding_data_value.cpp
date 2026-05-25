#include "agenui_data_binding_data_value.h"
#include "agenui_idata_value_context.h"
#include "surface/datamodel/agenui_idata_model.h"
#include "surface/datamodel/agenui_data_observer.h"
#include "surface/agenui_serializable_data_impl.h"


namespace agenui {

// Helper: normalize a path, resolving '.' and '..' segments
static std::string normalizePath(const std::string& path);

DataBindingDataValue::DataBindingDataValue(IDataValueContext* context, const std::string& bindingPath) : DataValue(context), _bindingPath(bindingPath), _observer(nullptr) {
}

DataBindingDataValue::~DataBindingDataValue() {
    unbind();
}

DataType DataBindingDataValue::getDataType() const {
    return DataType::DataBindingData;
}

DataBindingStatus DataBindingDataValue::getDataBindingStatus() const {
    if (!_context) {
        return DataBindingStatus::NotReady;
    }
    auto value = _context->getDataModel()->getValue(_bindingPath);
    return !value.isValid() ? DataBindingStatus::NotReady : DataBindingStatus::FullyReady;
}

SerializableData DataBindingDataValue::getValueData() const {
    if (!_context) {
        return SerializableData();
    }

    return _context->getDataModel()->getValue(_bindingPath);
}

void DataBindingDataValue::setContext(IDataValueContext* context) {
    if (_context == context) {
        return;
    }

    unbind();
    _context = context;
    bind(_observer);
}

std::string DataBindingDataValue::getBindingPath() const {
    return _bindingPath;
}

void DataBindingDataValue::bind(IDataChangedObserver* observer) {
    unbind();

    _observer = observer;

    if (_observer != nullptr && _context != nullptr) {
        _context->getDataModel()->bind(_bindingPath, _observer);
    }
}

void DataBindingDataValue::unbind() {
    if (_observer == nullptr) {
        return;
    }

    if (_context != nullptr) {
        _context->getDataModel()->unbind(_bindingPath, _observer);
    }

    _observer = nullptr;
}

void DataBindingDataValue::syncBindingValue(const std::string& value) {
    if (!_context) {
        return;
    }
    _context->getDataModel()->syncBindingValue(_bindingPath, value);
}

std::shared_ptr<DataValue> DataBindingDataValue::cloneAsTemplate(IDataValueContext* context, const std::string& rootDataPath) const {
    std::string newPath = _bindingPath;

    // Relative paths (not starting with '/') are resolved against rootDataPath
    if (!_bindingPath.empty() && _bindingPath[0] != '/') {
        if (!rootDataPath.empty()) {
            newPath = normalizePath(rootDataPath + "/" + _bindingPath);
        }
    }

    auto cloned = std::make_shared<DataBindingDataValue>(context, newPath);

    // If the original was already bound, bind the clone to the same observer
    if (_observer != nullptr) {
        cloned->bind(_observer);
    }

    return cloned;
}

// Helper implementation: normalize a path, resolving '.' and '..' segments
static std::string normalizePath(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;

    bool isAbsolute = (path[0] == '/');

    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == ".") {
            continue;
        }

        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else {
            parts.emplace_back(part);
        }
    }

    std::string result;
    if (isAbsolute) {
        result = "/";
    }

    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result += "/";
        }
        result += parts[i];
    }

    if (isAbsolute && result.empty()) {
        return "/";
    }

    return result;
}

}  // namespace agenui
