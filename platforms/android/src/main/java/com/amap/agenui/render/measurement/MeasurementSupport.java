package com.amap.agenui.render.measurement;

import android.content.Context;

import com.amap.agenui.render.style.ComponentStyleConfig;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Collections;
import java.util.Map;

/**
 * Shared parsing and sizing helpers reused by Android measurers.
 *
 * This class keeps the per-component measurers focused on their own size aggregation rules by
 * centralizing JSON parsing, A2UI dimension extraction, style lookup and Yoga constraint
 * resolution semantics.
 */
final class MeasurementSupport {

    static final int MODE_UNDEFINED = 0;
    static final int MODE_EXACTLY = 1;
    static final int MODE_AT_MOST = 2;

    private MeasurementSupport() {
    }

    /**
     * Parses the snapshot JSON serialized from C++ `ComponentSnapshot.stringify()`.
     */
    static JSONObject parseRoot(String paramJson) {
        try {
            return new JSONObject(paramJson);
        } catch (Exception ignored) {
            return null;
        }
    }

    /**
     * Returns the `styles` object from the snapshot JSON when present.
     */
    static JSONObject optStyles(JSONObject root) {
        return root == null ? null : root.optJSONObject("styles");
    }

    /**
     * Normalizes a dynamic object field into a JSONObject.
     */
    static JSONObject parseJsonObject(Object value) {
        if (value instanceof JSONObject) {
            return (JSONObject) value;
        }
        if (value == null || value == JSONObject.NULL) {
            return null;
        }
        try {
            String raw = String.valueOf(value).trim();
            if (raw.isEmpty()) {
                return null;
            }
            return new JSONObject(raw);
        } catch (Exception ignored) {
            return null;
        }
    }

    /**
     * Reads the default style config shipped for a component type.
     */
    static Map<String, String> getComponentStyle(Context context, String componentName) {
        if (context == null) {
            return Collections.emptyMap();
        }
        return ComponentStyleConfig.getInstance(context).getComponentStyle(componentName);
    }

    /**
     * Parses an A2UI dimension declaration.
     *
     * Returns null for semantic non-numeric cases (`auto`, percent, match-parent...), because
     * those values mean "still constrained by Yoga/parent" rather than a fixed scalar length.
     */
    static Float parseA2uiDimension(Object value) {
        if (value == null || value == JSONObject.NULL) {
            return null;
        }
        if (value instanceof Number) {
            return ((Number) value).floatValue();
        }

        String raw = String.valueOf(value).trim().toLowerCase();
        if (raw.isEmpty()
                || "auto".equals(raw)
                || "wrap_content".equals(raw)
                || "match_parent".equals(raw)
                || raw.endsWith("%")) {
            return null;
        }

        if (raw.endsWith("px")) {
            raw = raw.substring(0, raw.length() - 2);
        }
        try {
            return Float.parseFloat(raw);
        } catch (NumberFormatException ignored) {
            return null;
        }
    }

    /**
     * Reads a numeric dimension from component default-style config with fallback.
     */
    static float readStyleDimensionA2ui(Map<String, String> styleConfig,
                                        String key,
                                        float defaultValue) {
        Float parsed = styleConfig == null ? null : parseA2uiDimension(styleConfig.get(key));
        return parsed != null ? parsed : defaultValue;
    }

    /**
     * Finds the first parseable dimension from a list of candidate keys.
     */
    static Float firstDimension(JSONObject source, String... keys) {
        if (source == null || keys == null) {
            return null;
        }
        for (String key : keys) {
            Float parsed = parseA2uiDimension(source.opt(key));
            if (parsed != null) {
                return parsed;
            }
        }
        return null;
    }

    /**
     * Extracts a display string from literal/binding JSON values used by A2UI.
     */
    static String extractString(Object value) {
        if (value == null || value == JSONObject.NULL) {
            return "";
        }
        if (value instanceof JSONObject) {
            JSONObject object = (JSONObject) value;
            String literal = object.optString("literalString", "");
            if (!literal.isEmpty()) {
                return literal;
            }
            return object.optString("path", "");
        }
        return String.valueOf(value);
    }

