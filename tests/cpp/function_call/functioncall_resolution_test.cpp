#include <gtest/gtest.h>
#include "function_call/agenui_functioncall_resolution.h"

using agenui::FunctionCallResolution;
using agenui::FunctionCallResult;
using agenui::FunctionCallStatus;
using nlohmann::json;

// ============================================================================
// Factory methods: createSuccess
// ============================================================================

TEST(FunctionCallResolutionCreate, Success_BoolValue) {
    auto r = FunctionCallResolution::createSuccess(true);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(r.getValue(), true);
    EXPECT_TRUE(r.getError().empty());
}

TEST(FunctionCallResolutionCreate, Success_StringValue) {
    auto r = FunctionCallResolution::createSuccess("hello");
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(r.getValue(), "hello");
}

TEST(FunctionCallResolutionCreate, Success_NullValue) {
    auto r = FunctionCallResolution::createSuccess(nullptr);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Success);
    EXPECT_TRUE(r.getValue().is_null());
}

TEST(FunctionCallResolutionCreate, Success_ObjectValue) {
    json obj = {{"key", "val"}, {"num", 42}};
    auto r = FunctionCallResolution::createSuccess(obj);
    EXPECT_EQ(r.getValue()["key"], "val");
    EXPECT_EQ(r.getValue()["num"], 42);
}

// ============================================================================
// Factory methods: createError
// ============================================================================

TEST(FunctionCallResolutionCreate, Error_HasMessage) {
    auto r = FunctionCallResolution::createError("something broke");
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Error);
    EXPECT_EQ(r.getError(), "something broke");
    EXPECT_TRUE(r.getValue().is_null());
}

TEST(FunctionCallResolutionCreate, Error_EmptyMessage) {
    auto r = FunctionCallResolution::createError("");
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Error);
    EXPECT_TRUE(r.getError().empty());
}

// ============================================================================
// Factory methods: createPending / createCompleted
// ============================================================================

TEST(FunctionCallResolutionCreate, Pending_HasRequestId) {
    auto r = FunctionCallResolution::createPending("req-123");
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Pending);
    EXPECT_EQ(r.getRequestId(), "req-123");
    EXPECT_TRUE(r.getValue().is_null());
}

TEST(FunctionCallResolutionCreate, Completed_HasIdAndValue) {
    auto r = FunctionCallResolution::createCompleted("req-456", 99);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Completed);
    EXPECT_EQ(r.getRequestId(), "req-456");
    EXPECT_EQ(r.getValue(), 99);
}

// ============================================================================
// fromPlatformResult
// ============================================================================

TEST(FunctionCallResolutionFromPlatform, SuccessWithJsonData) {
    FunctionCallResult pr;
    pr.status = FunctionCallStatus::Success;
    pr.data = R"({"result": true})";

    auto r = FunctionCallResolution::fromPlatformResult(pr);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(r.getValue()["result"], true);
}

TEST(FunctionCallResolutionFromPlatform, SuccessWithEmptyData_ReturnsNull) {
    FunctionCallResult pr;
    pr.status = FunctionCallStatus::Success;
    pr.data = "";

    auto r = FunctionCallResolution::fromPlatformResult(pr);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Success);
    EXPECT_TRUE(r.getValue().is_null());
}

TEST(FunctionCallResolutionFromPlatform, SuccessWithInvalidJson_ReturnsError) {
    FunctionCallResult pr;
    pr.status = FunctionCallStatus::Success;
    pr.data = "not json {{{";

    auto r = FunctionCallResolution::fromPlatformResult(pr);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Error);
    EXPECT_FALSE(r.getError().empty());
    EXPECT_NE(r.getError().find("Invalid JSON"), std::string::npos);
}

