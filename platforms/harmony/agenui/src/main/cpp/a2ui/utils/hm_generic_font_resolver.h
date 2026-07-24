#pragma once

#include <string>
#include <unordered_map>

#include <native_drawing/drawing_text_typography.h>

#include "a2ui/utils/a2ui_string_utils.h"

namespace a2ui {

/// Queries the system font configuration at runtime (API 12+) to resolve CSS
/// generic family keywords (sans-serif, serif, monospace) to actual system
/// font family names. Falls back to hardcoded defaults if the API call fails.
///
/// Data source: OH_Drawing_GetSystemFontConfigInfo() returns
/// OH_Drawing_FontGenericInfo[] where each entry's familyName is the generic
/// group key (e.g. "sans-serif") and aliasInfoSet[0].familyName is the
/// concrete font name the system maps it to.
class HmGenericFontResolver {
public:
    static HmGenericFontResolver& instance() {
        static HmGenericFontResolver inst;
        return inst;
    }

    /// Returns the system font family name for a CSS generic keyword (lowercase).
    /// Returns empty string if the keyword is not a recognized generic family.
    std::string resolve(const std::string& lower) const {
        auto it = map_.find(lower);
        if (it != map_.end()) {
            return it->second;
        }
        return {};
    }

    const std::string& defaultFamily() const { return default_; }

private:
    HmGenericFontResolver() { init(); }
    HmGenericFontResolver(const HmGenericFontResolver&) = delete;
    HmGenericFontResolver& operator=(const HmGenericFontResolver&) = delete;

    void init() {
        OH_Drawing_FontConfigInfoErrorCode err = SUCCESS_FONT_CONFIG_INFO;
        OH_Drawing_FontConfigInfo* cfg = OH_Drawing_GetSystemFontConfigInfo(&err);

        if (err == SUCCESS_FONT_CONFIG_INFO && cfg != nullptr) {
            buildFromConfig(cfg);
            OH_Drawing_DestroySystemFontConfigInfo(cfg);
        } else {
            useDefaults();
        }

        if (map_.find("system") == map_.end()) {
            map_["system"] = default_;
        }
    }

    void buildFromConfig(const OH_Drawing_FontConfigInfo* cfg) {
        for (size_t i = 0; i < cfg->fontGenericInfoSize; ++i) {
            const auto& info = cfg->fontGenericInfoSet[i];
            if (!info.familyName) continue;

            std::string groupKey = toLowerAscii(info.familyName);

            // The first alias entry is the concrete font for this generic group.
            std::string concrete;
            if (info.aliasInfoSize > 0 && info.aliasInfoSet[0].familyName) {
                concrete = info.aliasInfoSet[0].familyName;
            } else {
                // If no aliases, the group key itself may be directly usable.
                concrete = info.familyName;
            }

            if (groupKey == "sans-serif") {
                map_["sans-serif"] = concrete;
                map_["system"] = concrete;
                default_ = concrete;
            } else if (groupKey == "serif") {
                map_["serif"] = concrete;
            } else if (groupKey == "monospace") {
                map_["monospace"] = concrete;
            }
        }

        if (default_.empty()) {
            default_ = "HarmonyOS Sans";
        }
    }

    void useDefaults() {
        // "HarmonyOS Sans" is the only officially documented default system font.
        // For serif/monospace, pass the generic keyword directly — ArkUI natively
        // supports "sans-serif", "serif", "monospace" as font-family values.
        default_ = "HarmonyOS Sans";
        map_["sans-serif"] = "HarmonyOS Sans";
        map_["system"] = "HarmonyOS Sans";
        map_["monospace"] = "monospace";
        map_["serif"] = "serif";
    }

    std::string default_;
    std::unordered_map<std::string, std::string> map_;
};

} // namespace a2ui
