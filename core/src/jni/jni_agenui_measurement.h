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
 * Registers native methods for MeasurementBridge Java class.
 *
 * Provides nativeRegisterMeasurement/nativeUnregisterMeasurement so the Java-side
 * ComponentRegistry can dynamically register/unregister measurement types at runtime.
 */
jint register_jni_MeasurementBridge(JNIEnv* env);

}  // namespace agenui
