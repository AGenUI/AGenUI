#include "textfield_component.h"
#include "../a2ui_node.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "a2ui/utils/a2ui_padding_utils.h"  // CSS padding shorthand parser

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

/**
 * @brief Extract a string from a JSON value.
 * @param value JSON value, either a plain string or a DynamicString object.
 * @return The extracted string, or an empty string on failure.
 */
static std::string extractStringValue(const nlohmann::json& value) {
    if (value.is_string()) {
        return value.get<std::string>();
    }

    // DynamicString format: {"literalString": "..."}
    if (value.is_object() && value.contains("literalString") && value["literalString"].is_string()) {
        return value["literalString"].get<std::string>();
    }

    return "";
}

/**
 * @brief Parse a CSS-like size string.
 * @param sizeStr Size string such as "10px" or "10".
 * @return Parsed numeric value.
 */
static float parseSizeValue(const std::string& sizeStr) {
    if (sizeStr.empty()) {
        return 0.0f;
    }

    const size_t pxPos = sizeStr.rfind("px");
    const std::string numericValue = (pxPos != std::string::npos) ? sizeStr.substr(0, pxPos) : sizeStr;
    return static_cast<float>(std::atof(numericValue.c_str()));
}

/**
 * Extract the raw URL from a CSS url(...) value.
 * Supported formats:
 *   url(https://example.com/image.png)
 *   url('https://example.com/image.png')
 *   url("https://example.com/image.png")
 * Returns the original string when the value is not a url(...) expression.
 *
 * NOTE: Verbatim copy of the file-local helper in
 * platforms/harmony/agenui/src/main/cpp/a2ui/render/a2ui_component.cpp.
 * Kept identical so that fixes to that helper can be ported here directly.
 */
static std::string extractUrlFromCssUrl(const std::string& value) {
    if (value.empty()) {
        return "";
    }

    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
        start++;
    }

    if (value.compare(start, 4, "url(") != 0) {
        return value;
    }

    size_t parenStart = start + 3;
    while (parenStart < value.size() && value[parenStart] != '(') {
        parenStart++;
    }
    if (parenStart >= value.size()) {
        return value;
    }
    parenStart++;

    while (parenStart < value.size() && (value[parenStart] == ' ' || value[parenStart] == '\t')) {
        parenStart++;
    }

    size_t parenEnd = value.rfind(')');
    if (parenEnd == std::string::npos || parenEnd <= parenStart) {
        return value;
    }

    std::string inner = value.substr(parenStart, parenEnd - parenStart);

    size_t innerEnd = inner.size();
    while (innerEnd > 0 && (inner[innerEnd - 1] == ' ' || inner[innerEnd - 1] == '\t')) {
        innerEnd--;
    }
    inner = inner.substr(0, innerEnd);

    if (inner.size() >= 2) {
        if ((inner[0] == '"' && inner[inner.size() - 1] == '"') ||
            (inner[0] == '\'' && inner[inner.size() - 1] == '\'')) {
            inner = inner.substr(1, inner.size() - 2);
        }
    }

    return inner;
}

