#include "richtext_component.h"
#include "a2ui/third_party/Html.h"
#include "../../bridge/open_url_helper.h"
#include "../../utils/a2ui_font_weight_utils.h"
#include "../a2ui_node.h"
#include "a2ui/measure/a2ui_platform_layout_bridge.h"
#include "a2ui/utils/a2ui_unit_utils.h"
#include "a2ui/utils/a2ui_color_palette.h"
#include "a2ui/utils/a2ui_shadow_utils.h"
#include "a2ui/utils/hm_font_utils.h"
#include "log/a2ui_capi_log.h"
#include <cstdlib>
#include <cstring>

namespace a2ui {

static constexpr float kDefaultFontSize = 32.0f;

RichTextComponent::RichTextComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "RichText") {

    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);

    {
        A2UITextNode t(m_nodeHandle);
        t.setFontSize(kDefaultFontSize);
        t.setFontColor(colors::kColorBlack);
    }

    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    HM_LOGI( "RichTextComponent - Created: id=%s, handle=%s",
                id.c_str(), m_nodeHandle ? "valid" : "null");
}

RichTextComponent::~RichTextComponent() {
    cleanSpanNodes();
    HM_LOGI( "RichTextComponent - Destroyed: id=%s", m_id.c_str());
}

void RichTextComponent::onDestroy() {
    // Unregister link clicks before the node tree is released.
    for (auto& clickNode : m_clickNodes) {
        if (clickNode->handle) {
            g_nodeAPI->unregisterNodeEvent(clickNode->handle, NODE_ON_CLICK);
        }
    }
}

// ---- Property Updates ----

void RichTextComponent::onUpdateProperties(const nlohmann::json& properties) {
    if (!m_nodeHandle) {
        HM_LOGE( "handle is null, id=%s", m_id.c_str());
        return;
    }

    applyVariant(properties);
    applyStyles(properties);

    if (properties.find("text") != properties.end()) {
        std::string htmlContent;
        const auto& textValue = properties["text"];

        if (textValue.is_object()) {
            if (textValue.find("literalString") != textValue.end() && textValue["literalString"].is_string()) {
                htmlContent = textValue["literalString"].get<std::string>();
            }
        }
        else if (textValue.is_string()) {
            htmlContent = textValue.get<std::string>();
        }

        if (!htmlContent.empty()) {
            setHtmlContent(htmlContent);
            HM_LOGI( "Set HTML content, length=%zu, id=%s",
                        htmlContent.size(), m_id.c_str());
        }
    }
}

// ---- HTML Preprocessing ----

static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string RichTextComponent::preprocessHtml(const std::string& html) {
    std::string result = html;

    replaceAll(result, "</h1>", "</h1>\n");
    replaceAll(result, "</h2>", "</h2>\n");
    replaceAll(result, "</h3>", "</h3>\n");
    replaceAll(result, "</h4>", "</h4>\n");
    replaceAll(result, "</h5>", "</h5>\n");
    replaceAll(result, "</h6>", "</h6>\n");
    replaceAll(result, "</p>", "</p>\n");
    replaceAll(result, "</div>", "</div>\n");

    return result;
}

// ---- HTML Rendering ----

void RichTextComponent::setHtmlContent(const std::string& html) {
    cleanSpanNodes();

    std::string processedHtml = preprocessHtml(html);

    a2ui::Html ho(processedHtml);
    if (ho.isMalformed()) {
        HM_LOGW( "Malformed HTML, id=%s", m_id.c_str());
    }

    HM_LOGI( "Parsed %d spans, id=%s",
                ho.getSpanSize(), m_id.c_str());

    for (int i = 0; i < ho.getSpanSize(); i++) {
        a2ui::Html::Span* span = ho.getSpan(i);
        if (!span) continue;

        bool isImageSpan = false;
        for (const auto& t : span->_tag_list) {
            if (t._tagID == a2ui::Html::img) {
                isImageSpan = true;
                break;
            }
        }

        ArkUI_NodeType nodeType = isImageSpan ? ARKUI_NODE_IMAGE_SPAN : ARKUI_NODE_SPAN;
        auto spanNodeHandle = g_nodeAPI->createNode(nodeType);
        m_spanNodes.push_back(spanNodeHandle);

        if (isImageSpan) {
            createImageSpanNode(spanNodeHandle, span);
        } else {
            createTextSpanNode(spanNodeHandle, span);
        }

        g_nodeAPI->addChild(m_nodeHandle, spanNodeHandle);
    }

    registerLinkClicks();
}

