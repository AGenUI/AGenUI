#include <gtest/gtest.h>
#include "function_call/agenui_functioncall_manager.h"
#include "function_call/agenui_ifunctioncall.h"
#include "agenui_platform_function.h"

using agenui::FunctionCallManager;
using agenui::FunctionCallConfig;
using agenui::FunctionCallResolution;
using agenui::FunctionCallStatus;
using agenui::IFunctionCall;
using agenui::FunctionCallPtr;
using agenui::IPlatformFunction;
using agenui::FunctionCallResult;
using agenui::FunctionCallContext;
using nlohmann::json;

// ============================================================================
// Mock IFunctionCall for testing
// ============================================================================

class MockFunctionCall : public IFunctionCall {
public:
    MockFunctionCall(const std::string& name, const std::string& returnType = "boolean")
        : _name(name), _returnType(returnType) {}

    FunctionCallResolution execute(const nlohmann::json& args) override {
        _lastArgs = args;
        return FunctionCallResolution::createSuccess(_returnValue);
    }

    FunctionCallConfig getConfig() const override {
        FunctionCallConfig config;
        config.setName(_name);
        config.setReturnType(_returnType);
        return config;
    }

    void setReturnValue(const nlohmann::json& val) { _returnValue = val; }
    const nlohmann::json& getLastArgs() const { return _lastArgs; }

private:
    std::string _name;
    std::string _returnType;
    nlohmann::json _returnValue = true;
    nlohmann::json _lastArgs;
};

// ============================================================================
// Mock IPlatformFunction for testing
// ============================================================================

class MockPlatformFunction : public IPlatformFunction {
public:
    FunctionCallResult callSync(const FunctionCallContext& context,
                                const std::string& params) override {
        _lastParams = params;
        return _result;
    }

    void setResult(FunctionCallResult r) { _result = r; }
    const std::string& getLastParams() const { return _lastParams; }

private:
    FunctionCallResult _result;
    std::string _lastParams;
};

// ============================================================================
// Register / Unregister
// ============================================================================

TEST(FunctionCallManager, RegisterCppFunctionCall_Success) {
    FunctionCallManager mgr;
    auto fc = std::make_shared<MockFunctionCall>("test_fn");
    EXPECT_TRUE(mgr.registerFunctionCall(fc));
}

TEST(FunctionCallManager, RegisterCppFunctionCall_Null_Fails) {
    FunctionCallManager mgr;
    EXPECT_FALSE(mgr.registerFunctionCall(FunctionCallPtr(nullptr)));
}

TEST(FunctionCallManager, RegisterPlatformFunctionCall_Success) {
    FunctionCallManager mgr;
    MockPlatformFunction fn;
    FunctionCallConfig config;
    config.setName("platform_fn");
    config.setReturnType("string");
    EXPECT_TRUE(mgr.registerFunctionCall(config, &fn));
}

TEST(FunctionCallManager, RegisterPlatformFunctionCall_InvalidConfig_Fails) {
    FunctionCallManager mgr;
    MockPlatformFunction fn;
    FunctionCallConfig config;  // name is empty => invalid
    EXPECT_FALSE(mgr.registerFunctionCall(config, &fn));
}

TEST(FunctionCallManager, RegisterPlatformFunctionCall_NullFunction_Fails) {
    FunctionCallManager mgr;
    FunctionCallConfig config;
    config.setName("fn");
    EXPECT_FALSE(mgr.registerFunctionCall(config, nullptr));
}

TEST(FunctionCallManager, Unregister_Existing_ReturnsTrue) {
    FunctionCallManager mgr;
    MockPlatformFunction fn;
    FunctionCallConfig config;
    config.setName("removable");
    mgr.registerFunctionCall(config, &fn);
    EXPECT_TRUE(mgr.unregisterFunctionCall("removable"));
}

TEST(FunctionCallManager, Unregister_NonExisting_ReturnsFalse) {
    FunctionCallManager mgr;
    EXPECT_FALSE(mgr.unregisterFunctionCall("does_not_exist"));
}

// ============================================================================
// getAllFunctionCalls / exportCatalog
// ============================================================================

