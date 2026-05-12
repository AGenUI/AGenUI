//
//  AGenUIPerfLoggerBridge.h
//  AGenUI
//
//  Created by acoder-ai-infra on 2026/4/29.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * @brief AGenUI Logger Bridge Class
 *
 * Manages the lifecycle of C++ IAGenUIPerfLogger implementation, forwarding
 * performance log calls from C++ side to Swift side PerfLogger singleton.
 *
 * Dependency Inversion Principle:
 * - C++ modules only depend on IAGenUIPerfLogger abstract interface
 * - Internal implementation uses C++ wrapper class to implement this interface, injected into C++ engine
 * - Swift PerfLogger acts as pass-through channel, with concrete logic implemented by consumer (e.g., A2UIRenderer)
 */
@interface AGenUILoggerBridge : NSObject

/// Initialize the bridge
- (instancetype)init;

- (void *)cppRumtimeLoggerPointer;

@end

NS_ASSUME_NONNULL_END
