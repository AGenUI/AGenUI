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
#include "agenui_surface_layout_observable.h"
#include "agenui_component_render_observable.h"
#include <memory>

// MARK: - Notification Constants

NSString * const AGenUICreateSurfaceNotification      = @"AGenUICreateSurfaceNotification";
NSString * const AGenUIComponentsUpdateNotification   = @"AGenUIComponentsUpdateNotification";
NSString * const AGenUIComponentsAddNotification      = @"AGenUIComponentsAddNotification";
NSString * const AGenUIComponentsRemoveNotification   = @"AGenUIComponentsRemoveNotification";
NSString * const AGenUIDeleteSurfaceNotification      = @"AGenUIDeleteSurfaceNotification";
NSString * const AGenUIActionEventRoutedNotification  = @"AGenUIActionEventRoutedNotification";

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

// MARK: - C++ Event Listener (per-instance)

/// Per-instance C++ event listener.
/// Posts NSNotifications with the owning AGenUIEngineSurfaceManagerBridge as `object`,
/// so each Swift SurfaceManager can subscribe filtered by its own bridge instance.
class AGenUIInstanceEventListener : public agenui::IAGenUIMessageListener {
public:
    explicit AGenUIInstanceEventListener(void* bridge, NSInteger instanceId)
        : _bridge(bridge) {
        NSString *suffix        = [NSString stringWithFormat:@"_%ld", (long)instanceId];
        _createNotifName        = [AGenUICreateSurfaceNotification      stringByAppendingString:suffix];
        _componentsUpdateNotifName = [AGenUIComponentsUpdateNotification stringByAppendingString:suffix];
        _componentsAddNotifName    = [AGenUIComponentsAddNotification    stringByAppendingString:suffix];
        _componentsRemoveNotifName = [AGenUIComponentsRemoveNotification stringByAppendingString:suffix];
        _deleteNotifName        = [AGenUIDeleteSurfaceNotification      stringByAppendingString:suffix];
        _actionNotifName        = [AGenUIActionEventRoutedNotification  stringByAppendingString:suffix];
    }
    virtual ~AGenUIInstanceEventListener() = default;

