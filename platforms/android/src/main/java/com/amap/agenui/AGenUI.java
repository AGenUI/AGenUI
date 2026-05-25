package com.amap.agenui;

import android.content.Context;
import android.util.DisplayMetrics;

import androidx.annotation.Keep;
import androidx.annotation.RestrictTo;

import com.amap.agenui.function.IFunction;
import com.amap.agenui.function.PlatformFunction;
import com.amap.agenui.render.component.ComponentRegistry;
import com.amap.agenui.render.component.IComponentFactory;
import com.amap.agenui.render.image.ImageLoader;
import com.amap.agenui.render.image.ImageLoaderConfig;
import com.amap.agenui.render.surface.ThemeException;
import com.amap.agenui.render.utils.AGenUILogger;

@Keep
public class AGenUI {
    private static final String TAG = "AGenUI";

    static {
        System.loadLibrary("amap_AGenUI");
    }

    private static volatile AGenUI sInstance = null;
    private static final Object sLock = new Object();

    private volatile long nativePtr = 0;
    private volatile boolean isInitialized = false;
    private volatile Context appContext = null;

    /**
     * Private constructor to prevent direct external instantiation
     */
    private AGenUI() {
    }

    /**
     * Returns the AGenUI singleton instance
     *
     * @return AGenUI singleton instance
     */
    public static AGenUI getInstance() {
        if (sInstance == null) {
            synchronized (sLock) {
                if (sInstance == null) {
                    sInstance = new AGenUI();
                }
            }
        }
        return sInstance;
    }

    /**
     * Initializes the AGenUI Engine
     *
     * Performs the following steps:
     * 1. Loads Native modules
     * 2. Creates the Engine instance (initAGenUIEngine)
     * 3. Initializes SkillManager and registers platform Skills
     *
     * @throws RuntimeException if initialization fails
     */
    public void initialize(Context applicationContext) {
        synchronized (sLock) {
            if (isInitialized) {
                AGenUILogger.w(TAG, "Module already initialized");
                return;
            }

            try {
                appContext = applicationContext.getApplicationContext();
                nativePtr = nativeInitAGenUIEngine();
                DisplayMetrics metrics = appContext.getResources().getDisplayMetrics();
                nativeUpdatePlatformLayoutInfo(metrics.widthPixels, metrics.heightPixels, metrics.density);
                if (AGenUILogger.isLoggingEnabled()) {
                    AGenUILogger.i(TAG, "AGenUI Engine created: nativePtr=" + nativePtr);
                }
                ComponentRegistry.registerBuiltInComponents();

                isInitialized = true;
                AGenUILogger.i(TAG, "AGenUI Engine initialized successfully");
            } catch (Exception e) {
                AGenUILogger.e(TAG, "Failed to initialize AGenUI Engine", e);
                throw new RuntimeException("Failed to initialize AGenUI Engine", e);
            }
        }
    }
    
    /**
     * Sets a custom logger delegate to receive log callbacks from the engine.
     * 
     * This method should be called BEFORE initialize() to ensure all engine logs
     * are captured. If called after initialization, only subsequent logs will
     * use the custom delegate.
     * 
     * Example usage:
     * <pre>
     * AGenUI.getInstance().setCustomLogger(new IAGenUILogger() {
     *     {@literal @}Override
     *     public void onLog(int level, String tag, String func, int line, String message) {
     *         // Custom logging implementation
     *         AGenUILogger.d(tag, "[" + func + "@" + line + "] " + message);
     *     }
     * });
     * AGenUI.getInstance().initialize(context);
     * </pre>
     * 
     * @param customLogger Custom logger implementation. Pass null to use default logging.
     */
    public void setCustomLogger(IAGenUILogger customLogger) {
        if (!isInitialized()) {
            AGenUILogger.w(TAG, "setCustomLogger: Engine not initialized");
            return;
        }

        AGenUILogger.getInstance().setCustomLogger(customLogger);
    }

    /**
     * Set the minimum log level. Messages with level below this threshold are filtered out
     * on both the Java path and the C++ engine path (via IRuntimeLogger::getMinLevel()),
     * so filtered levels skip variadic formatting entirely.
     *
     * @param level One of IAGenUILogger LEVEL_DEBUG(0) ... LEVEL_PERFORMANCE(5). Out-of-range
     *              values fall back to LEVEL_DEBUG (no filtering).
     */
    public void setMinLogLevel(int level) {
        AGenUILogger.getInstance().setMinLogLevel(level);
    }

