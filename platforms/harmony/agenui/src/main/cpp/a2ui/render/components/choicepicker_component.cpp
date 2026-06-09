#include "choicepicker_component.h"

#include "../a2ui_node.h"
#include "../../utils/a2ui_color_palette.h"
#include "log/a2ui_capi_log.h"
#include "a2ui/measure/a2ui_platform_layout_bridge.h"
#include "a2ui/utils/a2ui_unit_utils.h"
#include "surface/token_parser/agenui_token_parser.h"
#include <nlohmann/json.hpp>

#include <cstdlib>

namespace a2ui {

namespace {

constexpr float kDefaultCheckboxSize = 32.0f;
constexpr float kDefaultCheckboxMargin = 8.0f;
constexpr float kDefaultCheckboxItemPadding = 16.0f;
constexpr float kDefaultTextMargin = 16.0f;
constexpr float kDefaultTextSize = 32.0f;
constexpr float kDefaultChoiceGap = 40.0f;

constexpr float kDefaultChipPaddingHorizontal = 28.0f;
constexpr float kDefaultChipPaddingVertical = 16.0f;
constexpr float kDefaultChipGap = 24.0f;
constexpr float kDefaultChipRadius = 999.0f;
constexpr float kDefaultChipBorderWidth = 2.0f;
constexpr float kDefaultChipIndicatorGap = 16.0f;

constexpr float kDefaultSearchHeight = 96.0f;
constexpr float kDefaultSearchPaddingHorizontal = 24.0f;
constexpr float kDefaultSearchPaddingVertical = 16.0f;
constexpr float kDefaultSearchBorderWidth = 2.0f;
// Use a very large radius so ArkUI clamps to half of the search-input height,
// producing a capsule shape (matches iOS / Android visual).
constexpr float kDefaultSearchRadius = 999.0f;
constexpr float kDefaultSearchMarginBottom = 24.0f;
// Horizontal inset between the search input and the surrounding ChoicePicker
// edges; keeps the input from spanning the full screen width.
constexpr float kDefaultSearchMarginHorizontal = 24.0f;
constexpr float kDefaultSearchTextSize = 28.0f;

constexpr uint32_t kDefaultSelectedBlue = 0xFF2E82FF;
constexpr uint32_t kDefaultChipUnselectedBackground = 0xFFF0F1F3;
constexpr uint32_t kDefaultChipUnselectedBorder = 0xFFE0E3E8;
constexpr uint32_t kDefaultChipUnselectedText = 0xFF666666;
constexpr uint32_t kDefaultSearchBackground = 0xFFFFFFFF;
constexpr uint32_t kDefaultSearchBorder = 0x1A000000;
constexpr uint32_t kDefaultPlaceholderColor = 0x66000000;

void disposeNodeIfNeeded(ArkUI_NodeHandle& nodeHandle) {
    if (!nodeHandle) {
        return;
    }

    g_nodeAPI->disposeNode(nodeHandle);
    nodeHandle = nullptr;
}

} // namespace

ChoicePickerComponent::ChoicePickerComponent(const std::string& id, const nlohmann::json& properties)
    : A2UIComponent(id, "ChoicePicker") {

    // Root is always a vertical column so the optional search input can sit on top
    // of the options container. Variant / displayStyle / orientation only influence
    // the inner option container, created later in rebuildContent().
    //
    // align-items=START (rather than STRETCH) keeps the inner content wrapper at
    // its natural width (the widest option), instead of forcing it to span the
    // full ChoicePicker width. This is what visually keeps the search input the
    // same width as the longest option below it.
    m_nodeHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);
    A2UIColumnNode(m_nodeHandle).setAlignItems(ARKUI_ITEM_ALIGNMENT_START);

    if (!properties.is_null() && properties.is_object()) {
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            m_properties[it.key()] = it.value();
        }
    }

    HM_LOGI("ChoicePickerComponent - Created: id=%s", id.c_str());
}

ChoicePickerComponent::~ChoicePickerComponent() {
    clearContent();
    HM_LOGI("ChoicePickerComponent - Destroyed: id=%s", m_id.c_str());
}

