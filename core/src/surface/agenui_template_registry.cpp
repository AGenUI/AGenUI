#include "agenui_template_registry.h"
#include "agenui_engine_context.h"
#include "agenui_logger_internal.h"
#include "agenui_path_config.h"
#include "surface/virtual_dom/agenui_component_snapshot.h"
#include <fstream>
#include <regex>

namespace agenui {

// Regex pattern for placeholder matching: @placeholder(xxx) with optional path suffix
// Use non-greedy matching for path suffix to avoid consuming subsequent placeholders
static const std::regex PLACEHOLDER_PATTERN("@placeholder\\(([^)]+)\\)(/[^@]*)?");

TemplateRegistry::TemplateRegistry() {
}

TemplateRegistry::~TemplateRegistry() {
}

bool TemplateRegistry::initialize() {
    _registryLoaded = false;
    _templateRegistry.clear();
    return true;
}

bool TemplateRegistry::start() {
    //loadTemplateRegistry();
    return true;
}

void TemplateRegistry::stop() {
}

void TemplateRegistry::shutdown() {
    _templateRegistry.clear();
    _registryLoaded = false;
}

void TemplateRegistry::loadTemplateRegistry() {
    if (_registryLoaded) {
        return;
    }
    
    auto* ctx = getEngineContext();
    PathConfig* pathConfig = ctx ? ctx->getPathConfig() : nullptr;
    const std::string& templateDir = pathConfig ? pathConfig->getTemplateDir() : "";
    if (templateDir.empty()) {
        AGENUI_LOG("Template directory is empty");
        _registryLoaded = true;
        return;
    }
    
    std::string registryPath = templateDir + "/templates.json";
    std::ifstream file(registryPath);
    if (!file.is_open()) {
        AGENUI_LOG("Failed to open template registry: %s", registryPath.c_str());
        _registryLoaded = true;
        return;
    }
    
    nlohmann::json registryJson;
    try {
        file >> registryJson;
    } catch (const nlohmann::json::exception& e) {
        AGENUI_LOG("Failed to parse template registry: %s", e.what());
        _registryLoaded = true;
        return;
    }
    
    if (registryJson.contains("templates") && registryJson["templates"].is_array()) {
        for (const auto& templateName : registryJson["templates"]) {
            if (templateName.is_string()) {
                _templateRegistry.insert(templateName.get<std::string>());
                AGENUI_LOG("Registered template: %s", templateName.get<std::string>().c_str());
            }
        }
    }
    
    _registryLoaded = true;
    AGENUI_LOG("Loaded %zu templates", _templateRegistry.size());
}

bool TemplateRegistry::isTemplate(const std::string& componentName) {
    if (!_registryLoaded) {
        loadTemplateRegistry();
    }
    return _templateRegistry.find(componentName) != _templateRegistry.end();
}

nlohmann::json TemplateRegistry::loadTemplateFile(const std::string& templateName) {
    auto* ctx = getEngineContext();
    PathConfig* pathConfig = ctx ? ctx->getPathConfig() : nullptr;
    const std::string& templateDir = pathConfig ? pathConfig->getTemplateDir() : "";
    if (templateDir.empty()) {
        return nlohmann::json::object();
    }
    
    std::string filePath = templateDir + "/" + templateName + ".json";
    AGENUI_LOG("Loading template file: %s", filePath.c_str());
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        AGENUI_LOG("Failed to open template file: %s", filePath.c_str());
        return nlohmann::json::object();
    }
    
    nlohmann::json templateJson;
    try {
        file >> templateJson;
    } catch (const nlohmann::json::exception& e) {
        AGENUI_LOG("Failed to parse template file: %s, error: %s", filePath.c_str(), e.what());
        return nlohmann::json::object();
    }
    
    return templateJson;
}

std::map<std::string, nlohmann::json> TemplateRegistry::extractPlaceholderValues(const nlohmann::json& component) {
    std::map<std::string, nlohmann::json> values;
    
    for (auto it = component.begin(); it != component.end(); ++it) {
        const std::string& key = it.key();
        
        if (key == "id" || key == "component") {
            continue;
        }
        
        values[key] = it.value();
    }
    
    return values;
}

