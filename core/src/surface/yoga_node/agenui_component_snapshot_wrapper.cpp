#include "surface/yoga_node/agenui_component_snapshot_wrapper.h"

namespace agenui {

namespace {

inline const SerializableData* findValue(
    const std::map<std::string, SerializableData>& m, const std::string& key) {
    auto it = m.find(key);
    return (it == m.end()) ? nullptr : &it->second;
}

inline std::string asStringOrDefault(const SerializableData* v,
                                     const std::string& def) {
    if (!v || !v->isValid() || v->isNull()) return def;
    if (v->isString()) return v->asString(def);
    std::string s = v->dump();
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

}  // namespace

ComponentSnapshotWrapper::ComponentSnapshotWrapper(
    std::shared_ptr<ComponentSnapshot> snapshot)
    : _snapshot(std::move(snapshot)) {}

ComponentSnapshotWrapper::~ComponentSnapshotWrapper() = default;

const std::string& ComponentSnapshotWrapper::nodeId() const {
    return _snapshot->id;
}
const std::string& ComponentSnapshotWrapper::componentType() const {
    return _snapshot->component;
}

const std::vector<std::string>& ComponentSnapshotWrapper::childIds() const {
    return _snapshot->children;
}

std::string ComponentSnapshotWrapper::styleAsString(const std::string& key,
                                                    const std::string& def) const {
    return asStringOrDefault(findValue(_snapshot->styles, key), def);
}

void ComponentSnapshotWrapper::clearStyle(const std::string& key) {
    _snapshot->styles.erase(key);
}
void ComponentSnapshotWrapper::clearAttribute(const std::string& key) {
    _snapshot->attributes.erase(key);
}

std::string ComponentSnapshotWrapper::serializeForMeasure() const {
    return _snapshot->stringify();
}

void ComponentSnapshotWrapper::applyLayoutResult(float x, float y,
                                                 float width, float height,
                                                 int countOfLines) {
    _snapshot->layout.x = x;
    _snapshot->layout.y = y;
    _snapshot->layout.width = width;
    _snapshot->layout.height = height;
    if (countOfLines >= 0) {
        _snapshot->layout.lines = countOfLines;
    }
}

YogaValue ComponentSnapshotWrapper::getStyleValue(const std::string& key) const {
    auto it = _snapshot->styles.find(key);
    if (it == _snapshot->styles.end() || !it->second.isValid()) {
        return YogaValue();  // kNone
    }
    
    const SerializableData& data = it->second;
    if (data.isNumber()) {
        return YogaValue(static_cast<float>(data.asDouble()));
    } else if (data.isBool()) {
        return YogaValue(data.asBool());
    } else if (data.isString()) {
        return YogaValue(data.asString());
    } else {
        // Object, Array, Null and other invalid types -> return kNone
        return YogaValue();
    }
}

YogaValue ComponentSnapshotWrapper::getAttributeValue(const std::string& key) const {
    auto it = _snapshot->attributes.find(key);
    if (it == _snapshot->attributes.end() || !it->second.isValid()) {
        return YogaValue();  // kNone
    }
    
    const SerializableData& data = it->second;
    if (data.isNumber()) {
        return YogaValue(static_cast<float>(data.asDouble()));
    } else if (data.isBool()) {
        return YogaValue(data.asBool());
    } else if (data.isString()) {
        return YogaValue(data.asString());
    } else {
        // Object, Array, Null and other invalid types -> return kNone
        return YogaValue();
    }
}

}  // namespace agenui
