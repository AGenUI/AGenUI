#include <gtest/gtest.h>
#include "surface/style_defaults/agenui_style_defaults.h"

using agenui::StyleDefaults;

// ============================================================================
// getDefaults: Basic structure validation
// ============================================================================

TEST(StyleDefaults, ReturnsNonEmptyMap) {
    const auto& defaults = StyleDefaults::getDefaults();
    EXPECT_FALSE(defaults.empty());
}

TEST(StyleDefaults, ContainsExpectedKeys) {
    const auto& defaults = StyleDefaults::getDefaults();
    // These keys come from the embedded kStyleDefaultsConfig
    EXPECT_NE(defaults.find("width"), defaults.end());
    EXPECT_NE(defaults.find("height"), defaults.end());
    EXPECT_NE(defaults.find("flex-direction"), defaults.end());
    EXPECT_NE(defaults.find("justify-content"), defaults.end());
    EXPECT_NE(defaults.find("align-items"), defaults.end());
    EXPECT_NE(defaults.find("background-color"), defaults.end());
    EXPECT_NE(defaults.find("opacity"), defaults.end());
    EXPECT_NE(defaults.find("visibility"), defaults.end());
}

// ============================================================================
// getDefaults: Value correctness
// ============================================================================

TEST(StyleDefaults, WidthIsAuto) {
    const auto& defaults = StyleDefaults::getDefaults();
    // String values are stored as JSON-dumped strings (quoted)
    EXPECT_EQ(defaults.at("width"), "\"auto\"");
}

TEST(StyleDefaults, HeightIsAuto) {
    const auto& defaults = StyleDefaults::getDefaults();
    EXPECT_EQ(defaults.at("height"), "\"auto\"");
}

TEST(StyleDefaults, FlexDirectionIsColumn) {
    const auto& defaults = StyleDefaults::getDefaults();
    EXPECT_EQ(defaults.at("flex-direction"), "\"column\"");
}

TEST(StyleDefaults, FlexGrowIsZero) {
    const auto& defaults = StyleDefaults::getDefaults();
    // Numeric values are stored as their JSON representation (unquoted)
    EXPECT_EQ(defaults.at("flex-grow"), "0");
}

TEST(StyleDefaults, FlexShrinkIsOne) {
    const auto& defaults = StyleDefaults::getDefaults();
    EXPECT_EQ(defaults.at("flex-shrink"), "1");
}

TEST(StyleDefaults, OpacityIsOne) {
    const auto& defaults = StyleDefaults::getDefaults();
    EXPECT_EQ(defaults.at("opacity"), "1");
}

TEST(StyleDefaults, BackgroundColorIsTransparent) {
    const auto& defaults = StyleDefaults::getDefaults();
    EXPECT_EQ(defaults.at("background-color"), "\"transparent\"");
}

TEST(StyleDefaults, BorderRadiusIsZeroPx) {
    const auto& defaults = StyleDefaults::getDefaults();
    EXPECT_EQ(defaults.at("border-radius"), "\"0px\"");
}

TEST(StyleDefaults, VisibilityIsVisible) {
    const auto& defaults = StyleDefaults::getDefaults();
    EXPECT_EQ(defaults.at("visibility"), "\"visible\"");
}

// ============================================================================
// getDefaults: Stability — multiple calls return same reference
// ============================================================================

TEST(StyleDefaults, MultipleCalls_ReturnSameReference) {
    const auto& first = StyleDefaults::getDefaults();
    const auto& second = StyleDefaults::getDefaults();
    EXPECT_EQ(&first, &second);  // Static singleton
}

// ============================================================================
// getDefaults: Total count matches embedded config
// ============================================================================

TEST(StyleDefaults, TotalCount_MatchesConfigKeys) {
    const auto& defaults = StyleDefaults::getDefaults();
    // kStyleDefaultsConfig has 18 keys (verify count stays stable)
    EXPECT_EQ(defaults.size(), 18u);
}