void RichTextComponent::createImageSpanNode(ArkUI_NodeHandle spanHandle, const void* spanData) {
    auto* span = static_cast<const a2ui::Html::Span*>(spanData);

    for (const auto& t : span->_tag_list) {
        if (t._tagID != a2ui::Html::img) continue;

        std::string imgId;
        std::string imgSrc;
        std::string customEmoji;
        int imgWidth = 60;
        int imgHeight = 60;

        for (const auto& kv : t._attributes) {
            if (kv.first == "id") {
                imgId = kv.second;
            } else if (kv.first == "src") {
                imgSrc = kv.second;
            } else if (kv.first == "width") {
                imgWidth = std::atoi(kv.second.c_str());
                if (imgWidth <= 0) imgWidth = 300;
            } else if (kv.first == "height") {
                imgHeight = std::atoi(kv.second.c_str());
                if (imgHeight <= 0) imgHeight = 200;
            } else if (kv.first == "customEmoji") {
                customEmoji = kv.second;
            }
        }

        if (customEmoji == "true") {
            HM_LOGI( "RichTextComponent - customEmoji detected, id=%s", imgId.c_str());
        }

        ArkUI_NumberValue alignVal[] = {{.i32 = ARKUI_IMAGE_SPAN_ALIGNMENT_CENTER}};
        ArkUI_AttributeItem alignItem = {alignVal, 1, nullptr, nullptr};
        g_nodeAPI->setAttribute(spanHandle, NODE_IMAGE_SPAN_VERTICAL_ALIGNMENT, &alignItem);

        A2UINode imgSpan(spanHandle);
        imgSpan.setWidth(static_cast<float>(imgWidth));
        imgSpan.setHeight(static_cast<float>(imgHeight));

        if (!imgSrc.empty() && imgWidth > 0 && imgHeight > 0) {
            ArkUI_AttributeItem srcItem = {nullptr, 0, imgSrc.c_str(), nullptr};
            g_nodeAPI->setAttribute(spanHandle, NODE_IMAGE_SRC, &srcItem);

            HM_LOGI( "RichTextComponent - ImageSpan id=%s, src=%s, width=%d, height=%d, customEmoji=%s",
                        imgId.c_str(), imgSrc.c_str(), imgWidth, imgHeight, customEmoji.c_str());
        } else {
            HM_LOGE( "RichTextComponent - Invalid ImageSpan: src=%s, width=%d, height=%d",
                         imgSrc.c_str(), imgWidth, imgHeight);
        }

        break;
    }
}