    void onCreateSurface(const agenui::CreateSurfaceMessage& msg) override {
        NSLog(@"[AGenUI] onCreateSurface: surfaceId=%s, catalogId=%s, sendDataModel=%d, animated=%d, themeCount=%zu",
              msg.surfaceId.c_str(), msg.catalogId.c_str(), msg.sendDataModel, msg.animated, msg.theme.size());

        NSMutableDictionary *themeDict = [NSMutableDictionary dictionary];
        for (const auto& pair : msg.theme) {
            themeDict[[NSString stringWithUTF8String:pair.first.c_str()]] =
                [NSString stringWithUTF8String:pair.second.c_str()];
        }

        NSString *surfaceId  = [NSString stringWithUTF8String:msg.surfaceId.c_str()];
        NSString *catalogId  = [NSString stringWithUTF8String:msg.catalogId.c_str()];
        BOOL sendDataModel   = msg.sendDataModel;
        BOOL animated        = msg.animated;
        NSString *rawProtocolContent = [NSString stringWithUTF8String:msg.rawProtocolContent.c_str()];

        NSDictionary *userInfo = @{
            AGenUISurfaceIdKey:            surfaceId,
            AGenUICatalogIdKey:            catalogId,
            AGenUIThemeKey:                themeDict,
            AGenUISendDataModelKey:        @(sendDataModel),
            AGenUIAnimated:                @(animated),
            AGenUIRawProtocolContentKey:   rawProtocolContent
        };

        __weak id weakBridge = (__bridge id)_bridge;
        NSString *notifName = _createNotifName;
        auto postBlock = ^{
            __strong id strongBridge = weakBridge;
            if (!strongBridge) return;
            [[NSNotificationCenter defaultCenter]
                postNotificationName:notifName
                              object:strongBridge
                            userInfo:userInfo];
        };
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), postBlock);
        } else {
            postBlock();
        }
    }

    void onComponentsUpdate(const std::string& surfaceId, const std::vector<agenui::ComponentsUpdateMessage>& msg) override {
        NSLog(@"[AGenUI] onComponentsUpdate: surfaceId=%s, count=%zu", surfaceId.c_str(), msg.size());
        for (size_t i = 0; i < msg.size(); ++i) {
            NSLog(@"[AGenUI]   [%zu] componentId=%s, component=%s", i, msg[i].componentId.c_str(), msg[i].component.c_str());
        }

        NSMutableArray *messagesArray = [NSMutableArray array];
        for (const auto& m : msg) {
            [messagesArray addObject:@{
                @"componentId": [NSString stringWithUTF8String:m.componentId.c_str()],
                @"component":   [NSString stringWithUTF8String:m.component.c_str()]
            }];
        }

        NSString *surfaceIdStr = [NSString stringWithUTF8String:surfaceId.c_str()];
        NSDictionary *userInfo = @{
            AGenUISurfaceIdKey:        surfaceIdStr,
            AGenUIComponentsUpdateKey: messagesArray
        };

        __weak id weakBridge = (__bridge id)_bridge;
        NSString *notifName = _componentsUpdateNotifName;
        auto postBlock = ^{
            __strong id strongBridge = weakBridge;
            if (!strongBridge) return;
            [[NSNotificationCenter defaultCenter]
                postNotificationName:notifName
                              object:strongBridge
                            userInfo:userInfo];
        };
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), postBlock);
        } else {
            postBlock();
        }
    }

    void onComponentsAdd(const std::string& surfaceId, const std::vector<agenui::ComponentsAddMessage>& msg) override {
        NSLog(@"[AGenUI] onComponentsAdd: surfaceId=%s, count=%zu", surfaceId.c_str(), msg.size());
        for (size_t i = 0; i < msg.size(); ++i) {
            NSLog(@"[AGenUI]   [%zu] parentId=%s, componentId=%s, component=%s", i, msg[i].parentId.c_str(), msg[i].componentId.c_str(), msg[i].component.c_str());
        }

        NSMutableArray *messagesArray = [NSMutableArray array];
        for (const auto& m : msg) {
            [messagesArray addObject:@{
                @"parentId":    [NSString stringWithUTF8String:m.parentId.c_str()],
                @"componentId": [NSString stringWithUTF8String:m.componentId.c_str()],
                @"component":   [NSString stringWithUTF8String:m.component.c_str()]
            }];
        }

        NSString *surfaceIdStr = [NSString stringWithUTF8String:surfaceId.c_str()];
        NSDictionary *userInfo = @{
            AGenUISurfaceIdKey:     surfaceIdStr,
            AGenUIComponentsAddKey: messagesArray
        };

        __weak id weakBridge = (__bridge id)_bridge;
        NSString *notifName = _componentsAddNotifName;
        auto postBlock = ^{
            __strong id strongBridge = weakBridge;
            if (!strongBridge) return;
            [[NSNotificationCenter defaultCenter]
                postNotificationName:notifName
                              object:strongBridge
                            userInfo:userInfo];
        };
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), postBlock);
        } else {
            postBlock();
        }
    }

    void onComponentsRemove(const std::string& surfaceId, const std::vector<agenui::ComponentsRemoveMessage>& msg) override {
        NSLog(@"[AGenUI] onComponentsRemove: surfaceId=%s, count=%zu", surfaceId.c_str(), msg.size());
        for (size_t i = 0; i < msg.size(); ++i) {
            NSLog(@"[AGenUI]   [%zu] parentId=%s, componentId=%s", i, msg[i].parentId.c_str(), msg[i].componentId.c_str());
        }

        NSMutableArray *messagesArray = [NSMutableArray array];
        for (const auto& m : msg) {
            [messagesArray addObject:@{
                @"parentId":    [NSString stringWithUTF8String:m.parentId.c_str()],
                @"componentId": [NSString stringWithUTF8String:m.componentId.c_str()]
            }];
        }

        NSString *surfaceIdStr = [NSString stringWithUTF8String:surfaceId.c_str()];
        NSDictionary *userInfo = @{
            AGenUISurfaceIdKey:        surfaceIdStr,
            AGenUIComponentsRemoveKey: messagesArray
        };

        __weak id weakBridge = (__bridge id)_bridge;
        NSString *notifName = _componentsRemoveNotifName;
        auto postBlock = ^{
            __strong id strongBridge = weakBridge;
            if (!strongBridge) return;
            [[NSNotificationCenter defaultCenter]
                postNotificationName:notifName
                              object:strongBridge
                            userInfo:userInfo];
        };
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), postBlock);
        } else {
            postBlock();
        }
    }

    void onDeleteSurface(const agenui::DeleteSurfaceMessage& msg) override {
        NSLog(@"[AGenUI] onDeleteSurface: surfaceId=%s", msg.surfaceId.c_str());

        NSString *surfaceId = [NSString stringWithUTF8String:msg.surfaceId.c_str()];
        NSDictionary *userInfo = @{ AGenUISurfaceIdKey: surfaceId };

        __weak id weakBridge = (__bridge id)_bridge;
        NSString *notifName = _deleteNotifName;
        auto postBlock = ^{
            __strong id strongBridge = weakBridge;
            if (!strongBridge) return;
            [[NSNotificationCenter defaultCenter]
                postNotificationName:notifName
                              object:strongBridge
                            userInfo:userInfo];
        };
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), postBlock);
        } else {
            postBlock();
        }
    }

    void onActionEventRouted(const std::string& content) override {
        NSLog(@"[AGenUI] onActionEventRouted: content=%s", content.c_str());

        NSString *context = [NSString stringWithUTF8String:content.c_str()];
        NSDictionary *userInfo = @{ AGenUIContextKey: context };

        __weak id weakBridge = (__bridge id)_bridge;
        NSString *notifName = _actionNotifName;
        auto postBlock = ^{
            __strong id strongBridge = weakBridge;
            if (!strongBridge) return;
            [[NSNotificationCenter defaultCenter]
                postNotificationName:notifName
                              object:strongBridge
                            userInfo:userInfo];
        };
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), postBlock);
        } else {
            postBlock();
        }
    }

