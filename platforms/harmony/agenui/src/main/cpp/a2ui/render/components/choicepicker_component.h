#pragma once

#include "../a2ui_component.h"
#include "choicepicker_component_utils.h"
#include <vector>

namespace a2ui {

/** Native handles and metadata for a single visible option item. */
struct ChoicePickerOptionItem {
    ArkUI_NodeHandle containerHandle = nullptr; // Clickable container
    ArkUI_NodeHandle indicatorHandle = nullptr; // CheckBox or +/- text
    ArkUI_NodeHandle labelHandle = nullptr;     // Label text
    size_t optionIndex = 0;                     // Original option index
    std::string label;                          // Option label
    std::string value;                          // Option value
};

/**
 * Choice picker component with checkbox and chips display modes.
 *
 * Layout structure (composite layout):
 *   ARKUI_NODE_COLUMN (root node)
 *     ├── ARKUI_NODE_TEXT_INPUT (optional search input when filterable=true)
 *     └── option container (COLUMN / ROW, decided by config)
 *           ├── ARKUI_NODE_ROW (checkbox row or chip item)
 *           │     ├── ARKUI_NODE_CHECKBOX / ARKUI_NODE_TEXT (indicator)
 *           │     └── ARKUI_NODE_TEXT (label)
 *           ├── ...
 *
 * Note: chips display style stacks chips vertically (one per row) on Harmony
 * because the Harmony render layer here does not introduce ARKUI_NODE_FLEX,
 * matching the iOS / Android implementations that also avoid Flex.
 *
 * Supported properties:
 *   - variant: "mutuallyExclusive" for single-select or "multipleSelection" for multi-select
 *   - displayStyle: "checkbox" (default) or "chips"
 *   - filterable: whether to show an inline search input
 *   - options: option array [{label, value}, ...]
 *   - value: selected value, either a string or an array of strings
 *   - styles.orientation: "vertical" by default, or "horizontal" (checkbox mode)
 */
class ChoicePickerComponent : public A2UIComponent {
public:
    ChoicePickerComponent(const std::string& id, const nlohmann::json& properties);
    ~ChoicePickerComponent() override;

protected:
    void onUpdateProperties(const nlohmann::json& properties) override;

private:
    /** Rebuild the whole content tree under the root container. */
    void rebuildContent();

    /** Build or rebuild the visible option nodes. */
    void rebuildOptions();

    /** Create the optional search input when filterable=true. */
    void createSearchInput();

    /** Create the option container matching the active display mode. */
    void createOptionsContainer();

    /** Build checkbox-style option UI from visible indices. */
    void buildCheckboxOptions(const std::vector<size_t>& visibleIndices);

    /** Build chips-style option UI from visible indices. */
    void buildChipOptions(const std::vector<size_t>& visibleIndices);

    /** Remove all visible option nodes. */
    void clearOptions();

    /** Remove the search input and option container. */
    void clearContent();

    /** Apply the selected state to each visible option. */
    void applySelectedValues();

    /** Update a single option's appearance. */
    void applyOptionSelectedState(ChoicePickerOptionItem& item, bool selected);

    /** Handle container clicks for checkbox/chip options. */
    void handleOptionClick(size_t optionIndex);

    /** Handle runtime search keyword changes. */
    void handleSearchKeywordChange(const std::string& keyword);

    /** Return whether the current checks state disables editing. */
    bool isInteractionDisabled() const;

    /** Push the current local value back to the data model. */
    void syncCurrentValue();

    /** Read a numeric style token from the ChoicePicker style config. */
    float getStyleDimension(const char* key, float fallbackValue) const;

    /** Read a color style token from the ChoicePicker style config. */
    uint32_t getStyleColor(const char* key, uint32_t fallbackValue) const;

    /** Read a string style token from the ChoicePicker style config. */
    std::string getStyleString(const char* key, const std::string& fallbackValue) const;

    /** Shared option click callback. */
    static void onOptionClickCallback(ArkUI_NodeEvent* event);

    /** Search input text-change callback. */
    static void onSearchInputChangeCallback(ArkUI_NodeEvent* event);

    ChoicePickerConfig m_config;
    nlohmann::json m_styleConfig;
    std::vector<ChoicePickerOptionData> m_options;
    std::vector<ChoicePickerOptionItem> m_optionItems;
    // Inner wrapper used to keep search input + options container the same width.
    // Wrapper auto-sizes to the widest child; search + options-container then
    // stretch to wrapper width. See rebuildContent() for details.
    ArkUI_NodeHandle m_contentWrapperHandle = nullptr;
    ArkUI_NodeHandle m_searchInputHandle = nullptr;
    ArkUI_NodeHandle m_optionsContainerHandle = nullptr;
    std::string m_filterKeyword;
};

} // namespace a2ui
