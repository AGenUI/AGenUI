#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

// Include nlohmann json library
#include "nlohmann/json.hpp"

namespace agenui {

enum class DisplayRule;

// Template expansion result
struct ExpandedTemplate {
    std::vector<std::string> components;              // List of expanded component JSON strings
    std::map<std::string, DisplayRule> displayRules;  // Display rules per component, keyed by componentId
};

// Template registry for managing component templates
class TemplateRegistry {
public:
    TemplateRegistry();
    ~TemplateRegistry();

    // Lifecycle management
    bool initialize();
    bool start();
    void stop();
    void shutdown();

    // Determine if a component is a template
    bool isTemplate(const std::string& componentName);
    
    // Expand template into a list of component JSON strings
    ExpandedTemplate expandTemplate(const nlohmann::json& component, bool isNonRootNode = false);

private:
    // Load template registry from templates.json
    void loadTemplateRegistry();
    
    // Load template file content
    nlohmann::json loadTemplateFile(const std::string& templateName);
    
    // Extract placeholder values from input component
    std::map<std::string, nlohmann::json> extractPlaceholderValues(const nlohmann::json& component);
    
    // Recursively replace placeholders in template JSON
    void replacePlaceholders(nlohmann::json& node, const std::map<std::string, nlohmann::json>& values);
    
    // Prefix all component ids and id references to avoid duplicates
    void prefixComponentIds(nlohmann::json& components, const std::string& prefix, bool isNonRootNode = false);
    
private:
    std::set<std::string> _templateRegistry;
    bool _registryLoaded = false;
};

}  // namespace agenui
