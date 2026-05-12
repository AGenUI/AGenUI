#include "jni_agenui_measurement.h"

#include <jni.h>
#include <mutex>
#include <string>

#include "agenui_engine_entry.h"
#include "agenui_logger_internal.h"
#include "agenui_measurement.h"
#include "jni_helper.h"

namespace agenui {

namespace {

constexpr const char* kMeasurementBridgeClassName =
        "com/amap/agenui/render/measurement/MeasurementBridge";
constexpr const char* kMeasureResultClassName =
        "com/amap/agenui/render/measurement/MeasureResult";

std::mutex sInitMutex;
jclass sMeasurementBridgeClass = nullptr;
jmethodID sDirectMeasureMethod = nullptr;
jfieldID sCalcTypeField = nullptr;
jfieldID sWidthField = nullptr;
jfieldID sHeightField = nullptr;
jfieldID sLineCountField = nullptr;

const char* calcTypeToString(CalcType calcType) {
    return calcType == CalcType::Async ? "Async" : "Sync";
}

std::string formatMeasureResult(const MeasureResult& result) {
    return "MeasureResult{calcType=" + std::string(calcTypeToString(result.calcType))
           + ", width=" + std::to_string(result.width)
           + ", height=" + std::to_string(result.height)
           + ", countOfLines=" + std::to_string(result.countOfLines)
           + "}";
}

bool isMeasurementBridgeInitializedLocked() {
    return sMeasurementBridgeClass != nullptr && sDirectMeasureMethod != nullptr &&
           sCalcTypeField != nullptr && sWidthField != nullptr &&
           sHeightField != nullptr && sLineCountField != nullptr;
}

// Clears the cached JNI refs so a later re-init attempt always rebuilds a full, consistent set.
void resetMeasurementBridgeLocked(JNIEnv* env) {
    if (sMeasurementBridgeClass != nullptr) {
        env->DeleteGlobalRef(sMeasurementBridgeClass);
        sMeasurementBridgeClass = nullptr;
    }
    sDirectMeasureMethod = nullptr;
    sCalcTypeField = nullptr;
    sWidthField = nullptr;
    sHeightField = nullptr;
    sLineCountField = nullptr;
}

// Runs exactly once on the Java init thread to warm up every JNI ref needed by measure callbacks.
bool initializeMeasurementBridgeLocked(JNIEnv* env) {
    if (isMeasurementBridgeInitializedLocked()) {
        return true;
    }

    resetMeasurementBridgeLocked(env);

    jclass measurementBridgeLocal = env->FindClass(kMeasurementBridgeClassName);
    if (measurementBridgeLocal == nullptr) {
        env->ExceptionClear();
        AGENUI_LOG("[JNI] MeasurementBridge class not found");
        return false;
    }

    jclass measurementBridgeGlobal =
            reinterpret_cast<jclass>(env->NewGlobalRef(measurementBridgeLocal));
    env->DeleteLocalRef(measurementBridgeLocal);
    if (measurementBridgeGlobal == nullptr) {
        AGENUI_LOG("[JNI] Failed to create global ref for MeasurementBridge");
        return false;
    }

    jmethodID directMeasureMethod = env->GetStaticMethodID(
            measurementBridgeGlobal,
            "directMeasure",
            "(Ljava/lang/String;Ljava/lang/String;FIFI)Lcom/amap/agenui/render/measurement/MeasureResult;");
    if (directMeasureMethod == nullptr) {
        env->ExceptionClear();
        env->DeleteGlobalRef(measurementBridgeGlobal);
        AGENUI_LOG("[JNI] MeasurementBridge.directMeasure not found");
        return false;
    }

    jclass measureResultLocal = env->FindClass(kMeasureResultClassName);
    if (measureResultLocal == nullptr) {
        env->ExceptionClear();
        env->DeleteGlobalRef(measurementBridgeGlobal);
        AGENUI_LOG("[JNI] MeasureResult class not found");
        return false;
    }

    jfieldID calcTypeField = env->GetFieldID(measureResultLocal, "calcType", "I");
    jfieldID widthField = env->GetFieldID(measureResultLocal, "width", "F");
    jfieldID heightField = env->GetFieldID(measureResultLocal, "height", "F");
    jfieldID lineCountField = env->GetFieldID(measureResultLocal, "lineCount", "I");
    env->DeleteLocalRef(measureResultLocal);

    if (calcTypeField == nullptr || widthField == nullptr ||
        heightField == nullptr || lineCountField == nullptr) {
        env->ExceptionClear();
        env->DeleteGlobalRef(measurementBridgeGlobal);
        AGENUI_LOG("[JNI] MeasureResult fields not found");
        return false;
    }

    sMeasurementBridgeClass = measurementBridgeGlobal;
    sDirectMeasureMethod = directMeasureMethod;
    sCalcTypeField = calcTypeField;
    sWidthField = widthField;
    sHeightField = heightField;
    sLineCountField = lineCountField;
    return true;
}

// Copies the warmed-up JNI refs to the current measure call without exposing the globals directly.
bool getMeasurementBridgeRefs(
        jclass& measurementBridgeClass,
        jmethodID& directMeasureMethod,
        jfieldID& calcTypeField,
        jfieldID& widthField,
        jfieldID& heightField,
        jfieldID& lineCountField) {
    std::lock_guard<std::mutex> lock(sInitMutex);
    if (!isMeasurementBridgeInitializedLocked()) {
        return false;
    }

    measurementBridgeClass = sMeasurementBridgeClass;
    directMeasureMethod = sDirectMeasureMethod;
    calcTypeField = sCalcTypeField;
    widthField = sWidthField;
    heightField = sHeightField;
    lineCountField = sLineCountField;
    return true;
}

/**
 * Native-side IMeasurement adapter for Android platform components.
 *
 * Yoga calls this implementation from the engine measurement manager. The adapter never owns
 * measurement logic itself; it simply marshals `paramJson + MeasureModes` into
 * MeasurementBridge.directMeasure() and converts the returned MeasureResult back to C++.
 */
class AndroidBridgeMeasurement final : public IMeasurement {
public:
    explicit AndroidBridgeMeasurement(std::string type)
        : m_type(std::move(type)) {
    }