void ChoicePickerComponent::onUpdateProperties(const nlohmann::json& properties) {
    (void)properties;

    if (!m_nodeHandle) {
        HM_LOGE("handle is null, id=%s", m_id.c_str());
        return;
    }

    m_config = parseChoicePickerConfig(m_properties);
    m_styleConfig = getComponentStylesFor("ChoicePicker");
    m_options = parseChoicePickerOptions(m_properties);

    if (!m_config.filterable) {
        m_filterKeyword.clear();
    }

    rebuildContent();

    HM_LOGI("ChoicePickerComponent - Applied properties: id=%s, options=%zu, displayStyle=%s, filterable=%d",
            m_id.c_str(), m_options.size(), m_config.displayStyle.c_str(), m_config.filterable ? 1 : 0);
}

void ChoicePickerComponent::rebuildContent() {
    clearContent();

    // Create the inner content wrapper. With align-items=STRETCH on the wrapper
    // and no fixed width, ArkUI auto-sizes the wrapper to its widest child. Both
    // search input and options-container then stretch to that width, ensuring
    // the search input visually matches the longest option's width.
    m_contentWrapperHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);
    if (m_contentWrapperHandle) {
        A2UIColumnNode(m_contentWrapperHandle).setAlignItems(ARKUI_ITEM_ALIGNMENT_STRETCH);
        g_nodeAPI->addChild(m_nodeHandle, m_contentWrapperHandle);
    }

    // Create options container first so createSearchInput / rebuildOptions can
    // anchor against the wrapper. Order in the wrapper is set explicitly below.
    createOptionsContainer();

    if (m_config.filterable) {
        createSearchInput();
    }

    rebuildOptions();

    const bool disabled = isInteractionDisabled();
    A2UINode(m_nodeHandle).setEnabled(!disabled);
    A2UINode(m_nodeHandle).setOpacity(disabled ? 0.5f : 1.0f);
}

void ChoicePickerComponent::rebuildOptions() {
    clearOptions();

    if (!m_optionsContainerHandle) {
        return;
    }

    const std::vector<size_t> visibleIndices = filterChoicePickerOptionIndices(m_options, m_filterKeyword);
    if (m_config.displayStyle == "chips") {
        buildChipOptions(visibleIndices);
    } else {
        buildCheckboxOptions(visibleIndices);
    }

    applySelectedValues();
}

void ChoicePickerComponent::createSearchInput() {
    m_searchInputHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT_INPUT);
    if (!m_searchInputHandle) {
        return;
    }

    A2UINode node(m_searchInputHandle);
    A2UITextInputNode input(m_searchInputHandle);

    // No setPercentWidth: the parent contentWrapper has align-items=STRETCH and
    // auto-sizes to the widest sibling (typically the longest option), so the
    // search input gets stretched to exactly that width. This is what matches
    // the search input width to "the longest option" visually.
    node.setHeight(getStyleDimension("search-height", kDefaultSearchHeight));
    node.setPadding(getStyleDimension("search-padding-vertical", kDefaultSearchPaddingVertical),
                    getStyleDimension("search-padding-horizontal", kDefaultSearchPaddingHorizontal),
                    getStyleDimension("search-padding-vertical", kDefaultSearchPaddingVertical),
                    getStyleDimension("search-padding-horizontal", kDefaultSearchPaddingHorizontal));
    // Bottom margin only — left/right are 0 because the search input now lives
    // inside the auto-sized contentWrapper and shares its edges with options.
    node.setMargin(0.0f, 0.0f,
                   getStyleDimension("search-margin-bottom", kDefaultSearchMarginBottom),
                   0.0f);
    node.setBackgroundColor(getStyleColor("search-background-color", kDefaultSearchBackground));
    node.setBorderRadius(getStyleDimension("search-border-radius", kDefaultSearchRadius));
    const float borderWidth = getStyleDimension("search-border-width", kDefaultSearchBorderWidth);
    node.setBorderWidth(borderWidth, borderWidth, borderWidth, borderWidth);
    node.setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
    node.setBorderColor(getStyleColor("search-border-color", kDefaultSearchBorder));

    // Hide the underline that ArkUI TEXT_INPUT shows by default.
    ArkUI_NumberValue underlineValue[] = {{.i32 = 0}};
    ArkUI_AttributeItem underlineItem = {underlineValue, 1};
    g_nodeAPI->setAttribute(m_searchInputHandle, NODE_TEXT_INPUT_SHOW_UNDERLINE, &underlineItem);

    input.setFontSize(getStyleDimension("search-text-size", kDefaultSearchTextSize));
    input.setFontColor(getStyleColor("search-text-color", colors::kColorBlack));
    input.setPlaceholder(getStyleString("search-placeholder-text", "Search"));
    input.setPlaceholderFontSize(getStyleDimension("search-text-size", kDefaultSearchTextSize));
    input.setPlaceholderColor(getStyleColor("search-placeholder-color", kDefaultPlaceholderColor));
    input.setEnterKeyType(ARKUI_ENTER_KEY_TYPE_SEARCH);
    if (!m_filterKeyword.empty()) {
        input.setTextContent(m_filterKeyword);
    }

    g_nodeAPI->addNodeEventReceiver(m_searchInputHandle, onSearchInputChangeCallback);
    g_nodeAPI->registerNodeEvent(m_searchInputHandle, NODE_TEXT_INPUT_ON_CHANGE, 0, this);

    // Insert before options-container so the search input sits at the top of
    // the wrapper. createOptionsContainer is called before createSearchInput in
    // rebuildContent(), so m_optionsContainerHandle already exists here.
    if (m_contentWrapperHandle && m_optionsContainerHandle) {
        g_nodeAPI->insertChildBefore(m_contentWrapperHandle, m_searchInputHandle, m_optionsContainerHandle);
    } else if (m_contentWrapperHandle) {
        g_nodeAPI->addChild(m_contentWrapperHandle, m_searchInputHandle);
    } else {
        g_nodeAPI->addChild(m_nodeHandle, m_searchInputHandle);
    }
}

