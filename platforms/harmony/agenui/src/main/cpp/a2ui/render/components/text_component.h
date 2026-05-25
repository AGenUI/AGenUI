#pragma once

#include "../a2ui_component.h"

namespace a2ui {

/**
 * Text component backed by the Harmony ArkUI C-API.
 *
 * Supported properties:
 *   - text: text content, as either a literalString object or a plain string
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
 *       - text-decoration-line: underline / overline / line-through
 *       - text-decoration-color: decoration color
 *       - filter: drop-shadow(offsetX offsetY blur color)
 */
class TextComponent : public A2UIComponent {
public:
    TextComponent(const std::string& id, const nlohmann::json& properties);
    ~TextComponent() override;

protected:
    void onUpdateProperties(const nlohmann::json& properties) override;

private:
    /** Parse and apply NODE_TEXT_CONTENT. */
    void applyTextContent(const nlohmann::json& properties);

    /** Parse styles and apply native attributes. */
    void applyStyles(const nlohmann::json& properties);

    /** Parse filter: drop-shadow(offsetX offsetY blur color) for text shadow. */
    bool parseDropShadowFilter(const std::string& filterValue, float& offsetX, float& offsetY,
                               float& blur, uint32_t& color) const;

    /** Map font weight to the ArkUI enum value. */
    static int32_t mapFontWeight(const nlohmann::json& weight);

    /** Map text alignment to the ArkUI enum value. */
    static int32_t mapTextAlign(const std::string& align);
};

} // namespace a2ui
