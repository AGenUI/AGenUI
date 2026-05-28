//
//  AGenUIEngineSurfaceManagerBridge.mm
//  AGenUI
//
// Created on 2026/3/18.
//

#import "AGenUIEngineSurfaceManagerBridge.h"
#import "AGenUIEngineBridge.h"
#include "agenui_message_listener.h"
#include "agenui_surface_manager_interface.h"
#include "agenui_surface_size_provider.h"
#include "agenui_render_info_types.h"
#include <cmath>
#include <memory>
#include <mutex>

// MARK: - Safe std::string -> NSString helper
//
// `+[NSString stringWithUTF8String:]` returns nil when the input bytes are not
// valid UTF-8. If that nil flows into an `@{}` literal or `addObject:`, ObjC
// raises `NSInvalidArgumentException: attempt to insert nil object`, which is
// the dominant crash mode of the C++ -> OC bridge layer. This helper:
//   1) tries strict UTF-8 decoding;
//   2) falls back to lossy decoding via NSData;
//   3) finally returns @"" so callers can safely build dictionaries.
static inline NSString *AGenUIStringFromCxx(const std::string &s) {
    if (s.empty()) {
        return @"";
    }
    NSString *str = [[NSString alloc] initWithBytes:s.data()
                                             length:s.size()
                                           encoding:NSUTF8StringEncoding];
    if (str != nil) {
        return str;
    }
    // Lossy fallback: keep payload alive instead of crashing.
    NSData *data = [NSData dataWithBytes:s.data() length:s.size()];
    NSString *lossy = [[NSString alloc] initWithData:data
                                            encoding:NSNonLossyASCIIStringEncoding];
    return lossy ?: @"";
}

// MARK: - Notification Constants

NSString * const AGenUICreateSurfaceNotification      = @"AGenUICreateSurfaceNotification";
NSString * const AGenUIComponentsUpdateNotification   = @"AGenUIComponentsUpdateNotification";
NSString * const AGenUIComponentsAddNotification      = @"AGenUIComponentsAddNotification";
NSString * const AGenUIComponentsRemoveNotification   = @"AGenUIComponentsRemoveNotification";
NSString * const AGenUIDeleteSurfaceNotification      = @"AGenUIDeleteSurfaceNotification";
NSString * const AGenUIActionEventRoutedNotification  = @"AGenUIActionEventRoutedNotification";
NSString * const AGenUIErrorNotification              = @"AGenUIErrorNotification";

NSString * const AGenUISurfaceIdKey          = @"surfaceId";
NSString * const AGenUICatalogIdKey          = @"catalogId";
NSString * const AGenUIThemeKey              = @"theme";
NSString * const AGenUISendDataModelKey      = @"sendDataModel";
NSString * const AGenUIComponentsUpdateKey   = @"componentsUpdate";
NSString * const AGenUIComponentsAddKey      = @"componentsAdd";
NSString * const AGenUIComponentsRemoveKey   = @"componentsRemove";
NSString * const AGenUIContextKey            = @"context";
NSString * const AGenUIAnimated              = @"animated";
NSString * const AGenUIRawProtocolContentKey = @"rawProtocolContent";

NSString * const AGenUIErrorCodeKey     = @"code";
NSString * const AGenUIErrorMessageKey  = @"message";

// MARK: - C++ Event Listener (per-instance)

/// Per-instance C++ event listener.
/// Posts NSNotifications with the owning AGenUIEngineSurfaceManagerBridge as `object`,
/// so each Swift SurfaceManager can subscribe filtered by its own bridge instance.
class AGenUIInstanceEventListener : public agenui::IAGenUIMessageListener {
public:
    explicit AGenUIInstanceEventListener(void* bridge, NSInteger instanceId)
        : _bridge(bridge), _invalidated(false) {
        NSString *suffix        = [NSString stringWithFormat:@"_%ld", (long)instanceId];
        _createNotifName        = [AGenUICreateSurfaceNotification      stringByAppendingString:suffix];
        _componentsUpdateNotifName = [AGenUIComponentsUpdateNotification stringByAppendingString:suffix];
        _componentsAddNotifName    = [AGenUIComponentsAddNotification    stringByAppendingString:suffix];
        _componentsRemoveNotifName = [AGenUIComponentsRemoveNotification stringByAppendingString:suffix];
        _deleteNotifName        = [AGenUIDeleteSurfaceNotification      stringByAppendingString:suffix];
        _actionNotifName        = [AGenUIActionEventRoutedNotification  stringByAppendingString:suffix];
        _errorNotifName         = [AGenUIErrorNotification              stringByAppendingString:suffix];
    }
    virtual ~AGenUIInstanceEventListener() = default;