void ChoicePickerComponent::createOptionsContainer() {
    // chips and vertical-checkbox both use a COLUMN container; horizontal-checkbox uses a ROW.
    // chips on Harmony stack vertically (one per row) since the project does not
    // introduce ARKUI_NODE_FLEX (matching iOS/Android approaches).
    if (m_config.displayStyle != "chips" && m_config.orientation == "horizontal") {
        m_optionsContainerHandle = g_nodeAPI->createNode(ARKUI_NODE_ROW);
        if (m_optionsContainerHandle) {
            A2UIRowNode(m_optionsContainerHandle).setAlignItems(ARKUI_VERTICAL_ALIGNMENT_CENTER);
        }
    } else {
        m_optionsContainerHandle = g_nodeAPI->createNode(ARKUI_NODE_COLUMN);
        if (m_optionsContainerHandle) {
            A2UIColumnNode(m_optionsContainerHandle).setAlignItems(
                m_config.displayStyle == "chips"
                    ? ARKUI_ITEM_ALIGNMENT_START
                    : ARKUI_ITEM_ALIGNMENT_STRETCH);
        }
    }

    if (!m_optionsContainerHandle) {
        return;
    }

    // No percentWidth: options-container auto-sizes to its widest option. Then
    // the parent contentWrapper picks up that width and stretches the search
    // input to match it.
    if (m_contentWrapperHandle) {
        g_nodeAPI->addChild(m_contentWrapperHandle, m_optionsContainerHandle);
    } else {
        g_nodeAPI->addChild(m_nodeHandle, m_optionsContainerHandle);
    }
}

