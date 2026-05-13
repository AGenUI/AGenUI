#include "textfield_component.h"
#include "../a2ui_node.h"
#include "a2ui/utils/a2ui_color_palette.h"

#include "log/a2ui_capi_log.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace a2ui {

namespace {

constexpr float kDefaultFontSize = 32.0f;
constexpr float kDefaultHeight = 100.0f;
constexpr float kDefaultErrorTextSize = 24.0f;
constexpr float kDefaultErrorMarginTop = 8.0f;
constexpr float kValidationErrorBorderWidth = 2.0f;
constexpr uint32_t kValidationErrorColor = 0xFFFF4D4F;
constexpr const char* kDefaultValidationMessage = "Invalid input format.";

static std::string extractStringValue(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }

    if (value.is_object() && value.contains("literalString") && value["literalString"].is_string()) {
        return value["literalString"].get<std::string>();
    }

    return "";
}

static float parseSizeValue(const std::string& sizeStr) {
    if (sizeStr.empty()) {
        return 0.0f;
    }

    const size_t pxPos = sizeStr.rfind("px");
    const std::string numericValue = (pxPos != std::string::npos) ? sizeStr.substr(0, pxPos) : sizeStr;
    return static_cast<float>(std::atof(numericValue.c_str()));
}

static int32_t mapInputType(const std::string& variant) {
    if (variant == "number") {
        return ARKUI_TEXTINPUT_TYPE_NUMBER;
    }

    if (variant == "obscured") {
        return ARKUI_TEXTINPUT_TYPE_PASSWORD;
    }

    return ARKUI_TEXTINPUT_TYPE_NORMAL;
}

static void disposeNodeIfNeeded(ArkUI_NodeHandle& nodeHandle) {
    if (!nodeHandle) {
        return;
    }

    g_nodeAPI->disposeNode(nodeHandle);
    nodeHandle = nullptr;
}

} // namespace

TextFieldComponent::TextFieldComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, ComponentType::kTextField) {
    m_validationMessage = kDefaultValidationMessage;

    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);
    m_textInputHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT_INPUT);
    m_errorTextHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);

    if (m_nodeHandle) {
        A2UIColumnNode(m_nodeHandle).setAlignItems(ARKUI_ITEM_ALIGNMENT_STRETCH);
    }

    if (m_textInputHandle) {
        A2UINode(m_textInputHandle).setPercentWidth(1.0f);
        getTextFiledNode().setFontSize(kDefaultFontSize);
        g_nodeAPI->addNodeEventReceiver(m_textInputHandle, onTextChangeCallback);
        g_nodeAPI->registerNodeEvent(m_textInputHandle, NODE_TEXT_INPUT_ON_CHANGE, 0, this);
        if (m_nodeHandle) {
            g_nodeAPI->addChild(m_nodeHandle, m_textInputHandle);
        }
    }

    if (m_errorTextHandle) {
        A2UINode(m_errorTextHandle).setPercentWidth(1.0f);
        A2UINode(m_errorTextHandle).setMargin(kDefaultErrorMarginTop, 0.0f, 0.0f, 0.0f);
        A2UINode(m_errorTextHandle).setVisibility(ARKUI_VISIBILITY_NONE);
        getErrorTextNode().setFontSize(kDefaultErrorTextSize);
        getErrorTextNode().setFontColor(kValidationErrorColor);
        getErrorTextNode().setTextMaxLines(2);
        getErrorTextNode().setTextOverflowEllipsis();
        if (m_nodeHandle) {
            g_nodeAPI->addChild(m_nodeHandle, m_errorTextHandle);
        }
    }

    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }
}

TextFieldComponent::~TextFieldComponent() {
    HM_LOGI("TextFieldComponent - Destroyed: id=%s", m_id.c_str());
}

void TextFieldComponent::destroy() {
    if (m_textInputHandle) {
        g_nodeAPI->unregisterNodeEvent(m_textInputHandle, NODE_TEXT_INPUT_ON_CHANGE);
        if (m_nodeHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, m_textInputHandle);
        }
        disposeNodeIfNeeded(m_textInputHandle);
    }

    if (m_errorTextHandle) {
        if (m_nodeHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, m_errorTextHandle);
        }
        disposeNodeIfNeeded(m_errorTextHandle);
    }

    A2UIComponent::destroy();
}

void TextFieldComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle || !m_textInputHandle || !m_errorTextHandle) {
        HM_LOGE("handle is null, id=%s", m_id.c_str());
        return;
    }

    if (properties.contains("placeholder") || properties.contains("label")) {
        applyPlaceholder(m_properties);
    }

    if (properties.contains("validationRegexp") || properties.contains("validationMessage")) {
        applyValidationConfig(m_properties);
    }

    if (properties.contains("value")) {
        applyValue(properties);
    }

    if (properties.contains("variant")) {
        applyVariant(properties);
    }

    if (properties.contains("styles")) {
        applyStyles(properties);
    }

    if (properties.contains("checks")) {
        applyChecks(properties);
    }

    refreshLocalValidationState(false);
    updateValidationPresentation();

    if (getHeight() <= m_borderWidth * 2) {
        setHeight(kDefaultHeight);
    }
}

void TextFieldComponent::applyPlaceholder(const nlohmann::json& properties) {
    std::string placeholder;
    if (properties.contains("placeholder")) {
        placeholder = extractStringValue(properties["placeholder"]);
    } else if (properties.contains("label")) {
        placeholder = extractStringValue(properties["label"]);
    }

    if (placeholder.empty()) {
        return;
    }

    getTextFiledNode().setPlaceholder(placeholder);
}

void TextFieldComponent::applyValue(const nlohmann::json& properties) {
    if (!properties.contains("value")) {
        return;
    }

    const std::string text = extractStringValue(properties["value"]);
    m_isUpdatingFromNative = true;
    getTextFiledNode().setTextContent(text);
    m_isUpdatingFromNative = false;
    m_currentText = text;
    m_hasUserEdited = false;

    HM_LOGI("Set text value, length=%zu", text.length());
}

void TextFieldComponent::applyVariant(const nlohmann::json& properties) {
    if (!properties.contains("variant") || !properties["variant"].is_string()) {
        return;
    }

    const int32_t inputType = mapInputType(properties["variant"].get<std::string>());
    getTextFiledNode().setInputType(static_cast<ArkUI_TextInputType>(inputType));
}

void TextFieldComponent::applyValidationConfig(const nlohmann::json& properties) {
    if (properties.contains("validationRegexp")) {
        const std::string pattern = extractStringValue(properties["validationRegexp"]);
        m_validationRegexp = pattern;
        m_hasValidationRegexp = !pattern.empty();
        m_isValidationPatternValid = false;

        if (m_hasValidationRegexp) {
            try {
                m_compiledValidationRegex = std::regex(pattern, std::regex::ECMAScript);
                m_isValidationPatternValid = true;
            } catch (const std::regex_error& error) {
                HM_LOGW("Invalid validationRegexp for id=%s: %s", m_id.c_str(), error.what());
                m_validationRegexp.clear();
                m_hasValidationRegexp = false;
            }
        }
    }

    if (properties.contains("validationMessage")) {
        const std::string message = extractStringValue(properties["validationMessage"]);
        m_validationMessage = message.empty() ? kDefaultValidationMessage : message;
    }
}

void TextFieldComponent::applyChecks(const nlohmann::json& properties) {
    m_externalCheckFailed = false;
    m_externalCheckMessage.clear();

    if (!properties.contains("checks") || !properties["checks"].is_object()) {
        return;
    }

    const auto& checks = properties["checks"];
    bool result = true;
    if (checks.contains("result")) {
        const auto& rawResult = checks["result"];
        if (rawResult.is_boolean()) {
            result = rawResult.get<bool>();
        } else if (rawResult.is_string()) {
            result = rawResult.get<std::string>() == "true";
        }
    }

    m_externalCheckFailed = !result;
    if (checks.contains("message")) {
        m_externalCheckMessage = extractStringValue(checks["message"]);
    }
}

void TextFieldComponent::applyStyles(const nlohmann::json& properties) {
    if (!properties.contains("styles") || !properties["styles"].is_object()) {
        HM_LOGI("id=%s, no styles field", m_id.c_str());
        return;
    }

    const auto& styles = properties["styles"];
    applyBorderRadius(styles);
    applyBackgroundColor(styles);
    applyBorderWidth(styles);
    applyBorderColor(styles);
}

void TextFieldComponent::applyBorderRadius(const nlohmann::json& styles) {
    if (!styles.contains("border-radius")) {
        return;
    }

    const auto& borderRadius = styles["border-radius"];
    float radius = 0.0f;

    if (borderRadius.is_number()) {
        radius = borderRadius.get<float>();
    } else if (borderRadius.is_string()) {
        radius = parseSizeValue(borderRadius.get<std::string>());
    }

    if (radius > 0.0f) {
        getTextFiledNode().setBorderRadius(radius);
        HM_LOGI("id=%s, radius=%f", m_id.c_str(), radius);
    }
    m_borderRadius = radius;
}

void TextFieldComponent::applyBackgroundColor(const nlohmann::json& styles) {
    if (!styles.contains("background-color") || !styles["background-color"].is_string()) {
        return;
    }

    const uint32_t color = A2UIComponent::parseColor(styles["background-color"].get<std::string>());
    getTextFiledNode().setBackgroundColor(color);
    HM_LOGI("id=%s, color=0x%X", m_id.c_str(), color);
}

