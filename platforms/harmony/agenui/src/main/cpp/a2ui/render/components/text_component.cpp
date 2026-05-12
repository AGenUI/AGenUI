#include "text_component.h"
#include "log/a2ui_capi_log.h"
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>
#include "../../measure/a2ui_platform_layout_bridge.h"
#include "a2ui/utils/a2ui_unit_utils.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "a2ui/utils/a2ui_font_weight_utils.h"


namespace a2ui {

namespace {

bool isExplicitTransparentShadowColor(const std::string& colorStr) {
    return colorStr == "#00000000" ||
           colorStr == "rgba(0, 0, 0, 0)" ||
           colorStr == "rgba(0,0,0,0)";
}

} // namespace

TextComponent::TextComponent(const std::string& id, const nlohmann::json& properties) : A2UIComponent(id, "Text") {
    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
    
    // Merge initial properties.
    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    HM_LOGI( "TextComponent - Created: id=%s, handle=%s", id.c_str(), m_nodeHandle ? "valid" : "null");
}

TextComponent::~TextComponent() {
    HM_LOGI( "TextComponent - Destroyed: id=%s", m_id.c_str());
}

void TextComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE( "handle or nodeApi is null, id=%s",m_id.c_str());
        return;
    }

    applyTextContent(properties);
    applyStyles(properties);

    HM_LOGI( "Applied properties, id=%s", m_id.c_str());
}

// ---- Text Content ----

void TextComponent::applyTextContent(const nlohmann::json& properties) {
    if (properties.find("text") == properties.end()) {
        return;
    }

    std::string textContent;
    const auto& textValue = properties["text"];

    // Format 1: {"text": {"literalString": "Hello"}}
    if (textValue.is_object()) {
        if (textValue.find("literalString") != textValue.end() && textValue["literalString"].is_string()) {
            textContent = textValue["literalString"].get<std::string>();
        }
    }
    // Format 2: {"text": "Hello"}
    else if (textValue.is_string()) {
        textContent = textValue.get<std::string>();
    }

    if (!textContent.empty()) {
        A2UITextNode node(m_nodeHandle);
        node.setTextContent(textContent);
    }
}

