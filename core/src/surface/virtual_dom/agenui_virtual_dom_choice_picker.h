#pragma once

#include "agenui_component_snapshot.h"
#include "agenui_css_style_converter.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#if defined(__OHOS__)
#include <yoga/Yoga.h>

namespace agenui {

class IvirtualDomChoicePicker;
class VirtualDOMNode;

/**
 * @brief Text measurement unit used by ChoicePicker Yoga children
 */
class ChoicePickerUnitCell {
public:
    std::string text;
    int index = 0;
    IvirtualDomChoicePicker* _choicePicker = nullptr;
    ComponentSnapshot snapshot;
};

/**
 * @brief ChoicePicker component Yoga node tree builder
 * @remark Mirrors the Harmony render structure:
 *         root column -> optional search input -> options container -> option items.
 */
class IvirtualDomChoicePicker {
public:
    explicit IvirtualDomChoicePicker(
        const ComponentSnapshot& snapshot,
        const std::function<YGSize(const ComponentSnapshot&, float, int&)>& measureTextFunc,
        VirtualDOMNode* parentNode = nullptr);

    ~IvirtualDomChoicePicker();

    std::string getLabel(int index) const { return options[index]; }

    YGNodeRef getYogaNode() const { return _yogaNode; }

    void creatCellYogaNode(float maxWidth);

    static YGSize measureChoicePickerFunction(
        YGNodeRef node,
        float width,
        YGMeasureMode widthMode,
        float height,
        YGMeasureMode heightMode);

    YGNodeRef _yogaNode = nullptr;
    ComponentSnapshot snapshot;
    std::vector<std::string> options;
    std::vector<std::shared_ptr<ChoicePickerUnitCell>> cells;
    bool orientationHorizontal = false;
    std::string displayStyle = "checkbox";
    bool filterable = false;
    std::function<YGSize(const ComponentSnapshot&, float, int&)> _measureTextFunc;
    float _maxWidth = 0.0f;
    VirtualDOMNode* _parentNode = nullptr;

private:
    void freeYogaTree(YGNodeRef node);
    void parseSnapshot(const ComponentSnapshot& snapshot);
    YGNodeRef createSearchNode() const;
    YGNodeRef createOptionsContainerNode() const;
    YGNodeRef createTextMeasureNode(const std::shared_ptr<ChoicePickerUnitCell>& cell) const;
    YGNodeRef createCheckboxItemNode(const std::shared_ptr<ChoicePickerUnitCell>& cell) const;
    YGNodeRef createChipItemNode(const std::shared_ptr<ChoicePickerUnitCell>& labelCell) const;
    float getStyleDimension(const char* key, float fallbackValue) const;
};

}  // namespace agenui

#endif // __OHOS__