/**
 * @brief Map an input variant string to an ArkUI input type.
 * @param variant Input variant string.
 * @return ArkUI input type.
 */
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

    // Decide inner node type from the initial variant. longText needs TEXT_AREA so that
    // long input wraps onto multiple lines; everything else stays on TEXT_INPUT.
    if (!properties.is_null() && properties.is_object()
        && properties.contains("variant") && properties["variant"].is_string()
        && properties["variant"].get<std::string>() == "longText") {
        m_isTextArea = true;
    }

    // Composite layout: COLUMN(root) -> [TEXT_INPUT/TEXT_AREA, TEXT(error message)].
    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);
    m_textInputHandle = g_nodeAPI->createNode(m_isTextArea ? ARKUI_NODE_TEXT_AREA : ARKUI_NODE_TEXT_INPUT);
    m_errorTextHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);

    if (m_nodeHandle) {
        A2UIColumnNode(m_nodeHandle).setAlignItems(ARKUI_ITEM_ALIGNMENT_STRETCH);
    }

    if (m_textInputHandle) {
        A2UINode(m_textInputHandle).setPercentWidth(1.0f);
        // NODE_FONT_SIZE is shared by TEXT_INPUT and TEXT_AREA via the base text node attribute set.
        A2UITextNodeBase(m_textInputHandle).setFontSize(kDefaultFontSize);
        g_nodeAPI->addNodeEventReceiver(m_textInputHandle, onTextChangeCallback);
        // TEXT_INPUT and TEXT_AREA emit text-change events with different ids but share
        // the same StringAsyncEvent payload, so a single callback handles both.
        g_nodeAPI->registerNodeEvent(
            m_textInputHandle,
            m_isTextArea ? NODE_TEXT_AREA_ON_CHANGE : NODE_TEXT_INPUT_ON_CHANGE,
            0,
            this);
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

    // Merge initial properties.
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
        g_nodeAPI->unregisterNodeEvent(
            m_textInputHandle,
            m_isTextArea ? NODE_TEXT_AREA_ON_CHANGE : NODE_TEXT_INPUT_ON_CHANGE);
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
    // Fall back to label when placeholder is absent.
    std::string placeholder;
    if (properties.contains("placeholder")) {
        placeholder = extractStringValue(properties["placeholder"]);
    } else if (properties.contains("label")) {
        placeholder = extractStringValue(properties["label"]);
    }

    if (placeholder.empty()) {
        return;
    }

    if (m_isTextArea) {
        ArkUI_AttributeItem item = {nullptr, 0, placeholder.c_str()};
        g_nodeAPI->setAttribute(m_textInputHandle, NODE_TEXT_AREA_PLACEHOLDER, &item);
    } else {
        getTextInputNode().setPlaceholder(placeholder);
    }
}

void TextFieldComponent::applyValue(const nlohmann::json& properties) {
    if (!properties.contains("value")) {
        return;
    }

    const std::string text = extractStringValue(properties["value"]);

    // Set text content (different attribute id for TEXT_INPUT vs TEXT_AREA).
    m_isUpdatingFromNative = true;
    if (m_isTextArea) {
        ArkUI_AttributeItem item = {nullptr, 0, text.c_str()};
        g_nodeAPI->setAttribute(m_textInputHandle, NODE_TEXT_AREA_TEXT, &item);
    } else {
        getTextInputNode().setTextContent(text);
    }
    m_isUpdatingFromNative = false;
    m_currentText = text;
    m_hasUserEdited = false;

    HM_LOGI("Set text value, length=%zu", text.length());
}

void TextFieldComponent::applyVariant(const nlohmann::json& properties) {
    if (!properties.contains("variant") || !properties["variant"].is_string()) {
        return;
    }

    // Node type is fixed at construction; variant only tunes the keyboard input
    // type for TEXT_INPUT. TEXT_AREA does not expose a NODE_TEXT_INPUT_TYPE-equivalent.
    if (m_isTextArea) {
        return;
    }

    const int32_t inputType = mapInputType(properties["variant"].get<std::string>());
    getTextInputNode().setInputType(static_cast<ArkUI_TextInputType>(inputType));
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

    if (styles.contains("font-size")) {
        const auto& fs = styles["font-size"];
        float size = 0.0f;
        if (fs.is_number()) {
            size = fs.get<float>();
        } else if (fs.is_string()) {
            size = parseSizeValue(fs.get<std::string>());
        }
        if (size > 0.0f) {
            m_fontSize = size;
            A2UITextNodeBase(m_nodeHandle).setFontSize(size);
        }
    }

    applyBorderRadius(styles);
    applyBackgroundColor(styles);
    applyBackgroundImage(styles);
    applyBorderWidth(styles);
    applyBorderColor(styles);

    // Translate CSS padding into ArkUI insets.
    {
        float pt = 0.0f, pr = 0.0f, pb = 0.0f, pl = 0.0f;
        ::a2ui::padding_utils::resolveUserPadding(styles, pt, pr, pb, pl);
        //compute an extra top inset to keep the single line visually centered.
        if (m_isTextArea) {
            const float height = getHeight();
            const float lineHeight = m_fontSize > 0.0f ? m_fontSize : kDefaultFontSize;
            const float centerInset = std::max(0.0f, (height - pt - pb - lineHeight) / 2.0f);
            pt += centerInset;
        }

        A2UINode base(m_nodeHandle);
        if (::a2ui::padding_utils::hasAnyPadding(pt, pr, pb, pl)) {
            base.setPadding(pt, pr, pb, pl);
        } else {
            base.resetPadding();
        }
    }
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
        A2UINode(m_textInputHandle).setBorderRadius(radius);
        HM_LOGI("id=%s, radius=%f", m_id.c_str(), radius);
    }
    m_borderRadius = radius;
}