    MeasureResult measure(const std::string& paramJson, const MeasureModes& modes) override {
        JNIEnv* env = JNIHelper::getJNIEnv();
        if (env == nullptr) {
            AGENUI_LOG("[JNI] AndroidBridgeMeasurement.measure skipped: env is null, type=%s", m_type.c_str());
            return MeasureResult{CalcType::Sync, 0.0f, 0.0f, 0};
        }

        jclass measurementBridgeClass = nullptr;
        jmethodID directMeasureMethod = nullptr;
        jfieldID calcTypeField = nullptr;
        jfieldID widthField = nullptr;
        jfieldID heightField = nullptr;
        jfieldID lineCountField = nullptr;
        if (!getMeasurementBridgeRefs(
                    measurementBridgeClass,
                    directMeasureMethod,
                    calcTypeField,
                    widthField,
                    heightField,
                    lineCountField)) {
            AGENUI_LOG(
                    "[JNI] AndroidBridgeMeasurement.measure skipped: bridge not initialized, type=%s",
                    m_type.c_str());
            return MeasureResult{CalcType::Sync, 0.0f, 0.0f, 0};
        }

        AGENUI_LOG(
                "[JNI] AndroidBridgeMeasurement.measure start: type=%s, paramJson=%s, width={maxValue=%f, mode=%d}, height={maxValue=%f, mode=%d}",
                m_type.c_str(),
                paramJson.c_str(),
                modes.width.maxValue,
                modes.width.mode,
                modes.height.maxValue,
                modes.height.mode);

        jstring jType = env->NewStringUTF(m_type.c_str());
        jstring jParamJson = env->NewStringUTF(paramJson.c_str());

        jobject resultObj = env->CallStaticObjectMethod(
                measurementBridgeClass,
                directMeasureMethod,
                jType,
                jParamJson,
                modes.width.maxValue,
                modes.width.mode,
                modes.height.maxValue,
                modes.height.mode);

        env->DeleteLocalRef(jType);
        env->DeleteLocalRef(jParamJson);

        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            AGENUI_LOG("[JNI] MeasurementBridge.directMeasure threw, type=%s", m_type.c_str());
            if (resultObj != nullptr) {
                env->DeleteLocalRef(resultObj);
            }
            return MeasureResult{CalcType::Sync, 0.0f, 0.0f, 0};
        }

        if (resultObj == nullptr) {
            AGENUI_LOG("[JNI] MeasurementBridge.directMeasure returned null, type=%s", m_type.c_str());
            return MeasureResult{CalcType::Sync, 0.0f, 0.0f, 0};
        }

        MeasureResult result;
        jint calcType = env->GetIntField(resultObj, calcTypeField);
        result.calcType = calcType == 1 ? CalcType::Async : CalcType::Sync;
        result.width = env->GetFloatField(resultObj, widthField);
        result.height = env->GetFloatField(resultObj, heightField);
        result.countOfLines = env->GetIntField(resultObj, lineCountField);
        env->DeleteLocalRef(resultObj);
        AGENUI_LOG(
                "[JNI] AndroidBridgeMeasurement.measure result: type=%s, result=%s",
                m_type.c_str(),
                formatMeasureResult(result).c_str());
        return result;
    }

private:
    std::string m_type;
};

}  // namespace

bool initializeAndroidMeasurementBridge(JNIEnv* env) {
    if (env == nullptr) {
        AGENUI_LOG("[JNI] initializeAndroidMeasurementBridge: env is null");
        return false;
    }

    std::lock_guard<std::mutex> lock(sInitMutex);
    bool success = initializeMeasurementBridgeLocked(env);
    AGENUI_LOG("[JNI] initializeAndroidMeasurementBridge: %s", success ? "success" : "failed");
    return success;
}

void registerAndroidMeasurements() {
    IAGenUIEngine* engine = getAGenUIEngine();
    if (engine == nullptr) {
        AGENUI_LOG("[JNI] registerAndroidMeasurements: engine is null");
        return;
    }

    IMeasurementManager* measurementManager = engine->getMeasurementManager();
    if (measurementManager == nullptr) {
        AGENUI_LOG("[JNI] registerAndroidMeasurements: measurement manager is null");
        return;
    }

    static const char* const kMeasurementTypes[] = {
            "Text",
            "RichText",
//            "Image",
            "CheckBox",
            "ChoicePicker",
            "DateTimeInput",
            "Slider",
            "AudioPlayer",
            "Icon",
            "Table",
    };

    for (const char* type : kMeasurementTypes) {
        measurementManager->registerMeasurement(
                type,
                std::make_shared<AndroidBridgeMeasurement>(type));
    }
}

}  // namespace agenui
