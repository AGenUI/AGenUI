#pragma once

#include <native_drawing/drawing_font_collection.h>
#include <native_drawing/drawing_text_typography.h>

namespace a2ui {

struct TypoStyleGuard {
    OH_Drawing_TypographyStyle* p = nullptr;
    explicit TypoStyleGuard(OH_Drawing_TypographyStyle* ptr) : p(ptr) {}
    ~TypoStyleGuard() { if (p) OH_Drawing_DestroyTypographyStyle(p); }
    TypoStyleGuard(const TypoStyleGuard&) = delete;
    TypoStyleGuard& operator=(const TypoStyleGuard&) = delete;
};

struct FontCollGuard {
    OH_Drawing_FontCollection* p = nullptr;
    explicit FontCollGuard(OH_Drawing_FontCollection* ptr) : p(ptr) {}
    ~FontCollGuard() { if (p) OH_Drawing_DestroyFontCollection(p); }
    FontCollGuard(const FontCollGuard&) = delete;
    FontCollGuard& operator=(const FontCollGuard&) = delete;
};

struct TypoHandlerGuard {
    OH_Drawing_TypographyCreate* p = nullptr;
    explicit TypoHandlerGuard(OH_Drawing_TypographyCreate* ptr) : p(ptr) {}
    ~TypoHandlerGuard() { if (p) OH_Drawing_DestroyTypographyHandler(p); }
    TypoHandlerGuard(const TypoHandlerGuard&) = delete;
    TypoHandlerGuard& operator=(const TypoHandlerGuard&) = delete;
};

struct TextStyleGuard {
    OH_Drawing_TextStyle* p = nullptr;
    explicit TextStyleGuard(OH_Drawing_TextStyle* ptr) : p(ptr) {}
    ~TextStyleGuard() { if (p) OH_Drawing_DestroyTextStyle(p); }
    TextStyleGuard(const TextStyleGuard&) = delete;
    TextStyleGuard& operator=(const TextStyleGuard&) = delete;
};

struct TypographyGuard {
    OH_Drawing_Typography* p = nullptr;
    explicit TypographyGuard(OH_Drawing_Typography* ptr) : p(ptr) {}
    ~TypographyGuard() { if (p) OH_Drawing_DestroyTypography(p); }
    TypographyGuard(const TypographyGuard&) = delete;
    TypographyGuard& operator=(const TypographyGuard&) = delete;
};

struct LineMetricsGuard {
    OH_Drawing_LineMetrics* p = nullptr;
    explicit LineMetricsGuard(OH_Drawing_LineMetrics* ptr) : p(ptr) {}
    ~LineMetricsGuard() { if (p) OH_Drawing_DestroyLineMetrics(p); }
    LineMetricsGuard(const LineMetricsGuard&) = delete;
    LineMetricsGuard& operator=(const LineMetricsGuard&) = delete;
};

} // namespace a2ui
