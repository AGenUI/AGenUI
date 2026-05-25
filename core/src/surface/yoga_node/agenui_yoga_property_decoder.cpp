#include "surface/yoga_node/agenui_yoga_property_decoder.h"

#include "surface/yoga_node/agenui_component_snapshot_wrapper.h"
#include "surface/yoga_node/agenui_css_style_converter.h"
#include "surface/yoga_node/agenui_a2ui_attribute_converter.h"

namespace agenui {

// Hard constraint #2: the cast lives in the concrete decoder that pairs with
// ComponentSnapshotWrapper. The base ILayoutDataWrapper interface stays
// free of asXxx hooks; SDK consumers providing their own wrapper subclass
// pair it with their own decoder subclass and do their own cast in their
// own apply() body.
//
// AGenUI's bundled BuiltinYogaPropertyDecoder pairs with
// ComponentSnapshotWrapper. RTTI is disabled, so static_cast is the
// (unchecked) tool — safety relies on the pairing discipline enforced
// by ILayoutDelegate::setPropertyDecoder.

void BuiltinYogaPropertyDecoder::apply(ILayoutDataWrapper& wrapper,
                                       YGNodeRef node,
                                       bool clearAfterDecode) {
    if (!node) return;
    auto& concrete = static_cast<ComponentSnapshotWrapper&>(wrapper);
    CSSStyleConverter::convertToYoga(concrete.mutableRaw(), node, clearAfterDecode);
    A2UIAttributeConverter::convertToYoga(concrete.mutableRaw(), node, clearAfterDecode);
}

}  // namespace agenui
