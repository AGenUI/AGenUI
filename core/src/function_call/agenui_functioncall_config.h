#pragma once

#include <string>
#include "nlohmann/json.hpp"

namespace agenui {

/**
 * @brief FunctionCall configuration class
 *
 * Stores functionCall metadata including namespace, name, description, and return type.
 * Example FunctionCallConfig for ToastFunctionCall:
 *  {
     "namespace": "agenui.platform",
     "name": "toast",
     "description": "Show a Toast message",
     "returnType": "object",
    }
 */
class FunctionCallConfig {
public:
    FunctionCallConfig();
    ~FunctionCallConfig();
    
    /**
     * @brief Create a FunctionCallConfig from a JSON object
     * @param json JSON object
     * @return FunctionCallConfig instance
     */
    static FunctionCallConfig fromJson(const nlohmann::json& json);

    /**
     * @brief Serialize to a JSON object
     * @return JSON object
     */
    nlohmann::json toJson() const;

    /**
     * @brief Check whether the configuration is valid
     * @return true if valid, false otherwise
     */
    bool isValid() const;

    /**
     * @brief Get the fully qualified functionCall name (namespace::name)
     * @return Full name
     */
    std::string getFullName() const;

    // Getters
    const std::string& getNamespace() const { return _namespace; }
    const std::string& getName() const { return _name; }
    const std::string& getDescription() const { return _description; }
    const std::string& getReturnType() const { return _returnType; }

    // Setters
    void setNamespace(const std::string& ns) { _namespace = ns; }
    void setName(const std::string& name) { _name = name; }
    void setDescription(const std::string& desc) { _description = desc; }
    void setReturnType(const std::string& type) { _returnType = type; }

private:
    std::string _namespace;      // FunctionCall namespace
    std::string _name;           // FunctionCall name
    std::string _description;    // FunctionCall description
    std::string _returnType;     // Return type
};

} // namespace agenui