void TextComponent::applyStyles(const nlohmann::json& properties) {
    if (properties.find("styles") == properties.end() || !properties["styles"].is_object()) {
        return;
    }

    const auto& styles = properties["styles"];
    A2UITextNode node(m_nodeHandle);
    A2UINode baseNode(m_nodeHandle);

    // Support both kebab-case and camelCase background color keys.
    {
        std::string bgColorStr;
        if (styles.find("background-color") != styles.end() && styles["background-color"].is_string()) {
            bgColorStr = styles["background-color"].get<std::string>();
        } else if (styles.find("backgroundColor") != styles.end() && styles["backgroundColor"].is_string()) {
            bgColorStr = styles["backgroundColor"].get<std::string>();
        }
        if (!bgColorStr.empty()) {
            node.setBackgroundColor(parseColor(bgColorStr));
        }
    }

    // color -> NODE_FONT_COLOR
    if (styles.find("color") != styles.end() && styles["color"].is_string()) {
        uint32_t color = parseColor(styles["color"].get<std::string>());
        node.setFontColor(color);
    }
    float size = 14.0f;
    // font-size overrides the variant-derived font size.
    if (styles.find("font-size") != styles.end()) {
        const auto& fontSizeVal = styles["font-size"];
        if (fontSizeVal.is_number()) {
            size = fontSizeVal.get<float>();
        } else if (fontSizeVal.is_string()) {
            size = static_cast<float>(std::atof(fontSizeVal.get<std::string>().c_str()));
        }
        node.setFontSize(size);
    }

    // font-weight -> NODE_FONT_WEIGHT
    if (styles.find("font-weight") != styles.end()) {
        ArkUI_FontWeight weight = (ArkUI_FontWeight)mapFontWeight(styles["font-weight"]);
        node.setFontWeight(weight);
    }

    // Support both kebab-case and camelCase font family keys.
    {
        std::string fontFamily;
        if (styles.find("font-family") != styles.end() && styles["font-family"].is_string()) {
            fontFamily = styles["font-family"].get<std::string>();
        } else if (styles.find("fontFamily") != styles.end() && styles["fontFamily"].is_string()) {
            fontFamily = styles["fontFamily"].get<std::string>();
        }
        if (!fontFamily.empty()) {
            node.setFontFamily(fontFamily);
        }
    }

    // Support both kebab-case and camelCase text alignment keys.
    {
        std::string alignStr;
        if (styles.find("text-align") != styles.end() && styles["text-align"].is_string()) {
            alignStr = styles["text-align"].get<std::string>();
        } else if (styles.find("textAlign") != styles.end() && styles["textAlign"].is_string()) {
            alignStr = styles["textAlign"].get<std::string>();
        }
        
        // Tabs always force START alignment.
        if (m_parent && m_parent->getComponentType() == "Tabs") {
            node.setTextAlign(ARKUI_TEXT_ALIGNMENT_START);
            HM_LOGI("Parent is Tabs, forcing text-align to START, id=%s", m_id.c_str());
        } else if (!alignStr.empty()) {
            ArkUI_TextAlignment align = (ArkUI_TextAlignment)mapTextAlign(alignStr);
            node.setTextAlign(align);
        }
    }

    // line-clamp -> NODE_TEXT_MAX_LINES
    // line-clamp: 0 means "no limit" — treat as if no line-clamp was set so
    // that the height-based max-lines calculation below can still apply.
    int32_t maxLines = 0;
    bool hasLineClamp = false;
    if (styles.find("line-clamp") != styles.end()) {
        const auto& lineClampVal = styles["line-clamp"];
        if (lineClampVal.is_number_integer()) {
            maxLines = lineClampVal.get<int32_t>();
        } else if (lineClampVal.is_string()) {
            maxLines = std::atoi(lineClampVal.get<std::string>().c_str());
        }
        if (maxLines > 0) {
            hasLineClamp = true;
            node.setTextMaxLines(maxLines);
        }
    }

    int renderedLines = 0;
    if (styles.find("lines") != styles.end()) {
        const auto& linesVal = styles["lines"];
        if (linesVal.is_number_integer()) {
            renderedLines = linesVal.get<int>();
        } else if (linesVal.is_string()) {
            renderedLines = std::atoi(linesVal.get<std::string>().c_str());
        }
    }

    bool textContainsLineBreak = false;
    if (properties.find("text") != properties.end()) {
        const auto& textValue = properties["text"];
        if (textValue.is_string()) {
            const std::string text = textValue.get<std::string>();
            textContainsLineBreak = text.find('\n') != std::string::npos || text.find('\r') != std::string::npos;
        } else if (textValue.is_object()) {
            const auto literalIt = textValue.find("literalString");
            if (literalIt != textValue.end() && literalIt->is_string()) {
                const std::string text = literalIt->get<std::string>();
                textContainsLineBreak = text.find('\n') != std::string::npos || text.find('\r') != std::string::npos;
            }
        }
    }

    // line-height:
    // - multi-line text must use the native line-height attribute so inter-line
    //   spacing matches the engine's measured layout.
    // - single-line text keeps the padding-based centering workaround used by
    //   Text_ComplexCombined, but merges it with any explicit user padding.
    float verticalPadding = 0.0f;
    if (styles.find("line-height") != styles.end()) {
        float lineHeight = 0.0f;
        const auto& lineHeightVal = styles["line-height"];
        if (lineHeightVal.is_number()) {
            lineHeight = lineHeightVal.get<float>();
        } else if (lineHeightVal.is_string()) {
            lineHeight = static_cast<float>(std::atof(lineHeightVal.get<std::string>().c_str()));
        }
        if (lineHeight > 0.0f) {
            const float resolvedLineHeight = (lineHeight < 5.0f) ? (lineHeight * size) : lineHeight;
            const bool allowsMultipleLines = !hasLineClamp || maxLines != 1;
            const bool isMultiLineText =
                renderedLines > 1 ||
                (renderedLines == 0 && allowsMultipleLines && textContainsLineBreak);
            if (isMultiLineText) {
                node.setLineHeight(UnitConverter::a2uiToVp(resolvedLineHeight));
                baseNode.resetPadding();
            } else {
                node.resetLineHeight();
                verticalPadding = (resolvedLineHeight - size) / 2.0f;
                if (verticalPadding > 0.0f) {
                    baseNode.setPadding(verticalPadding, 0.0f, verticalPadding, 0.0f);
                } else {
                    verticalPadding = 0.0f;
                    baseNode.resetPadding();
                }
            }
        } else {
            node.resetLineHeight();
            baseNode.resetPadding();
        }
    } else {
        node.resetLineHeight();
        baseNode.resetPadding();
    }

    // height / max-height -> maxLines + overflow clip
    // ArkUI's ARKUI_NODE_TEXT does not respect NODE_CLIP for its text content.
    // The correct way to prevent text overflowing a fixed height is to compute
    // the maximum number of lines that fit and set NODE_TEXT_MAX_LINES +
    // NODE_TEXT_OVERFLOW=CLIP.  We only do this when no explicit line-clamp has
    // already been applied by the caller.
    if (!hasLineClamp) {
        // Resolve the constraining height: prefer the Yoga-computed fixed height,
        // fall back to an explicit max-height value.
        float constraintH = 0.0f;

        if (styles.find("height") != styles.end()) {
            const auto& hVal = styles["height"];
            if (hVal.is_number()) {
                constraintH = hVal.get<float>();
            } else if (hVal.is_string()) {
                constraintH = static_cast<float>(std::atof(hVal.get<std::string>().c_str()));
            }
        }

        if (constraintH <= 0.0f) {
            // Fallback: max-height sent directly.
            std::string mhKey;
            if (styles.find("max-height") != styles.end()) {
                mhKey = "max-height";
            } else if (styles.find("maxHeight") != styles.end()) {
                mhKey = "maxHeight";
            }
            if (!mhKey.empty()) {
                const auto& mhVal = styles[mhKey];
                if (mhVal.is_number()) {
                    constraintH = mhVal.get<float>();
                } else if (mhVal.is_string()) {
                    constraintH = static_cast<float>(std::atof(mhVal.get<std::string>().c_str()));
                }
                if (constraintH > 0.0f) {
                    baseNode.setConstraintSize(0.0f, 100000.0f, 0.0f, constraintH);
                }
            }
        }

        if (constraintH > 0.0f) {
            // Determine per-line height: use resolved line-height when available,
            // otherwise fall back to font-size.
            float lineH = 0.0f;
            if (styles.find("line-height") != styles.end()) {
                const auto& lhVal = styles["line-height"];
                if (lhVal.is_number()) {
                    lineH = lhVal.get<float>();
                } else if (lhVal.is_string()) {
                    lineH = static_cast<float>(std::atof(lhVal.get<std::string>().c_str()));
                }
                // Relative line-height (e.g. 1.5 meaning 1.5 * font-size).
                if (lineH > 0.0f && lineH < 5.0f) {
                    lineH = lineH * size;
                }
            }
            if (lineH <= 0.0f) {
                // When measurement provided a line count, derive per-line height
                // from the Yoga-computed constraint to avoid font-size default
                // mismatch between measurement (fontSize=24) and rendering.
                if (renderedLines > 0 && constraintH > 0.0f) {
                    lineH = constraintH / static_cast<float>(renderedLines);
                } else {
                    lineH = size > 0.0f ? size : 14.0f;
                }
            }

            // Subtract the dynamically calculated vertical padding that was
            // applied above for single-line line-height centering.
            float availH = constraintH - verticalPadding * 2.0f;
            if (availH <= 0.0f) {
                availH = constraintH;
            }

            int32_t computedMaxLines = static_cast<int32_t>(availH / lineH);
            if (computedMaxLines < 1) {
                computedMaxLines = 1;
            }

            node.setTextMaxLines(computedMaxLines);
            node.setTextOverflowClip();
        }
    }

    // text-overflow -> NODE_TEXT_OVERFLOW
    if (styles.find("text-overflow") != styles.end() && styles["text-overflow"].is_string()) {
        std::string overflow = styles["text-overflow"].get<std::string>();
        if (overflow == "ellipsis") {
            node.setTextOverflowEllipsis();
        } else {
            node.setTextOverflowClip();
        }
    }


    // border-radius -> NODE_BORDER_RADIUS
    {
        std::string radiusKey;
        if (styles.find("border-radius") != styles.end()) {
            radiusKey = "border-radius";
        } else if (styles.find("borderRadius") != styles.end()) {
            radiusKey = "borderRadius";
        }
        if (!radiusKey.empty()) {
            float radius = 0.0f;
            const auto& radiusVal = styles[radiusKey];
            if (radiusVal.is_number()) {
                radius = radiusVal.get<float>();
            } else if (radiusVal.is_string()) {
                radius = static_cast<float>(std::atof(radiusVal.get<std::string>().c_str()));
            }
            if (radius > 0.0f) {
                // When border-radius >= font-size the intent is a pill/capsule
                // shape.  On iOS padding lives outside the view, so the view is
                // short and a radius >= half-height automatically becomes a
                // semicircle.  On ArkUI padding is inside the node, making the
                // node taller, so the same radius may be < half-height and won't
                // form a pill.  Use a very large value so ArkUI caps it at
                // half the shorter dimension, producing the expected capsule.
                float effectiveRadius = (radius >= size) ? 9999.0f : radius;
                node.setBorderRadius(effectiveRadius);
                // Clip the background/content to the rounded border path.
                node.setClip(true);
            } else {
                node.resetBorderRadius();
                node.resetClip();
            }
        }
    }

    // border-width -> NODE_BORDER_WIDTH
    {
        std::string bwKey;
        if (styles.find("border-width") != styles.end()) {
            bwKey = "border-width";
        } else if (styles.find("borderWidth") != styles.end()) {
            bwKey = "borderWidth";
        }
        if (!bwKey.empty()) {
            float bw = 0.0f;
            const auto& bwVal = styles[bwKey];
            if (bwVal.is_number()) {
                bw = bwVal.get<float>();
            } else if (bwVal.is_string()) {
                bw = static_cast<float>(std::atof(bwVal.get<std::string>().c_str()));
            }
            if (bw > 0.0f) {
                node.setBorderWidth(bw, bw, bw, bw);
                node.setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
            } else {
                node.resetBorderWidth();
                node.resetBorderStyle();
            }
        }
    }

    // border-color -> NODE_BORDER_COLOR
    {
        std::string bcKey;
        if (styles.find("border-color") != styles.end()) {
            bcKey = "border-color";
        } else if (styles.find("borderColor") != styles.end()) {
            bcKey = "borderColor";
        }
        if (!bcKey.empty() && styles[bcKey].is_string()) {
            uint32_t color = parseColor(styles[bcKey].get<std::string>());
            node.setBorderColor(color);
        }
    }


    // text-decoration-line, text-decoration-style, text-decoration-color -> NODE_TEXT_DECORATION
    if (styles.find("text-decoration-line") != styles.end() && styles["text-decoration-line"].is_string()) {
        std::string decorationLine = styles["text-decoration-line"].get<std::string>();
        
        // Default to black.
        uint32_t decorationColor = colors::kColorBlack;
        if (styles.find("text-decoration-color") != styles.end() && styles["text-decoration-color"].is_string()) {
            decorationColor = parseColor(styles["text-decoration-color"].get<std::string>());
        }

        // Resolve the decoration type.
        ArkUI_TextDecorationType decorationType = ARKUI_TEXT_DECORATION_TYPE_NONE;
        if (decorationLine == "underline") {
            decorationType = ARKUI_TEXT_DECORATION_TYPE_UNDERLINE;
        } else if (decorationLine == "overline") {
            decorationType = ARKUI_TEXT_DECORATION_TYPE_OVERLINE;
        } else if (decorationLine == "line-through") {
            decorationType = ARKUI_TEXT_DECORATION_TYPE_LINE_THROUGH;
        }

        // Apply text decoration.
        if (decorationType != ARKUI_TEXT_DECORATION_TYPE_NONE) {
            node.setTextDecoration(decorationType, decorationColor);
        }
    }

    if (styles.find("filter") != styles.end()) {
        if (styles["filter"].is_string()) {
            float shadowOffsetX = 0.0f;
            float shadowOffsetY = 0.0f;
            float shadowBlur = 0.0f;
            uint32_t shadowColor = colors::kColorTransparent;
            if (parseDropShadowFilter(styles["filter"].get<std::string>(),
                                      shadowOffsetX, shadowOffsetY, shadowBlur, shadowColor)) {
                // Map CSS drop-shadow directly to ArkUI text shadow semantics.
                node.setTextShadow(shadowBlur, shadowOffsetX, shadowOffsetY, shadowColor);
            } else {
                node.resetTextShadow();
            }
        } else {
            node.resetTextShadow();
        }
    }

}

