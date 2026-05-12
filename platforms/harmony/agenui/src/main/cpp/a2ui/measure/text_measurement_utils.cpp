#include "text_measurement_utils.h"
#include "hm_text_measure_utils.h"
#include "a2ui/third_party/key_define.h"
#include <climits>

namespace a2ui {

void TextMeasurementUtils::buildSimpleParam(const std::string& text,
                                             float fontSize,
                                             TextMeasureParam& outParam,
                                             int fontWeight) {
    outParam.text             = text.c_str();
    outParam.fontSize         = (fontSize > 0.0f) ? static_cast<int>(fontSize) : 24;
    outParam.fontWeight       = fontWeight;
    outParam.fontStyle        = NODE_PROPERTY_FONT_NORMAL;
    outParam.textAlign        = TEXT_ALIGN_LEFT_TOP;
    outParam.isMultLineHeight = true;
    outParam.lineHeight       = 1.0f;
    outParam.maxLines         = INT_MAX;
    outParam.id               = 0;
    outParam.textOverflow     = NODE_PROPERTY_TEXT_OVERFLOW_UNDEFINED;
    outParam.isRichtext       = false;
    outParam.fontFamily       = "";
    outParam.extras           = "";
    outParam.letter_spacing   = 0.0f;
    outParam.ctx_id           = 0;
}

float TextMeasurementUtils::doMeasureHeight(const TextMeasureParam& param,
                                             float maxWidth,
                                             MeasureMode widthMode) {
    float baseLine = 0.f, ascent = 0.f, descent = 0.f;
    auto r = TextMeasureUtils::doMeasure(
        param,
        maxWidth, widthMode,
        0.0f,     MeasureModeUndefined,
        baseLine, ascent, descent);
    return r.height;
}

float TextMeasurementUtils::measureTextHeight(const std::string& text,
                                               float maxWidth,
                                               float fontSize,
                                               int fontWeight) {
    TextMeasureParam param;
    buildSimpleParam(text, fontSize, param, fontWeight);
    return doMeasureHeight(param, maxWidth);
}

}  // namespace a2ui