void ChoicePickerComponent::buildCheckboxOptions(const std::vector<size_t>& visibleIndices) {
    const float checkboxSize = getStyleDimension("checkbox-size", kDefaultCheckboxSize);
    const float checkboxMargin = getStyleDimension("checkbox-margin", kDefaultCheckboxMargin);
    const float itemPadding = getStyleDimension("checkbox-item-padding", kDefaultCheckboxItemPadding);
    const float textMargin = getStyleDimension("text-margin", kDefaultTextMargin);
    const float textSize = getStyleDimension("text-size", kDefaultTextSize);
    const float choiceGap = getStyleDimension("choice-gap", kDefaultChoiceGap);
    const uint32_t selectedColor = getStyleColor("checkbox-background-color-selected", kDefaultSelectedBlue);
    const uint32_t unselectedColor = getStyleColor("checkbox-border-color", 0x1A000000);
    const uint32_t textColor = getStyleColor("text-color", colors::kColorBlack);

    size_t visiblePosition = 0;
    for (size_t optionIndex : visibleIndices) {
        const ChoicePickerOptionData& option = m_options[optionIndex];
        ChoicePickerOptionItem item;
        item.optionIndex = optionIndex;
        item.label = option.label;
        item.value = option.value;

        item.containerHandle = g_nodeAPI->createNode(ARKUI_NODE_ROW);
        item.indicatorHandle = g_nodeAPI->createNode(ARKUI_NODE_CHECKBOX);
        item.labelHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
        if (!item.containerHandle || !item.indicatorHandle || !item.labelHandle) {
            disposeNodeIfNeeded(item.labelHandle);
            disposeNodeIfNeeded(item.indicatorHandle);
            disposeNodeIfNeeded(item.containerHandle);
            continue;
        }

        A2UIRowNode(item.containerHandle).setAlignItems(ARKUI_VERTICAL_ALIGNMENT_CENTER);
        A2UINode(item.containerHandle).setPadding(itemPadding);
        // Each option uses content-width (no percentWidth). Within the COLUMN
        // options-container with align-items=STRETCH, all options visually
        // stretch to the widest option's width — and the search input above
        // (sibling in the contentWrapper) also stretches to that same width.
        if (visiblePosition > 0) {
            if (m_config.orientation == "horizontal") {
                A2UINode(item.containerHandle).setMargin(0.0f, 0.0f, 0.0f, choiceGap);
            } else {
                A2UINode(item.containerHandle).setMargin(choiceGap, 0.0f, 0.0f, 0.0f);
            }
        }

        A2UICheckboxNode(item.indicatorHandle).setShape(ArkUI_CHECKBOX_SHAPE_ROUNDED_SQUARE);
        A2UICheckboxNode(item.indicatorHandle).setSelectedColor(selectedColor);
        A2UICheckboxNode(item.indicatorHandle).setUnselectedColor(unselectedColor);
        A2UINode(item.indicatorHandle).setWidth(checkboxSize);
        A2UINode(item.indicatorHandle).setHeight(checkboxSize);
        A2UINode(item.indicatorHandle).setMargin(checkboxMargin, checkboxMargin,
                                                 checkboxMargin, checkboxMargin);
        // ArkUI Checkbox has a built-in tap-to-toggle handler that bypasses the
        // parent's NODE_ON_CLICK. Use HitTestMode.None so the checkbox does not
        // respond to touches at all; the parent row handles the click and the
        // checked state is driven programmatically via setChecked() in
        // applyOptionSelectedState. This keeps mutuallyExclusive variant working
        // when the user taps directly on the checkbox.
        A2UINode(item.indicatorHandle).setHitTestBehavior(ARKUI_HIT_TEST_MODE_NONE);

        A2UITextNode(item.labelHandle).setTextContent(item.label);
        A2UITextNode(item.labelHandle).setFontSize(textSize);
        A2UITextNode(item.labelHandle).setFontColor(textColor);
        A2UINode(item.labelHandle).setMargin(0.0f, 0.0f, 0.0f, textMargin);
        if (m_config.orientation != "horizontal") {
            A2UINode(item.labelHandle).setLayoutWeight(1.0f);
        }
        A2UINode(item.labelHandle).setHitTestBehavior(ARKUI_HIT_TEST_MODE_TRANSPARENT);

        g_nodeAPI->addChild(item.containerHandle, item.indicatorHandle);
        g_nodeAPI->addChild(item.containerHandle, item.labelHandle);
        g_nodeAPI->addNodeEventReceiver(item.containerHandle, onOptionClickCallback);
        g_nodeAPI->registerNodeEvent(item.containerHandle, NODE_ON_CLICK, static_cast<int32_t>(optionIndex), this);
        g_nodeAPI->addChild(m_optionsContainerHandle, item.containerHandle);

        m_optionItems.push_back(item);
        visiblePosition++;
    }
}