bool TextComponent::parseDropShadowFilter(const std::string& filterValue, float& offsetX, float& offsetY,
                                          float& blur, uint32_t& color) const {
    const std::string dropShadowPrefix = "drop-shadow(";
    size_t dropShadowStart = filterValue.find(dropShadowPrefix);
    if (dropShadowStart == std::string::npos) {
        return false;
    }
    dropShadowStart += dropShadowPrefix.size();

    size_t dropShadowEnd = filterValue.rfind(')');
    if (dropShadowEnd == std::string::npos || dropShadowEnd < dropShadowStart) {
        return false;
    }

    std::string inner = filterValue.substr(dropShadowStart, dropShadowEnd - dropShadowStart);
    const char* cursor = inner.c_str();
    char* endPtr = nullptr;

    auto skipSeparators = [](const char*& current) {
        while (*current == ' ' || *current == ',') {
            current++;
        }
    };

    auto parseLength = [&](float& outValue) -> bool {
        skipSeparators(cursor);
        outValue = std::strtof(cursor, &endPtr);
        if (endPtr == cursor) {
            return false;
        }
        cursor = endPtr;
        while (*cursor && *cursor != ' ' && *cursor != ',' && *cursor != '(') {
            cursor++;
        }
        return true;
    };

    float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; i++) {
        if (!parseLength(values[i])) {
            return false;
        }
    }

    const char* lookahead = cursor;
    skipSeparators(lookahead);
    if (*lookahead != '\0' && *lookahead != 'r' && *lookahead != '#' && *lookahead != 't') {
        cursor = lookahead;
        if (!parseLength(values[3])) {
            return false;
        }
    } else {
        cursor = lookahead;
    }

    skipSeparators(cursor);
    std::string colorStr = cursor;
    if (colorStr.empty()) {
        return false;
    }

    uint32_t parsedColor = parseColor(colorStr);
    if (parsedColor == colors::kColorTransparent && !isExplicitTransparentShadowColor(colorStr)) {
        return false;
    }

    offsetX = values[0];
    offsetY = values[1];
    blur = values[2];
    color = parsedColor;
    return true;
}

