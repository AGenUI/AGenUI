#include "a2ui/utils/a2ui_parse_utils.h"

#include <gtest/gtest.h>

namespace {

void expectMargins(const nlohmann::json& styles, bool rtl,
                   float expectedTop, float expectedRight, float expectedBottom, float expectedLeft) {
    float top = 0.0f, right = 0.0f, bottom = 0.0f, left = 0.0f;
    a2ui::resolveUserMargin(styles, top, right, bottom, left, rtl);
    EXPECT_FLOAT_EQ(top, expectedTop) << "margin top";
    EXPECT_FLOAT_EQ(right, expectedRight) << "margin right";
    EXPECT_FLOAT_EQ(bottom, expectedBottom) << "margin bottom";
    EXPECT_FLOAT_EQ(left, expectedLeft) << "margin left";
}

void expectPadding(const nlohmann::json& styles, bool rtl,
                   float expectedTop, float expectedRight, float expectedBottom, float expectedLeft) {
    float top = 91.0f, right = 92.0f, bottom = 93.0f, left = 94.0f;
    a2ui::padding_utils::resolveUserPadding(styles, rtl, top, right, bottom, left);
    EXPECT_FLOAT_EQ(top, expectedTop) << "padding top";
    EXPECT_FLOAT_EQ(right, expectedRight) << "padding right";
    EXPECT_FLOAT_EQ(bottom, expectedBottom) << "padding bottom";
    EXPECT_FLOAT_EQ(left, expectedLeft) << "padding left";
}

}

TEST(HarmonyRtlLayoutTest, ObjectStylesUseTheOriginalObject) {
    const nlohmann::json styles = {{"padding", "8px 16px"}};
    nlohmann::json storage;
    EXPECT_EQ(a2ui::padding_utils::resolveStyleObject(styles, storage), &styles);
}

TEST(HarmonyRtlLayoutTest, StringStylesPreservePhysicalAndLogicalPadding) {
    const nlohmann::json styles = R"({"padding":"2px 4px 6px 8px","padding-inline-start":"32px","padding-inline-end":"12px"})";
    nlohmann::json storage;
    const auto* resolved = a2ui::padding_utils::resolveStyleObject(styles, storage);
    ASSERT_NE(resolved, nullptr);
    expectPadding(*resolved, false, 2, 12, 6, 32);
    expectPadding(*resolved, true, 2, 32, 6, 12);
}

TEST(HarmonyRtlLayoutTest, EmptyStringStylesClearPaddingLikeAnEmptyObject) {
    const nlohmann::json styles = "{}";
    nlohmann::json storage;
    const auto* resolved = a2ui::padding_utils::resolveStyleObject(styles, storage);
    ASSERT_NE(resolved, nullptr);
    expectPadding(*resolved, false, 0, 0, 0, 0);
    expectPadding(*resolved, true, 0, 0, 0, 0);
}

TEST(HarmonyRtlLayoutTest, InvalidStyleValuesAreNotTreatedAsPaddingDeletion) {
    for (const auto& styles : {nlohmann::json("invalid"), nlohmann::json("[]"),
                              nlohmann::json("null"), nlohmann::json(42), nlohmann::json(nullptr)}) {
        nlohmann::json storage;
        EXPECT_EQ(a2ui::padding_utils::resolveStyleObject(styles, storage), nullptr);
    }
}

TEST(HarmonyRtlLayoutTest, PhysicalAndLogicalMargins) {
    for (bool rtl : {false, true}) {
        SCOPED_TRACE(rtl ? "rtl" : "ltr");
        expectMargins({{"margin", "1px 2px 3px 4px"}}, rtl, 1, 2, 3, 4);
        expectMargins({{"margin", 5}, {"marginRight", 9}, {"margin-left", 7}}, rtl, 5, 5, 5, 7);
        expectMargins({{"margin", "0"}}, rtl, 0, 0, 0, 0);
        expectMargins(nlohmann::json::object(), rtl, 0, 0, 0, 0);
        expectMargins({{"margin-inline", "12px 28px"}, {"margin-block-start", 3}, {"margin-block-end", 7}},
                      rtl, 3, rtl ? 12 : 28, 7, rtl ? 28 : 12);
        expectMargins({{"margin", 9}, {"marginInline", "12px 28px"}, {"marginInlineStart", 16},
                       {"margin-inline-end", 4}}, rtl, 9, 4, 9, 4);
        expectMargins({{"margin-inline-start", -2}, {"margin-inline-end", 8}}, rtl,
                      0, rtl ? 0 : 8, 0, rtl ? 8 : 0);
    }
}

TEST(HarmonyRtlLayoutTest, PhysicalAndLogicalPadding) {
    for (bool rtl : {false, true}) {
        SCOPED_TRACE(rtl ? "rtl" : "ltr");
        float top = 0, right = 0, bottom = 0, left = 0;
        const nlohmann::json padding = {{"padding", "2px 5px 3px 7px"},
                                       {"padding-inline", "11px 23px"}, {"padding-inline-end", 17}};
        a2ui::padding_utils::resolveUserPadding(padding, rtl, top, right, bottom, left);
        EXPECT_FLOAT_EQ(top, 2);
        EXPECT_FLOAT_EQ(right, rtl ? 11 : 17);
        EXPECT_FLOAT_EQ(bottom, 3);
        EXPECT_FLOAT_EQ(left, rtl ? 17 : 11);
    }
}