void ChoicePickerComponent::buildChipOptions(const std::vector<size_t>& visibleIndices) {
    const float chipPaddingHorizontal = getStyleDimension("chip-padding-horizontal", kDefaultChipPaddingHorizontal);
    const float chipPaddingVertical = getStyleDimension("chip-padding-vertical", kDefaultChipPaddingVertical);
    const float chipGap = getStyleDimension("chip-gap", kDefaultChipGap);
    const float chipRadius = getStyleDimension("chip-border-radius", kDefaultChipRadius);
    const float chipBorderWidth = getStyleDimension("chip-border-width", kDefaultChipBorderWidth);
    const float chipIndicatorGap = getStyleDimension("chip-indicator-gap", kDefaultChipIndicatorGap);
    const float textSize = getStyleDimension("text-size", kDefaultTextSize);

    size_t visiblePosition = 0;
    for (size_t optionIndex : visibleIndices) {
        const ChoicePickerOptionData& option = m_options[optionIndex];
        ChoicePickerOptionItem item;
        item.optionIndex = optionIndex;
        item.label = option.label;
        item.value = option.value;

        item.containerHandle = g_nodeAPI->createNode(ARKUI_NODE_ROW);
        item.indicatorHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
        item.labelHandle = g_nodeAPI->createNode(ARKUI_NODE_TEXT);
        if (!item.containerHandle || !item.indicatorHandle || !item.labelHandle) {
            disposeNodeIfNeeded(item.labelHandle);
            disposeNodeIfNeeded(item.indicatorHandle);
            disposeNodeIfNeeded(item.containerHandle);
            continue;
        }

        A2UIRowNode(item.containerHandle).setAlignItems(ARKUI_VERTICAL_ALIGNMENT_CENTER);
        A2UINode(item.containerHandle).setPadding(chipPaddingVertical, chipPaddingHorizontal,
                                                  chipPaddingVertical, chipPaddingHorizontal);
        A2UINode(item.containerHandle).setBorderRadius(chipRadius);
        A2UINode(item.containerHandle).setBorderWidth(chipBorderWidth, chipBorderWidth,
                                                      chipBorderWidth, chipBorderWidth);
        A2UINode(item.containerHandle).setBorderStyle(ARKUI_BORDER_STYLE_SOLID);
        if (visiblePosition > 0) {
            // Vertical gap between chips since chips stack one per row on Harmony.
            A2UINode(item.containerHandle).setMargin(chipGap, 0.0f, 0.0f, 0.0f);
        }

        A2UITextNode(item.labelHandle).setTextContent(item.label);
        A2UITextNode(item.labelHandle).setFontSize(textSize);
        A2UINode(item.labelHandle).setHitTestBehavior(ARKUI_HIT_TEST_MODE_TRANSPARENT);

        A2UITextNode(item.indicatorHandle).setTextContent("+");
        A2UITextNode(item.indicatorHandle).setFontSize(textSize);
        A2UINode(item.indicatorHandle).setMargin(0.0f, 0.0f, 0.0f, chipIndicatorGap);
        A2UINode(item.indicatorHandle).setHitTestBehavior(ARKUI_HIT_TEST_MODE_TRANSPARENT);

        g_nodeAPI->addChild(item.containerHandle, item.labelHandle);
        g_nodeAPI->addChild(item.containerHandle, item.indicatorHandle);
        g_nodeAPI->addNodeEventReceiver(item.containerHandle, onOptionClickCallback);
        g_nodeAPI->registerNodeEvent(item.containerHandle, NODE_ON_CLICK, static_cast<int32_t>(optionIndex), this);
        g_nodeAPI->addChild(m_optionsContainerHandle, item.containerHandle);

        m_optionItems.push_back(item);
        visiblePosition++;
    }
}

void ChoicePickerComponent::clearOptions() {
    for (auto& item : m_optionItems) {
        if (item.containerHandle) {
            g_nodeAPI->unregisterNodeEvent(item.containerHandle, NODE_ON_CLICK);
        }
        if (m_optionsContainerHandle && item.containerHandle) {
            g_nodeAPI->removeChild(m_optionsContainerHandle, item.containerHandle);
        }
        if (item.labelHandle) {
            g_nodeAPI->disposeNode(item.labelHandle);
            item.labelHandle = nullptr;
        }
        if (item.indicatorHandle) {
            g_nodeAPI->disposeNode(item.indicatorHandle);
            item.indicatorHandle = nullptr;
        }
        if (item.containerHandle) {
            g_nodeAPI->disposeNode(item.containerHandle);
            item.containerHandle = nullptr;
        }
    }
    m_optionItems.clear();
}

