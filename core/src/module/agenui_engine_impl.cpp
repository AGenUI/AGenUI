#include "agenui_engine_impl.h"
#include "agenui_logger_internal.h"
#include "agenui_type_define.h"
#include "agenui_surface_manager.h"
#include "function_call/agenui_functioncall_manager.h"
#include "function_call/agenui_functioncall_config.h"
#include "agenui_platform_function.h"
#include "surface/agenui_template_registry.h"
#include "surface/agenui_path_config.h"
#include "surface/agenui_surface_coordinator.h"
#include "agenui_thread_manager.h"
#include "surface/token_parser/agenui_token_parser.h"
#include "surface/component_property_spec/agenui_component_property_spec_manager.h"
#include "surface/yoga_node/agenui_measurement_manager.h"

namespace agenui {

AGenUIEngine::AGenUIEngine() {
}

AGenUIEngine::~AGenUIEngine() {
    stop();
}

void AGenUIEngine::start() {
    if (_isRunning.load()) {
        return;
    }
    _functionCallManager = new FunctionCallManager();
    _componentPropertySpecManager = new ComponentPropertySpecManager();
    _templateRegistry = new TemplateRegistry();
    _pathConfig = new PathConfig();
    _templateRegistry->initialize();
    _templateRegistry->start();

    // Create shared worker thread
    ThreadManager::getInstance().createThread(AGENUI_SHARED_THREAD_ID);

    // Create shared MeasurementManager
    _measurementManager = std::make_unique<MeasurementManagerImpl>();

    // Register engine context for global access
    setEngineContext(this);
    _isRunning.store(true);
    AGENUI_LOG("started");
}

void AGenUIEngine::stop() {
    AGENUI_LOG("begin stopping");
    if (!_isRunning.load()) {
        return;
    }
    _isRunning.store(false);
    // Destroy the shared worker thread first to ensure:
    // 1. All object instances remain valid while the thread is running
    // 2. No thread is running when objects are destroyed
    ThreadManager::getInstance().destroyThread(AGENUI_SHARED_THREAD_ID);
    // Destroy all SurfaceManagers first
    for (auto& pair : _surfaceManagers) {
        pair.second->uninit();
    }
    _surfaceManagers.clear();
    _measurementManager.reset();

    // Clear engine context before destroying modules
    setEngineContext(nullptr);

    // Destroy single-instance modules in reverse order
    SAFELY_DELETE(_componentPropertySpecManager);

    if (_templateRegistry) {
        _templateRegistry->stop();
        _templateRegistry->shutdown();
        SAFELY_DELETE(_templateRegistry);
    }

    SAFELY_DELETE(_pathConfig);

    SAFELY_DELETE(_functionCallManager);
}

ISurfaceManager* AGenUIEngine::createSurfaceManager() {
    AGENUI_LOG("begin creating");
    if (!_isRunning.load()) {
        return nullptr;
    }
    int instanceId = _nextInstanceId.fetch_add(1);
    auto sm = std::make_shared<SurfaceManager>(instanceId);
    sm->enterRunning();
    _surfaceManagers[instanceId] = sm;

    IThread* messageThread = ThreadManager::getInstance().getMessageThread(AGENUI_SHARED_THREAD_ID);
    if (!messageThread) {
        return nullptr;
    }
    messageThread->post([sm]() {
        sm->init();
    });
    AGENUI_LOG("created, %d", instanceId);
    return sm.get();
}

void AGenUIEngine::destroySurfaceManager(ISurfaceManager* surfaceManager) {
    if (!_isRunning.load()) {
        return;
    }
    auto* sm = static_cast<SurfaceManager*>(surfaceManager);
    for (auto it = _surfaceManagers.begin(); it != _surfaceManagers.end(); ++it) {
        if (it->second.get() == sm) {
            auto shared = it->second;
            int instanceId = it->first;
            shared->exitRunning();
            _surfaceManagers.erase(it);
            IThread* messageThread = ThreadManager::getInstance().getMessageThread(AGENUI_SHARED_THREAD_ID);
            if (!messageThread) {
                return;
            }
            messageThread->post([shared]() {
                shared->uninit();
            });
            AGENUI_LOG("Destroying SurfaceManager %d", instanceId);
            return;
        }
    }

    AGENUI_LOG("SurfaceManager not found for destruction");
}

bool AGenUIEngine::setPathConfig(const std::string &configJson) {
    if (!_isRunning.load()) {
        return false;
    }
    if (!_pathConfig) {
        return false;
    }
    return _pathConfig->setPathConfig(configJson);
}

void AGenUIEngine::setPlatformLayoutBridge(IPlatformLayoutBridge* platformLayoutBridge) {
    if (!_isRunning.load()) {
        return;
    }
    _platformLayoutBridge = platformLayoutBridge;
}

IPlatformLayoutBridge* AGenUIEngine::getPlatformLayoutBridge() {
    return _platformLayoutBridge;
}

bool AGenUIEngine::registerFunction(const std::string& config, IPlatformFunction* function) {
    AGENUI_LOG("config:%s, function:%p", config.c_str(), function);
    if (!_isRunning.load()) {
        return false;
    }
    if (!_functionCallManager) {
        return false;
    }
    if (!function) {
        return false;
    }
    nlohmann::json configJson = nlohmann::json::parse(config, nullptr, false);
    if (configJson.is_discarded()) {
        AGENUI_LOG("registerFunction failed: invalid JSON config");
        return false;
    }
    FunctionCallConfig functionCallConfig = FunctionCallConfig::fromJson(configJson);
    if (functionCallConfig.getName().empty()) {
        AGENUI_LOG("registerFunction failed: missing function name in config");
        return false;
    }
    return _functionCallManager->registerFunctionCall(functionCallConfig, function);
}

bool AGenUIEngine::unregisterFunction(const std::string& name) {
    AGENUI_LOG("name:%s", name.c_str());
    if (!_isRunning.load()) {
        return false;
    }
    if (!_functionCallManager) {
        return false;
    }
    return _functionCallManager->unregisterFunctionCall(name);
}

bool AGenUIEngine::loadThemeConfig(const std::string &themeConfig, std::string &result) {
    if (!_isRunning.load()) {
        return false;
    }
    if (!_componentPropertySpecManager) {
        result = "ComponentPropertySpecManager not initialized";
        return false;
    }
    bool success = _componentPropertySpecManager->loadFromString(themeConfig);
    if (!success) {
        result = "Failed to parse theme config";
    }
    return success;
}

bool AGenUIEngine::loadDesignTokenConfig(const std::string &designTokenConfig, std::string &result) {
    if (!_isRunning.load()) {
        return false;
    }
    bool success = TokenParser::getInstance().loadFromJsonString(designTokenConfig);
    if (!success) {
        result = "Failed to parse design token config";
    }
    return success;
}

void AGenUIEngine::setDayNightMode(const std::string &mode) {
    AGENUI_LOG("theme mode set to %s", mode.c_str());
    if (!_isRunning.load()) {
        return;
    }
    ThemeMode themeMode = ThemeMode::Light;
    if (mode == "dark") {
        themeMode = ThemeMode::Dark;
    } else if (mode == "light") {
        themeMode = ThemeMode::Light;
    } else {
        AGENUI_LOG("invalid mode '%s', using Light mode as default", mode.c_str());
    }
    bool setResult = TokenParser::getInstance().setThemeMode(themeMode);
    if (!setResult) {
        return;
    }

    for (auto& pair : _surfaceManagers) {
        pair.second->invalidateFunctionCallValues();
    }
}

ISurfaceManager* AGenUIEngine::findSurfaceManager(int instanceId) {
    if (!_isRunning.load()) {
        return nullptr;
    }
    auto it = _surfaceManagers.find(instanceId);
    if (it != _surfaceManagers.end()) {
        return it->second.get();
    }
    return nullptr;
}

IMeasurementManager* AGenUIEngine::getMeasurementManager() {
    return _measurementManager.get();
}

} // namespace agenui
