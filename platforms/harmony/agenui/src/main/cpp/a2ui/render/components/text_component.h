#pragma once

#include "../a2ui_component.h"

namespace a2ui {

/**
 * Text component backed by the Harmony ArkUI C-API.
 *
 * Supported properties:
 *   - text: text content, as either a literalString object or a plain string
 *   - textChunk: content to append to existing text (streaming mode)
 *   - variant: text variant such as h1-h5, caption, or body
 *   - styles:
 *       - color: text color (#RGB / #RRGGBB / #AARRGGBB)
 *       - font-size: font size in fp
 *       - font-weight: bold / normal / 100-900
 *       - text-align: start / center / end
 *       - line-clamp: maximum number of lines
 *       - line-height: line height in fp
 *       - text-overflow: ellipsis / clip
 *       - border-width: border width (kebab-case or camelCase borderWidth)
 *       - border-color: border color (kebab-case or camelCase borderColor)
 *       - border-radius: border corner radius (kebab-case or camelCase borderRadius)
 *       - padding: inner spacing, supports CSS shorthand (e.g. "10px" or "10px 20px")
 *       - background-color: background fill color
 *       - text-decoration: shorthand "line style color" (e.g. "underline dashed #FF0000")
 *       - text-decoration-line: underline / line-through / none
 *       - text-decoration-style: solid / dashed / dotted / double / wavy
 *       - text-decoration-color: decoration color
 *       - filter: drop-shadow(offsetX offsetY blur color)
 */
class TextComponent final : public A2UIComponent {
public:
    TextComponent(const std::string& id, const nlohmann::json& properties);
    ~TextComponent() override;

protected:
    void onUpdateProperties(const nlohmann::json& properties) override;

private:
    void applyTextContent(const nlohmann::json& properties);
    void applyStyles(const nlohmann::json& properties);

    float applyFontStyles(const nlohmann::json& styles);
    void applyTextLayoutStyles(const nlohmann::json& properties, const nlohmann::json& styles, float fontSize);
    void applyBorderDecorationStyles(const nlohmann::json& styles, float fontSize);

    bool parseDropShadowFilter(const std::string& filterValue, float& offsetX, float& offsetY,
                               float& blur, uint32_t& color) const;

    static int32_t mapFontWeight(const nlohmann::json& weight);
    static int32_t mapTextAlign(const std::string& align);
};

} // namespace a2ui