void RichTextComponent::createTextSpanNode(ArkUI_NodeHandle spanHandle, const void* spanData) {
    auto* span = static_cast<const a2ui::Html::Span*>(spanData);

    {
        ArkUI_AttributeItem familyItem = {nullptr, 0, harmonyDefaultFontFamily().c_str(), nullptr};
        g_nodeAPI->setAttribute(spanHandle, NODE_FONT_FAMILY, &familyItem);
    }

    ArkUI_AttributeItem textItem = {nullptr, 0, span->_text.c_str(), nullptr};
    g_nodeAPI->setAttribute(spanHandle, NODE_SPAN_CONTENT, &textItem);

    uint32_t fontColor = m_fontColor;
    int32_t decorationType = ARKUI_TEXT_DECORATION_TYPE_NONE;

    for (const auto& t : span->_tag_list) {
        switch (t._tagID) {
            case a2ui::Html::font: {
                for (const auto& kv : t._attributes) {
                    if (kv.first == "color") {
                        fontColor = parseColor(kv.second);
                    } else if (kv.first == "size") {
                        std::string sizeStr = kv.second;
                        size_t pxPos = sizeStr.rfind("px");
                        if (pxPos != std::string::npos) {
                            sizeStr = sizeStr.substr(0, pxPos);
                        }
                        float fontSize = static_cast<float>(std::atof(sizeStr.c_str()));
                        if (fontSize > 0) {
                            ArkUI_NumberValue sizeVal[] = {{.f32 = UnitConverter::a2uiToVp(fontSize)}};
                            ArkUI_AttributeItem sizeItem = {sizeVal, 1, nullptr, nullptr};
                            g_nodeAPI->setAttribute(spanHandle, NODE_FONT_SIZE, &sizeItem);
                        }
                    } else if (kv.first == "face") {
                        const std::string fontFamily = normalizeHarmonyFontFamily(kv.second);
                        ArkUI_AttributeItem faceItem = {nullptr, 0, fontFamily.c_str(), nullptr};
                        g_nodeAPI->setAttribute(spanHandle, NODE_FONT_FAMILY, &faceItem);
                    } else if (kv.first == "font-weight") {
                        int32_t weight = font_weight::mapStringToArkUIFontWeight(kv.second);
                        ArkUI_NumberValue weightVal[] = {{.i32 = weight}};
                        ArkUI_AttributeItem weightItem = {weightVal, 1, nullptr, nullptr};
                        g_nodeAPI->setAttribute(spanHandle, NODE_FONT_WEIGHT, &weightItem);
                    } else if (kv.first == "background-color") {
                        uint32_t bgColor = parseColor(kv.second);
                        ArkUI_NumberValue bgVal[] = {{.u32 = bgColor}};
                        ArkUI_AttributeItem bgItem = {bgVal, 1, nullptr, nullptr};
                        g_nodeAPI->setAttribute(spanHandle, NODE_BACKGROUND_COLOR, &bgItem);
                    }
                }
                break;
            }

            case a2ui::Html::b:
            case a2ui::Html::strong: {
                ArkUI_NumberValue weightVal[] = {{.i32 = ARKUI_FONT_WEIGHT_BOLD}};
                ArkUI_AttributeItem weightItem = {weightVal, 1, nullptr, nullptr};
                g_nodeAPI->setAttribute(spanHandle, NODE_FONT_WEIGHT, &weightItem);
                break;
            }

            case a2ui::Html::i: {
                ArkUI_NumberValue styleVal[] = {{.i32 = ARKUI_FONT_STYLE_ITALIC}};
                ArkUI_AttributeItem styleItem = {styleVal, 1, nullptr, nullptr};
                g_nodeAPI->setAttribute(spanHandle, NODE_FONT_STYLE, &styleItem);
                break;
            }

            case a2ui::Html::u: {
                decorationType = ARKUI_TEXT_DECORATION_TYPE_UNDERLINE;
                break;
            }

            case a2ui::Html::strike: {
                decorationType = ARKUI_TEXT_DECORATION_TYPE_LINE_THROUGH;
                break;
            }

            case a2ui::Html::a: {
                fontColor = 0xFF007FFF;
                decorationType = ARKUI_TEXT_DECORATION_TYPE_UNDERLINE;

                std::string id;
                std::string href;
                for (const auto& kv : t._attributes) {
                    if (kv.first == "id") {
                        id = kv.second;
                    } else if (kv.first == "href") {
                        href = kv.second;
                    } else if (kv.first == "face") {
                        const std::string fontFamily = normalizeHarmonyFontFamily(kv.second);
                        ArkUI_AttributeItem faceItem = {nullptr, 0, fontFamily.c_str(), nullptr};
                        g_nodeAPI->setAttribute(spanHandle, NODE_FONT_FAMILY, &faceItem);
                    }
                }

                m_clickNodes.push_back(std::unique_ptr<ClickNode>(
                    new ClickNode{this, spanHandle, id, href}));
                HM_LOGI( "RichTextComponent - Link found: id=%s, href=%s",
                            id.c_str(), href.c_str());
                break;
            }

            case a2ui::Html::br:
            case a2ui::Html::blockquote:
                break;

            case a2ui::Html::text:
            case a2ui::Html::img:
            case a2ui::Html::sub:
            case a2ui::Html::sup:
            case a2ui::Html::small:
                break;
        }
    }

    ArkUI_NumberValue colorVal[] = {{.u32 = fontColor}};
    ArkUI_AttributeItem colorItem = {colorVal, 1, nullptr, nullptr};
    g_nodeAPI->setAttribute(spanHandle, NODE_FONT_COLOR, &colorItem);

    if (decorationType != ARKUI_TEXT_DECORATION_TYPE_NONE) {
        ArkUI_NumberValue decoVal[] = {{.i32 = decorationType}, {.u32 = fontColor}};
        ArkUI_AttributeItem decoItem = {decoVal, 2, nullptr, nullptr};
        g_nodeAPI->setAttribute(spanHandle, NODE_TEXT_DECORATION, &decoItem);
    }
}


void RichTextComponent::registerLinkClicks() {
    for (auto& clickNode : m_clickNodes) {
        g_nodeAPI->addNodeEventReceiver(clickNode->handle, onLinkClickCallback);
        g_nodeAPI->registerNodeEvent(clickNode->handle, NODE_ON_CLICK, 0, clickNode.get());
    }

    if (!m_clickNodes.empty()) {
        HM_LOGI( "RichTextComponent - Registered %zu link click events, id=%s",
                    m_clickNodes.size(), m_id.c_str());
    }
}


void RichTextComponent::onLinkClickCallback(ArkUI_NodeEvent* event) {
    auto* clickNode = static_cast<ClickNode*>(OH_ArkUI_NodeEvent_GetUserData(event));
    if (!clickNode) {
        HM_LOGW( "userData is null");
        return;
    }

    HM_LOGI( "RichTextComponent - Link clicked: href=%s", clickNode->href.c_str());

    OpenUrlHelper::getInstance().openUrl(clickNode->href);
}


void RichTextComponent::cleanSpanNodes() {
    for (auto& clickNode : m_clickNodes) {
        if (clickNode->handle) {
            g_nodeAPI->unregisterNodeEvent(clickNode->handle, NODE_ON_CLICK);
        }
    }
    m_clickNodes.clear();

    for (auto& spanNode : m_spanNodes) {
        g_nodeAPI->removeChild(m_nodeHandle, spanNode);
        g_nodeAPI->disposeNode(spanNode);
    }
    m_spanNodes.clear();
}