TEST(FunctionCallManager, GetAllFunctionCalls_Empty) {
    FunctionCallManager mgr;
    auto all = mgr.getAllFunctionCalls();
    EXPECT_TRUE(all.empty());
}

TEST(FunctionCallManager, GetAllFunctionCalls_MixedTypes) {
    FunctionCallManager mgr;

    // Register a C++ functionCall
    auto fc = std::make_shared<MockFunctionCall>("cpp_fn");
    mgr.registerFunctionCall(fc);

    // Register a platform functionCall
    MockPlatformFunction fn;
    FunctionCallConfig config;
    config.setName("plat_fn");
    config.setReturnType("void");
    mgr.registerFunctionCall(config, &fn);

    auto all = mgr.getAllFunctionCalls();
    EXPECT_EQ(all.size(), 2u);

    // Check both names are present
    bool hasCpp = false, hasPlat = false;
    for (const auto& c : all) {
        if (c.getName() == "cpp_fn") hasCpp = true;
        if (c.getName() == "plat_fn") hasPlat = true;
    }
    EXPECT_TRUE(hasCpp);
    EXPECT_TRUE(hasPlat);
}

TEST(FunctionCallManager, ExportCatalog_ContainsFunctionsArray) {
    FunctionCallManager mgr;
    auto fc = std::make_shared<MockFunctionCall>("cat_fn", "object");
    mgr.registerFunctionCall(fc);

    auto catalog = mgr.exportCatalog();
    ASSERT_TRUE(catalog.contains("functions"));
    ASSERT_TRUE(catalog["functions"].is_array());
    EXPECT_EQ(catalog["functions"].size(), 1u);
    EXPECT_EQ(catalog["functions"][0]["name"], "cat_fn");
}

// ============================================================================
// executeFunctionCallSync
// ============================================================================

TEST(FunctionCallManager, Execute_CppFunctionCall_Success) {
    FunctionCallManager mgr;
    auto fc = std::make_shared<MockFunctionCall>("exec_fn");
    fc->setReturnValue(42);
    mgr.registerFunctionCall(fc);

    FunctionCallContext ctx{};
    auto result = mgr.executeFunctionCallSync("exec_fn", ctx, json({{"x", 1}}));
    EXPECT_EQ(result.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(result.getValue(), 42);
}

TEST(FunctionCallManager, Execute_NotFound_ReturnsError) {
    FunctionCallManager mgr;
    FunctionCallContext ctx{};
    auto result = mgr.executeFunctionCallSync("ghost", ctx, json::object());
    EXPECT_EQ(result.getStatus(), FunctionCallStatus::Error);
    EXPECT_NE(result.getError().find("not found"), std::string::npos);
}

TEST(FunctionCallManager, Execute_PlatformFunctionCall_Success) {
    FunctionCallManager mgr;
    MockPlatformFunction fn;
    FunctionCallResult platformResult;
    platformResult.status = FunctionCallStatus::Success;
    platformResult.data = R"({"ok": true})";
    fn.setResult(platformResult);

    FunctionCallConfig config;
    config.setName("plat_exec");
    config.setReturnType("object");
    mgr.registerFunctionCall(config, &fn);

    FunctionCallContext ctx{};
    auto result = mgr.executeFunctionCallSync("plat_exec", ctx, json({{"a", 1}}));
    EXPECT_EQ(result.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(result.getValue()["ok"], true);
}

TEST(FunctionCallManager, Execute_CppTakesPriorityOverPlatform) {
    FunctionCallManager mgr;

    // Register both with same name
    auto fc = std::make_shared<MockFunctionCall>("dual");
    fc->setReturnValue("from_cpp");
    mgr.registerFunctionCall(fc);

    MockPlatformFunction fn;
    FunctionCallResult pr;
    pr.status = FunctionCallStatus::Success;
    pr.data = R"("from_platform")";
    fn.setResult(pr);
    FunctionCallConfig config;
    config.setName("dual");
    mgr.registerFunctionCall(config, &fn);

    FunctionCallContext ctx{};
    auto result = mgr.executeFunctionCallSync("dual", ctx, json::object());
    EXPECT_EQ(result.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(result.getValue(), "from_cpp");  // C++ takes priority
}
