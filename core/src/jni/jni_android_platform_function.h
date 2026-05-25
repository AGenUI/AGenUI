#pragma once
#ifdef __ANDROID__

#include <jni.h>
#include <string>
#include "agenui_platform_function.h"

namespace agenui {

/**
 * @brief Android platform function implementation
 * @note Wraps a Java object that implements platform function calls via JNI
 */
class AndroidPlatformFunction : public IPlatformFunction {
public:
    /**
     * @brief Construct with Java function object
     * @param env JNI environment (must be valid)
     * @param javaFunction Java object implementing the function (local ref, will be converted to global ref)
     */
    AndroidPlatformFunction(JNIEnv* env, jobject javaFunction);
    ~AndroidPlatformFunction() override;

    FunctionCallResult callSync(const FunctionCallContext& context, const std::string& params) override;

private:
    JavaVM* _jvm = nullptr;
    jobject _javaFunction = nullptr;    // GlobalRef
    jmethodID _callSyncMethod = nullptr;
    jclass _contextClass = nullptr;     // GlobalRef to FunctionCallContext class
    jmethodID _contextConstructor = nullptr;
};

} // namespace agenui

#endif // __ANDROID__
