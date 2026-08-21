#pragma once
#include <memory>
#include <string>
#include "agenui_measurement.h"

namespace agenui {

class ISurfaceManager;
class IPlatformInvoker;
class IPlatformFunction;
class IMeasurementManager;
class IRuntimeLogger;
class IPerfLogger;

/**
 * @brief AGenUI Engine Interface
 *
 * The globally unique engine instance, responsible for:
 * 1. Managing singleton modules (FunctionCallManager, TemplateRegistry, TokenParser, etc.)
 * 2. Creating and destroying multi-instance SurfaceManagers
 * 3. Managing global configurations (theme, DesignToken, day/night mode)
 *
 * Thread convention: All external interfaces are called on the main thread
 */
class IAGenUIEngine {
public:
    virtual ~IAGenUIEngine() = default;
    /**
     * @brief Creates a SurfaceManager instance
     * @return SurfaceManager interface pointer
     */
    virtual ISurfaceManager* createSurfaceManager() = 0;

    /**
     * @brief Destroys a SurfaceManager instance
     * @param surfaceManager The SurfaceManager pointer to destroy
     */
    virtual void destroySurfaceManager(ISurfaceManager* surfaceManager) = 0;

    /**
     * @brief Finds a SurfaceManager by instanceId
     * @param instanceId The unique ID assigned at creation
     * @return Shared ownership of the SurfaceManager, nullptr if not found.
     *         The lookup is serialized with destroySurfaceManager() under the
     *         same lock, so the object stays alive until the caller releases
     *         the shared_ptr. Safe to use from any thread.
     */
    virtual std::shared_ptr<ISurfaceManager> findSurfaceManagerShared(int instanceId) = 0;

    /**
     * @brief Sets the path configuration
     * @param configJson Path configuration in JSON format
     *        Supported keys: "templateDir" - absolute path to the template directory
     * @return true if configuration was applied successfully, false if JSON parsing failed
     */
    virtual bool setPathConfig(const std::string &configJson) = 0;

    /**
     * @brief Registers a platform function
     * @param config Function configuration in JSON format
     * @param function Platform function implementation pointer, must not be null
     * @return true if registration succeeds, false otherwise
     * @note Ownership convention: When registration succeeds, the caller must ensure the function
     *       remains valid until unregisterFunction is called. When registration fails, ownership
     *       remains with the caller, who is responsible for releasing it.
     */
    virtual bool registerFunction(const std::string& config, IPlatformFunction* function) = 0;

    /**
     * @brief Unregisters a platform function
     * @param name Function name
     * @return true if unregistration succeeds, false if not found or engine not ready
     */
    virtual bool unregisterFunction(const std::string& name) = 0;

    /**
     * @brief Declares a component property as a container of dynamic values
     * @param componentType Component type name, i.e. the JSON "component" field
     *        (e.g. "AmapText") — applies to every instance of that type
     * @param propertyName Property name (e.g. "spans")
     * @return true on success; false when either name is empty or the engine is not ready
     * @note By default a property value is parsed but not descended into, so a nested
     *       object or array is stored verbatim and any {"path":...} binding or
     *       {"call":...} function call inside it never resolves. Registering the
     *       property makes the parser walk its entire subtree instead, resolving
     *       nested dynamic values at any depth. Intended for host-registered custom
     *       components. Must be called before the first render; a declaration made
     *       later has no effect on components already parsed.
     */
    virtual bool registerDeepParseProperty(const std::string& componentType,
                                           const std::string& propertyName) = 0;

    /**
     * @brief Loads the theme configuration file
     * @param themeConfig Theme configuration content in JSON format
     * @param[out] result Error content when function execution fails
     * @return Execution result, true for success, false for failure
     */
    virtual bool loadThemeConfig(const std::string &themeConfig, std::string &result) = 0;

    /**
     * @brief Loads the DesignToken configuration file
     * @param designTokenConfig DesignToken configuration content in JSON format
     * @param[out] result Error content when function execution fails
     * @return Execution result, true for success, false for failure
     */
    virtual bool loadDesignTokenConfig(const std::string &designTokenConfig, std::string &result) = 0;

    /**
     * @brief Sets the day/night mode
     * @param mode Mode configuration, "light" or "dark"
     */
    virtual void setDayNightMode(const std::string &mode) = 0;

    /**
     * @brief Sets whether the host app is a debug build at runtime
     * @param isDebug true for debug builds, false for release. Defaults to false if never called.
     */
    virtual void setDebug(bool isDebug) = 0;

    /**
     * @brief Gets whether the host app is a debug build, previously set via setDebug
     * @return true for debug builds. Returns false if setDebug was never called.
     */
    virtual bool isDebug() = 0;

    /**
     * @brief Get MeasurementManager
     * @return MeasurementManager pointer, returns nullptr if uninitialized
     */
    virtual IMeasurementManager* getMeasurementManager() = 0;

    /**
     * @brief Set runtime logger
     * @param logger Runtime logger implementation (for DEBUG/INFO/WARN/ERROR/FATAL logs).
     *               Pass nullptr to restore the built-in default logger.
     */
    virtual void setRuntimeLogger(IRuntimeLogger* logger) = 0;

    /**
     * @brief Get the currently active runtime logger
     * @return Runtime logger interface pointer, never null (falls back to the built-in default)
     */
    virtual IRuntimeLogger* getRuntimeLogger() = 0;
};

} // namespace agenui