    /**
     * Extracts a boolean from literal/binding JSON values used by A2UI.
     */
    static boolean extractBoolean(Object value, boolean defaultValue) {
        if (value == null || value == JSONObject.NULL) {
            return defaultValue;
        }
        if (value instanceof Boolean) {
            return (Boolean) value;
        }
        if (value instanceof JSONObject) {
            JSONObject object = (JSONObject) value;
            if (object.has("literalBoolean")) {
                return object.optBoolean("literalBoolean", defaultValue);
            }
            return defaultValue;
        }
        if (value instanceof Number) {
            return ((Number) value).intValue() != 0;
        }
        return Boolean.parseBoolean(String.valueOf(value));
    }

    /**
     * Builds a compact text-style JSON object that can be fed back into {@link TextMeasurer}.
     */
    static JSONObject buildTextStyles(JSONObject sourceStyles,
                                      float defaultFontSize,
                                      boolean defaultBold,
                                      String... preferredFontSizeKeys) {
        JSONObject styles = new JSONObject();
        try {
            styles.put("font-size", defaultFontSize);
            if (defaultBold) {
                styles.put("font-weight", "bold");
            }

            Float fontSize = firstDimension(sourceStyles, preferredFontSizeKeys);
            if (fontSize == null) {
                fontSize = firstDimension(sourceStyles, "font-size", "fontSize");
            }
            if (fontSize != null) {
                styles.put("font-size", fontSize);
            }

            copyIfPresent(sourceStyles, styles, "font-weight", "line-height", "lineHeight",
                    "line-clamp", "lineClamp");
        } catch (Exception ignored) {
        }
        return styles;
    }

    /**
     * Copies the specified style keys when they exist.
     */
    static void copyIfPresent(JSONObject source, JSONObject target, String... keys) {
        if (source == null || target == null || keys == null) {
            return;
        }
        for (String key : keys) {
            Object value = source.opt(key);
            if (value != null && value != JSONObject.NULL) {
                try {
                    target.put(key, value);
                } catch (Exception ignored) {
                }
            }
        }
    }

    /**
     * Calculates the text area width left after subtracting fixed-width accessory UI.
     */
    static float resolveTextMaxWidth(float maxWidth, int widthMode, float reservedWidth) {
        if ((widthMode == MODE_AT_MOST || widthMode == MODE_EXACTLY) && maxWidth > reservedWidth) {
            return maxWidth - reservedWidth;
        }
        return 0f;
    }

    /**
     * Applies Yoga measure-mode constraints to a desired size and returns a sync result.
     */
    static MeasureResult resolveSize(float desiredWidth,
                                     float desiredHeight,
                                     float maxWidth,
                                     int widthMode,
                                     float maxHeight,
                                     int heightMode) {
        float resolvedWidth = Math.max(0f, desiredWidth);
        if (widthMode == MODE_EXACTLY) {
            resolvedWidth = Math.max(0f, maxWidth);
        } else if (widthMode == MODE_AT_MOST && maxWidth > 0f) {
            resolvedWidth = Math.min(resolvedWidth, maxWidth);
        }

        float resolvedHeight = Math.max(0f, desiredHeight);
        if (heightMode == MODE_EXACTLY) {
            resolvedHeight = Math.max(0f, maxHeight);
        } else if (heightMode == MODE_AT_MOST && maxHeight > 0f) {
            resolvedHeight = Math.min(resolvedHeight, maxHeight);
        }

        return MeasureResult.sync(resolvedWidth, resolvedHeight);
    }

    /**
     * Returns the first JSONArray found under any of the given candidate keys.
     */
    static JSONArray optArray(JSONObject root, String... keys) {
        if (root == null || keys == null) {
            return null;
        }
        for (String key : keys) {
            JSONArray array = root.optJSONArray(key);
            if (array != null) {
                return array;
            }
        }
        return null;
    }
}
