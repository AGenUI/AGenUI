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

inline double asNumberOrDefault(const SerializableData* v, double def) {
    if (!v || !v->isValid() || v->isNull()) return def;
    return v->asDouble(def);
}

inline bool asBoolOrDefault(const SerializableData* v, bool def) {
    if (!v || !v->isValid() || v->isNull()) return def;
    return v->asBool(def);
}

inline bool dispatchVisit(ILayoutValueVisitor& visitor,
                          const std::string& key,
                          const SerializableData& v) {
    if (!v.isValid() || v.isNull()) return visitor.onNull(key);
    if (v.isString())               return visitor.onString(key, v.asString());
    if (v.isNumber())               return visitor.onNumber(key, v.asDouble());
    if (v.isBool())                 return visitor.onBool(key, v.asBool());
    return visitor.onString(key, v.dump());
}

}  // namespace

ComponentSnapshotWrapper::ComponentSnapshotWrapper(
    std::shared_ptr<ComponentSnapshot> snapshot)
    : _snapshot(std::move(snapshot)) {}

ComponentSnapshotWrapper::~ComponentSnapshotWrapper() = default;

const std::string& ComponentSnapshotWrapper::nodeId() const {
    return _snapshot->id;
}
const std::string& ComponentSnapshotWrapper::rawId() const {
    return _snapshot->rawId;
}
const std::string& ComponentSnapshotWrapper::componentType() const {
    return _snapshot->component;
}

const std::vector<std::string>& ComponentSnapshotWrapper::childIds() const {
    return _snapshot->children;
}
bool ComponentSnapshotWrapper::appendMode() const {
    return _snapshot->appendMode;
}

bool ComponentSnapshotWrapper::hasStyle(const std::string& key) const {
    return _snapshot->styles.find(key) != _snapshot->styles.end();
}
bool ComponentSnapshotWrapper::hasAttribute(const std::string& key) const {
    return _snapshot->attributes.find(key) != _snapshot->attributes.end();
}

std::string ComponentSnapshotWrapper::styleAsString(const std::string& key,
                                                    const std::string& def) const {
    return asStringOrDefault(findValue(_snapshot->styles, key), def);
}
double ComponentSnapshotWrapper::styleAsNumber(const std::string& key, double def) const {
    return asNumberOrDefault(findValue(_snapshot->styles, key), def);
}
bool ComponentSnapshotWrapper::styleAsBool(const std::string& key, bool def) const {
    return asBoolOrDefault(findValue(_snapshot->styles, key), def);
}

std::string ComponentSnapshotWrapper::attributeAsString(const std::string& key,
                                                        const std::string& def) const {
    return asStringOrDefault(findValue(_snapshot->attributes, key), def);
}
double ComponentSnapshotWrapper::attributeAsNumber(const std::string& key, double def) const {
    return asNumberOrDefault(findValue(_snapshot->attributes, key), def);
}
bool ComponentSnapshotWrapper::attributeAsBool(const std::string& key, bool def) const {
    return asBoolOrDefault(findValue(_snapshot->attributes, key), def);
}

void ComponentSnapshotWrapper::forEachStyle(ILayoutValueVisitor& visitor) const {
    for (const auto& kv : _snapshot->styles) {
        if (!dispatchVisit(visitor, kv.first, kv.second)) return;
    }
}
void ComponentSnapshotWrapper::forEachAttribute(ILayoutValueVisitor& visitor) const {
    for (const auto& kv : _snapshot->attributes) {
        if (!dispatchVisit(visitor, kv.first, kv.second)) return;
    }
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

}  // namespace agenui
