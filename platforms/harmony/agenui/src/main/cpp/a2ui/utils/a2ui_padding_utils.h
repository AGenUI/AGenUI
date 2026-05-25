#ifndef A2UI_PADDING_UTILS_H
#define A2UI_PADDING_UTILS_H

// CSS `padding` resolution shared by all leaf-style components (Text,
// RichText, Image, Button, ...).
//
// Background:
//   The C++ Yoga engine has already translated CSS `padding` into the leaf
//   node's borderBox layout, but the native ArkUI control still draws its
//   content over the entire frame. Translating the parsed padding into a
//   `BaseNode::setPadding(...)` call shrinks the glyph/image rendering area
//   to the contentBox, matching Android's `View.setPadding(...)` and iOS's
//   subview anchor constants. This is NOT a double-count with Yoga.
//
// Parsing rules (kept in lockstep with iOS / Android):
//   All shorthand and per-edge string parsing is delegated to the canonical
//   C++ `agenui::EdgeInsetsParser`. Only `px` (and unitless, which the parser
//   normalizes to px) is honored on the platform layer; every other CSS unit
//   collapses to 0. The render layer intentionally stays px-only so the three
//   platforms render identically without dragging viewport / font-baseline /
//   physical-unit math into platform code.
//   `padding-top/right/bottom/left` always override the shorthand.
//   Negative values are clamped to 0.
//   Numeric tokens (JSON number) are accepted and treated as raw a2ui-px
//   (legacy contract used by some style sources).
//
// Output: four a2ui-px edges. The caller is responsible for converting to
// vp via `BaseNode::setPadding`.

#include <cstdlib>
#include <string>
#include <nlohmann/json.hpp>

#include "style_parser/agenui_edge_insets_parser.h"

namespace a2ui {
namespace padding_utils {

namespace detail {

// Convert a single parsed edge value into a2ui-px. Only `px` (and unitless,
// which the C++ parser normalizes to px) is honored; every other CSS unit
// collapses to 0 so the three platforms stay aligned.
inline float resolveSidePx(const ::agenui::EdgeInsetValue& side) {
    if (side.isCalc) return 0.0f;
    if (side.unit == ::agenui::EdgeInsetUnit::Px) {
        return side.value;
    }
    return 0.0f;
}

} // namespace detail

// Parse a single CSS length token (e.g. "12px", "12em", or numeric JSON
// value). Returns true on success and writes the a2ui-px value into `outPx`.
inline bool parseLength(const nlohmann::json& v, float& outPx) {
    if (v.is_number()) {
        outPx = v.get<float>();
        return true;
    }
    if (v.is_string()) {
        const std::string& s = v.get_ref<const std::string&>();
        if (s.empty()) return false;
        // Reuse the shorthand parser by treating a single token as a
        // 1-value shorthand; all four sides are equal.
        ::agenui::EdgeInsets parsed;
        if (!::agenui::EdgeInsetsParser::parse(s, parsed)) return false;
        outPx = detail::resolveSidePx(parsed.top);
        return true;
    }
    return false;
}

// Resolve CSS `padding` shorthand + `padding-*` overrides into four a2ui-px
// edge values. Negative results are clamped to 0. When the styles object has
// no `padding*` keys, all four outputs are 0.
inline void resolveUserPadding(const nlohmann::json& styles,
                               float& top, float& right, float& bottom, float& left) {
    top = right = bottom = left = 0.0f;
    if (styles.is_null() || !styles.is_object()) return;

    // Shorthand: `padding`
    auto it = styles.find("padding");
    if (it != styles.end()) {
        if (it->is_number()) {
            const float v = it->get<float>();
            const float clamped = v > 0.0f ? v : 0.0f;
            top = right = bottom = left = clamped;
        } else if (it->is_string()) {
            const std::string& raw = it->get_ref<const std::string&>();
            ::agenui::EdgeInsets parsed;
            if (::agenui::EdgeInsetsParser::parse(raw, parsed)) {
                top    = detail::resolveSidePx(parsed.top);
                right  = detail::resolveSidePx(parsed.right);
                bottom = detail::resolveSidePx(parsed.bottom);
                left   = detail::resolveSidePx(parsed.left);
            }
        }
    }

    // Per-edge overrides take precedence over the shorthand.
    float v = 0.0f;
    if ((it = styles.find("padding-top")) != styles.end() && parseLength(*it, v)) top = v;
    if ((it = styles.find("padding-right")) != styles.end() && parseLength(*it, v)) right = v;
    if ((it = styles.find("padding-bottom")) != styles.end() && parseLength(*it, v)) bottom = v;
    if ((it = styles.find("padding-left")) != styles.end() && parseLength(*it, v)) left = v;

    if (top < 0.0f) top = 0.0f;
    if (right < 0.0f) right = 0.0f;
    if (bottom < 0.0f) bottom = 0.0f;
    if (left < 0.0f) left = 0.0f;
}

// Convenience: returns true when any of the four edges is > 0.
inline bool hasAnyPadding(float t, float r, float b, float l) {
    return t > 0.0f || r > 0.0f || b > 0.0f || l > 0.0f;
}

} // namespace padding_utils
} // namespace a2ui

#endif // A2UI_PADDING_UTILS_H