void TextFieldComponent::applyBackgroundColor(const nlohmann::json& styles) {
    if (!styles.contains("background-color") || !styles["background-color"].is_string()) {
        return;
    }

    const uint32_t color = A2UIComponent::parseColor(styles["background-color"].get<std::string>());
    A2UINode(m_textInputHandle).setBackgroundColor(color);
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
        A2UINode(m_textInputHandle).setBorderWidth(width, width, width, width);
        A2UINode(m_textInputHandle).setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
    }

    m_borderWidth = width;
}

void TextFieldComponent::applyBorderColor(const nlohmann::json& styles) {
    if (!styles.contains("border-color") || !styles["border-color"].is_string()) {
        return;
    }

    const uint32_t color = A2UIComponent::parseColor(styles["border-color"].get<std::string>());
    A2UINode(m_textInputHandle).setBorderColor(color);
    m_borderColor = color;
    m_hasCustomBorderColor = true;
}

/**
 * @brief Apply the background image on the leaf TextInput / TextArea node.
 *        TEXT_INPUT / TEXT_AREA do not support addChild, so the base class's
 *        "child Image node" approach does not work. We use the leaf-friendly
 *        NODE_BACKGROUND_IMAGE attribute instead.
 * @param styles Style JSON object.
 */
void TextFieldComponent::applyBackgroundImage(const nlohmann::json& styles) {
    if (!styles.contains("background-image") || !styles["background-image"].is_string()) {
        return;
    }

    std::string url = extractUrlFromCssUrl(styles["background-image"].get<std::string>());
    if (url.empty()) {
        return;
    }

    ArkUI_AttributeItem item = {nullptr, 0, url.c_str()};
    int32_t ret = g_nodeAPI->setAttribute(m_textInputHandle, NODE_BACKGROUND_IMAGE, &item);

    // Make the background image cover the full node area.
    // Default behaviour pins the original-size image at top-left, so we explicitly
    // set the size mode to COVER (preserve aspect ratio, may crop edges).
    ArkUI_NumberValue sizeValue[] = {{.i32 = ARKUI_IMAGE_SIZE_COVER}};
    ArkUI_AttributeItem sizeItem  = {sizeValue, 1};
    int32_t sizeRet = g_nodeAPI->setAttribute(
        m_textInputHandle, NODE_BACKGROUND_IMAGE_SIZE_WITH_STYLE, &sizeItem);

    HM_LOGI("TextField applyBackgroundImage: id=%s, url=%s, ret=%d, sizeRet=%d",
            m_id.c_str(), url.c_str(), ret, sizeRet);
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
        A2UINode(m_textInputHandle).setBorderWidth(borderWidth, borderWidth, borderWidth, borderWidth);
        A2UINode(m_textInputHandle).setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
        A2UINode(m_textInputHandle).setBorderColor(kValidationErrorColor);
        return;
    }

    getErrorTextNode().setTextContent("");
    A2UINode(m_errorTextHandle).setVisibility(ARKUI_VISIBILITY_NONE);

    if (m_borderWidth > 0.0f) {
        A2UINode(m_textInputHandle).setBorderWidth(m_borderWidth, m_borderWidth, m_borderWidth, m_borderWidth);
        A2UINode(m_textInputHandle).setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
    } else {
        A2UINode(m_textInputHandle).resetBorderWidth();
        A2UINode(m_textInputHandle).resetBorderStyle();
    }

    if (m_hasCustomBorderColor) {
        A2UINode(m_textInputHandle).setBorderColor(m_borderColor);
    } else {
        A2UINode(m_textInputHandle).resetBorderColor();
    }
}

void TextFieldComponent::onTextChangeCallback(ArkUI_NodeEvent* event) {
    if (!event) {
        return;
    }

    const auto eventType = OH_ArkUI_NodeEvent_GetEventType(event);
    if (eventType != ArkUI_NodeEventType::NODE_TEXT_INPUT_ON_CHANGE
        && eventType != ArkUI_NodeEventType::NODE_TEXT_AREA_ON_CHANGE) {
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