    /**
     * @return The currently configured minimum log level.
     */
    public int getMinLogLevel() {
        return AGenUILogger.getInstance().getMinLogLevel();
    }

    /**
     * Checks whether the Engine has been initialized
     *
     * @return true if the Engine is initialized
     */
    public boolean isInitialized() {
        return isInitialized && nativePtr != 0;
    }

    @RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
    public Context getApplicationContextForSdk() {
        return appContext;
    }

    /**
     * Creates a SurfaceManager instance
     *
     * @return instanceId (instance identifier)
     * @throws IllegalStateException if the Engine is not initialized or the native layer fails to create one
     */
    public int createSurfaceManager() throws IllegalStateException {
        if (!isInitialized()) {
            throw new IllegalStateException("createSurfaceManager: AGenUI engine is not initialized");
        }
        int instanceId = nativeCreateSurfaceManager();
        if (instanceId == 0) {
            throw new IllegalStateException("createSurfaceManager: native call failed");
        }

        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "SurfaceManager created: instanceId=" + instanceId);
        }
        return instanceId;
    }

    /**
     * Destroys a SurfaceManager instance
     *
     * @param instanceId The instanceId of the SurfaceManager to destroy
     */
    public void destroySurfaceManager(int instanceId) {
        if (!isInitialized()) {
            AGenUILogger.w(TAG, "destroySurfaceManager: Engine not initialized");
            return;
        }
        nativeDestroySurfaceManager(instanceId);
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "SurfaceManager destroyed: engineId=" + instanceId);
        }
    }


    private boolean isConfigValid(String methodName, String config) {
        if (config == null || config.isEmpty()) {
            if (AGenUILogger.isLoggingEnabled()) {
                AGenUILogger.w(TAG, methodName + ": config is null or empty");
            }
            return false;
        }
        if (!isInitialized()) {
            AGenUILogger.e(TAG, methodName + ": Engine not initialized");
            return false;
        }
        return true;
    }

    private boolean loadThemeConfig(String themeConfig) {
        if (!isConfigValid("loadThemeConfig", themeConfig)) {
            return false;
        }
        return nativeLoadThemeConfig(themeConfig);
    }

    private boolean loadDesignTokenConfig(String designTokenConfig) {
        if (!isConfigValid("loadDesignTokenConfig", designTokenConfig)) {
            return false;
        }
        return nativeLoadDesignTokenConfig(designTokenConfig);
    }

    /**
     * Sets the day/night mode
     *
     * @param mode Mode value: "light" or "dark"
     */
    public void setDayNightMode(String mode) {
        if (mode == null || mode.isEmpty()) {
            AGenUILogger.w(TAG, "setDayNightMode: mode is null or empty");
            return;
        }
        if (!isInitialized()) {
            AGenUILogger.e(TAG, "setDayNightMode: Engine not initialized");
            return;
        }
        nativeSetDayNightMode(mode);
    }

    /**
     * Sets path configuration for the engine
     *
     * @param configJson Path configuration JSON string
     *                   Supported keys: "templateDir" - absolute path to the template directory
     * @return true if configuration was applied successfully, false otherwise
     */
    public boolean setPathConfig(String configJson) {
        if (!isConfigValid("setPathConfig", configJson)) {
            return false;
        }
        return nativeSetPathConfig(configJson);
    }

    /**
     * Registers the default theme configuration
     * <p>
     * Registers both the theme JSON and DesignToken JSON simultaneously.
     *
     * @param theme       Theme configuration JSON string
     * @param designToken DesignToken configuration JSON string
     * @throws ThemeException if registration fails
     */
    public void registerDefaultTheme(String theme, String designToken) throws ThemeException {
        boolean themeOk = loadThemeConfig(theme);
        if (!themeOk) {
            throw new ThemeException("Failed to register theme config");
        }
        boolean tokenOk = loadDesignTokenConfig(designToken);
        if (!tokenOk) {
            throw new ThemeException("Failed to register design token config");
        }
        AGenUILogger.i(TAG, "✓ Default theme registered successfully");
    }


    public void registerFunction(IFunction function) {
        nativeRegisterFunction(
                function.getConfig().getName(),
                function.getConfig().toJSON(),
                new PlatformFunction(function));
    }

    public void unregisterFunction(String name) {
        nativeUnregisterFunction(name);
    }


    /**
     * Registers a custom component factory
     * <p>
     * If the component type already exists, it will be overwritten. Takes effect immediately
     * after registration and is shared across all Surfaces.
     *
     * @param type    Component type (e.g. "MyCustomCard")
     * @param creator Component factory instance
     */
    public void registerComponent(String type, IComponentFactory creator) {
        if (type == null || type.isEmpty() || creator == null) {
            AGenUILogger.w(TAG, "registerComponent: invalid parameters");
            return;
        }
        ComponentRegistry.registerComponent(type, creator);
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "registerComponent: type=" + type);
        }
    }

    public void unregisterComponent(String type) {
        if (type == null || type.isEmpty()) {
            AGenUILogger.w(TAG, "unregisterComponent: invalid parameters");
            return;
        }
        ComponentRegistry.unregisterComponent(type);
        if (AGenUILogger.isLoggingEnabled()) {
            AGenUILogger.i(TAG, "unregisterComponent: type=" + type);
        }
    }


    /**
     * Registers a global image loader
     * <p>
     * All image components will use this loader to load network images.
     *
     * @param loader ImageLoader instance
     */
    public void registerImageLoader(ImageLoader loader) {
        if (loader == null) {
            AGenUILogger.w(TAG, "registerImageLoader: loader is null");
            return;
        }
        ImageLoaderConfig.getInstance().setLoader(loader);
        AGenUILogger.i(TAG, "registerImageLoader: success");
    }


    /**
     * Returns the AGenUI SDK version number
     *
     * @return SDK version number
     */
    public static String getVersion() {
        return nativeGetVersion();
    }


    /**
     * Returns the Native Engine pointer
     *
     * @return Native Engine pointer
     * @throws IllegalStateException if the Engine is not initialized
     */
    public long getNativePtr() {
        if (!isInitialized()) {
            throw new IllegalStateException("Engine not initialized");
        }
        return nativePtr;
    }

    /**
     * Destroys the Engine and releases all Native resources.
     * This method should be called when the application exits.
     */
    public void destroy() {
        synchronized (sLock) {
            if (!isInitialized) {
                AGenUILogger.w(TAG, "Engine not initialized, nothing to destroy");
                return;
            }

            try {
                if (nativePtr != 0) {
                    nativeDestroyAGenUIEngine();
                    AGenUILogger.i(TAG, "Engine destroyed successfully");
                }
            } catch (Exception e) {
                AGenUILogger.e(TAG, "Error destroying engine", e);
            } finally {
                nativePtr = 0;
                isInitialized = false;
                sInstance = null;
            }
        }
    }

    private native long nativeInitAGenUIEngine();
    private native void nativeDestroyAGenUIEngine();

    public static native String nativeGetVersion();
    public static native int nativeCreateSurfaceManager();
    public static native void nativeDestroySurfaceManager(int instanceId);

    public static native boolean nativeSetPathConfig(String configJson);
    private static native void nativeUpdatePlatformLayoutInfo(int widthPx, int heightPx, float density);

    private static native boolean nativeLoadThemeConfig(String themeConfig);
    private static native boolean nativeLoadDesignTokenConfig(String designTokenConfig);
    private static native void nativeSetDayNightMode(String mode);

    public static native void nativeRegisterFunction(String name, String config, Object function);
    public static native void nativeUnregisterFunction(String name);
    public static native void nativeOnAsyncCallbackResult(long callbackPtr, int status, String data, String error);

    /**
     * Parses a CSS color value string (solid color or gradient).
     *
     * @param cssValue CSS color string, e.g. "red", "#ff0000", "linear-gradient(...)"
     * @return Parsed ColorValue object, or null if parsing fails
     */
    public static native ColorValue nativeParseColor(String cssValue);

    /**
     * Parses a CSS edge insets shorthand string (margin / padding / inset etc.).
     *
     * @param cssValue CSS shorthand string, e.g. "10px", "10px 20%", "10px 20px 30px 40px"
     * @return Parsed EdgeInsetsValue object, or null if parsing fails
     */
    public static native EdgeInsetsValue nativeParseEdgeInsets(String cssValue);
}
