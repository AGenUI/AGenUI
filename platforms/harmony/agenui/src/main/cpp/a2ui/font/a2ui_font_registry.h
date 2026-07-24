#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "a2ui/utils/a2ui_string_utils.h"

namespace a2ui {

class FontRegistry {
public:
    static FontRegistry& instance() {
        static FontRegistry s;
        return s;
    }

    bool registerFont(const std::string& familyName, const std::string& resolvedName) {
        std::lock_guard<std::mutex> lk(mutex_);
        std::string key = toLowerAscii(familyName);
        bool isNew = registry_.find(key) == registry_.end();
        registry_[key] = resolvedName;
        return isNew;
    }

    std::string resolve(const std::string& familyName) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = registry_.find(toLowerAscii(familyName));
        return it != registry_.end() ? it->second : std::string();
    }

private:
    FontRegistry() = default;
    FontRegistry(const FontRegistry&) = delete;
    FontRegistry& operator=(const FontRegistry&) = delete;

    std::unordered_map<std::string, std::string> registry_;
    mutable std::mutex mutex_;
};

} // namespace a2ui
