#pragma once

#include <string>

namespace agenui {

/**
 * @brief Path configuration manager
 *
 * Parses and stores path configuration from JSON string.
 * Currently supports:
 *   - "templateDir": absolute path to the template directory
 *
 * Extensible: new path keys can be added without changing the interface.
 */
class PathConfig {
public:
    PathConfig() = default;
    ~PathConfig() = default;

    /**
     * @brief Parse and store path configuration from JSON string
     * @param configJson JSON string, e.g. {"templateDir": "/path/to/templates"}
     * @return true if parsed successfully, false if JSON is invalid
     */
    bool setPathConfig(const std::string& configJson);

    /**
     * @brief Get the template directory path
     * @return Template directory path, empty string if not set
     */
    const std::string& getTemplateDir() const { return _templateDir; }

private:
    std::string _templateDir;
};

} // namespace agenui