    /// Invalidate the bridge pointer, must be called before dealloc of the ObjC object.
    void invalidate() {
        std::lock_guard<std::mutex> lock(_mutex);
        _invalidated = true;
        _bridge = nullptr;
    }

    void onCreateSurface(const agenui::CreateSurfaceMessage& msg) override {
        @autoreleasepool {
            NSMutableDictionary *themeDict = [NSMutableDictionary dictionary];
            for (const auto& pair : msg.theme) {
                NSString *k = AGenUIStringFromCxx(pair.first);
                NSString *v = AGenUIStringFromCxx(pair.second);
                if (k.length == 0) { continue; }   // skip empty keys, never crash
                themeDict[k] = v;
            }
            
            NSString *surfaceId          = AGenUIStringFromCxx(msg.surfaceId);
            NSString *catalogId          = AGenUIStringFromCxx(msg.catalogId);
            BOOL sendDataModel           = msg.sendDataModel;
            BOOL animated                = msg.animated;
            NSString *rawProtocolContent = AGenUIStringFromCxx(msg.rawProtocolContent);
            
            NSDictionary *userInfo = @{
                AGenUISurfaceIdKey:            surfaceId,
                AGenUICatalogIdKey:            catalogId,
                AGenUIThemeKey:                themeDict,
                AGenUISendDataModelKey:        @(sendDataModel),
                AGenUIAnimated:                @(animated),
                AGenUIRawProtocolContentKey:   rawProtocolContent
            };
            
            id bridgeObj = nil;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_invalidated) return;
                bridgeObj = (__bridge id)_bridge;
            }
            NSString *notifName = _createNotifName;
            auto postBlock = ^{
                [[NSNotificationCenter defaultCenter]
                 postNotificationName:notifName
                 object:bridgeObj
                 userInfo:userInfo];
            };
            if (![NSThread isMainThread]) {
                dispatch_async(dispatch_get_main_queue(), postBlock);
            } else {
                postBlock();
            }
        }
    }

    void onComponentsUpdate(const std::string& surfaceId, const std::vector<agenui::ComponentsUpdateMessage>& msg) override {
        @autoreleasepool {
            NSMutableArray *messagesArray = [NSMutableArray array];
            for (const auto& m : msg) {
                [messagesArray addObject:@{
                    @"componentId": AGenUIStringFromCxx(m.componentId),
                    @"component":   AGenUIStringFromCxx(m.component)
                }];
            }
            
            NSString *surfaceIdStr = AGenUIStringFromCxx(surfaceId);
            NSDictionary *userInfo = @{
                AGenUISurfaceIdKey:        surfaceIdStr,
                AGenUIComponentsUpdateKey: messagesArray
            };
            
            id bridgeObj = nil;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_invalidated) return;
                bridgeObj = (__bridge id)_bridge;
            }
            NSString *notifName = _componentsUpdateNotifName;
            auto postBlock = ^{
                [[NSNotificationCenter defaultCenter]
                 postNotificationName:notifName
                 object:bridgeObj
                 userInfo:userInfo];
            };
            if (![NSThread isMainThread]) {
                dispatch_async(dispatch_get_main_queue(), postBlock);
            } else {
                postBlock();
            }
        }
    }

    void onComponentsAdd(const std::string& surfaceId, const std::vector<agenui::ComponentsAddMessage>& msg) override {
        @autoreleasepool {
            NSMutableArray *messagesArray = [NSMutableArray array];
            for (const auto& m : msg) {
                [messagesArray addObject:@{
                    @"parentId":    AGenUIStringFromCxx(m.parentId),
                    @"componentId": AGenUIStringFromCxx(m.componentId),
                    @"component":   AGenUIStringFromCxx(m.component)
                }];
            }
            
            NSString *surfaceIdStr = AGenUIStringFromCxx(surfaceId);
            NSDictionary *userInfo = @{
                AGenUISurfaceIdKey:     surfaceIdStr,
                AGenUIComponentsAddKey: messagesArray
            };
            
            id bridgeObj = nil;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_invalidated) return;
                bridgeObj = (__bridge id)_bridge;
            }
            NSString *notifName = _componentsAddNotifName;
            auto postBlock = ^{
                [[NSNotificationCenter defaultCenter]
                 postNotificationName:notifName
                 object:bridgeObj
                 userInfo:userInfo];
            };
            if (![NSThread isMainThread]) {
                dispatch_async(dispatch_get_main_queue(), postBlock);
            } else {
                postBlock();
            }
        }
    }

    void onComponentsRemove(const std::string& surfaceId, const std::vector<agenui::ComponentsRemoveMessage>& msg) override {
        @autoreleasepool {
            NSMutableArray *messagesArray = [NSMutableArray array];
            for (const auto& m : msg) {
                [messagesArray addObject:@{
                    @"parentId":    AGenUIStringFromCxx(m.parentId),
                    @"componentId": AGenUIStringFromCxx(m.componentId)
                }];
            }
            
            NSString *surfaceIdStr = AGenUIStringFromCxx(surfaceId);
            NSDictionary *userInfo = @{
                AGenUISurfaceIdKey:        surfaceIdStr,
                AGenUIComponentsRemoveKey: messagesArray
            };
            
            id bridgeObj = nil;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_invalidated) return;
                bridgeObj = (__bridge id)_bridge;
            }
            NSString *notifName = _componentsRemoveNotifName;
            auto postBlock = ^{
                [[NSNotificationCenter defaultCenter]
                 postNotificationName:notifName
                 object:bridgeObj
                 userInfo:userInfo];
            };
            if (![NSThread isMainThread]) {
                dispatch_async(dispatch_get_main_queue(), postBlock);
            } else {
                postBlock();
            }
        }
    }

    void onDeleteSurface(const agenui::DeleteSurfaceMessage& msg) override {
        @autoreleasepool {
            NSString *surfaceId = AGenUIStringFromCxx(msg.surfaceId);
            NSDictionary *userInfo = @{ AGenUISurfaceIdKey: surfaceId };
            
            id bridgeObj = nil;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_invalidated) return;
                bridgeObj = (__bridge id)_bridge;
            }
            NSString *notifName = _deleteNotifName;
            auto postBlock = ^{
                [[NSNotificationCenter defaultCenter]
                 postNotificationName:notifName
                 object:bridgeObj
                 userInfo:userInfo];
            };
            if (![NSThread isMainThread]) {
                dispatch_async(dispatch_get_main_queue(), postBlock);
            } else {
                postBlock();
            }
        }
    }

    void onActionEventRouted(const std::string& content) override {
        @autoreleasepool {
            NSString *context = AGenUIStringFromCxx(content);
            NSDictionary *userInfo = @{ AGenUIContextKey: context };
            
            id bridgeObj = nil;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_invalidated) return;
                bridgeObj = (__bridge id)_bridge;
            }
            NSString *notifName = _actionNotifName;
            auto postBlock = ^{
                [[NSNotificationCenter defaultCenter]
                 postNotificationName:notifName
                 object:bridgeObj
                 userInfo:userInfo];
            };
            if (![NSThread isMainThread]) {
                dispatch_async(dispatch_get_main_queue(), postBlock);
            } else {
                postBlock();
            }
        }
    }

    void onError(const agenui::ErrorMessage& msg) override {
        @autoreleasepool {
            NSString *surfaceIdStr = AGenUIStringFromCxx(msg.surfaceId);
            NSString *messageStr   = AGenUIStringFromCxx(msg.message);
            NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];
            userInfo[AGenUIErrorCodeKey]    = @(msg.code);
            userInfo[AGenUISurfaceIdKey]    = surfaceIdStr;
            userInfo[AGenUIErrorMessageKey] = messageStr;
            
            id bridgeObj = nil;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_invalidated) return;
                bridgeObj = (__bridge id)_bridge;
            }
            NSString *notifName = _errorNotifName;
            NSDictionary *capturedUserInfo = [userInfo copy];
            auto postBlock = ^{
                [[NSNotificationCenter defaultCenter]
                 postNotificationName:notifName
                 object:bridgeObj
                 userInfo:capturedUserInfo];
            };
            if (![NSThread isMainThread]) {
                dispatch_async(dispatch_get_main_queue(), postBlock);
            } else {
                postBlock();
            }
        }
    }

