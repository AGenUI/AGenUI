#include "text_component.h"
#include "log/a2ui_capi_log.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>
#include "../../measure/a2ui_platform_layout_bridge.h"
#include "a2ui/utils/a2ui_unit_utils.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "a2ui/utils/a2ui_font_weight_utils.h"
#include "a2ui/utils/a2ui_padding_utils.h"


namespace a2ui {

namespace {

bool isExplicitTransparentShadowColor(const std::string& colorStr) {
    return colorStr == "#00000000" ||
           colorStr == "rgba(0, 0, 0, 0)" ||
           colorStr == "rgba(0,0,0,0)";
}

// Resolve a W3C `line-height` value to an absolute a2ui-px line box height.
// Parsing rules (kept in lockstep with iOS `extractLineHeight` and Android `StyleHelper`):
//   - JSON number                    -> treat as unitless multiplier (`value * fontSize`)
//   - JSON string ending with "px"   -> treat as absolute px value
//   - JSON string without unit       -> treat as unitless multiplier (`atof(value) * fontSize`)
// A return value of 0 means the style is absent or invalid and the caller should leave
// the native line-height untouched.
float resolveLineHeightPx(const nlohmann::json& lhVal, float fontSize) {
    if (lhVal.is_number()) {
        float mult = lhVal.get<float>();
        return (mult > 0.0f && fontSize > 0.0f) ? mult * fontSize : 0.0f;
    }
    if (lhVal.is_string()) {
        const std::string& s = lhVal.get_ref<const std::string&>();
        if (s.empty()) {
            return 0.0f;
        }
        const float raw = static_cast<float>(std::atof(s.c_str()));
        if (raw <= 0.0f) {
            return 0.0f;
        }
        const bool hasPxSuffix = s.size() >= 2 &&
                                 s.compare(s.size() - 2, 2, "px") == 0;
        if (hasPxSuffix) {
            return raw;
        }
        return fontSize > 0.0f ? raw * fontSize : 0.0f;
    }
    return 0.0f;
}

// Parse a single CSS length token (e.g. "12px", "12", number) into an a2ui-px
// value. Returns true on success. Trailing/leading whitespace is allowed; the
// `px` suffix is optional and stripped before atof.
// (Migrated to a2ui::padding_utils; thin local wrappers retained for callers.)
bool parsePaddingLength(const nlohmann::json& v, float& outPx) {
    return ::a2ui::padding_utils::parseLength(v, outPx);
}

// Resolve CSS `padding` shorthand + `padding-top/right/bottom/left` overrides
// into four a2ui-px edge values (top/right/bottom/left). Negative results are
// clamped to 0. Values are returned in a2ui-px; the caller is responsible for
// the eventual vp conversion (handled by `BaseNode::setPadding`).
void resolveUserPadding(const nlohmann::json& styles,
                        float& top, float& right, float& bottom, float& left) {
    ::a2ui::padding_utils::resolveUserPadding(styles, top, right, bottom, left);
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
    // textChunk: incremental append (streaming mode), prioritized over "text"
    if (properties.find("textChunk") != properties.end()) {
        std::string chunkContent;
        const auto& chunkValue = properties["textChunk"];

        if (chunkValue.is_string()) {
            chunkContent = chunkValue.get<std::string>();
        } else if (chunkValue.is_object()) {
            auto litIt = chunkValue.find("literalString");
            if (litIt != chunkValue.end() && litIt->is_string()) {
                chunkContent = litIt->get<std::string>();
            }
        }

        if (!chunkContent.empty()) {
            A2UITextNode node(m_nodeHandle);
            std::string currentText = node.getTextContent();
            node.setTextContent(currentText + chunkContent);
            HM_LOGI("TextComponent - textChunk appended, id=%s, chunk_len=%zu, total_len=%zu",
                    m_id.c_str(), chunkContent.size(), currentText.size() + chunkContent.size());
        }
    }
    // text: full overwrite
    else if (properties.find("text") != properties.end()) {
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
}

void TextComponent::applyStyles(const nlohmann::json& properties) {
    if (properties.find("styles") == properties.end() || !properties["styles"].is_object()) {
        return;
    }
    const auto& styles = properties["styles"];
    float fontSize = applyFontStyles(styles);
    applyTextLayoutStyles(properties, styles, fontSize);
    applyBorderDecorationStyles(styles, fontSize);
}

// ---- Font Styles ----

float TextComponent::applyFontStyles(const nlohmann::json& styles) {
    A2UITextNode node(m_nodeHandle);

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

    if (styles.find("color") != styles.end() && styles["color"].is_string()) {
        uint32_t color = parseColor(styles["color"].get<std::string>());
        node.setFontColor(color);
    }

    float size = 14.0f;
    if (styles.find("font-size") != styles.end()) {
        const auto& fontSizeVal = styles["font-size"];
        if (fontSizeVal.is_number()) {
            size = fontSizeVal.get<float>();
        } else if (fontSizeVal.is_string()) {
            size = static_cast<float>(std::atof(fontSizeVal.get<std::string>().c_str()));
        }
        node.setFontSize(size);
    }

    if (styles.find("font-weight") != styles.end()) {
        ArkUI_FontWeight weight = static_cast<ArkUI_FontWeight>(mapFontWeight(styles["font-weight"]));
        node.setFontWeight(weight);
    }

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

    return size;
}

// ---- Text Layout Styles ----

void TextComponent::applyTextLayoutStyles(const nlohmann::json& properties, const nlohmann::json& styles, float fontSize) {
    A2UITextNode node(m_nodeHandle);
    A2UINode baseNode(m_nodeHandle);

    // text-align
    {
        std::string alignStr;
        if (styles.find("text-align") != styles.end() && styles["text-align"].is_string()) {
            alignStr = styles["text-align"].get<std::string>();
        } else if (styles.find("textAlign") != styles.end() && styles["textAlign"].is_string()) {
            alignStr = styles["textAlign"].get<std::string>();
        }
        if (m_parent && m_parent->getComponentType() == "Tabs") {
            node.setTextAlign(ARKUI_TEXT_ALIGNMENT_START);
            HM_LOGI("Parent is Tabs, forcing text-align to START, id=%s", m_id.c_str());
        } else if (!alignStr.empty()) {
            ArkUI_TextAlignment align = static_cast<ArkUI_TextAlignment>(mapTextAlign(alignStr));
            node.setTextAlign(align);
        }
    }

    // line-clamp
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

    // line-height + padding
    float userPadTop = 0.0f, userPadRight = 0.0f, userPadBottom = 0.0f, userPadLeft = 0.0f;
    ::a2ui::padding_utils::resolveUserPadding(styles, userPadTop, userPadRight, userPadBottom, userPadLeft);

    auto applyPadding = [&](float extraTop, float extraBottom) {
        const float t = userPadTop + extraTop;
        const float r = userPadRight;
        const float b = userPadBottom + extraBottom;
        const float l = userPadLeft;
        if (t > 0.0f || r > 0.0f || b > 0.0f || l > 0.0f) {
            baseNode.setPadding(t, r, b, l);
        } else {
            baseNode.resetPadding();
        }
    };

    float verticalPadding = 0.0f;
    if (styles.find("line-height") != styles.end()) {
        const float resolvedLineHeight = resolveLineHeightPx(styles["line-height"], fontSize);
        if (resolvedLineHeight > 0.0f) {
            const bool allowsMultipleLines = !hasLineClamp || maxLines != 1;
            const bool isMultiLineText =
                renderedLines > 1 ||
                (renderedLines == 0 && allowsMultipleLines && textContainsLineBreak);
            if (isMultiLineText) {
                node.setLineHeight(UnitConverter::a2uiToVp(resolvedLineHeight));
                node.setHalfLeading(true);
                applyPadding(0.0f, 0.0f);
            } else {
                node.resetLineHeight();
                node.resetHalfLeading();
                verticalPadding = (resolvedLineHeight - fontSize) / 2.0f;
                if (verticalPadding > 0.0f) {
                    applyPadding(verticalPadding, verticalPadding);
                } else {
                    verticalPadding = 0.0f;
                    applyPadding(0.0f, 0.0f);
                }
            }
        } else {
            node.resetLineHeight();
            node.resetHalfLeading();
            applyPadding(0.0f, 0.0f);
        }
    } else {
        node.resetLineHeight();
        node.resetHalfLeading();
        applyPadding(0.0f, 0.0f);
    }

    // height / max-height -> maxLines + overflow clip
    if (!hasLineClamp) {
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
            float lineH = 0.0f;
            if (styles.find("line-height") != styles.end()) {
                lineH = resolveLineHeightPx(styles["line-height"], fontSize);
            }
            if (lineH <= 0.0f) {
                if (renderedLines > 0 && constraintH > 0.0f) {
                    lineH = constraintH / static_cast<float>(renderedLines);
                } else {
                    lineH = fontSize > 0.0f ? fontSize : 14.0f;
                }
            }

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

    // text-overflow
    if (styles.find("text-overflow") != styles.end() && styles["text-overflow"].is_string()) {
        std::string overflow = styles["text-overflow"].get<std::string>();
        if (overflow == "ellipsis") {
            node.setTextOverflowEllipsis();
        } else {
            node.setTextOverflowClip();
        }
    }
}

// ---- Border & Decoration Styles ----

void TextComponent::applyBorderDecorationStyles(const nlohmann::json& styles, float fontSize) {
    A2UITextNode node(m_nodeHandle);

    // border-radius
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
                float effectiveRadius = (radius >= fontSize) ? 9999.0f : radius;
                node.setBorderRadius(effectiveRadius);
                node.setClip(true);
            } else {
                node.resetBorderRadius();
                node.resetClip();
            }
        }
    }

