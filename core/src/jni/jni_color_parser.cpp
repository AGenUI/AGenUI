#include "jni_scoped_local_ref.h"
#include "jni_scoped_utf_chars.h"
#include <jni.h>
#include "style_parser/agenui_color_parser.h"

namespace agenui {

// Cached JNI class/field IDs, populated once during registration.
static jclass    sColorValueClass     = nullptr;
static jclass    sGradientInfoClass   = nullptr;
static jclass    sColorStopClass      = nullptr;
static jclass    sLinearParamsClass   = nullptr;
static jclass    sRadialParamsClass   = nullptr;
static jclass    sConicParamsClass    = nullptr;

// ColorValue fields
static jfieldID  sCV_type             = nullptr;
static jfieldID  sCV_solidColor       = nullptr;
static jfieldID  sCV_isCurrentColor   = nullptr;
static jfieldID  sCV_gradient         = nullptr;

// GradientInfo fields
static jfieldID  sGI_gradientType     = nullptr;
static jfieldID  sGI_isRepeating      = nullptr;
static jfieldID  sGI_colorInterp      = nullptr;
static jfieldID  sGI_colorStops       = nullptr;
static jfieldID  sGI_linear           = nullptr;
static jfieldID  sGI_radial           = nullptr;
static jfieldID  sGI_conic            = nullptr;

// ColorStop fields
static jfieldID  sCS_color            = nullptr;
static jfieldID  sCS_position         = nullptr;
static jfieldID  sCS_positionEnd      = nullptr;
static jfieldID  sCS_unit             = nullptr;
static jfieldID  sCS_unitEnd          = nullptr;
static jfieldID  sCS_hasPosition      = nullptr;
static jfieldID  sCS_hasPositionEnd   = nullptr;
static jfieldID  sCS_isHint           = nullptr;
static jfieldID  sCS_isCurrentColor   = nullptr;
static jfieldID  sCS_positionIsCalc   = nullptr;
static jfieldID  sCS_positionCalcExpr = nullptr;

// LinearParams fields
static jfieldID  sLP_angle            = nullptr;
static jfieldID  sLP_angleIsCalc      = nullptr;
static jfieldID  sLP_angleCalcExpr    = nullptr;

// RadialParams fields
static jfieldID  sRP_shape            = nullptr;
static jfieldID  sRP_size             = nullptr;
static jfieldID  sRP_centerX          = nullptr;
static jfieldID  sRP_centerY          = nullptr;
static jfieldID  sRP_centerXIsPx      = nullptr;
static jfieldID  sRP_centerYIsPx      = nullptr;
static jfieldID  sRP_radiusX          = nullptr;
static jfieldID  sRP_radiusY          = nullptr;
static jfieldID  sRP_hasExplicitSize  = nullptr;
static jfieldID  sRP_radiusXIsPercent = nullptr;
static jfieldID  sRP_radiusYIsPercent = nullptr;
static jfieldID  sRP_radiusXIsCalc    = nullptr;
static jfieldID  sRP_radiusYIsCalc    = nullptr;
static jfieldID  sRP_radiusXCalcExpr  = nullptr;
static jfieldID  sRP_radiusYCalcExpr  = nullptr;

// ConicParams fields
static jfieldID  sCP_startAngle       = nullptr;
static jfieldID  sCP_centerX          = nullptr;
static jfieldID  sCP_centerY          = nullptr;
static jfieldID  sCP_centerXIsPx      = nullptr;
static jfieldID  sCP_centerYIsPx      = nullptr;
static jfieldID  sCP_startAngleIsCalc = nullptr;
static jfieldID  sCP_startAngleCalcExpr = nullptr;

static bool cacheFieldIds(JNIEnv* env) {
    // ColorValue
    sCV_type             = env->GetFieldID(sColorValueClass, "type", "I");
    sCV_solidColor       = env->GetFieldID(sColorValueClass, "solidColor", "I");
    sCV_isCurrentColor   = env->GetFieldID(sColorValueClass, "isCurrentColor", "Z");
    sCV_gradient         = env->GetFieldID(sColorValueClass, "gradient", "Lcom/amap/agenui/ColorValue$GradientInfo;");

    // GradientInfo
    sGI_gradientType     = env->GetFieldID(sGradientInfoClass, "gradientType", "I");
    sGI_isRepeating      = env->GetFieldID(sGradientInfoClass, "isRepeating", "Z");
    sGI_colorInterp      = env->GetFieldID(sGradientInfoClass, "colorInterpolationMethod", "Ljava/lang/String;");
    sGI_colorStops       = env->GetFieldID(sGradientInfoClass, "colorStops", "[Lcom/amap/agenui/ColorValue$ColorStop;");
    sGI_linear           = env->GetFieldID(sGradientInfoClass, "linear", "Lcom/amap/agenui/ColorValue$LinearParams;");
    sGI_radial           = env->GetFieldID(sGradientInfoClass, "radial", "Lcom/amap/agenui/ColorValue$RadialParams;");
    sGI_conic            = env->GetFieldID(sGradientInfoClass, "conic", "Lcom/amap/agenui/ColorValue$ConicParams;");

    // ColorStop
    sCS_color            = env->GetFieldID(sColorStopClass, "color", "I");
    sCS_position         = env->GetFieldID(sColorStopClass, "position", "F");
    sCS_positionEnd      = env->GetFieldID(sColorStopClass, "positionEnd", "F");
    sCS_unit             = env->GetFieldID(sColorStopClass, "unit", "I");
    sCS_unitEnd          = env->GetFieldID(sColorStopClass, "unitEnd", "I");
    sCS_hasPosition      = env->GetFieldID(sColorStopClass, "hasPosition", "Z");
    sCS_hasPositionEnd   = env->GetFieldID(sColorStopClass, "hasPositionEnd", "Z");
    sCS_isHint           = env->GetFieldID(sColorStopClass, "isHint", "Z");
    sCS_isCurrentColor   = env->GetFieldID(sColorStopClass, "isCurrentColor", "Z");
    sCS_positionIsCalc   = env->GetFieldID(sColorStopClass, "positionIsCalc", "Z");
    sCS_positionCalcExpr = env->GetFieldID(sColorStopClass, "positionCalcExpr", "Ljava/lang/String;");

    // LinearParams
    sLP_angle            = env->GetFieldID(sLinearParamsClass, "angle", "F");
    sLP_angleIsCalc      = env->GetFieldID(sLinearParamsClass, "angleIsCalc", "Z");
    sLP_angleCalcExpr    = env->GetFieldID(sLinearParamsClass, "angleCalcExpr", "Ljava/lang/String;");

    // RadialParams
    sRP_shape            = env->GetFieldID(sRadialParamsClass, "shape", "I");
    sRP_size             = env->GetFieldID(sRadialParamsClass, "size", "I");
    sRP_centerX          = env->GetFieldID(sRadialParamsClass, "centerX", "F");
    sRP_centerY          = env->GetFieldID(sRadialParamsClass, "centerY", "F");
    sRP_centerXIsPx      = env->GetFieldID(sRadialParamsClass, "centerXIsPx", "Z");
    sRP_centerYIsPx      = env->GetFieldID(sRadialParamsClass, "centerYIsPx", "Z");
    sRP_radiusX          = env->GetFieldID(sRadialParamsClass, "radiusX", "F");
    sRP_radiusY          = env->GetFieldID(sRadialParamsClass, "radiusY", "F");
    sRP_hasExplicitSize  = env->GetFieldID(sRadialParamsClass, "hasExplicitSize", "Z");
    sRP_radiusXIsPercent = env->GetFieldID(sRadialParamsClass, "radiusXIsPercent", "Z");
    sRP_radiusYIsPercent = env->GetFieldID(sRadialParamsClass, "radiusYIsPercent", "Z");
    sRP_radiusXIsCalc    = env->GetFieldID(sRadialParamsClass, "radiusXIsCalc", "Z");
    sRP_radiusYIsCalc    = env->GetFieldID(sRadialParamsClass, "radiusYIsCalc", "Z");
    sRP_radiusXCalcExpr  = env->GetFieldID(sRadialParamsClass, "radiusXCalcExpr", "Ljava/lang/String;");
    sRP_radiusYCalcExpr  = env->GetFieldID(sRadialParamsClass, "radiusYCalcExpr", "Ljava/lang/String;");

    // ConicParams
    sCP_startAngle       = env->GetFieldID(sConicParamsClass, "startAngle", "F");
    sCP_centerX          = env->GetFieldID(sConicParamsClass, "centerX", "F");
    sCP_centerY          = env->GetFieldID(sConicParamsClass, "centerY", "F");
    sCP_centerXIsPx      = env->GetFieldID(sConicParamsClass, "centerXIsPx", "Z");
    sCP_centerYIsPx      = env->GetFieldID(sConicParamsClass, "centerYIsPx", "Z");
    sCP_startAngleIsCalc = env->GetFieldID(sConicParamsClass, "startAngleIsCalc", "Z");
    sCP_startAngleCalcExpr = env->GetFieldID(sConicParamsClass, "startAngleCalcExpr", "Ljava/lang/String;");

    return !env->ExceptionCheck();
}

static jobject buildColorStop(JNIEnv* env, const ColorStop& stop) {
    jobject obj = env->AllocObject(sColorStopClass);
    if (obj == nullptr) return nullptr;

    env->SetIntField(obj, sCS_color, static_cast<jint>(stop.color));
    env->SetFloatField(obj, sCS_position, stop.position);
    env->SetFloatField(obj, sCS_positionEnd, stop.positionEnd);
    env->SetIntField(obj, sCS_unit, static_cast<jint>(stop.unit));
    env->SetIntField(obj, sCS_unitEnd, static_cast<jint>(stop.unitEnd));
    env->SetBooleanField(obj, sCS_hasPosition, stop.hasPosition);
    env->SetBooleanField(obj, sCS_hasPositionEnd, stop.hasPositionEnd);
    env->SetBooleanField(obj, sCS_isHint, stop.isHint);
    env->SetBooleanField(obj, sCS_isCurrentColor, stop.isCurrentColor);
    env->SetBooleanField(obj, sCS_positionIsCalc, stop.positionIsCalc);
    if (stop.positionIsCalc) {
        ScopedLocalRef<jstring> expr(env, env->NewStringUTF(stop.positionCalcExpr.c_str()));
        env->SetObjectField(obj, sCS_positionCalcExpr, expr.get());
    }
    return obj;
}

static jobject buildLinearParams(JNIEnv* env, const LinearGradientParams& p) {
    jobject obj = env->AllocObject(sLinearParamsClass);
    if (obj == nullptr) return nullptr;
    env->SetFloatField(obj, sLP_angle, p.angle);
    env->SetBooleanField(obj, sLP_angleIsCalc, p.angleIsCalc);
    if (p.angleIsCalc) {
        ScopedLocalRef<jstring> expr(env, env->NewStringUTF(p.angleCalcExpr.c_str()));
        env->SetObjectField(obj, sLP_angleCalcExpr, expr.get());
    }
    return obj;
}

static jobject buildRadialParams(JNIEnv* env, const RadialGradientParams& p) {
    jobject obj = env->AllocObject(sRadialParamsClass);
    if (obj == nullptr) return nullptr;
    env->SetIntField(obj, sRP_shape, static_cast<jint>(p.shape));
    env->SetIntField(obj, sRP_size, static_cast<jint>(p.size));
    env->SetFloatField(obj, sRP_centerX, p.centerX);
    env->SetFloatField(obj, sRP_centerY, p.centerY);
    env->SetBooleanField(obj, sRP_centerXIsPx, p.centerXIsPx);
    env->SetBooleanField(obj, sRP_centerYIsPx, p.centerYIsPx);
    env->SetFloatField(obj, sRP_radiusX, p.radiusX);
    env->SetFloatField(obj, sRP_radiusY, p.radiusY);
    env->SetBooleanField(obj, sRP_hasExplicitSize, p.hasExplicitSize);
    env->SetBooleanField(obj, sRP_radiusXIsPercent, p.radiusXIsPercent);
    env->SetBooleanField(obj, sRP_radiusYIsPercent, p.radiusYIsPercent);
    env->SetBooleanField(obj, sRP_radiusXIsCalc, p.radiusXIsCalc);
    env->SetBooleanField(obj, sRP_radiusYIsCalc, p.radiusYIsCalc);
    if (p.radiusXIsCalc) {
        ScopedLocalRef<jstring> expr(env, env->NewStringUTF(p.radiusXCalcExpr.c_str()));
        env->SetObjectField(obj, sRP_radiusXCalcExpr, expr.get());
    }
    if (p.radiusYIsCalc) {
        ScopedLocalRef<jstring> expr(env, env->NewStringUTF(p.radiusYCalcExpr.c_str()));
        env->SetObjectField(obj, sRP_radiusYCalcExpr, expr.get());
    }
    return obj;
}

static jobject buildConicParams(JNIEnv* env, const ConicGradientParams& p) {
    jobject obj = env->AllocObject(sConicParamsClass);
    if (obj == nullptr) return nullptr;
    env->SetFloatField(obj, sCP_startAngle, p.startAngle);
    env->SetFloatField(obj, sCP_centerX, p.centerX);
    env->SetFloatField(obj, sCP_centerY, p.centerY);
    env->SetBooleanField(obj, sCP_centerXIsPx, p.centerXIsPx);
    env->SetBooleanField(obj, sCP_centerYIsPx, p.centerYIsPx);
    env->SetBooleanField(obj, sCP_startAngleIsCalc, p.startAngleIsCalc);
    if (p.startAngleIsCalc) {
        ScopedLocalRef<jstring> expr(env, env->NewStringUTF(p.startAngleCalcExpr.c_str()));
        env->SetObjectField(obj, sCP_startAngleCalcExpr, expr.get());
    }
    return obj;
}

static jobject buildGradientInfo(JNIEnv* env, const GradientInfo& info) {
    jobject obj = env->AllocObject(sGradientInfoClass);
    if (obj == nullptr) return nullptr;

    env->SetIntField(obj, sGI_gradientType, static_cast<jint>(info.type));
    env->SetBooleanField(obj, sGI_isRepeating, info.isRepeating);
    if (!info.colorInterpolationMethod.empty()) {
        ScopedLocalRef<jstring> interp(env, env->NewStringUTF(info.colorInterpolationMethod.c_str()));
        env->SetObjectField(obj, sGI_colorInterp, interp.get());
    }

    // Build ColorStop array
    jsize stopCount = static_cast<jsize>(info.colorStops.size());
    ScopedLocalRef<jobjectArray> stopsArray(env, env->NewObjectArray(stopCount, sColorStopClass, nullptr));
    for (jsize i = 0; i < stopCount; ++i) {
        ScopedLocalRef<jobject> stopObj(env, buildColorStop(env, info.colorStops[i]));
        env->SetObjectArrayElement(stopsArray.get(), i, stopObj.get());
    }
    env->SetObjectField(obj, sGI_colorStops, stopsArray.get());

    if (info.type == GradientType::Linear) {
        ScopedLocalRef<jobject> lp(env, buildLinearParams(env, info.linear));
        env->SetObjectField(obj, sGI_linear, lp.get());
    } else if (info.type == GradientType::Radial) {
        ScopedLocalRef<jobject> rp(env, buildRadialParams(env, info.radial));
        env->SetObjectField(obj, sGI_radial, rp.get());
    } else if (info.type == GradientType::Conic) {
        ScopedLocalRef<jobject> cp(env, buildConicParams(env, info.conic));
        env->SetObjectField(obj, sGI_conic, cp.get());
    }
    return obj;
}

static jobject jni_parseColor(JNIEnv* env, jclass /* clazz */, jstring jInput) {
    if (jInput == nullptr) return nullptr;

    ScopedUtfChars inputStr(env, jInput);
    std::string input = inputStr.c_str();

    ColorValue value;
    if (!ColorParser::parse(input, value)) return nullptr;

    jobject result = env->AllocObject(sColorValueClass);
    if (result == nullptr) return nullptr;

    env->SetIntField(result, sCV_type, static_cast<jint>(value.type));
    env->SetIntField(result, sCV_solidColor, static_cast<jint>(value.solidColor));
    env->SetBooleanField(result, sCV_isCurrentColor, value.isCurrentColor);

    if (value.type == ColorValueType::Gradient) {
        ScopedLocalRef<jobject> gi(env, buildGradientInfo(env, value.gradient));
        env->SetObjectField(result, sCV_gradient, gi.get());
    }

    return result;
}

jint register_jni_ColorParser(JNIEnv* env) {
    // Cache global class references
    jclass localCV = env->FindClass("com/amap/agenui/ColorValue");
    if (localCV == nullptr) return JNI_ERR;
    sColorValueClass = (jclass)env->NewGlobalRef(localCV);
    env->DeleteLocalRef(localCV);

    jclass localGI = env->FindClass("com/amap/agenui/ColorValue$GradientInfo");
    if (localGI == nullptr) return JNI_ERR;
    sGradientInfoClass = (jclass)env->NewGlobalRef(localGI);
    env->DeleteLocalRef(localGI);

    jclass localCS = env->FindClass("com/amap/agenui/ColorValue$ColorStop");
    if (localCS == nullptr) return JNI_ERR;
    sColorStopClass = (jclass)env->NewGlobalRef(localCS);
    env->DeleteLocalRef(localCS);

    jclass localLP = env->FindClass("com/amap/agenui/ColorValue$LinearParams");
    if (localLP == nullptr) return JNI_ERR;
    sLinearParamsClass = (jclass)env->NewGlobalRef(localLP);
    env->DeleteLocalRef(localLP);

    jclass localRP = env->FindClass("com/amap/agenui/ColorValue$RadialParams");
    if (localRP == nullptr) return JNI_ERR;
    sRadialParamsClass = (jclass)env->NewGlobalRef(localRP);
    env->DeleteLocalRef(localRP);

    jclass localCP = env->FindClass("com/amap/agenui/ColorValue$ConicParams");
    if (localCP == nullptr) return JNI_ERR;
    sConicParamsClass = (jclass)env->NewGlobalRef(localCP);
    env->DeleteLocalRef(localCP);

    if (!cacheFieldIds(env)) return JNI_ERR;

    ScopedLocalRef<jclass> agenui(env, env->FindClass("com/amap/agenui/AGenUI"));
    if (agenui.get() == nullptr) return JNI_ERR;

    JNINativeMethod nativeMethods[] = {
        {"nativeParseColor", "(Ljava/lang/String;)Lcom/amap/agenui/ColorValue;", (void*)jni_parseColor},
    };

    jint ret = env->RegisterNatives(agenui.get(), nativeMethods, sizeof(nativeMethods) / sizeof(nativeMethods[0]));
    return (ret == JNI_OK) ? JNI_OK : JNI_ERR;
}

} // namespace agenui