int32_t TextComponent::mapFontWeight(const nlohmann::json& weight) {
    if (weight.is_string()) {
        const std::string weightStr = weight.get<std::string>();
        if (weightStr == "bold") {
            return ARKUI_FONT_WEIGHT_BOLD;
        } else if (weightStr == "normal") {
            return ARKUI_FONT_WEIGHT_NORMAL;
        } else if (weightStr == "medium") {
            return ARKUI_FONT_WEIGHT_MEDIUM;
        }
        // Try to parse numeric font-weight values (e.g. "100" .. "900").
        const int numWeight = std::atoi(weightStr.c_str());
        if (numWeight > 0) {
            return font_weight::mapNumericToArkUIFontWeight(numWeight, /*useNormalBoldAlias=*/true);
        }
    } else if (weight.is_number_integer()) {
        return font_weight::mapNumericToArkUIFontWeight(weight.get<int>(), /*useNormalBoldAlias=*/true);
    }
    return ARKUI_FONT_WEIGHT_NORMAL;
}

int32_t TextComponent::mapTextAlign(const std::string& align) {
    if (align == "center") {
        return ARKUI_TEXT_ALIGNMENT_CENTER;
    } else if (align == "end" || align == "right") {
        return ARKUI_TEXT_ALIGNMENT_END;
    }
    // Default to START.
    return ARKUI_TEXT_ALIGNMENT_START;
}

} // namespace a2ui