private:
    mutable std::mutex _mutex;
    bool _invalidated;
    void* _bridge;          // Weak (unretained) reference to owning ObjC bridge
    NSString* _createNotifName;
    NSString* _componentsUpdateNotifName;
    NSString* _componentsAddNotifName;
    NSString* _componentsRemoveNotifName;
    NSString* _deleteNotifName;
    NSString* _actionNotifName;
    NSString* _errorNotifName;
};

// MARK: - C++ Surface Size Provider (per-instance)

/// Per-instance C++ ISurfaceSizeProvider implementation.
/// Forwards getSurfaceSize() queries to the owning ObjC bridge's surfaceSizeProvider block.
class AGenUIInstanceSurfaceSizeProvider : public agenui::ISurfaceSizeProvider {
public:
    explicit AGenUIInstanceSurfaceSizeProvider(void* bridge)
        : _bridge(bridge), _invalidated(false) {}
    virtual ~AGenUIInstanceSurfaceSizeProvider() = default;

    /// Invalidate the bridge pointer; must be called before dealloc of the ObjC object
    /// or before unregistering this provider from the C++ engine.
    void invalidate() {
        std::lock_guard<std::mutex> lock(_mutex);
        _invalidated = true;
        _bridge = nullptr;
    }

    agenui::SurfaceSize getSurfaceSize(const std::string& surfaceId) override {
        // Snapshot the bridge + block under the lock, then release the lock before
        // invoking the block — never call back into ObjC while holding our mutex.
        @autoreleasepool {
            AGenUISurfaceSizeProviderBlock block = nil;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_invalidated || _bridge == nullptr) {
                    return {0.0f, 0.0f};
                }
                AGenUIEngineSurfaceManagerBridge *bridge = (__bridge AGenUIEngineSurfaceManagerBridge *)_bridge;
                block = bridge.surfaceSizeProviderBlock;
            }
            if (block == nil) {
                return {0.0f, 0.0f};
            }

