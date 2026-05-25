package com.amap.agenui.render.style;

import android.content.Context;

import com.amap.agenui.render.utils.AGenUILogger;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Component style configuration manager.
 * <p>
 * Responsible for reading component style configurations from assets/component_styles.json
 * and providing a unified configuration access interface.
 *
 */
public class ComponentStyleConfig {

    private static final String TAG = "ComponentStyleConfig";
    private static final String CONFIG_FILE = "component_styles.json";

    private static volatile ComponentStyleConfig instance;
    private Map<String, StyleHashMap<String, String>> styles;

    /**
     * Private constructor for singleton pattern.
     */
    private ComponentStyleConfig(Context context) {
        styles = new StyleHashMap<>();
        loadConfig(context);
    }

    /**
     * Returns the singleton instance.
     *
     * @param context Android Context
     * @return ComponentStyleConfig instance
     */
    public static ComponentStyleConfig getInstance(Context context) {
        if (instance == null) {
            synchronized (ComponentStyleConfig.class) {
                if (instance == null) {
                    instance = new ComponentStyleConfig(context.getApplicationContext());
                }
            }
        }
        return instance;
    }

    /**
     * Loads the configuration file from assets.
     */
    private void loadConfig(Context context) {
        try (InputStream is = context.getAssets().open(CONFIG_FILE);
             BufferedReader reader = new BufferedReader(new InputStreamReader(is))) {
            StringBuilder sb = new StringBuilder();
            String line;

            while ((line = reader.readLine()) != null) {
                sb.append(line);
            }

            // Parse JSON
            JSONObject root = new JSONObject(sb.toString());

            // Iterate over all component configurations
            Iterator<String> componentKeys = root.keys();
            while (componentKeys.hasNext()) {
                String componentName = componentKeys.next();
                JSONObject componentConfig = root.getJSONObject(componentName);

                StyleHashMap<String, String> styleMap = new StyleHashMap<>();

                // Parse configuration (supports nested structures)
                parseJsonObject(componentConfig, "", styleMap);

                styles.put(componentName, styleMap);
                if (AGenUILogger.isLoggingEnabled()) {
                    AGenUILogger.d(TAG, "Loaded config for component: " + componentName + ", styles: " + styleMap.size());
                }
            }

            AGenUILogger.d(TAG, "Successfully loaded component styles config");

        } catch (IOException e) {
            AGenUILogger.e(TAG, "Failed to load config file: " + CONFIG_FILE, e);
        } catch (JSONException e) {
            AGenUILogger.e(TAG, "Failed to parse config file: " + CONFIG_FILE, e);
        }
    }

    /**
     * Recursively parses a JSON object, supporting nested structures.
     * Flattens nested JSON into keys formatted as "parent.child".
     *
     * @param jsonObject JSON object to parse
     * @param prefix     Key prefix
     * @param resultMap  Result map
     */
    private void parseJsonObject(JSONObject jsonObject, String prefix, Map<String, String> resultMap) {
        try {
            Iterator<String> keys = jsonObject.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                Object value = jsonObject.get(key);

                String fullKey = prefix.isEmpty() ? key : prefix + "." + key;

                if (value instanceof JSONObject) {
                    // Nested object, recurse
                    parseJsonObject((JSONObject) value, fullKey, resultMap);
                } else {
                    // Primitive type, store directly
                    resultMap.put(fullKey, value.toString());
                }
            }
        } catch (JSONException e) {
            AGenUILogger.e(TAG, "Failed to parse JSON object with prefix: " + prefix, e);
        }
    }

    /**
     * Returns the style configuration for the specified component.
     *
     * @param componentName Component name
     * @return Component style configuration map (StyleHashMap), or an empty map if not found
     */
    public StyleHashMap<String, String> getComponentStyle(String componentName) {
        StyleHashMap<String, String> componentStyle = styles.get(componentName);
        if (componentStyle == null) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, "Component style config not found for: " + componentName + ", returning empty map");
            }
            return new StyleHashMap<>();
        }
        return componentStyle;
    }

    /**
     * Returns the value of a specific style property for the specified component.
     *
     * @param componentName Component name
     * @param styleKey      Style property key
     * @param defaultValue  Default value
     * @return Style property value, or the default value if not found
     */
    public String getStyleValue(String componentName, String styleKey, String defaultValue) {
        Map<String, String> componentStyle = styles.get(componentName);
        if (componentStyle == null) {
            return defaultValue;
        }

        String value = componentStyle.get(styleKey);
        return value != null ? value : defaultValue;
    }

    public static class StyleHashMap<K, V> extends HashMap<K, V> {

        /**
         * Backward-compatible getOrDefault for older API levels.
         */
        public V getOrDefault(Object key, V defaultValue) {
            V value = get(key);
            return (value != null || containsKey(key)) ? value : defaultValue;
        }
    }
}