void ChoicePickerComponent::clearContent() {
    clearOptions();

    if (m_searchInputHandle) {
        g_nodeAPI->unregisterNodeEvent(m_searchInputHandle, NODE_TEXT_INPUT_ON_CHANGE);
        // Search input now lives inside m_contentWrapperHandle (when it exists);
        // fall back to m_nodeHandle for safety.
        if (m_contentWrapperHandle) {
            g_nodeAPI->removeChild(m_contentWrapperHandle, m_searchInputHandle);
        } else if (m_nodeHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, m_searchInputHandle);
        }
        g_nodeAPI->disposeNode(m_searchInputHandle);
        m_searchInputHandle = nullptr;
    }

    if (m_optionsContainerHandle) {
        if (m_contentWrapperHandle) {
            g_nodeAPI->removeChild(m_contentWrapperHandle, m_optionsContainerHandle);
        } else if (m_nodeHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, m_optionsContainerHandle);
        }
        g_nodeAPI->disposeNode(m_optionsContainerHandle);
        m_optionsContainerHandle = nullptr;
    }

    if (m_contentWrapperHandle) {
        if (m_nodeHandle) {
            g_nodeAPI->removeChild(m_nodeHandle, m_contentWrapperHandle);
        }
        g_nodeAPI->disposeNode(m_contentWrapperHandle);
        m_contentWrapperHandle = nullptr;
    }
}

void ChoicePickerComponent::applySelectedValues() {
    nlohmann::json currentValue = m_properties.contains("value")
        ? m_properties["value"]
        : (m_config.variant == "multipleSelection" ? nlohmann::json::array() : nlohmann::json(""));

    // Coerce value to the type expected by the active variant. This mirrors iOS
    // (`value as? String` / `value as? [String]`) and Android (`value instanceof String`
    // / `instanceof List`) behaviour: when the host sends a value whose type does
    // not match the variant (e.g. mutuallyExclusive + value=["a"]), treat it as
    // "no selection" rather than coercing the array's first item into a default
    // selection. Otherwise the picker would silently default-select the first
    // matching option even though the variant cannot represent multi-values.
    if (m_config.variant == "multipleSelection") {
        if (!currentValue.is_array()) {
            currentValue = nlohmann::json::array();
        }
    } else {
        if (!currentValue.is_string()) {
            currentValue = nlohmann::json("");
        }
    }

    for (auto& item : m_optionItems) {
        applyOptionSelectedState(item, isChoicePickerValueSelected(currentValue, item.value));
    }
}

void ChoicePickerComponent::applyOptionSelectedState(ChoicePickerOptionItem& item, bool selected) {
    if (m_config.displayStyle == "chips") {
        A2UINode(item.containerHandle).setBackgroundColor(
            selected
                ? getStyleColor("chip-background-color-selected", kDefaultSelectedBlue)
                : getStyleColor("chip-background-color", kDefaultChipUnselectedBackground));
        A2UINode(item.containerHandle).setBorderColor(
            selected
                ? getStyleColor("chip-border-color-selected", kDefaultSelectedBlue)
                : getStyleColor("chip-border-color", kDefaultChipUnselectedBorder));

        A2UITextNode(item.labelHandle).setFontColor(
            selected
                ? getStyleColor("chip-text-color-selected", 0xFFFFFFFF)
                : getStyleColor("chip-text-color", kDefaultChipUnselectedText));
        A2UITextNode(item.indicatorHandle).setTextContent(selected ? "x" : "+");
        A2UITextNode(item.indicatorHandle).setFontColor(
            selected
                ? getStyleColor("chip-text-color-selected", 0xFFFFFFFF)
                : getStyleColor("chip-text-color", kDefaultChipUnselectedText));
        return;
    }

    A2UICheckboxNode(item.indicatorHandle).setChecked(selected);
}

void ChoicePickerComponent::handleOptionClick(size_t optionIndex) {
    if (isInteractionDisabled() || optionIndex >= m_options.size()) {
        return;
    }

    const nlohmann::json currentValue = m_properties.contains("value")
        ? m_properties["value"]
        : (m_config.variant == "multipleSelection" ? nlohmann::json::array() : nlohmann::json(""));

    const bool currentlySelected = isChoicePickerValueSelected(currentValue, m_options[optionIndex].value);
    const bool nextSelected = (m_config.variant == "multipleSelection") ? !currentlySelected : true;

    m_properties["value"] = updateChoicePickerValue(
        currentValue,
        m_config,
        m_options[optionIndex].value,
        nextSelected);

    applySelectedValues();
    syncCurrentValue();
}

