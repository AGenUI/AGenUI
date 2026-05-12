#pragma once

#include <jni.h>

namespace agenui {

/**
 * Initializes and caches the Java measurement bridge on the Java engine-init thread.
 *
 * This preloads the MeasurementBridge/MeasureResult JNI references so later Yoga measure
 * callbacks can run on native-attached worker threads without doing class lookup again.
 */
bool initializeAndroidMeasurementBridge(JNIEnv* env);

/**
 * Registers all Android measurement implementations into the engine-level MeasurementManager.
 *
 * Registration is type based (Text/Image/ChoicePicker...) and is intentionally separated from
 * initializeAndroidMeasurementBridge(): the former wires the native registry, the latter warms
 * up the Java bridge references that those implementations call into.
 */
void registerAndroidMeasurements();

}  // namespace agenui
