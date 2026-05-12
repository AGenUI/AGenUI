//
//  AGenUIEngineSurfaceManagerBridge.h
//  AGenUI
//
// Created on 2026/3/18.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// MARK: - Notification Names

/// Notification sent when a new Surface is created
/// userInfo contains: surfaceId, catalogId, theme, sendDataModel, animated, rawProtocolContent
extern NSString * const AGenUICreateSurfaceNotification;

/// Notification sent when components are incrementally updated
/// userInfo contains: surfaceId, componentsUpdate
extern NSString * const AGenUIComponentsUpdateNotification;

/// Notification sent when components are added
/// userInfo contains: surfaceId, componentsAdd
extern NSString * const AGenUIComponentsAddNotification;

/// Notification sent when components are removed
/// userInfo contains: surfaceId, componentsRemove
extern NSString * const AGenUIComponentsRemoveNotification;

/// Notification sent when a Surface is deleted
/// userInfo contains: surfaceId
extern NSString * const AGenUIDeleteSurfaceNotification;

/// Notification sent when an action event is routed
/// userInfo contains: surfaceId, componentId, context
extern NSString * const AGenUIActionEventRoutedNotification;

// MARK: - Notification UserInfo Keys

extern NSString * const AGenUISurfaceIdKey;
extern NSString * const AGenUICatalogIdKey;
extern NSString * const AGenUIThemeKey;
extern NSString * const AGenUISendDataModelKey;
extern NSString * const AGenUIComponentsUpdateKey;
extern NSString * const AGenUIComponentsAddKey;
extern NSString * const AGenUIComponentsRemoveKey;
extern NSString * const AGenUIContextKey;
extern NSString * const AGenUIRawProtocolContentKey;

/// AGenUI Engine SurfaceManager Bridge (Multi-instance)
///
/// Each SurfaceManager holds one instance of this bridge.
/// Owns an independent C++ ISurfaceManager for isolated stream parsing, data binding and event callbacks.
/// Posts NSNotifications using self as object — subscribers must filter by object to receive only their events.
@interface AGenUIEngineSurfaceManagerBridge : NSObject

@property (nonatomic, assign, readonly) NSInteger instanceId;

/// Initialize and create the C++ ISurfaceManager
- (instancetype)init;

/// Tear down: remove listener and destroy C++ ISurfaceManager
- (void)teardown;

// MARK: - Data Transmission

/// Begin a text stream session
- (void)beginTextStream;

/// End a text stream session
- (void)endTextStream;

/// Receive A2UI protocol text chunk
/// @param data Text chunk string
- (void)receiveTextChunk:(NSString *)data;

/// Trigger user action on a component
/// @param surfaceId Surface ID
/// @param componentId Component ID
/// @param contextJson Context data JSON string
- (void)triggerAction:(NSString *)surfaceId
          componentId:(NSString *)componentId
              context:(nullable NSString *)contextJson;

/// Sync UI state to data model
/// @param surfaceId Surface ID
/// @param componentId Component ID
/// @param contextJson Context data JSON string
- (void)syncState:(NSString *)surfaceId
      componentId:(NSString *)componentId
          context:(nullable NSString *)contextJson;

/// Notify C++ engine that surface size changed
/// @param surfaceId Surface ID
/// @param widthA2ui New width in a2ui units (pt * 2)
/// @param heightA2ui New height in a2ui units (pt * 2)
- (void)notifySurfaceSizeChanged:(NSString *)surfaceId
                           width:(float)widthA2ui
                          height:(float)heightA2ui;

/// Notify C++ engine that a component has finished rendering with its actual size
/// @param surfaceId Surface ID
/// @param componentId Component ID
/// @param type Component type
/// @param widthA2ui Rendered width in a2ui units (pt * 2)
/// @param heightA2ui Rendered height in a2ui units (pt * 2)
- (void)notifyComponentRenderFinish:(NSString *)surfaceId
                         componentId:(NSString *)componentId
                                type:(NSString *)type
                               width:(float)widthA2ui
                              height:(float)heightA2ui;

@end

NS_ASSUME_NONNULL_END