private:
    void* _bridge;          // Weak (unretained) reference to owning ObjC bridge
    NSString* _createNotifName;
    NSString* _componentsUpdateNotifName;
    NSString* _componentsAddNotifName;
    NSString* _componentsRemoveNotifName;
    NSString* _deleteNotifName;
    NSString* _actionNotifName;
};

// MARK: - AGenUIEngineSurfaceManagerBridge Private Interface

@interface AGenUIEngineSurfaceManagerBridge ()

@property (nonatomic, unsafe_unretained) agenui::ISurfaceManager* surfaceManager;
@property (nonatomic, unsafe_unretained) AGenUIInstanceEventListener* eventListener;

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
        }
    }
    return self;
}

- (void)dealloc {
    [self teardown];
}

- (void)teardown {
    if (_surfaceManager != nullptr && _eventListener != nullptr) {
        _surfaceManager->removeSurfaceEventListener(_eventListener);
        delete _eventListener;
        _eventListener = nullptr;
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
    if (_surfaceManager == nullptr) { return; }

    agenui::ComponentRenderInfo info;
    info.surfaceId   = [surfaceId UTF8String];
    info.componentId = [componentId UTF8String];
    info.type        = [type UTF8String];
    info.width       = widthA2ui;
    info.height      = heightA2ui;
    _surfaceManager->onRenderFinish(info);
}

@end
