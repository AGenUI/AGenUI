//
//  AGenUIEngineBridge.mm
//  AGenUI
//
// Created on 2026/3/18.
//

#import "AGenUIEngineBridge.h"
#import "AGenUIEngineFunction.h"
#import "AGenUILoggerBridge.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include <memory>
#include <unordered_map>
#include <mutex>

// MARK: - AGenUIEngineBridge Private Interface

@interface AGenUIEngineBridge ()

@property (nonatomic, unsafe_unretained) agenui::IAGenUIEngine* engine;
@property (nonatomic, strong) NSMutableDictionary<NSString*, AGenUIFunctionCallCallback>* functionCallCallbacks;
@property (nonatomic, strong) AGenUILoggerBridge* perfLoggerBridge;

@end

// C++ storage for platform function instances (cannot be an OC property)
static std::unordered_map<std::string, agenui::AGenUIEngineFunction*> sPlatformFunctions;
// Mutex protecting sPlatformFunctions and _functionCallCallbacks from concurrent access
static std::mutex sFunctionMutex;

// MARK: - AGenUIEngineBridge Implementation

@implementation AGenUIEngineBridge

// MARK: - Singleton

+ (instancetype)sharedInstance {
    static AGenUIEngineBridge *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[AGenUIEngineBridge alloc] init];
    });
    return instance;
}

// MARK: - Version

+ (NSString *)sdkVersion {
    return [NSString stringWithUTF8String:agenui::getAGenUIVersion()];
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _functionCallCallbacks = [NSMutableDictionary dictionary];
        [self initialize];
    }
    return self;
}

- (void)dealloc {
    [self shutdown];
}

// MARK: - Module Lifecycle

- (void)initialize {
    if (_engine != nullptr) {
        return;
    }
    _engine = agenui::initAGenUIEngine();
    
    if (_engine == nullptr) {
        return;
    }
}

- (void)shutdown {
    
     // Unregister all platform functions before engine destruction
     if (_engine != nullptr) {
         std::lock_guard<std::mutex> lock(sFunctionMutex);
         for (auto& pair : sPlatformFunctions) {
             _engine->unregisterFunction(pair.first);
             delete pair.second;
         }
         sPlatformFunctions.clear();
     }
    
    // Clear FunctionCall callbacks
    @synchronized (_functionCallCallbacks) {
        [_functionCallCallbacks removeAllObjects];
    }

    // Destroy engine
    if (_engine != nullptr) {
        agenui::destroyAGenUIEngine();
        _engine = nullptr;
    }
}

// MARK: - C++ SurfaceManager Factory

- (void *)createSurfaceManager {
    if (_engine == nullptr) {
        return nullptr;
    }
    return (void *)_engine->createSurfaceManager();
}

- (void)destroySurfaceManager:(void *)surfaceManager {
    if (_engine == nullptr || surfaceManager == nullptr) {
        return;
    }
    _engine->destroySurfaceManager((agenui::ISurfaceManager *)surfaceManager);
}

// MARK: - Theme Configuration

- (BOOL)loadThemeConfig:(NSString *)themeConfigJson {
    if (!themeConfigJson || themeConfigJson.length == 0) {
        return NO;
    }
    if (_engine == nullptr) {
        return NO;
    }
    std::string themeConfigStr = [themeConfigJson UTF8String];
    std::string errorResult;
    bool success = _engine->loadThemeConfig(themeConfigStr, errorResult);
    return success;
}

// MARK: - DesignToken Configuration

- (BOOL)loadDesignTokenConfig:(NSString *)designTokenConfigJson {
    if (!designTokenConfigJson || designTokenConfigJson.length == 0) {
        return NO;
    }
    if (_engine == nullptr) {
        return NO;
    }
    std::string designTokenConfigStr = [designTokenConfigJson UTF8String];
    std::string errorResult;
    bool success = _engine->loadDesignTokenConfig(designTokenConfigStr, errorResult);
    return success;
}

// MARK: - Theme Mode Management

- (void)setDayNightMode:(NSString *)mode {
    if (!mode || mode.length == 0) {
        return;
    }
    if (_engine == nullptr) {
        return;
    }
    std::string modeStr = [mode UTF8String];
    _engine->setDayNightMode(modeStr);
}

// MARK: - FunctionCall Management

- (BOOL)registerFunction:(NSString *)functionCallName
                      config:(NSString *)configJson
                    callback:(AGenUIFunctionCallCallback)callback {
    if (!functionCallName || functionCallName.length == 0) {
        return NO;
    }
    if (!configJson || configJson.length == 0) {
        return NO;
    }
    if (_engine == nullptr) {
        return NO;
    }

    std::string nameStr = [functionCallName UTF8String];
    std::string configStr = [configJson UTF8String];

    {
        std::lock_guard<std::mutex> lock(sFunctionMutex);

        // 1. If already registered, unregister the old one first
        auto existingIt = sPlatformFunctions.find(nameStr);
        if (existingIt != sPlatformFunctions.end()) {
            _engine->unregisterFunction(nameStr);
            delete existingIt->second;
            sPlatformFunctions.erase(existingIt);
        }

        // 2. Save callback
        @synchronized (_functionCallCallbacks) {
            _functionCallCallbacks[functionCallName] = [callback copy];
        }

        // 3. Create IPlatformFunction instance bound to this function
        auto* platformFunction = new agenui::AGenUIEngineFunction((__bridge void*)self, nameStr);

        // 4. Register config + function instance to engine
        _engine->registerFunction(configStr, platformFunction);

        // 5. Cache the instance for later unregister
        sPlatformFunctions[nameStr] = platformFunction;
    }

    return YES;
}

- (void)unregisterFunction:(NSString *)functionCallName {
    if (!functionCallName || functionCallName.length == 0) {
        return;
    }
    
    if (_engine == nullptr) {
        return;
    }
    
    std::string nameStr = [functionCallName UTF8String];

    {
        std::lock_guard<std::mutex> lock(sFunctionMutex);

        // 1. Unregister from C++ engine
        _engine->unregisterFunction(nameStr);

        // 2. Clean up IPlatformFunction instance
        auto it = sPlatformFunctions.find(nameStr);
        if (it != sPlatformFunctions.end()) {
            delete it->second;
            sPlatformFunctions.erase(it);
        }

        // 3. Remove OC callback
        @synchronized (_functionCallCallbacks) {
            [_functionCallCallbacks removeObjectForKey:functionCallName];
        }
    }
}


- (nullable AGenUIFunctionCallCallback)getFunctionCallCallback:(NSString *)functionCallName {
    if (!functionCallName || functionCallName.length == 0) {
        return nil;
    }
    @synchronized (_functionCallCallbacks) {
        return _functionCallCallbacks[functionCallName];
    }
}

- (BOOL)setPathConfig:(NSString *)configJson {
    if (!configJson || configJson.length == 0) {
        return NO;
    }
    if (_engine == nullptr) {
        return NO;
    }
    std::string configStr = [configJson UTF8String];
    bool success = _engine->setPathConfig(configStr);
    return success;
}

- (void)setRuntimeLogEnabled:(BOOL)enabled {
    if (_engine == nullptr) {
        return;
    }
    
    if (enabled) {
        if (_perfLoggerBridge == nil) {
            _perfLoggerBridge = [[AGenUILoggerBridge alloc] init];
        }
        _engine->setRuntimeLogger(static_cast<agenui::IRuntimeLogger*>([_perfLoggerBridge cppRumtimeLoggerPointer]));
    } else {
        _engine->setRuntimeLogger(nullptr);
    }
}

@end
