//
//  AGenUIEngineSurfaceManagerBridge.h
//  AGenUI
//
// Created on 2026/3/18.
//

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

/// External-supplied source of truth for surface size, queried synchronously by the C++ engine
/// when its locally cached size for the given surfaceId has not been initialized yet.
///
/// - Parameter surfaceId: The surface identifier assigned by the engine.
/// - Returns: Current surface size in points (pt). Return CGSizeZero if not measurable yet;
///            the bridge will convert pt to a2ui units (× 2) internally.
///
/// Thread convention: invoked synchronously on the engine worker thread; the block
/// implementation must be thread-safe.
typedef CGSize (^AGenUISurfaceSizeProviderBlock)(NSString *surfaceId);

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

/// Notification sent when a C++ execution error occurs
/// userInfo contains: code, surfaceId, message
extern NSString * const AGenUIErrorNotification;

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

// MARK: - Error Notification UserInfo Keys
extern NSString * const AGenUIErrorCodeKey;
extern NSString * const AGenUIErrorMessageKey;

/// AGenUI Engine SurfaceManager Bridge (Multi-instance)
///
/// Each SurfaceManager holds one instance of this bridge.
/// Owns an independent C++ ISurfaceManager for isolated stream parsing, data binding and event callbacks.
/// Posts NSNotifications using self as object — subscribers must filter by object to receive only their events.
@interface AGenUIEngineSurfaceManagerBridge : NSObject

@property (nonatomic, assign, readonly) NSInteger instanceId;

@property (nonatomic, copy, nullable) AGenUISurfaceSizeProviderBlock surfaceSizeProviderBlock;

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

/// Notify C++ engine that a Tabs component switched its selected tab
/// @param surfaceId Surface ID
/// @param componentId Component ID
/// @param type Component type
/// @param selectedIndex Selected tab index
- (void)notifyTabSelection:(NSString *)surfaceId
               componentId:(NSString *)componentId
                      type:(NSString *)type
             selectedIndex:(int)selectedIndex;

/// Re-evaluate every component's attributes and styles across all surfaces managed by
/// this bridge, then emit field-level diffs to the native renderer for any value that
/// actually changed.
///
/// Call this when host-owned external state has changed in ways the SDK cannot observe
/// (theme, locale, orientation, etc.) and registered FunctionCalls that read from that
/// state need to be re-run. Action handlers are not in scope.
- (void)invalidateFunctionCallValues;

@end

NS_ASSUME_NONNULL_END
