#include "agenui_functioncall_config.h"

namespace agenui {

FunctionCallConfig::FunctionCallConfig() {
}

FunctionCallConfig::~FunctionCallConfig() {
}

FunctionCallConfig FunctionCallConfig::fromJson(const nlohmann::json& json) {
    FunctionCallConfig config;
    
    if (json.contains("namespace") && json["namespace"].is_string()) {
        config._namespace = json["namespace"].get<std::string>();
    }
    
    if (json.contains("name") && json["name"].is_string()) {
        config._name = json["name"].get<std::string>();
    }
    
    if (json.contains("description") && json["description"].is_string()) {
        config._description = json["description"].get<std::string>();
    }
    
    if (json.contains("returnType") && json["returnType"].is_string()) {
        config._returnType = json["returnType"].get<std::string>();
    }
    
    return config;
}

nlohmann::json FunctionCallConfig::toJson() const {
    nlohmann::json json;
    
    if (!_namespace.empty()) {
        json["namespace"] = _namespace;
    }
    
    json["name"] = _name;
    json["description"] = _description;
    json["returnType"] = _returnType;
    
    return json;
}

bool FunctionCallConfig::isValid() const {
    if (_name.empty()) {
        return false;
    }
    
    return true;
}

std::string FunctionCallConfig::getFullName() const {
    if (_namespace.empty()) {
        return _name;
    }
    return _namespace + "::" + _name;
}

} // namespace agenui
