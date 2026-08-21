#include "napi_internal.h"
#include "style_parser/agenui_color_parser.h"
#include "style_parser/agenui_edge_insets_parser.h"

napi_value ParseColor(napi_env env, napi_callback_info info) {
    napi_value args[1];
    NAPI_GET_ARGS(env, info, 1, args);

    // Non-string input: napiGetString yields an empty string, which
    // ColorParser rejects -> falls into the undefined branch below.
    std::string css = napiGetString(env, args[0]);

    agenui::ColorValue cv;
    if (agenui::ColorParser::parse(css, cv)
        && cv.type == agenui::ColorValueType::Solid) {
        // Legal solid color -> ARGB uint32. This includes explicit transparent
        // ('transparent' / rgba(0,0,0,0), solidColor == 0), which must stay a
        // number so callers can tell it apart from a parse failure. It also
        // includes 'currentcolor': iOS CSSPropertyParser does not check
        // isCurrentColor and consumes the solid placeholder (0xFF000000)
        // directly, so all three platforms treat it as a successful solid.
        napi_value result;
        napi_create_uint32(env, cv.solidColor, &result);
        return result;
    }

    // gradient / parse failure -> undefined, so ArkTS callers
    // (StyleHelper.tryParseColor) can distinguish "unresolvable" from a legal
    // explicit transparent (0). StyleHelper.parseColor keeps its number
    // contract: its `typeof === 'number'` guard maps undefined to 0.
    napi_value undefinedValue;
    napi_get_undefined(env, &undefinedValue);
    return undefinedValue;
}

// ---------------------------------------------------------------- edge insets

namespace {

napi_value makeNull(napi_env env) {
    napi_value result = nullptr;
    napi_get_null(env, &result);
    return result;
}

void setUint32(napi_env env, napi_value obj, const char* key, uint32_t value) {
    napi_value v = nullptr;
    napi_create_uint32(env, value, &v);
    napi_set_named_property(env, obj, key, v);
}

void setDouble(napi_env env, napi_value obj, const char* key, double value) {
    napi_value v = nullptr;
    napi_create_double(env, value, &v);
    napi_set_named_property(env, obj, key, v);
}

void setBool(napi_env env, napi_value obj, const char* key, bool value) {
    napi_value v = nullptr;
    napi_get_boolean(env, value, &v);
    napi_set_named_property(env, obj, key, v);
}

void setString(napi_env env, napi_value obj, const char* key, const std::string& value) {
    napi_value v = nullptr;
    napi_create_string_utf8(env, value.c_str(), value.size(), &v);
    napi_set_named_property(env, obj, key, v);
}

napi_value buildEdgeInsetSide(napi_env env, const agenui::EdgeInsetValue& side) {
    napi_value obj = nullptr;
    napi_create_object(env, &obj);
    setDouble(env, obj, "value", side.value);
    setUint32(env, obj, "unit", static_cast<uint32_t>(side.unit));
    setBool(env, obj, "isCalc", side.isCalc);
    if (side.isCalc) {
        setString(env, obj, "calcExpr", side.calcExpr);
    }
    return obj;
}

}  // namespace

/**
 * Exposes the shared core `agenui::EdgeInsetsParser` to ArkTS. Android reaches the
 * same parser through jni_edge_insets_parser.cpp and iOS links it directly, so this
 * is what keeps the CSS edge-insets grammar identical on all three platforms.
 *
 * Needed by the amap host's AmapText (SpanText) component for `padding` / `margin`
 * shorthand; without it the ArkTS layer would have to reimplement the grammar and
 * would inevitably drift from the other platforms.
 *
 * Returns null when the value cannot be parsed.
 */
napi_value ParseEdgeInsets(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 1) {
        return makeNull(env);
    }
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, args[0], &valueType);
    if (valueType != napi_string) {
        return makeNull(env);
    }
    const std::string css = napiGetString(env, args[0]);
    if (css.empty()) {
        return makeNull(env);
    }

    agenui::EdgeInsets parsed;
    if (!agenui::EdgeInsetsParser::parse(css, parsed)) {
        return makeNull(env);
    }

    napi_value obj = nullptr;
    napi_create_object(env, &obj);
    napi_set_named_property(env, obj, "top", buildEdgeInsetSide(env, parsed.top));
    napi_set_named_property(env, obj, "right", buildEdgeInsetSide(env, parsed.right));
    napi_set_named_property(env, obj, "bottom", buildEdgeInsetSide(env, parsed.bottom));
    napi_set_named_property(env, obj, "left", buildEdgeInsetSide(env, parsed.left));
    return obj;
}