void TemplateRegistry::replacePlaceholders(nlohmann::json& node, const std::map<std::string, nlohmann::json>& values) {
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            if (it.value().is_string()) {
                std::string str = it.value().get<std::string>();
                std::string result = str;
                std::smatch match;
                bool hasReplacement = false;
                
                size_t searchOffset = 0;
                while (std::regex_search(str.cbegin() + searchOffset, str.cend(), match, PLACEHOLDER_PATTERN)) {
                    std::string placeholderName = match[1].str();
                    std::string pathSuffix = match[2].matched ? match[2].str() : "";
                    auto valueIt = values.find(placeholderName);
                    
                    if (valueIt != values.end()) {
                        std::string replacement;
                        if (!pathSuffix.empty() && valueIt->second.is_string()) {
                            const std::string& placeholderValue = valueIt->second.get<std::string>();
                            if (placeholderValue == "/" && !pathSuffix.empty() && pathSuffix[0] == '/') {
                                replacement = pathSuffix;
                            } else {
                                replacement = placeholderValue + pathSuffix;
                            }
                            AGENUI_LOG("Replaced placeholder '%s' with path suffix '%s'", placeholderName.c_str(), pathSuffix.c_str());
                        } else if (valueIt->second.is_string()) {
                            replacement = valueIt->second.get<std::string>();
                            AGENUI_LOG("Replaced placeholder '%s'", placeholderName.c_str());
                        } else {
                            if (!hasReplacement) {
                                it.value() = valueIt->second;
                            }
                            searchOffset += match.position(0) + match.length(0);
                            continue;
                        }
                        
                        size_t absolutePos = searchOffset + match.position(0);
                        size_t len = match.length(0);
                        result.replace(absolutePos, len, replacement);
                        
                        str = result;
                        searchOffset = absolutePos + replacement.length();
                        hasReplacement = true;
                    } else {
                        AGENUI_LOG("Placeholder '%s' not found in input data", placeholderName.c_str());
                        searchOffset += match.position(0) + match.length(0);
                    }
                }
                
                if (hasReplacement) {
                    it.value() = result;
                }
            } else {
                replacePlaceholders(it.value(), values);
            }
        }
    } else if (node.is_array()) {
        for (auto& element : node) {
            replacePlaceholders(element, values);
        }
    }
}

void TemplateRegistry::prefixComponentIds(nlohmann::json& components, const std::string& prefix, bool isNonRootNode) {
    bool isFirstComponent = true;
    
    for (auto& comp : components) {
        if (!comp.is_object()) {
            continue;
        }
        
        if (comp.contains("id") && comp["id"].is_string()) {
            const std::string& nodeId = comp["id"].get<std::string>();
            
            if (isNonRootNode && isFirstComponent) {
                comp["id"] = prefix;
                isFirstComponent = false;
            } else if (nodeId == "root" && !isNonRootNode) {
                comp["id"] = prefix;
            } else {
                comp["id"] = prefix + "_" + comp["id"].get<std::string>();
            }
        }
        
        if (comp.contains("child") && comp["child"].is_string()) {
            comp["child"] = prefix + "_" + comp["child"].get<std::string>();
        }
        
        if (comp.contains("component") && comp["component"].is_string() && comp["component"].get<std::string>() == "Tabs") {
            if (comp.contains("tabs") && comp["tabs"].is_array()) {
                for (auto& tab : comp["tabs"]) {
                    if (tab.is_object() && tab.contains("child") && tab["child"].is_string()) {
                        tab["child"] = prefix + "_" + tab["child"].get<std::string>();
                    }
                }
            }
        }
        
        if (comp.contains("children")) {
            auto& children = comp["children"];
            
            if (children.is_array()) {
                for (auto& child : children) {
                    if (child.is_string()) {
                        child = prefix + "_" + child.get<std::string>();
                    }
                }
            } else if (children.is_object()) {
                if (children.contains("componentId") && children["componentId"].is_string()) {
                    children["componentId"] = prefix + "_" + children["componentId"].get<std::string>();
                }
            }
        }
    }
}

ExpandedTemplate TemplateRegistry::expandTemplate(const nlohmann::json& component, bool isNonRootNode) {
    ExpandedTemplate result;
    
    if (!component.contains("component") || !component["component"].is_string()) {
        AGENUI_LOG("Invalid component: missing 'component' field");
        return result;
    }
    
    std::string templateName = component["component"].get<std::string>();
    
    nlohmann::json templateJson = loadTemplateFile(templateName);
    if (templateJson.empty()) {
        AGENUI_LOG("Failed to load template: %s", templateName.c_str());
        return result;
    }
    
    auto placeholderValues = extractPlaceholderValues(component);
    replacePlaceholders(templateJson, placeholderValues);
    
    if (!templateJson.contains("components") || !templateJson["components"].is_array()) {
        AGENUI_LOG("Template '%s' has no 'components' array", templateName.c_str());
        return result;
    }
    
    std::string temId;
    if (component.contains("id") && component["id"].is_string()) {
        temId = component["id"].get<std::string>();
    }
    
    if (!temId.empty()) {
        prefixComponentIds(templateJson["components"], temId, isNonRootNode);
    }
    
    const auto& components = templateJson["components"];
    result.components.reserve(components.size());
    
    for (const auto& comp : components) {
        result.components.emplace_back(comp.dump());
    }
    AGENUI_LOG("Expanding template: %s", templateName.c_str());
    
    // Set display rule: root component uses AnyDataReady after template expansion
    if (!temId.empty()) {
        result.displayRules[temId] = DisplayRule::AnyDataReady;
    }
    
    return result;
}

}  // namespace agenui