TEST(HarmonyRtlLayoutTest, PaddingKeepsPhysicalEdgesWhenDirectionChanges) {
    for (bool rtl : {false, true}) {
        SCOPED_TRACE(rtl ? "rtl" : "ltr");
        expectPadding({{"padding", "2px 5px 3px 7px"}}, rtl, 2, 5, 3, 7);
        expectPadding({{"padding", 8}, {"padding-top", 1}, {"padding-right", 2},
                       {"padding-bottom", 3}, {"padding-left", 4}}, rtl, 1, 2, 3, 4);
        expectPadding({{"padding", "0"}}, rtl, 0, 0, 0, 0);
    }
}

TEST(HarmonyRtlLayoutTest, PaddingResolvesLogicalAliasesOverPhysicalBase) {
    for (bool rtl : {false, true}) {
        SCOPED_TRACE(rtl ? "rtl" : "ltr");
        expectPadding({{"padding", "2px 5px 3px 7px"}, {"padding-inline-start", 11}},
                      rtl, 2, rtl ? 11 : 5, 3, rtl ? 7 : 11);
        expectPadding({{"padding", 4}, {"padding-inline", "11px 23px"}, {"padding-inline-end", 17},
                       {"padding-block-start", 13}, {"padding-block-end", 19}},
                      rtl, 13, rtl ? 11 : 17, 19, rtl ? 17 : 11);
        expectPadding({{"padding", 4}, {"paddingInline", "11px 23px"}, {"paddingInlineStart", 29},
                       {"paddingInlineEnd", 17}, {"paddingBlockStart", 13}, {"paddingBlockEnd", 19}},
                      rtl, 4, 4, 4, 4);
        expectPadding({{"paddingInline", 6}, {"padding-inline", 12},
                       {"paddingInlineStart", 9}, {"padding-inline-start", 18}},
                      rtl, 0, rtl ? 18 : 12, 0, rtl ? 12 : 18);
    }
}

TEST(HarmonyRtlLayoutTest, PaddingDirectionChangesDoNotAccumulateOrMutateStyles) {
    const nlohmann::json styles = {{"padding", "2px 5px 3px 7px"},
                                   {"padding-inline-start", 11}, {"padding-block-start", 13}};
    const auto originalStyles = styles;
    for (bool rtl : {false, true, true, false, true, false}) {
        SCOPED_TRACE(rtl ? "rtl" : "ltr");
        expectPadding(styles, rtl, 13, rtl ? 11 : 5, 3, rtl ? 7 : 11);
    }
    EXPECT_EQ(styles, originalStyles);
}

TEST(HarmonyRtlLayoutTest, PaddingRestoresPhysicalBaseAfterLogicalRemoval) {
    for (bool rtl : {false, true}) {
        SCOPED_TRACE(rtl ? "rtl" : "ltr");
        nlohmann::json styles = {{"padding", "2px 5px 3px 7px"},
                                 {"padding-inline", "11px 23px"}, {"padding-inline-start", 29},
                                 {"padding-block-start", 13}};
        expectPadding(styles, rtl, 13, rtl ? 29 : 23, 3, rtl ? 23 : 29);
        styles.erase("padding-inline-start");
        expectPadding(styles, rtl, 13, rtl ? 11 : 23, 3, rtl ? 23 : 11);
        styles.erase("padding-inline");
        styles.erase("padding-block-start");
        expectPadding(styles, rtl, 2, 5, 3, 7);
        styles["padding-inline-start"] = 0;
        expectPadding(styles, rtl, 2, rtl ? 0 : 5, 3, rtl ? 7 : 0);
        styles["padding-inline-start"] = nullptr;
        expectPadding(styles, rtl, 2, 5, 3, 7);
        styles["padding-inline-start"] = 11;
        expectPadding(styles, rtl, 2, rtl ? 11 : 5, 3, rtl ? 7 : 11);
        styles.clear();
        expectPadding(styles, rtl, 0, 0, 0, 0);
        expectPadding(nullptr, rtl, 0, 0, 0, 0);
    }
}

TEST(HarmonyRtlLayoutTest, PaddingClampsNegativeAndKeepsPxOnlyFallbacks) {
    for (bool rtl : {false, true}) {
        SCOPED_TRACE(rtl ? "rtl" : "ltr");
        expectPadding({{"padding", -5}, {"padding-inline", "-2px 8px"},
                       {"padding-block-start", -3}}, rtl, 0, rtl ? 0 : 8, 0, rtl ? 8 : 0);
        expectPadding({{"padding", "2px 5px 3px 7px"}, {"padding-inline", "invalid"},
                       {"padding-inline-start", false}, {"padding-block-start", nullptr}},
                      rtl, 2, 5, 3, 7);
        expectPadding({{"padding", 4}, {"padding-inline", "10% 2em"},
                       {"padding-block-start", "calc(1px + 2px)"}},
                      rtl, 0, 0, 4, 0);
        expectPadding(nlohmann::json::array(), rtl, 0, 0, 0, 0);
    }
}
