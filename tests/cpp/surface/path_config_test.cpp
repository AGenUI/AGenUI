#include <gtest/gtest.h>
#include "surface/agenui_path_config.h"

using agenui::PathConfig;

// ============================================================================
// setPathConfig: Success paths
// ============================================================================

TEST(PathConfig, SetTemplateDir_Success) {
    PathConfig config;
    ASSERT_TRUE(config.setPathConfig(R"({"templateDir": "/data/templates"})"));
    EXPECT_EQ(config.getTemplateDir(), "/data/templates");
}

TEST(PathConfig, EmptyTemplateDir_StoresEmpty) {
    PathConfig config;
    ASSERT_TRUE(config.setPathConfig(R"({"templateDir": ""})"));
    EXPECT_EQ(config.getTemplateDir(), "");
}

TEST(PathConfig, EmptyJsonObject_TemplateDirUnset) {
    PathConfig config;
    ASSERT_TRUE(config.setPathConfig("{}"));
    EXPECT_TRUE(config.getTemplateDir().empty());
}

TEST(PathConfig, ExtraFields_Ignored) {
    PathConfig config;
    ASSERT_TRUE(config.setPathConfig(R"({"templateDir": "/tmp", "unknownKey": 123})"));
    EXPECT_EQ(config.getTemplateDir(), "/tmp");
}

// ============================================================================
// setPathConfig: Error paths
// ============================================================================

TEST(PathConfig, InvalidJson_ReturnsFalse) {
    PathConfig config;
    EXPECT_FALSE(config.setPathConfig("not json"));
}

TEST(PathConfig, EmptyString_ReturnsFalse) {
    PathConfig config;
    EXPECT_FALSE(config.setPathConfig(""));
}

TEST(PathConfig, TemplateDirWrongType_Ignored) {
    PathConfig config;
    // templateDir is a number, should be ignored (not set)
    ASSERT_TRUE(config.setPathConfig(R"({"templateDir": 12345})"));
    EXPECT_TRUE(config.getTemplateDir().empty());
}

TEST(PathConfig, TemplateDirNull_Ignored) {
    PathConfig config;
    ASSERT_TRUE(config.setPathConfig(R"({"templateDir": null})"));
    EXPECT_TRUE(config.getTemplateDir().empty());
}

// ============================================================================
// Multiple calls: last wins
// ============================================================================

TEST(PathConfig, MultipleSets_LastWins) {
    PathConfig config;
    ASSERT_TRUE(config.setPathConfig(R"({"templateDir": "/first"})"));
    ASSERT_TRUE(config.setPathConfig(R"({"templateDir": "/second"})"));
    EXPECT_EQ(config.getTemplateDir(), "/second");
}

TEST(PathConfig, SetThenSetWithoutKey_PreservesOld) {
    PathConfig config;
    ASSERT_TRUE(config.setPathConfig(R"({"templateDir": "/original"})"));
    // Second call has no templateDir key — old value should remain
    ASSERT_TRUE(config.setPathConfig(R"({"otherKey": "val"})"));
    EXPECT_EQ(config.getTemplateDir(), "/original");
}