TEST(FunctionCallResolutionFromPlatform, ErrorStatus_PropagatesMessage) {
    FunctionCallResult pr;
    pr.status = FunctionCallStatus::Error;
    pr.error = "platform error detail";

    auto r = FunctionCallResolution::fromPlatformResult(pr);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Error);
    EXPECT_EQ(r.getError(), "platform error detail");
}

TEST(FunctionCallResolutionFromPlatform, PendingStatus_TreatedAsError) {
    FunctionCallResult pr;
    pr.status = FunctionCallStatus::Pending;

    auto r = FunctionCallResolution::fromPlatformResult(pr);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Error);
    EXPECT_NE(r.getError().find("Pending"), std::string::npos);
}

TEST(FunctionCallResolutionFromPlatform, SuccessWithScalarJson) {
    FunctionCallResult pr;
    pr.status = FunctionCallStatus::Success;
    pr.data = "42";

    auto r = FunctionCallResolution::fromPlatformResult(pr);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(r.getValue(), 42);
}

TEST(FunctionCallResolutionFromPlatform, SuccessWithStringJson) {
    FunctionCallResult pr;
    pr.status = FunctionCallStatus::Success;
    pr.data = R"("hello world")";

    auto r = FunctionCallResolution::fromPlatformResult(pr);
    EXPECT_EQ(r.getStatus(), FunctionCallStatus::Success);
    EXPECT_EQ(r.getValue(), "hello world");
}

// ============================================================================
// toJson
// ============================================================================

TEST(FunctionCallResolutionToJson, Success_HasStatusAndValue) {
    auto r = FunctionCallResolution::createSuccess(42);
    auto j = r.toJson();
    EXPECT_EQ(j["status"], "success");
    EXPECT_EQ(j["value"], 42);
    EXPECT_FALSE(j.contains("error"));
    EXPECT_FALSE(j.contains("requestId"));
}

TEST(FunctionCallResolutionToJson, Error_HasStatusAndError) {
    auto r = FunctionCallResolution::createError("oops");
    auto j = r.toJson();
    EXPECT_EQ(j["status"], "error");
    EXPECT_EQ(j["error"], "oops");
    EXPECT_FALSE(j.contains("value"));
}

TEST(FunctionCallResolutionToJson, Pending_HasStatusAndRequestId) {
    auto r = FunctionCallResolution::createPending("abc");
    auto j = r.toJson();
    EXPECT_EQ(j["status"], "pending");
    EXPECT_EQ(j["requestId"], "abc");
    EXPECT_FALSE(j.contains("value"));
}

TEST(FunctionCallResolutionToJson, Completed_HasAllFields) {
    auto r = FunctionCallResolution::createCompleted("req-1", json({{"done", true}}));
    auto j = r.toJson();
    EXPECT_EQ(j["status"], "completed");
    EXPECT_EQ(j["requestId"], "req-1");
    EXPECT_EQ(j["value"]["done"], true);
}

// ============================================================================
// isAsync / isCompleted
// ============================================================================

TEST(FunctionCallResolutionState, Success_IsNotAsync_IsCompleted) {
    auto r = FunctionCallResolution::createSuccess(true);
    EXPECT_FALSE(r.isAsync());
    EXPECT_TRUE(r.isCompleted());
}

TEST(FunctionCallResolutionState, Error_IsNotAsync_IsCompleted) {
    auto r = FunctionCallResolution::createError("e");
    EXPECT_FALSE(r.isAsync());
    EXPECT_TRUE(r.isCompleted());
}

TEST(FunctionCallResolutionState, Pending_IsAsync_IsNotCompleted) {
    auto r = FunctionCallResolution::createPending("id");
    EXPECT_TRUE(r.isAsync());
    EXPECT_FALSE(r.isCompleted());
}

TEST(FunctionCallResolutionState, Completed_IsAsync_IsCompleted) {
    auto r = FunctionCallResolution::createCompleted("id", nullptr);
    EXPECT_TRUE(r.isAsync());
    EXPECT_TRUE(r.isCompleted());
}