            NSString *surfaceIdStr = AGenUIStringFromCxx(surfaceId);
            CGSize sizePt = block(surfaceIdStr);

            // Sanitize each axis independently so a fixed-width / unbounded-height
            // (or vice versa) request stays expressible. Any non-finite (NaN / ±Inf)
            // or non-positive component is normalized to 0 on its own axis only;
            // we never poison the other axis just because this one is unmeasurable.
            // The engine treats 0 on a given axis as "no constraint" on that axis,
            // and {0, 0} as "not measurable yet" per the provider contract.
            auto sanitize = [](float v) -> float {
                return (std::isfinite(v) && v > 0.0f) ? v : 0.0f;
            };

            float widthPt  = sanitize((float)sizePt.width);
            float heightPt = sanitize((float)sizePt.height);

            // pt -> a2ui units (× 2), consistent with notifySurfaceSizeChanged.
            agenui::SurfaceSize result;
            result.width  = widthPt  * 2.0f;
            result.height = heightPt * 2.0f;
            return result;
        }
    }

private:
    mutable std::mutex _mutex;
    bool _invalidated;
    void* _bridge;          // Weak (unretained) reference to owning ObjC bridge
};

// MARK: - AGenUIEngineSurfaceManagerBridge Private Interface

@interface AGenUIEngineSurfaceManagerBridge ()

@property (nonatomic, unsafe_unretained) agenui::ISurfaceManager* surfaceManager;
@property (nonatomic, unsafe_unretained) AGenUIInstanceEventListener* eventListener;
@property (nonatomic, unsafe_unretained) AGenUIInstanceSurfaceSizeProvider* surfaceSizeProvider;

@end

// MARK: - AGenUIEngineSurfaceManagerBridge Implementation

@implementation AGenUIEngineSurfaceManagerBridge

- (instancetype)init {
    self = [super init];
    if (self) {
        // Create an independent C++ ISurfaceManager via the engine singleton
        _surfaceManager = (agenui::ISurfaceManager *)[AGenUIEngineBridge.sharedInstance createSurfaceManager];

        // Register per-instance event listener; use self (unretained) as bridge pointer
        if (_surfaceManager != nullptr) {
            _eventListener = new AGenUIInstanceEventListener((__bridge void*)self, self.instanceId);
            _surfaceManager->addSurfaceEventListener(_eventListener);

            // Inject per-instance ISurfaceSizeProvider; forwards getSurfaceSize() to the
            // host-supplied surfaceSizeProvider block on this bridge.
            _surfaceSizeProvider = new AGenUIInstanceSurfaceSizeProvider((__bridge void*)self);
            _surfaceManager->setSurfaceSizeProvider(_surfaceSizeProvider);
        }
    }
    return self;
}

- (void)dealloc {
    [self teardown];
}

