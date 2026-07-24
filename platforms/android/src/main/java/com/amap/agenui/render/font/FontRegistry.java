package com.amap.agenui.render.font;

import android.content.res.AssetManager;
import android.graphics.Typeface;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.amap.agenui.render.utils.AGenUILogger;

import java.util.Locale;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Global registry for custom fonts loaded at runtime.
 *
 * <p>Host applications register fonts via {@link com.amap.agenui.AGenUI#registerFont}
 * or {@link com.amap.agenui.AGenUI#registerFontFromAsset}. The registered
 * {@link Typeface} objects are then looked up during font-family CSS resolution
 * in {@link com.amap.agenui.render.style.StyleHelper#parseFontFamily}.
 *
 * <p>Thread-safe: backed by {@link ConcurrentHashMap}.
 */
public final class FontRegistry {

    private static final String TAG = "FontRegistry";
    private static final FontRegistry INSTANCE = new FontRegistry();

    private final ConcurrentHashMap<String, Typeface> registry = new ConcurrentHashMap<>();

    private FontRegistry() {}

    @NonNull
    public static FontRegistry getInstance() {
        return INSTANCE;
    }

    /**
     * Register a typeface under the given family name.
     *
     * @param familyName CSS font-family name (case-insensitive)
     * @param typeface   loaded Typeface; must not be null
     * @return true if newly registered; false if it replaced an existing entry
     */
    public boolean register(@NonNull String familyName, @NonNull Typeface typeface) {
        String key = familyName.toLowerCase(Locale.ROOT);
        return registry.put(key, typeface) == null;
    }

    /**
     * Register a font from a file system path.
     *
     * @param familyName CSS font-family name (case-insensitive)
     * @param filePath   absolute path to a .ttf or .otf file
     * @return true if the typeface was loaded and registered successfully
     */
    public boolean registerFont(String familyName, String filePath) {
        if (familyName == null || familyName.isEmpty() || filePath == null || filePath.isEmpty()) {
            AGenUILogger.w(TAG, "registerFont: invalid parameters, familyName="
                    + familyName + ", filePath=" + filePath);
            return false;
        }
        try {
            Typeface typeface = Typeface.createFromFile(filePath);
            return register(familyName, typeface);
        } catch (Exception e) {
            AGenUILogger.e(TAG, "registerFont: failed to load font, familyName="
                    + familyName + ", filePath=" + filePath, e);
            return false;
        }
    }

    /**
     * Register a font from the app's assets directory.
     *
     * @param familyName   CSS font-family name (case-insensitive)
     * @param assetPath    path relative to the assets directory
     * @param assetManager the application's AssetManager
     * @return true if the typeface was loaded and registered successfully
     */
    public boolean registerFontFromAsset(String familyName, String assetPath,
                                         AssetManager assetManager) {
        if (familyName == null || familyName.isEmpty() || assetPath == null || assetPath.isEmpty()
                || assetManager == null) {
            AGenUILogger.w(TAG, "registerFontFromAsset: invalid parameters, familyName="
                    + familyName + ", assetPath=" + assetPath
                    + ", assetManager=" + (assetManager == null ? "null" : "non-null"));
            return false;
        }
        try {
            Typeface typeface = Typeface.createFromAsset(assetManager, assetPath);
            return register(familyName, typeface);
        } catch (Exception e) {
            AGenUILogger.e(TAG, "registerFontFromAsset: failed to load font, familyName="
                    + familyName + ", assetPath=" + assetPath, e);
            return false;
        }
    }

    /**
     * Look up a registered typeface by family name.
     *
     * @param familyName CSS font-family name (case-insensitive)
     * @return the registered Typeface, or null if not found
     */
    @Nullable
    public Typeface resolve(@NonNull String familyName) {
        return registry.get(familyName.toLowerCase(Locale.ROOT));
    }
}
