#include "jni_scoped_local_ref.h"
#include "jni_scoped_utf_chars.h"
#include <jni.h>
#include "style_parser/agenui_edge_insets_parser.h"

namespace agenui {

// Cached JNI class/field IDs, populated once during registration.
static jclass   sEdgeInsetsValueClass = nullptr;
static jclass   sEdgeInsetSideClass   = nullptr;

// EdgeInsetsValue fields
static jfieldID sEIV_top              = nullptr;
static jfieldID sEIV_right            = nullptr;
static jfieldID sEIV_bottom           = nullptr;
static jfieldID sEIV_left             = nullptr;

// EdgeInsetSide fields
static jfieldID sEIS_value            = nullptr;
static jfieldID sEIS_unit             = nullptr;
static jfieldID sEIS_isCalc           = nullptr;
static jfieldID sEIS_calcExpr         = nullptr;

static bool cacheFieldIds(JNIEnv* env) {
    sEIV_top    = env->GetFieldID(sEdgeInsetsValueClass, "top",
                      "Lcom/amap/agenui/EdgeInsetsValue$EdgeInsetSide;");
    sEIV_right  = env->GetFieldID(sEdgeInsetsValueClass, "right",
                      "Lcom/amap/agenui/EdgeInsetsValue$EdgeInsetSide;");
    sEIV_bottom = env->GetFieldID(sEdgeInsetsValueClass, "bottom",
                      "Lcom/amap/agenui/EdgeInsetsValue$EdgeInsetSide;");
    sEIV_left   = env->GetFieldID(sEdgeInsetsValueClass, "left",
                      "Lcom/amap/agenui/EdgeInsetsValue$EdgeInsetSide;");

    sEIS_value    = env->GetFieldID(sEdgeInsetSideClass, "value", "F");
    sEIS_unit     = env->GetFieldID(sEdgeInsetSideClass, "unit", "I");
    sEIS_isCalc   = env->GetFieldID(sEdgeInsetSideClass, "isCalc", "Z");
    sEIS_calcExpr = env->GetFieldID(sEdgeInsetSideClass, "calcExpr",
                        "Ljava/lang/String;");

    return !env->ExceptionCheck();
}

static jobject buildEdgeInsetSide(JNIEnv* env, const EdgeInsetValue& side) {
    jobject obj = env->AllocObject(sEdgeInsetSideClass);
    if (obj == nullptr) return nullptr;

    env->SetFloatField(obj, sEIS_value, side.value);
    env->SetIntField(obj, sEIS_unit, static_cast<jint>(side.unit));
    env->SetBooleanField(obj, sEIS_isCalc, side.isCalc);
    if (side.isCalc) {
        ScopedLocalRef<jstring> expr(env,
            env->NewStringUTF(side.calcExpr.c_str()));
        env->SetObjectField(obj, sEIS_calcExpr, expr.get());
    }
    return obj;
}

static jobject jni_parseEdgeInsets(JNIEnv* env, jclass, jstring jInput) {
    if (jInput == nullptr) return nullptr;

    ScopedUtfChars inputStr(env, jInput);
    std::string input = inputStr.c_str();

    EdgeInsets insets;
    if (!EdgeInsetsParser::parse(input, insets)) return nullptr;

    jobject result = env->AllocObject(sEdgeInsetsValueClass);
    if (result == nullptr) return nullptr;

    ScopedLocalRef<jobject> top(env, buildEdgeInsetSide(env, insets.top));
    ScopedLocalRef<jobject> right(env, buildEdgeInsetSide(env, insets.right));
    ScopedLocalRef<jobject> bottom(env, buildEdgeInsetSide(env, insets.bottom));
    ScopedLocalRef<jobject> left(env, buildEdgeInsetSide(env, insets.left));

    env->SetObjectField(result, sEIV_top, top.get());
    env->SetObjectField(result, sEIV_right, right.get());
    env->SetObjectField(result, sEIV_bottom, bottom.get());
    env->SetObjectField(result, sEIV_left, left.get());

    return result;
}

jint register_jni_EdgeInsetsParser(JNIEnv* env) {
    jclass localEIV = env->FindClass("com/amap/agenui/EdgeInsetsValue");
    if (localEIV == nullptr) return JNI_ERR;
    sEdgeInsetsValueClass = (jclass)env->NewGlobalRef(localEIV);
    env->DeleteLocalRef(localEIV);

    jclass localEIS = env->FindClass(
        "com/amap/agenui/EdgeInsetsValue$EdgeInsetSide");
    if (localEIS == nullptr) return JNI_ERR;
    sEdgeInsetSideClass = (jclass)env->NewGlobalRef(localEIS);
    env->DeleteLocalRef(localEIS);

    if (!cacheFieldIds(env)) return JNI_ERR;

    ScopedLocalRef<jclass> agenui(env,
        env->FindClass("com/amap/agenui/AGenUI"));
    if (agenui.get() == nullptr) return JNI_ERR;

    JNINativeMethod nativeMethods[] = {
        {"nativeParseEdgeInsets",
         "(Ljava/lang/String;)Lcom/amap/agenui/EdgeInsetsValue;",
         (void*)jni_parseEdgeInsets},
    };

    jint ret = env->RegisterNatives(agenui.get(), nativeMethods,
        sizeof(nativeMethods) / sizeof(nativeMethods[0]));
    return (ret == JNI_OK) ? JNI_OK : JNI_ERR;
}

} // namespace agenui