    // border-width
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

    // border-color
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

    // text-decoration
    if (styles.find("text-decoration-line") != styles.end() && styles["text-decoration-line"].is_string()) {
        std::string decorationLine = styles["text-decoration-line"].get<std::string>();

        uint32_t decorationColor = colors::kColorBlack;
        if (styles.find("text-decoration-color") != styles.end() && styles["text-decoration-color"].is_string()) {
            decorationColor = parseColor(styles["text-decoration-color"].get<std::string>());
        }

        ArkUI_TextDecorationType decorationType = ARKUI_TEXT_DECORATION_TYPE_NONE;
        if (decorationLine == "underline") {
            decorationType = ARKUI_TEXT_DECORATION_TYPE_UNDERLINE;
        } else if (decorationLine == "overline") {
            decorationType = ARKUI_TEXT_DECORATION_TYPE_OVERLINE;
        } else if (decorationLine == "line-through") {
            decorationType = ARKUI_TEXT_DECORATION_TYPE_LINE_THROUGH;
        }

        if (decorationType != ARKUI_TEXT_DECORATION_TYPE_NONE) {
            node.setTextDecoration(decorationType, decorationColor);
        }
    }

    // filter: drop-shadow
    if (styles.find("filter") != styles.end()) {
        if (styles["filter"].is_string()) {
            float shadowOffsetX = 0.0f;
            float shadowOffsetY = 0.0f;
            float shadowBlur = 0.0f;
            uint32_t shadowColor = colors::kColorTransparent;
            if (parseDropShadowFilter(styles["filter"].get<std::string>(),
                                      shadowOffsetX, shadowOffsetY, shadowBlur, shadowColor)) {
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
        return font_weight::mapStringToArkUIFontWeight(weight.get<std::string>());
    } else if (weight.is_number_integer()) {
        return font_weight::mapNumericToArkUIFontWeight(weight.get<int>());
    }
    return ARKUI_FONT_WEIGHT_NORMAL;
}

int32_t TextComponent::mapTextAlign(const std::string& align) {
    // Extract the horizontal token (first whitespace-separated segment) so that
    // two-axis CSS values like "center top" / "center center" / "right bottom"
    // still resolve to the correct horizontal alignment. This matches the
    // Android `StyleHelper.parseTextAlign` behaviour which also splits on
    // whitespace and uses `parts[0]` for the horizontal axis.
    //
    // The vertical token is currently dropped on Harmony because the ArkUI
    // Text node has no built-in vertical text-alignment attribute that mirrors
    // Android `Gravity.TOP/CENTER_VERTICAL/BOTTOM`.
    const auto firstNonSpace = align.find_first_not_of(" \t");
    if (firstNonSpace == std::string::npos) {
        return ARKUI_TEXT_ALIGNMENT_START;
    }
    const auto tokenEnd = align.find_first_of(" \t", firstNonSpace);
    std::string token = align.substr(
        firstNonSpace,
        tokenEnd == std::string::npos ? std::string::npos : tokenEnd - firstNonSpace);
    for (auto& c : token) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (token == "center") {
        return ARKUI_TEXT_ALIGNMENT_CENTER;
    } else if (token == "end" || token == "right") {
        return ARKUI_TEXT_ALIGNMENT_END;
    }
    // Default to START (covers "left", "start", and any unrecognised value).
    return ARKUI_TEXT_ALIGNMENT_START;
}

} // namespace a2ui
