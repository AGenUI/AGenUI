#include <gtest/gtest.h>
#include "function_call/agenui_functioncall_config.h"

using agenui::FunctionCallConfig;
using nlohmann::json;

// ============================================================================
// fromJson: Normal paths
// ============================================================================

TEST(FunctionCallConfigFromJson, AllFields) {
    json j = {
        {"namespace", "agenui.platform"},
        {"name", "toast"},
        {"description", "Show a toast"},
        {"returnType", "object"}
    };
    auto config = FunctionCallConfig::fromJson(j);
    EXPECT_EQ(config.getNamespace(), "agenui.platform");
    EXPECT_EQ(config.getName(), "toast");
    EXPECT_EQ(config.getDescription(), "Show a toast");
    EXPECT_EQ(config.getReturnType(), "object");
}

TEST(FunctionCallConfigFromJson, WithoutNamespace) {
    json j = {
        {"name", "regex"},
        {"description", "Match regex"},
        {"returnType", "boolean"}
    };
    auto config = FunctionCallConfig::fromJson(j);
    EXPECT_TRUE(config.getNamespace().empty());
    EXPECT_EQ(config.getName(), "regex");
}

TEST(FunctionCallConfigFromJson, EmptyJson_AllFieldsEmpty) {
    json j = json::object();
    auto config = FunctionCallConfig::fromJson(j);
    EXPECT_TRUE(config.getNamespace().empty());
    EXPECT_TRUE(config.getName().empty());
    EXPECT_TRUE(config.getDescription().empty());
    EXPECT_TRUE(config.getReturnType().empty());
}

// ============================================================================
// fromJson: Edge cases — wrong types ignored gracefully
// ============================================================================

TEST(FunctionCallConfigFromJson, NameIsNumber_IgnoredAsEmpty) {
    json j = {{"name", 123}, {"description", "desc"}, {"returnType", "void"}};
    auto config = FunctionCallConfig::fromJson(j);
    EXPECT_TRUE(config.getName().empty());
    EXPECT_EQ(config.getDescription(), "desc");
}

TEST(FunctionCallConfigFromJson, NullFields_IgnoredAsEmpty) {
    json j = {{"name", nullptr}, {"namespace", nullptr}};
    auto config = FunctionCallConfig::fromJson(j);
    EXPECT_TRUE(config.getName().empty());
    EXPECT_TRUE(config.getNamespace().empty());
}

TEST(FunctionCallConfigFromJson, ExtraFields_Ignored) {
    json j = {
        {"name", "test"},
        {"description", "d"},
        {"returnType", "string"},
        {"unknown_field", "whatever"},
        {"version", 2}
    };
    auto config = FunctionCallConfig::fromJson(j);
    EXPECT_EQ(config.getName(), "test");
}

// ============================================================================
// toJson
// ============================================================================

TEST(FunctionCallConfigToJson, WithNamespace_IncludesAll) {
    FunctionCallConfig config;
    config.setNamespace("ns");
    config.setName("fn");
    config.setDescription("desc");
    config.setReturnType("boolean");

    auto j = config.toJson();
    EXPECT_EQ(j["namespace"], "ns");
    EXPECT_EQ(j["name"], "fn");
    EXPECT_EQ(j["description"], "desc");
    EXPECT_EQ(j["returnType"], "boolean");
}

TEST(FunctionCallConfigToJson, WithoutNamespace_OmitsNamespaceKey) {
    FunctionCallConfig config;
    config.setName("fn");
    config.setDescription("desc");
    config.setReturnType("void");

    auto j = config.toJson();
    EXPECT_FALSE(j.contains("namespace"));
    EXPECT_EQ(j["name"], "fn");
}

TEST(FunctionCallConfigToJson, RoundTrip_Preserves) {
    json original = {
        {"namespace", "agenui.builtin"},
        {"name", "length"},
        {"description", "Check length"},
        {"returnType", "boolean"}
    };
    auto config = FunctionCallConfig::fromJson(original);
    auto serialized = config.toJson();
    EXPECT_EQ(serialized, original);
}

// ============================================================================
// isValid
// ============================================================================

TEST(FunctionCallConfigIsValid, NameSet_ReturnsTrue) {
    FunctionCallConfig config;
    config.setName("toast");
    EXPECT_TRUE(config.isValid());
}

TEST(FunctionCallConfigIsValid, NameEmpty_ReturnsFalse) {
    FunctionCallConfig config;
    EXPECT_FALSE(config.isValid());
}

TEST(FunctionCallConfigIsValid, FromJsonWithName_ReturnsTrue) {
    json j = {{"name", "x"}};
    auto config = FunctionCallConfig::fromJson(j);
    EXPECT_TRUE(config.isValid());
}

TEST(FunctionCallConfigIsValid, FromEmptyJson_ReturnsFalse) {
    json j = json::object();
    auto config = FunctionCallConfig::fromJson(j);
    EXPECT_FALSE(config.isValid());
}

// ============================================================================
// getFullName
// ============================================================================

TEST(FunctionCallConfigFullName, WithNamespace) {
    FunctionCallConfig config;
    config.setNamespace("agenui.platform");
    config.setName("toast");
    EXPECT_EQ(config.getFullName(), "agenui.platform::toast");
}

TEST(FunctionCallConfigFullName, WithoutNamespace_ReturnsNameOnly) {
    FunctionCallConfig config;
    config.setName("regex");
    EXPECT_EQ(config.getFullName(), "regex");
}

TEST(FunctionCallConfigFullName, BothEmpty_ReturnsEmpty) {
    FunctionCallConfig config;
    EXPECT_EQ(config.getFullName(), "");
}