void RichTextComponent::applyVariant(const nlohmann::json& properties) {
    if (properties.find("variant") == properties.end() || !properties["variant"].is_string()) {
        return;
    }

    std::string variant = properties["variant"].get<std::string>();

    float fontSize = kDefaultFontSize;
    int32_t fontWeight = ARKUI_FONT_WEIGHT_NORMAL;

    if (variant == "h1") {
        fontSize = 32.0f;
        fontWeight = ARKUI_FONT_WEIGHT_BOLD;
    } else if (variant == "h2") {
        fontSize = 28.0f;
        fontWeight = ARKUI_FONT_WEIGHT_BOLD;
    } else if (variant == "h3") {
        fontSize = 24.0f;
        fontWeight = ARKUI_FONT_WEIGHT_BOLD;
    } else if (variant == "h4") {
        fontSize = 20.0f;
        fontWeight = ARKUI_FONT_WEIGHT_BOLD;
    } else if (variant == "h5") {
        fontSize = 18.0f;
        fontWeight = ARKUI_FONT_WEIGHT_BOLD;
    } else if (variant == "caption") {
        fontSize = 12.0f;
    } else {
        fontSize = kDefaultFontSize;
    }

    ArkUI_NumberValue fontSizeVal[] = {{.f32 = UnitConverter::a2uiToVp(fontSize)}};
    ArkUI_AttributeItem fontSizeItem = {fontSizeVal, 1, nullptr, nullptr};
    g_nodeAPI->setAttribute(m_nodeHandle, NODE_FONT_SIZE, &fontSizeItem);

    ArkUI_NumberValue fontWeightVal[] = {{.i32 = fontWeight}};
    ArkUI_AttributeItem fontWeightItem = {fontWeightVal, 1, nullptr, nullptr};
    g_nodeAPI->setAttribute(m_nodeHandle, NODE_FONT_WEIGHT, &fontWeightItem);
}

// ---- Custom Styles ----

void RichTextComponent::applyStyles(const nlohmann::json& properties) {
    if (properties.find("styles") == properties.end() || !properties["styles"].is_object()) {
        return;
    }

    const auto& styles = properties["styles"];

    // color -> NODE_FONT_COLOR (cache for span inheritance)
    if (styles.find("color") != styles.end() && styles["color"].is_string()) {
        m_fontColor = parseColor(styles["color"].get<std::string>());
        ArkUI_NumberValue colorVal[] = {{.u32 = m_fontColor}};
        ArkUI_AttributeItem colorItem = {colorVal, 1, nullptr, nullptr};
        g_nodeAPI->setAttribute(m_nodeHandle, NODE_FONT_COLOR, &colorItem);
    }

    if (styles.find("font-family") != styles.end() && styles["font-family"].is_string()) {
        A2UITextNode(m_nodeHandle).setFontFamily(styles["font-family"].get<std::string>());
    } else if (styles.find("fontFamily") != styles.end() && styles["fontFamily"].is_string()) {
        A2UITextNode(m_nodeHandle).setFontFamily(styles["fontFamily"].get<std::string>());
    }

    // opacity
    if (styles.contains("opacity") && (styles["opacity"].is_number() || styles["opacity"].is_string())) {
        float opacity = 1.0f;
        if (styles["opacity"].is_number()) {
            opacity = styles["opacity"].get<float>();
        } else {
            opacity = static_cast<float>(std::atof(styles["opacity"].get<std::string>().c_str()));
        }
        if (opacity < 0.0f) opacity = 0.0f;
        if (opacity > 1.0f) opacity = 1.0f;
        A2UINode(m_nodeHandle).setOpacity(opacity);
    }

    // Apply background and border styles from base class
    applyBackgroundColor(properties);
    applyBorderStyles(properties);

    // filter: drop-shadow
    applyFilter(styles);
}

// ---- Filter: drop-shadow ----

void RichTextComponent::applyFilter(const nlohmann::json& styles) {
    if (!styles.contains("filter") || !styles["filter"].is_string()) return;

    std::string filterVal = styles["filter"].get<std::string>();

    auto params = parseDropShadow(filterVal);
    if (!params.valid) return;

    // Parse the shadow color.
    uint32_t color = parseColor(params.colorStr);
    if (color == colors::kColorTransparent && params.colorStr != "#00000000" && params.colorStr != "rgba(0, 0, 0, 0)" &&
        params.colorStr != "rgba(0,0,0,0)" && params.colorStr != "rgb(0, 0, 0)") {
        return;
    }

    // Apply the shadow.
    A2UINode(m_nodeHandle).setCustomShadow(params.blurRadius, params.offsetX, params.offsetY, color);
}

} // namespace a2ui
