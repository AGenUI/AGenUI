#pragma once

#include <jni.h>
#include <string>

#include "agenui_surface_size_provider.h"

namespace agenui {

/**
 * @brief JNI surface-size provider bridge.
 *
 * Adapts a Java {@code ISurfaceSizeProviderHost} (implemented by
 * {@code SurfaceManager}) to the C++ {@link ISurfaceSizeProvider} interface.
 *
 * Thread-safety contract: {@code getSurfaceSize} is invoked synchronously
 * on the engine worker thread (the same thread that processes streaming
 * protocol chunks and dispatches listener events). It uses
 * {@link JNIHelper::getJNIEnv} to acquire a JNIEnv, attaching the worker
 * thread to the JVM on first use (consistent with the existing measurement
 * bridge on the same worker thread).
 *
 * Lifetime: the bridge holds a JNI global ref to the Java host. Owners must
 * (a) detach the bridge from the C++ ISurfaceManager via
 * {@code setSurfaceSizeProvider(nullptr)} before deleting the bridge, and
 * (b) delete the bridge before the engine destroys the underlying
 * ISurfaceManager.
 */
class JNISurfaceSizeProviderBridge : public ISurfaceSizeProvider {
public:
    JNISurfaceSizeProviderBridge(JNIEnv* env, jobject javaHost);
    ~JNISurfaceSizeProviderBridge() override;

    // ISurfaceSizeProvider — invoked on the engine worker thread.
    SurfaceSize getSurfaceSize(const std::string& surfaceId) override;

    JNISurfaceSizeProviderBridge(const JNISurfaceSizeProviderBridge&) = delete;
    JNISurfaceSizeProviderBridge& operator=(const JNISurfaceSizeProviderBridge&) = delete;

private:
    jobject _javaHost;               // global ref; null when init failed
    jmethodID _getSurfaceSizeMethod; // ISurfaceSizeProviderHost.getSurfaceSize

    jclass _surfaceSizeClass;        // global ref; null when init failed
    jfieldID _widthField;
    jfieldID _heightField;
};

}  // namespace agenui
