#include "agenui_virtual_dom_choice_picker.h"
#include "agenui_virtual_dom_node.h"

#if defined(__OHOS__)

#include "agenui_a2ui_attribute_converter.h"
#include "agenui_component_snapshot.h"
#include "a2ui/render/components/choicepicker_component_utils.h"
#include "nlohmann/json.hpp"
#include "surface/agenui_serializable_data_impl.h"

namespace agenui {

namespace {

constexpr float kDefaultCheckboxSize = 32.0f;
constexpr float kDefaultCheckboxMargin = 8.0f;
constexpr float kDefaultCheckboxItemPadding = 16.0f;
constexpr float kDefaultTextMargin = 16.0f;
constexpr float kDefaultTextSize = 32.0f;
constexpr float kDefaultChoiceGap = 40.0f;
constexpr float kDefaultChipPaddingHorizontal = 28.0f;
constexpr float kDefaultChipPaddingVertical = 12.0f;
constexpr float kDefaultChipGap = 24.0f;
constexpr float kDefaultChipBorderWidth = 2.0f;
constexpr float kDefaultSearchHeight = 72.0f;
constexpr float kDefaultSearchMarginBottom = 20.0f;
constexpr float kDefaultSearchBorderWidth = 2.0f;

nlohmann::json serializableDataToJson(const SerializableData& value) {
    if (!value.isValid()) {
        return nlohmann::json();
    }

    try {
        return nlohmann::json::parse(value.dump());
    } catch (...) {
        if (value.isString()) {
            return value.asString();
        }
        if (value.isNumber()) {
            return value.asDouble();
        }
        if (value.isBool()) {
            return value.asBool();
        }
    }

    return nlohmann::json();
}

}  // namespace

IvirtualDomChoicePicker::IvirtualDomChoicePicker(
    const ComponentSnapshot& snap,
    const std::function<YGSize(const ComponentSnapshot&, float, int&)>& measureTextFunc,
    VirtualDOMNode* parentNode) {
    _measureTextFunc = measureTextFunc;
    _parentNode = parentNode;

    _yogaNode = YGNodeNew();
    YGNodeStyleSetFlexDirection(_yogaNode, YGFlexDirectionColumn);
    YGNodeStyleSetAlignItems(_yogaNode, YGAlignStretch);

    parseSnapshot(snap);
}

IvirtualDomChoicePicker::~IvirtualDomChoicePicker() {
    if (_yogaNode) {
        freeYogaTree(_yogaNode);
        _yogaNode = nullptr;
    }
}

void IvirtualDomChoicePicker::freeYogaTree(YGNodeRef node) {
    if (!node) {
        return;
    }

    uint32_t count = YGNodeGetChildCount(node);
    for (uint32_t i = 0; i < count; ++i) {
        freeYogaTree(YGNodeGetChild(node, i));
    }
    YGNodeFree(node);
}

void IvirtualDomChoicePicker::parseSnapshot(const ComponentSnapshot& snap) {
    snapshot = snap;
    options.clear();
    orientationHorizontal = false;
    displayStyle = "checkbox";
    filterable = false;

    nlohmann::json properties = nlohmann::json::object();

    auto copyAttribute = [&](const char* key) {
        auto it = snap.attributes.find(key);
        if (it != snap.attributes.end() && it->second.isValid()) {
            properties[key] = serializableDataToJson(it->second);
        }
    };

    copyAttribute("variant");
    copyAttribute("displayStyle");
    copyAttribute("filterable");
    copyAttribute(A2UIPropertyNames::kOptions);

    auto orientationIt = snap.styles.find(CSSPropertyNames::kOrientation);
    if (orientationIt != snap.styles.end() && orientationIt->second.isValid()) {
        properties["styles"] = {
            {CSSPropertyNames::kOrientation, serializableDataToJson(orientationIt->second)}
        };
    }

    const a2ui::ChoicePickerConfig config = a2ui::parseChoicePickerConfig(properties);
    orientationHorizontal = config.orientation == "horizontal";
    displayStyle = config.displayStyle;
    filterable = config.filterable;

    for (const auto& option : a2ui::parseChoicePickerOptions(properties)) {
        options.push_back(option.label);
    }
}

YGNodeRef IvirtualDomChoicePicker::createSearchNode() const {
    YGNodeRef searchNode = YGNodeNew();
    YGNodeStyleSetWidthPercent(searchNode, 100.0f);
    YGNodeStyleSetHeight(searchNode, getStyleDimension("search-height", kDefaultSearchHeight));
    YGNodeStyleSetMargin(searchNode, YGEdgeBottom,
                         getStyleDimension("search-margin-bottom", kDefaultSearchMarginBottom));
    YGNodeStyleSetBorder(searchNode, YGEdgeAll,
                         getStyleDimension("search-border-width", kDefaultSearchBorderWidth));
    YGNodeStyleSetFlexShrink(searchNode, 0.0f);
    return searchNode;
}

YGNodeRef IvirtualDomChoicePicker::createOptionsContainerNode() const {
    YGNodeRef optionsNode = YGNodeNew();
    YGNodeStyleSetWidthPercent(optionsNode, 100.0f);

    if (displayStyle == "chips") {
        YGNodeStyleSetFlexDirection(optionsNode, YGFlexDirectionRow);
        YGNodeStyleSetFlexWrap(optionsNode, YGWrapWrap);
        YGNodeStyleSetAlignItems(optionsNode, YGAlignCenter);
        YGNodeStyleSetGap(optionsNode, YGGutterAll, getStyleDimension("chip-gap", kDefaultChipGap));
        return optionsNode;
    }

    YGNodeStyleSetFlexDirection(optionsNode,
                                orientationHorizontal ? YGFlexDirectionRow : YGFlexDirectionColumn);
    YGNodeStyleSetAlignItems(optionsNode,
                             orientationHorizontal ? YGAlignCenter : YGAlignStretch);
    YGNodeStyleSetGap(optionsNode, YGGutterAll, getStyleDimension("choice-gap", kDefaultChoiceGap));
    return optionsNode;
}

YGNodeRef IvirtualDomChoicePicker::createTextMeasureNode(
    const std::shared_ptr<ChoicePickerUnitCell>& cell) const {
    YGNodeRef textNode = YGNodeNew();
    YGNodeSetContext(textNode, cell.get());
    YGNodeSetMeasureFunc(textNode, measureChoicePickerFunction);
    if (YGNodeHasMeasureFunc(textNode)) {
        YGNodeMarkDirty(textNode);
    }
    return textNode;
}

YGNodeRef IvirtualDomChoicePicker::createCheckboxItemNode(
    const std::shared_ptr<ChoicePickerUnitCell>& cell) const {
    YGNodeRef itemNode = YGNodeNew();
    YGNodeStyleSetFlexDirection(itemNode, YGFlexDirectionRow);
    YGNodeStyleSetAlignItems(itemNode, YGAlignCenter);
    YGNodeStyleSetPadding(itemNode, YGEdgeAll,
                          getStyleDimension("checkbox-item-padding", kDefaultCheckboxItemPadding));
    YGNodeStyleSetFlexShrink(itemNode, 0.0f);
    if (!orientationHorizontal) {
        YGNodeStyleSetWidthPercent(itemNode, 100.0f);
    }

    YGNodeRef checkboxNode = YGNodeNew();
    YGNodeStyleSetWidth(checkboxNode, getStyleDimension("checkbox-size", kDefaultCheckboxSize));
    YGNodeStyleSetHeight(checkboxNode, getStyleDimension("checkbox-size", kDefaultCheckboxSize));
    YGNodeStyleSetMargin(checkboxNode, YGEdgeAll,
                         getStyleDimension("checkbox-margin", kDefaultCheckboxMargin));
    YGNodeStyleSetBorder(checkboxNode, YGEdgeAll,
                         getStyleDimension("checkbox-border-width", 0.0f));
    YGNodeInsertChild(itemNode, checkboxNode, 0);

    cell->snapshot = snapshot;
    cell->snapshot.component = "ChoicePicker";
    cell->snapshot.attributes["label"] = SerializableData(SerializableData::Impl::create(cell->text));
    cell->snapshot.styles["font-size"] = SerializableData(
        SerializableData::Impl::create(static_cast<double>(
            getStyleDimension("text-size", kDefaultTextSize))));

    YGNodeRef textNode = createTextMeasureNode(cell);
    YGNodeStyleSetMargin(textNode, YGEdgeLeft, getStyleDimension("text-margin", kDefaultTextMargin));
    if (!orientationHorizontal) {
        YGNodeStyleSetFlexGrow(textNode, 1.0f);
        YGNodeStyleSetFlexShrink(textNode, 1.0f);
    }
    YGNodeInsertChild(itemNode, textNode, 1);

    return itemNode;
}

YGNodeRef IvirtualDomChoicePicker::createChipItemNode(
    const std::shared_ptr<ChoicePickerUnitCell>& labelCell) const {
    YGNodeRef itemNode = YGNodeNew();
    YGNodeStyleSetFlexDirection(itemNode, YGFlexDirectionRow);
    YGNodeStyleSetAlignItems(itemNode, YGAlignCenter);
    YGNodeStyleSetPadding(itemNode, YGEdgeLeft,
                          getStyleDimension("chip-padding-horizontal", kDefaultChipPaddingHorizontal));
    YGNodeStyleSetPadding(itemNode, YGEdgeRight,
                          getStyleDimension("chip-padding-horizontal", kDefaultChipPaddingHorizontal));
    YGNodeStyleSetPadding(itemNode, YGEdgeTop,
                          getStyleDimension("chip-padding-vertical", kDefaultChipPaddingVertical));
    YGNodeStyleSetPadding(itemNode, YGEdgeBottom,
                          getStyleDimension("chip-padding-vertical", kDefaultChipPaddingVertical));
    YGNodeStyleSetBorder(itemNode, YGEdgeAll,
                         getStyleDimension("chip-border-width", kDefaultChipBorderWidth));
    YGNodeStyleSetFlexShrink(itemNode, 0.0f);

    labelCell->snapshot = snapshot;
    labelCell->snapshot.component = "ChoicePicker";
    labelCell->snapshot.attributes["label"] =
        SerializableData(SerializableData::Impl::create(labelCell->text));
    labelCell->snapshot.styles["font-size"] = SerializableData(
        SerializableData::Impl::create(static_cast<double>(
            getStyleDimension("text-size", kDefaultTextSize))));

    YGNodeRef labelNode = createTextMeasureNode(labelCell);
    YGNodeInsertChild(itemNode, labelNode, 0);

    return itemNode;
}

float IvirtualDomChoicePicker::getStyleDimension(const char* key, float fallbackValue) const {
    const nlohmann::json& componentStyles = CSSStyleConverter::getDeviceComponentStylesJson();
    const nlohmann::json pickerStyles = componentStyles.contains("ChoicePicker") &&
                                                componentStyles["ChoicePicker"].is_object()
        ? componentStyles["ChoicePicker"]
        : nlohmann::json::object();
    return CSSStyleConverter::parseStyleDimension(pickerStyles, key, fallbackValue);
}

void IvirtualDomChoicePicker::creatCellYogaNode(float maxWidth) {
    _maxWidth = maxWidth;
    if (!_yogaNode || !_parentNode || !_parentNode->getSnapshot()) {
        return;
    }

    parseSnapshot(*_parentNode->getSnapshot());

    while (YGNodeGetChildCount(_yogaNode) > 0) {
        YGNodeRef child = YGNodeGetChild(_yogaNode, 0);
        YGNodeRemoveChild(_yogaNode, child);
        freeYogaTree(child);
    }
    cells.clear();

    YGNodeStyleSetFlexDirection(_yogaNode, YGFlexDirectionColumn);
    YGNodeStyleSetAlignItems(_yogaNode, YGAlignStretch);
    if (_maxWidth > 0.0f) {
        YGNodeStyleSetMaxWidth(_yogaNode, _maxWidth);
    }

    uint32_t childIndex = 0;
    if (filterable) {
        YGNodeInsertChild(_yogaNode, createSearchNode(), childIndex++);
    }

    YGNodeRef optionsNode = createOptionsContainerNode();
    YGNodeInsertChild(_yogaNode, optionsNode, childIndex);

    for (size_t i = 0; i < options.size(); ++i) {
        if (displayStyle == "chips") {
            auto labelCell = std::make_shared<ChoicePickerUnitCell>();
            labelCell->text = options[i];
            labelCell->index = static_cast<int>(i);
            labelCell->_choicePicker = this;
            cells.push_back(labelCell);

            YGNodeInsertChild(optionsNode,
                              createChipItemNode(labelCell),
                              static_cast<uint32_t>(YGNodeGetChildCount(optionsNode)));
            continue;
        }

        auto cell = std::make_shared<ChoicePickerUnitCell>();
        cell->text = options[i];
        cell->index = static_cast<int>(i);
        cell->_choicePicker = this;
        cells.push_back(cell);

        YGNodeInsertChild(optionsNode,
                          createCheckboxItemNode(cell),
                          static_cast<uint32_t>(YGNodeGetChildCount(optionsNode)));
    }

    YGNodeCalculateLayout(_yogaNode,
                          _maxWidth > 0.0f ? _maxWidth : YGUndefined,
                          YGUndefined,
                          YGDirectionLTR);
}

YGSize IvirtualDomChoicePicker::measureChoicePickerFunction(
    YGNodeRef node,
    float width,
    YGMeasureMode widthMode,
    float height,
    YGMeasureMode heightMode) {
    (void)height;
    (void)heightMode;

    ChoicePickerUnitCell* cell = static_cast<ChoicePickerUnitCell*>(YGNodeGetContext(node));
    if (!cell || !cell->_choicePicker) {
        return {0.0f, 0.0f};
    }

    IvirtualDomChoicePicker* picker = cell->_choicePicker;
    if (!picker->_measureTextFunc) {
        return {width, 0.0f};
    }

    ComponentSnapshot itemSnap = cell->snapshot;
    itemSnap.component = "ChoicePicker";
    int dummyLines = 0;
    return picker->_measureTextFunc(
        itemSnap,
        widthMode == YGMeasureModeUndefined ? YGUndefined : width,
        dummyLines);
}

}  // namespace agenui

#endif