void TextFieldComponent::applyBorderWidth(const nlohmann::json& styles) {
    if (!styles.contains("border-width")) {
        return;
    }

    const auto& borderWidth = styles["border-width"];
    float width = 0.0f;

    if (borderWidth.is_number()) {
        width = borderWidth.get<float>();
    } else if (borderWidth.is_string()) {
        width = parseSizeValue(borderWidth.get<std::string>());
    }

    if (width > 0.0f) {
        getTextFiledNode().setBorderWidth(width, width, width, width);
        getTextFiledNode().setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
    }

    m_borderWidth = width;
}

void TextFieldComponent::applyBorderColor(const nlohmann::json& styles) {
    if (!styles.contains("border-color") || !styles["border-color"].is_string()) {
        return;
    }

    const uint32_t color = A2UIComponent::parseColor(styles["border-color"].get<std::string>());
    getTextFiledNode().setBorderColor(color);
    m_borderColor = color;
    m_hasCustomBorderColor = true;
}

void TextFieldComponent::handleTextChanged(const std::string& text) {
    if (m_isUpdatingFromNative) {
        return;
    }

    m_currentText = text;
    m_hasUserEdited = true;

    refreshLocalValidationState(true);
    updateValidationPresentation();

    if (m_localValidationFailed) {
        HM_LOGI("Rejected invalid text change for id=%s, length=%zu", m_id.c_str(), text.length());
        return;
    }

    nlohmann::json changeJson;
    changeJson["value"] = text;
    syncState(changeJson);
}

void TextFieldComponent::refreshLocalValidationState(bool userInitiated) {
    if (userInitiated) {
        m_hasUserEdited = true;
    }

    if (!m_hasValidationRegexp || !m_isValidationPatternValid || !m_hasUserEdited) {
        m_localValidationFailed = false;
        return;
    }

    m_localValidationFailed = !validateText(m_currentText);
}

bool TextFieldComponent::validateText(const std::string& text) const {
    if (!m_hasValidationRegexp || !m_isValidationPatternValid) {
        return true;
    }

    return std::regex_match(text, m_compiledValidationRegex);
}

std::string TextFieldComponent::resolveValidationMessage() const {
    if (!m_validationMessage.empty()) {
        return m_validationMessage;
    }

    return kDefaultValidationMessage;
}

void TextFieldComponent::updateValidationPresentation() {
    bool showError = false;
    std::string errorMessage;

    if (m_localValidationFailed) {
        showError = true;
        errorMessage = resolveValidationMessage();
    } else if (m_externalCheckFailed) {
        showError = true;
        errorMessage = m_externalCheckMessage.empty() ? resolveValidationMessage() : m_externalCheckMessage;
    }

    if (showError) {
        getErrorTextNode().setTextContent(errorMessage);
        A2UINode(m_errorTextHandle).setVisibility(ARKUI_VISIBILITY_VISIBLE);

        const float borderWidth = std::max(m_borderWidth, kValidationErrorBorderWidth);
        getTextFiledNode().setBorderWidth(borderWidth, borderWidth, borderWidth, borderWidth);
        getTextFiledNode().setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
        getTextFiledNode().setBorderColor(kValidationErrorColor);
        return;
    }

    getErrorTextNode().setTextContent("");
    A2UINode(m_errorTextHandle).setVisibility(ARKUI_VISIBILITY_NONE);

    if (m_borderWidth > 0.0f) {
        getTextFiledNode().setBorderWidth(m_borderWidth, m_borderWidth, m_borderWidth, m_borderWidth);
        getTextFiledNode().setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
    } else {
        getTextFiledNode().resetBorderWidth();
        getTextFiledNode().resetBorderStyle();
    }

    if (m_hasCustomBorderColor) {
        getTextFiledNode().setBorderColor(m_borderColor);
    } else {
        getTextFiledNode().resetBorderColor();
    }
}

void TextFieldComponent::onTextChangeCallback(ArkUI_NodeEvent* event) {
    if (!event || OH_ArkUI_NodeEvent_GetEventType(event) != ArkUI_NodeEventType::NODE_TEXT_INPUT_ON_CHANGE) {
        return;
    }

    auto* component = static_cast<TextFieldComponent*>(OH_ArkUI_NodeEvent_GetUserData(event));
    if (!component) {
        return;
    }

    ArkUI_StringAsyncEvent* textEvent = OH_ArkUI_NodeEvent_GetStringAsyncEvent(event);
    if (!textEvent || !textEvent->pStr) {
        component->handleTextChanged("");
        return;
    }

    component->handleTextChanged(textEvent->pStr);
}

} // namespace a2ui
