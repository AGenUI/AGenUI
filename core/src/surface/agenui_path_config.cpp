#include "agenui_path_config.h"
#include "agenui_logger_internal.h"
#include "nlohmann/json.hpp"

namespace agenui {

bool PathConfig::setPathConfig(const std::string& configJson) {
    nlohmann::json config = nlohmann::json::parse(configJson, nullptr, false);
    if (config.is_discarded()) {
        AGENUI_LOG("setPathConfig: invalid JSON");
        return false;
    }

    if (config.contains("templateDir") && config["templateDir"].is_string()) {
        _templateDir = config["templateDir"].get<std::string>();
        AGENUI_LOG("setPathConfig: templateDir=%s", _templateDir.c_str());
    }

    return true;
}

} // namespace agenui