- (void)teardown {
    if (_surfaceManager != nullptr && _eventListener != nullptr) {
        _eventListener->invalidate();
        _surfaceManager->removeSurfaceEventListener(_eventListener);
        delete _eventListener;
        _eventListener = nullptr;
    }

    if (_surfaceManager != nullptr && _surfaceSizeProvider != nullptr) {
        _surfaceSizeProvider->invalidate();
        _surfaceManager->setSurfaceSizeProvider(nullptr);
        delete _surfaceSizeProvider;
        _surfaceSizeProvider = nullptr;
    }

    if (_surfaceManager != nullptr) {
        [AGenUIEngineBridge.sharedInstance destroySurfaceManager:(void *)_surfaceManager];
        _surfaceManager = nullptr;
    }
}

- (NSInteger)instanceId
{
    if (_surfaceManager == nullptr) {
        return 0;
    }
    
    return _surfaceManager->getInstanceId();
}

// MARK: - Data Transmission

- (void)beginTextStream {
    if (_surfaceManager == nullptr) { return; }
    _surfaceManager->beginTextStream();
}

- (void)endTextStream {
    if (_surfaceManager == nullptr) { return; }
    _surfaceManager->endTextStream();
}

- (void)receiveTextChunk:(NSString *)data {
    if (!data || data.length == 0 || _surfaceManager == nullptr) { return; }
    std::string dataStr = [data UTF8String];
    _surfaceManager->receiveTextChunk(dataStr);
}

- (void)triggerAction:(NSString *)surfaceId
          componentId:(NSString *)componentId
              context:(NSString *)contextJson {
    if (!surfaceId || surfaceId.length == 0) { return; }
    if (!componentId || componentId.length == 0) { return; }
    if (_surfaceManager == nullptr) { return; }

    agenui::ActionMessage msg;
    msg.surfaceId         = [surfaceId UTF8String];
    msg.sourceComponentId = [componentId UTF8String];
    msg.contextJson       = contextJson ? [contextJson UTF8String] : "";
    _surfaceManager->submitUIAction(msg);
}

- (void)syncState:(NSString *)surfaceId
      componentId:(NSString *)componentId
          context:(NSString *)contextJson {
    if (!surfaceId || surfaceId.length == 0) { return; }
    if (!componentId || componentId.length == 0) { return; }
    if (_surfaceManager == nullptr) { return; }

    agenui::SyncUIToDataMessage msg;
    msg.surfaceId   = [surfaceId UTF8String];
    msg.componentId = [componentId UTF8String];
    msg.change      = contextJson ? [contextJson UTF8String] : "";
    _surfaceManager->submitUIDataModel(msg);
}

- (void)notifySurfaceSizeChanged:(NSString *)surfaceId
                           width:(float)width
                          height:(float)height {
    if (!surfaceId || surfaceId.length == 0) { return; }
    if (_surfaceManager == nullptr) { return; }

    agenui::SurfaceLayoutInfo info;
    info.surfaceId = [surfaceId UTF8String];
    info.width     = width * 2.0f;
    info.height    = height * 2.0f;
    _surfaceManager->onSurfaceSizeChanged(info);
}

- (void)notifyComponentRenderFinish:(NSString *)surfaceId
                         componentId:(NSString *)componentId
                                type:(NSString *)type
                               width:(float)widthA2ui
                              height:(float)heightA2ui {
    if (!surfaceId || surfaceId.length == 0) { return; }
    if (!componentId || componentId.length == 0) { return; }
    if (!type || type.length == 0) { return; }
    if (_surfaceManager == nullptr) { return; }

    agenui::ComponentRenderInfo info;
    info.surfaceId   = [surfaceId UTF8String];
    info.componentId = [componentId UTF8String];
    info.type        = [type UTF8String];
    info.width       = widthA2ui;
    info.height      = heightA2ui;
    _surfaceManager->onRenderFinish(info);
}

- (void)notifyTabSelection:(NSString *)surfaceId
               componentId:(NSString *)componentId
                      type:(NSString *)type
             selectedIndex:(int)selectedIndex {
    if (!surfaceId || surfaceId.length == 0) { return; }
    if (!componentId || componentId.length == 0) { return; }
    if (_surfaceManager == nullptr) { return; }

    agenui::ComponentRenderInfo info;
    info.surfaceId   = [surfaceId UTF8String];
    info.componentId = [componentId UTF8String];
    info.type        = [type UTF8String];
    info.selectedIndex = selectedIndex;
    _surfaceManager->onRenderFinish(info);

}

- (void)invalidateFunctionCallValues {
    if (_surfaceManager == nullptr) { return; }
    _surfaceManager->invalidateFunctionCallValues();
}

@end
