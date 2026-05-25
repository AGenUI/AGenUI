#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace a2ui {

struct ChoicePickerConfig {
    std::string variant = "mutuallyExclusive";
    std::string orientation = "vertical";
    std::string displayStyle = "checkbox";
    bool filterable = false;
};

struct ChoicePickerOptionData {
    std::string label;
    std::string value;
};

inline std::string extractChoicePickerStringValue(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }

    if (value.is_object() && value.contains("literalString") && value["literalString"].is_string()) {
        return value["literalString"].get<std::string>();
    }

    return "";
}

inline bool extractChoicePickerBooleanValue(const nlohmann::json& value, bool fallbackValue = false) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }

    if (value.is_string()) {
        return value.get<std::string>() == "true";
    }

    if (value.is_object() && value.contains("literalBoolean")) {
        const auto& literalBoolean = value["literalBoolean"];
        if (literalBoolean.is_boolean()) {
            return literalBoolean.get<bool>();
        }
        if (literalBoolean.is_string()) {
            return literalBoolean.get<std::string>() == "true";
        }
    }

    return fallbackValue;
}

inline std::string toChoicePickerLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::string trimChoicePickerString(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }

    return value.substr(start, end - start);
}

inline ChoicePickerConfig parseChoicePickerConfig(const nlohmann::json& properties) {
    ChoicePickerConfig config;

    if (properties.contains("variant") && properties["variant"].is_string()) {
        config.variant = properties["variant"].get<std::string>();
    }

    if (properties.contains("displayStyle") && properties["displayStyle"].is_string()) {
        const std::string displayStyle = properties["displayStyle"].get<std::string>();
        if (displayStyle == "chips") {
            config.displayStyle = displayStyle;
        }
    }

    if (properties.contains("filterable")) {
        config.filterable = extractChoicePickerBooleanValue(properties["filterable"]);
    }

    if (properties.contains("styles") && properties["styles"].is_object()) {
        const auto& styles = properties["styles"];
        if (styles.contains("orientation") && styles["orientation"].is_string()) {
            config.orientation = styles["orientation"].get<std::string>();
        }
    }

    return config;
}

inline std::vector<ChoicePickerOptionData> parseChoicePickerOptions(const nlohmann::json& properties) {
    std::vector<ChoicePickerOptionData> options;
    if (!properties.contains("options") || !properties["options"].is_array()) {
        return options;
    }

    for (const auto& item : properties["options"]) {
        if (!item.is_object()) {
            continue;
        }

        ChoicePickerOptionData option;
        if (item.contains("label")) {
            option.label = extractChoicePickerStringValue(item["label"]);
        }
        if (item.contains("value")) {
            option.value = extractChoicePickerStringValue(item["value"]);
        }
        if (option.label.empty()) {
            option.label = option.value;
        }
        options.push_back(option);
    }

    return options;
}

inline bool isChoicePickerValueSelected(const nlohmann::json& valueField, const std::string& optionValue) {
    if (valueField.is_string()) {
        return valueField.get<std::string>() == optionValue;
    }

    if (valueField.is_array()) {
        for (const auto& item : valueField) {
            if (extractChoicePickerStringValue(item) == optionValue) {
                return true;
            }
        }
    }

    return false;
}

inline std::vector<size_t> filterChoicePickerOptionIndices(const std::vector<ChoicePickerOptionData>& options,
                                                           const std::string& keyword) {
    std::vector<size_t> indices;
    const std::string normalizedKeyword = toChoicePickerLower(trimChoicePickerString(keyword));

    for (size_t index = 0; index < options.size(); ++index) {
        if (normalizedKeyword.empty()) {
            indices.push_back(index);
            continue;
        }

        const std::string label = toChoicePickerLower(options[index].label);
        const std::string value = toChoicePickerLower(options[index].value);
        if (label.find(normalizedKeyword) != std::string::npos ||
            value.find(normalizedKeyword) != std::string::npos) {
            indices.push_back(index);
        }
    }

    return indices;
}

inline nlohmann::json updateChoicePickerValue(const nlohmann::json& currentValue,
                                              const ChoicePickerConfig& config,
                                              const std::string& optionValue,
                                              bool selected) {
    if (config.variant == "multipleSelection") {
        std::vector<std::string> values;
        if (currentValue.is_array()) {
            for (const auto& item : currentValue) {
                const std::string value = extractChoicePickerStringValue(item);
                if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
                    values.push_back(value);
                }
            }
        } else if (currentValue.is_string()) {
            const std::string value = currentValue.get<std::string>();
            if (!value.empty()) {
                values.push_back(value);
            }
        }

        auto existing = std::find(values.begin(), values.end(), optionValue);
        if (selected) {
            if (existing == values.end()) {
                values.push_back(optionValue);
            }
        } else if (existing != values.end()) {
            values.erase(existing);
        }

        return nlohmann::json(values);
    }

    if (selected) {
        return nlohmann::json(optionValue);
    }

    if (currentValue.is_string() && currentValue.get<std::string>() != optionValue) {
        return currentValue;
    }

    return nlohmann::json("");
}

}  // namespace a2ui
