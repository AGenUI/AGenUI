#include "a2ui/utils/a2ui_writing_direction.h"
#include "a2ui/utils/a2ui_list_layout_utils.h"
#include <gtest/gtest.h>
#include <string>
#include <utility>

using a2ui::writing_direction::Direction;
using a2ui::writing_direction::TextAlignment;

struct AlignmentCase {
    const char* value;
    TextAlignment ltr;
    TextAlignment rtl;
};

TEST(HarmonyWritingDirectionTest, PhysicalTextAlignment) {
    const AlignmentCase cases[] = {
        {"left", TextAlignment::Left, TextAlignment::Left},
        {"right", TextAlignment::Right, TextAlignment::Right},
        {"center", TextAlignment::Center, TextAlignment::Center},
        {"start", TextAlignment::Left, TextAlignment::Right},
        {"end", TextAlignment::Right, TextAlignment::Left},
        {"", TextAlignment::Left, TextAlignment::Left},
        {"invalid", TextAlignment::Left, TextAlignment::Left},
        {" \t\r\n", TextAlignment::Left, TextAlignment::Left},
        {"  RIGHT\tbottom", TextAlignment::Right, TextAlignment::Right},
        {"left bottom", TextAlignment::Left, TextAlignment::Left},
        {"center center", TextAlignment::Center, TextAlignment::Center},
        {"start top", TextAlignment::Left, TextAlignment::Right},
        {"END\ncenter", TextAlignment::Right, TextAlignment::Left},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.value);
        for (const auto direction : {Direction::Ltr, Direction::Rtl}) {
            const auto expected = direction == Direction::Rtl ? testCase.rtl : testCase.ltr;
            EXPECT_EQ(a2ui::writing_direction::physicalTextAlignment(testCase.value, direction), expected);
        }
    }
}

TEST(HarmonyWritingDirectionTest, ResolvePageDirection) {
    const char* directions[] = {"ltr", "rtl", "", "invalid", "horizontal", "vertical", "RTL"};
    for (const auto* value : directions) {
        const auto expected = std::string(value) == "rtl" ? Direction::Rtl : Direction::Ltr;
        EXPECT_EQ(a2ui::writing_direction::resolve(value), expected) << value;
    }
}

TEST(HarmonyListLayoutTest, PhysicalSpacing) {
    using a2ui::list_layout::itemSpacing;
    using a2ui::list_layout::leadingSpacing;
    using a2ui::list_layout::trailingSpacing;
    const std::pair<float, float> spacingCases[] = {
        {leadingSpacing(24, 100, 400, false), 24},
        {leadingSpacing(276, 100, 400, true), 24},
        {leadingSpacing(-24, 100, 400, false), 0},
        {leadingSpacing(324, 100, 400, true), 0},
        {itemSpacing(140, 80, 24, 100, false), 16},
        {itemSpacing(180, 80, 276, 100, true), 16},
        {itemSpacing(-96, 160, 84, 80, true), 20},
        {itemSpacing(80, 100, 24, 100, false), 0},
        {itemSpacing(250, 100, 276, 100, true), 0},
        {trailingSpacing(32, 10), 42},
        {trailingSpacing(24, 18), 42},
        {trailingSpacing(24, 0), 24},
        {trailingSpacing(0, 18), 18},
        {trailingSpacing(-1, -2), 0},
    };
    for (const auto& testCase : spacingCases) {
        EXPECT_FLOAT_EQ(testCase.first, testCase.second);
    }
}
