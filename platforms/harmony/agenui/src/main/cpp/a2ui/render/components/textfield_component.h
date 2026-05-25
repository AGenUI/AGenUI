#pragma once

#include "../a2ui_component.h"

#include <regex>

namespace a2ui {

/**
 * Text input component backed by a composite ARKUI_NODE_COLUMN layout.
 *
 * Layout:
 *   COLUMN (m_nodeHandle)
 *     - TEXT_INPUT or TEXT_AREA (m_textInputHandle, decided by variant at construction)
 *     - TEXT (m_errorTextHandle, hidden by default; shown when validation fails)
 *
 * Node type for the inner text node is decided at construction time by the
 * "variant" property:
 *   - longText -> ARKUI_NODE_TEXT_AREA (multi-line, supports line wrapping)
 *   - others   -> ARKUI_NODE_TEXT_INPUT (single line)
 *
 * Supported properties:
 *   - label: fallback text for the placeholder
 *   - placeholder: placeholder text
 *   - value: text value, including DynamicString input
 *   - variant: shortText by default, longText, number, or obscured
 *   - validationRegexp: optional regular expression used for runtime validation
 *   - validationMessage: optional error message shown when validationRegexp fails
 *   - styles:
 *     - width: "100%" or a numeric value
 *     - height: "100%" or a numeric value
 *     - border-radius: "16px" or a numeric value
 *     - background-color: #hex or rgba()
 *     - border-width: "1px" or a numeric value
 *     - border-color: #hex or rgba()
 */
class TextFieldComponent : public A2UIComponent {
public:
    TextFieldComponent(const std::string& id, const nlohmann::json& properties);
    ~TextFieldComponent() override;
    void destroy() override;

public:
    A2UITextInputNode getTextInputNode() {
        return A2UITextInputNode(m_textInputHandle);
    }

    A2UITextNode getErrorTextNode() {
        return A2UITextNode(m_errorTextHandle);
    }

protected:
    void onUpdateProperties(const nlohmann::json& properties) override;

private:
    /**
     * @brief Apply the placeholder text.
     * @param properties Property JSON object
     */
    void applyPlaceholder(const nlohmann::json& properties);

    /**
     * @brief Apply the text value.
     * @param properties Property JSON object
     */
    void applyValue(const nlohmann::json& properties);

    /**
     * @brief Apply the input type variant.
     * @param properties Property JSON object
     */
    void applyVariant(const nlohmann::json& properties);

    /**
     * @brief Apply the runtime validation configuration.
     * @param properties Property JSON object
     */
    void applyValidationConfig(const nlohmann::json& properties);

    /**
     * @brief Apply external checks-based error state.
     * @param properties Property JSON object
     */
    void applyChecks(const nlohmann::json& properties);

    /**
     * @brief Apply style attributes.
     * @param properties Property JSON object
     */
    void applyStyles(const nlohmann::json& properties);

    /**
     * @brief Apply the border radius.
     * @param styles Style JSON object
     */
    void applyBorderRadius(const nlohmann::json& styles);

    /**
     * @brief Apply the background color.
     * @param styles Style JSON object
     */
    void applyBackgroundColor(const nlohmann::json& styles);

    /**
     * @brief Apply the border width.
     * @param styles Style JSON object
     */
    void applyBorderWidth(const nlohmann::json& styles);

    /**
     * @brief Apply the border color.
     * @param styles Style JSON object
     */
    void applyBorderColor(const nlohmann::json& styles);

    /**
     * @brief Apply the background image on the inner text node.
     */
    void applyBackgroundImage(const nlohmann::json& styles);

    /**
     * @brief Handle runtime text changes from the native input node.
     * @param text Current text content
     */
    void handleTextChanged(const std::string& text);

    /**
     * @brief Recalculate the local validation state.
     * @param userInitiated Whether this refresh came from user input
     */
    void refreshLocalValidationState(bool userInitiated);

    /**
     * @brief Validate the current text against validationRegexp.
     * @param text Text to validate
     * @return Whether the text is currently valid
     */
    bool validateText(const std::string& text) const;

    /**
     * @brief Resolve the runtime validation error message.
     * @return Error message shown under the field
     */
    std::string resolveValidationMessage() const;

    /**
     * @brief Update the visible error text and border state.
     */
    void updateValidationPresentation();

    /**
     * @brief Shared native text change callback (TEXT_INPUT and TEXT_AREA share the same string-async-event shape).
     * @param event ArkUI text input change event
     */
    static void onTextChangeCallback(ArkUI_NodeEvent* event);

private:
    ArkUI_NodeHandle m_textInputHandle = nullptr;  // Inner TEXT_INPUT or TEXT_AREA node
    ArkUI_NodeHandle m_errorTextHandle = nullptr;  // Validation error message node
    std::string m_currentText;
    std::string m_validationRegexp;
    std::string m_validationMessage;
    std::regex m_compiledValidationRegex;
    bool m_isTextArea = false;  // true if the inner node is ARKUI_NODE_TEXT_AREA
    bool m_isUpdatingFromNative = false;
    bool m_hasUserEdited = false;
    bool m_hasValidationRegexp = false;
    bool m_isValidationPatternValid = false;
    bool m_localValidationFailed = false;
    bool m_externalCheckFailed = false;
    bool m_hasCustomBorderColor = false;
    uint32_t m_borderColor = 0;
    std::string m_externalCheckMessage;
    float m_borderWidth = 0.0f;   // Border width
    float m_borderRadius = 0.0f;  // Border radius
    float m_fontSize = 0.0f;      // Parsed font size (0 = use kDefaultFontSize)
};

} // namespace a2ui
