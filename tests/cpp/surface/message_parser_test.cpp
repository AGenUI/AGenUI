#include <gtest/gtest.h>
#include "surface/agenui_message_parser.h"
#include "agenui_errorcode_define.h"

using agenui::AGenUIMessageParser;
using agenui::AGenUIExeCode;
using agenui::CreateSurfaceMessage;
using agenui::DeleteSurfaceMessage;
using nlohmann::json;

// ============================================================================
// parseCreateSurfaceData
// ============================================================================

TEST(MessageParserCreateSurface, Success_AllFields) {
    json j = {{"createSurface", {
        {"surfaceId", "s1"},
        {"catalogId", "cat1"},
        {"sendDataModel", true},
        {"animated", false}
    }}};
    CreateSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseCreateSurfaceData(j.dump(), msg), agenui::Execute_Success);
    EXPECT_EQ(msg.surfaceId, "s1");
    EXPECT_EQ(msg.catalogId, "cat1");
    EXPECT_TRUE(msg.sendDataModel);
    EXPECT_FALSE(msg.animated);
}

TEST(MessageParserCreateSurface, Success_OnlyRequired) {
    json j = {{"createSurface", {{"surfaceId", "s2"}}}};
    CreateSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseCreateSurfaceData(j.dump(), msg), agenui::Execute_Success);
    EXPECT_EQ(msg.surfaceId, "s2");
    EXPECT_TRUE(msg.catalogId.empty());
}

TEST(MessageParserCreateSurface, MissingKeyword) {
    json j = {{"wrongKey", {{"surfaceId", "s1"}}}};
    CreateSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseCreateSurfaceData(j.dump(), msg), agenui::CreateSurface_MissingKeyword);
}

TEST(MessageParserCreateSurface, MissingSurfaceId) {
    json j = {{"createSurface", {{"catalogId", "cat"}}}};
    CreateSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseCreateSurfaceData(j.dump(), msg), agenui::CreateSurface_MissingSurfaceId);
}

TEST(MessageParserCreateSurface, InvalidJson) {
    CreateSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseCreateSurfaceData("not json {{{", msg), agenui::CreateSurface_JsonError);
}

TEST(MessageParserCreateSurface, EmptyString) {
    CreateSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseCreateSurfaceData("", msg), agenui::CreateSurface_JsonError);
}

// ============================================================================
// parseUpdateComponentsData
// ============================================================================

TEST(MessageParserUpdateComponents, Success) {
    json j = {{"updateComponents", {
        {"surfaceId", "s1"},
        {"components", json::array({{"type", "text"}})}
    }}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateComponentsData(j.dump(), surfaceId, outJson), agenui::Execute_Success);
    EXPECT_EQ(surfaceId, "s1");
    EXPECT_TRUE(outJson.contains("surfaceId"));
}

