#include "napi_internal.h"
#include "agenui_measurement.h"
#include "agenui_render_info_types.h"
#include "a2ui/utils/a2ui_unit_utils.h"

#include <condition_variable>

napi_value ReportComponentRenderSize(napi_env env, napi_callback_info info) {
    size_t argc = 6;
    napi_value args[6];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 5) {
        HM_LOGE("ReportComponentRenderSize: Expected 5 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    std::string surfaceId   = napiGetString(env, args[0]);
    std::string componentId = napiGetString(env, args[1]);
    std::string type        = napiGetString(env, args[2]);

    double height = 0.0;
    napi_get_value_double(env, args[3], &height);

    double width = 0.0;
    napi_get_value_double(env, args[4], &width);

    HM_LOGI("ReportComponentRenderSize: surfaceId=%s, componentId=%s, type=%s, height=%f, width=%f", surfaceId.c_str(), componentId.c_str(), type.c_str(), height, width);

    int instanceId = agenui::A2UIMessageListener::findInstanceIdBySurfaceId(surfaceId);
    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("ReportComponentRenderSize: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }
    agenui::ISurfaceManager* sm = engine->findSurfaceManager(instanceId);
    if (!sm) {
        HM_LOGE("ReportComponentRenderSize: SurfaceManager not found for surfaceId=%s", surfaceId.c_str());
        NAPI_RETURN_UNDEFINED(env);
    }

    agenui::ComponentRenderInfo markdownInfo;
    markdownInfo.surfaceId   = surfaceId;
    markdownInfo.componentId = componentId;
    markdownInfo.type        = type;
    markdownInfo.height      = a2ui::UnitConverter::vpToA2ui(height);
    markdownInfo.width       = a2ui::UnitConverter::vpToA2ui(width);
    sm->onRenderFinish(markdownInfo);

    NAPI_RETURN_UNDEFINED(env);
}

napi_value RegisterMeasurement(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 3) {
        HM_LOGE("RegisterMeasurement: Expected 3 arguments, got %zu", argc);
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string type = napiGetString(env, args[1]);

    napi_valuetype callbackType = napi_undefined;
    napi_typeof(env, args[2], &callbackType);
    if (callbackType != napi_function) {
        HM_LOGE("RegisterMeasurement: Third argument is not a function");
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("RegisterMeasurement: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }

    auto* mm = engine->getMeasurementManager();
    if (!mm) {
        HM_LOGE("RegisterMeasurement: No MeasurementManager for instanceId=%d", instanceId);
        NAPI_RETURN_UNDEFINED(env);
    }

    struct MeasureCallData {
        std::string paramJson;
        float widthMaxValue  = 0.0f;
        int   widthMode      = 0;
        float heightMaxValue = 0.0f;
        int   heightMode     = 0;
        float resultWidth    = 0.0f;
        float resultHeight   = 0.0f;
        int   resultCalcType = 0;
        bool  done = false;
        std::mutex mtx;
        std::condition_variable cv;
    };
    auto measureCallJs = [](napi_env env, napi_value js_func, void* /*context*/, void* data) {
        if (!data) return;
        auto* cd = static_cast<MeasureCallData*>(data);

        napi_value jsParamJson = nullptr;
        napi_create_string_utf8(env, cd->paramJson.c_str(), cd->paramJson.size(), &jsParamJson);
        napi_value jsWidthMode = nullptr;
        napi_create_int32(env, cd->widthMode, &jsWidthMode);
        napi_value jsMaxWidth = nullptr;
        napi_create_double(env, cd->widthMaxValue, &jsMaxWidth);
        napi_value jsHeightMode = nullptr;
        napi_create_int32(env, cd->heightMode, &jsHeightMode);
        napi_value jsMaxHeight = nullptr;
        napi_create_double(env, cd->heightMaxValue, &jsMaxHeight);

        napi_value argv[5] = { jsParamJson, jsWidthMode, jsMaxWidth, jsHeightMode, jsMaxHeight };
        napi_value result = nullptr;
        napi_call_function(env, nullptr, js_func, 5, argv, &result);

        if (result) {
            napi_value widthVal = nullptr, heightVal = nullptr, calcTypeVal = nullptr;
            napi_get_named_property(env, result, "width",    &widthVal);
            napi_get_named_property(env, result, "height",   &heightVal);
            napi_get_named_property(env, result, "calcType", &calcTypeVal);

            double w = 0.0, h = 0.0;
            napi_get_value_double(env, widthVal,  &w);
            napi_get_value_double(env, heightVal, &h);
            cd->resultWidth  = static_cast<float>(w);
            cd->resultHeight = static_cast<float>(h);

            napi_valuetype ct = napi_undefined;
            if (calcTypeVal) napi_typeof(env, calcTypeVal, &ct);
            if (ct == napi_number) {
                int32_t calcType = 0;
                napi_get_value_int32(env, calcTypeVal, &calcType);
                cd->resultCalcType = calcType;
            }
        }

        {
            std::lock_guard<std::mutex> lk(cd->mtx);
            cd->done = true;
        }
        cd->cv.notify_one();
    };

    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "MeasurementCallback", NAPI_AUTO_LENGTH, &resourceName);
    napi_threadsafe_function tsfn = nullptr;
    napi_create_threadsafe_function(env, args[2], nullptr, resourceName,
                                    0, 1, nullptr, nullptr, nullptr,
                                    measureCallJs, &tsfn);
    if (!tsfn) {
        HM_LOGE("RegisterMeasurement: Failed to create threadsafe function");
        NAPI_RETURN_UNDEFINED(env);
    }

    class ETSMeasurement : public agenui::IMeasurement {
    public:
        explicit ETSMeasurement(napi_threadsafe_function tsfn) : _tsfn(tsfn) {}
        ~ETSMeasurement() override {
            if (_tsfn) napi_release_threadsafe_function(_tsfn, napi_tsfn_release);
        }

        agenui::MeasureResult measure(
                const std::string& paramJson,
                const agenui::MeasureModes& modes) override {
            MeasureCallData data;
            data.paramJson       = paramJson;
            data.widthMaxValue   = modes.width.maxValue;
            data.widthMode       = modes.width.mode;
            data.heightMaxValue  = modes.height.maxValue;
            data.heightMode      = modes.height.mode;

            napi_call_threadsafe_function(
                _tsfn, &data, napi_tsfn_blocking);

            {
                std::unique_lock<std::mutex> lk(data.mtx);
                data.cv.wait(lk, [&]{ return data.done; });
            }
            const auto ct = (data.resultCalcType == 1)
                ? agenui::CalcType::Async
                : agenui::CalcType::Sync;
            return {ct, data.resultWidth, data.resultHeight, 0};
        }

    private:
        napi_threadsafe_function _tsfn = nullptr;
    };

    auto impl = std::make_shared<ETSMeasurement>(tsfn);
    mm->registerMeasurement(type, impl);
    HM_LOGI("RegisterMeasurement: instanceId=%d type=%s registered", instanceId, type.c_str());
    NAPI_RETURN_UNDEFINED(env);
}

napi_value UnregisterMeasurement(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        HM_LOGE("UnregisterMeasurement: Expected 2 arguments");
        NAPI_RETURN_UNDEFINED(env);
    }

    int32_t instanceId = 0;
    napi_get_value_int32(env, args[0], &instanceId);

    std::string type = napiGetString(env, args[1]);

    auto* engine = agenui::getAGenUIEngine();
    if (!engine) {
        HM_LOGE("UnregisterMeasurement: Engine not initialized");
        NAPI_RETURN_UNDEFINED(env);
    }
    auto* mm = engine->getMeasurementManager();
    if (mm) {
        mm->unregisterMeasurement(type);
        HM_LOGI("UnregisterMeasurement: instanceId=%d type=%s removed", instanceId, type.c_str());
    }
    NAPI_RETURN_UNDEFINED(env);
}