void ChoicePickerComponent::handleSearchKeywordChange(const std::string& keyword) {
    if (m_filterKeyword == keyword) {
        return;
    }

    m_filterKeyword = keyword;
    rebuildOptions();
}

bool ChoicePickerComponent::isInteractionDisabled() const {
    if (!m_properties.contains("checks") || !m_properties["checks"].is_object()) {
        return false;
    }

    const auto& checks = m_properties["checks"];
    if (!checks.contains("result")) {
        return false;
    }

    const auto& result = checks["result"];
    if (result.is_boolean()) {
        return !result.get<bool>();
    }
    if (result.is_string()) {
        return result.get<std::string>() != "true";
    }
    return false;
}

void ChoicePickerComponent::syncCurrentValue() {
    nlohmann::json changeJson;
    changeJson["value"] = m_properties.contains("value")
        ? m_properties["value"]
        : (m_config.variant == "multipleSelection" ? nlohmann::json::array() : nlohmann::json(""));
    syncState(changeJson);
}

float ChoicePickerComponent::getStyleDimension(const char* key, float fallbackValue) const {
    if (!m_styleConfig.is_object() || !m_styleConfig.contains(key)) {
        return fallbackValue;
    }

    const auto& value = m_styleConfig[key];
    if (value.is_number()) {
        return value.get<float>();
    }
    if (value.is_string()) {
        return static_cast<float>(std::atof(value.get<std::string>().c_str()));
    }
    return fallbackValue;
}

uint32_t ChoicePickerComponent::getStyleColor(const char* key, uint32_t fallbackValue) const {
    if (!m_styleConfig.is_object() || !m_styleConfig.contains(key)) {
        return fallbackValue;
    }
    if (m_styleConfig[key].is_string()) {
        return parseColor(m_styleConfig[key].get<std::string>());
    } else if (m_styleConfig[key].is_object()) {
        if (m_styleConfig[key].contains("call") && m_styleConfig[key]["call"].is_string()) {
            std::string callType = m_styleConfig[key]["call"].get<std::string>();
            if (callType == "token" && m_styleConfig[key].contains("args")) {
                const auto& args = m_styleConfig[key]["args"];
                if (args.contains("name") && args["name"].is_string()) {
                    std::string tokenName = args["name"].get<std::string>();
                    std::string resolvedColor = agenui::TokenParser::getInstance().resolve(tokenName);
                    HM_LOGI("Resolved FunctionCall token '%s' to '%s'", tokenName.c_str(), resolvedColor.c_str());
                    return parseColor(resolvedColor);
                }
            }
        }
    }
    return fallbackValue;
}

std::string ChoicePickerComponent::getStyleString(const char* key, const std::string& fallbackValue) const {
    if (!m_styleConfig.is_object() || !m_styleConfig.contains(key) || !m_styleConfig[key].is_string()) {
        return fallbackValue;
    }
    return m_styleConfig[key].get<std::string>();
}

void ChoicePickerComponent::onOptionClickCallback(ArkUI_NodeEvent* event) {
    if (!event || OH_ArkUI_NodeEvent_GetEventType(event) != ArkUI_NodeEventType::NODE_ON_CLICK) {
        return;
    }

    auto* component = static_cast<ChoicePickerComponent*>(OH_ArkUI_NodeEvent_GetUserData(event));
    if (!component) {
        return;
    }

    const int32_t optionIndex = OH_ArkUI_NodeEvent_GetTargetId(event);
    if (optionIndex < 0) {
        return;
    }

    component->handleOptionClick(static_cast<size_t>(optionIndex));
}

void ChoicePickerComponent::onSearchInputChangeCallback(ArkUI_NodeEvent* event) {
    if (!event || OH_ArkUI_NodeEvent_GetEventType(event) != ArkUI_NodeEventType::NODE_TEXT_INPUT_ON_CHANGE) {
        return;
    }

    auto* component = static_cast<ChoicePickerComponent*>(OH_ArkUI_NodeEvent_GetUserData(event));
    if (!component) {
        return;
    }

    ArkUI_StringAsyncEvent* textEvent = OH_ArkUI_NodeEvent_GetStringAsyncEvent(event);
    if (!textEvent || !textEvent->pStr) {
        return;
    }

    component->handleSearchKeywordChange(textEvent->pStr);
}

} // namespace a2ui