TEST(MessageParserUpdateComponents, MissingKeyword) {
    json j = {{"wrong", {{"surfaceId", "s1"}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateComponentsData(j.dump(), surfaceId, outJson), agenui::UpdateComponents_MissingKeyword);
}

TEST(MessageParserUpdateComponents, MissingSurfaceId) {
    json j = {{"updateComponents", {{"data", "x"}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateComponentsData(j.dump(), surfaceId, outJson), agenui::UpdateComponents_MissingSurfaceId);
}

TEST(MessageParserUpdateComponents, EmptySurfaceId) {
    json j = {{"updateComponents", {{"surfaceId", ""}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateComponentsData(j.dump(), surfaceId, outJson), agenui::UpdateComponents_EmptySurfaceId);
}

TEST(MessageParserUpdateComponents, InvalidJson) {
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateComponentsData("{broken", surfaceId, outJson), agenui::UpdateComponents_JsonError);
}

// ============================================================================
// parseUpdateDataModelData
// ============================================================================

TEST(MessageParserUpdateDataModel, Success) {
    json j = {{"updateDataModel", {
        {"surfaceId", "s1"},
        {"model", {{"key", "val"}}}
    }}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateDataModelData(j.dump(), surfaceId, outJson), agenui::Execute_Success);
    EXPECT_EQ(surfaceId, "s1");
}

TEST(MessageParserUpdateDataModel, MissingKeyword) {
    json j = {{"noMatch", {{"surfaceId", "s1"}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateDataModelData(j.dump(), surfaceId, outJson), agenui::UpdateDataModel_MissingKeyword);
}

TEST(MessageParserUpdateDataModel, MissingSurfaceId) {
    json j = {{"updateDataModel", {{"data", 1}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateDataModelData(j.dump(), surfaceId, outJson), agenui::UpdateDataModel_MissingSurfaceId);
}

TEST(MessageParserUpdateDataModel, EmptySurfaceId) {
    json j = {{"updateDataModel", {{"surfaceId", ""}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateDataModelData(j.dump(), surfaceId, outJson), agenui::UpdateDataModel_EmptySurfaceId);
}

TEST(MessageParserUpdateDataModel, InvalidJson) {
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseUpdateDataModelData("]]", surfaceId, outJson), agenui::UpdateDataModel_JsonError);
}

// ============================================================================
// parseAppendDataModelData
// ============================================================================

TEST(MessageParserAppendDataModel, Success) {
    json j = {{"appendDataModel", {
        {"surfaceId", "s1"},
        {"delta", {{"add", true}}}
    }}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseAppendDataModelData(j.dump(), surfaceId, outJson), agenui::Execute_Success);
    EXPECT_EQ(surfaceId, "s1");
    EXPECT_TRUE(outJson.contains("delta"));
}

TEST(MessageParserAppendDataModel, MissingKeyword) {
    json j = {{"nope", {{"surfaceId", "s1"}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseAppendDataModelData(j.dump(), surfaceId, outJson), agenui::AppendDataModel_MissingKeyword);
}

TEST(MessageParserAppendDataModel, MissingSurfaceId) {
    json j = {{"appendDataModel", {{"x", 1}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseAppendDataModelData(j.dump(), surfaceId, outJson), agenui::AppendDataModel_MissingSurfaceId);
}

TEST(MessageParserAppendDataModel, EmptySurfaceId) {
    json j = {{"appendDataModel", {{"surfaceId", ""}}}};
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseAppendDataModelData(j.dump(), surfaceId, outJson), agenui::AppendDataModel_EmptySurfaceId);
}

TEST(MessageParserAppendDataModel, InvalidJson) {
    std::string surfaceId;
    nlohmann::json outJson;
    EXPECT_EQ(AGenUIMessageParser::parseAppendDataModelData("xxx", surfaceId, outJson), agenui::AppendDataModel_JsonError);
}

// ============================================================================
// parseDeleteSurfaceData
// ============================================================================

TEST(MessageParserDeleteSurface, Success) {
    json j = {{"deleteSurface", {{"surfaceId", "s1"}}}};
    DeleteSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseDeleteSurfaceData(j.dump(), msg), agenui::Execute_Success);
    EXPECT_EQ(msg.surfaceId, "s1");
}

TEST(MessageParserDeleteSurface, MissingKeyword) {
    json j = {{"other", {{"surfaceId", "s1"}}}};
    DeleteSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseDeleteSurfaceData(j.dump(), msg), agenui::DeleteSurface_MissingKeyword);
}

TEST(MessageParserDeleteSurface, MissingSurfaceId) {
    json j = {{"deleteSurface", {{"x", 1}}}};
    DeleteSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseDeleteSurfaceData(j.dump(), msg), agenui::DeleteSurface_MissingSurfaceId);
}

TEST(MessageParserDeleteSurface, EmptySurfaceId) {
    json j = {{"deleteSurface", {{"surfaceId", ""}}}};
    DeleteSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseDeleteSurfaceData(j.dump(), msg), agenui::DeleteSurface_EmptySurfaceId);
}

TEST(MessageParserDeleteSurface, InvalidJson) {
    DeleteSurfaceMessage msg;
    EXPECT_EQ(AGenUIMessageParser::parseDeleteSurfaceData("{{invalid", msg), agenui::DeleteSurface_JsonError);
}
