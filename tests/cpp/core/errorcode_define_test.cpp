#include <gtest/gtest.h>
#include "agenui_errorcode_define.h"

using namespace agenui;

// =============================================================================
// getExeCodeString Tests — verify every enum maps to a non-"unknown" string
// =============================================================================

class ErrorCodeDefineTest : public ::testing::TestWithParam<std::pair<AGenUIExeCode, std::string>> {};

TEST_P(ErrorCodeDefineTest, CodeReturnsExpectedString) {
    auto [code, expected] = GetParam();
    EXPECT_EQ(getExeCodeString(code), expected);
}

INSTANTIATE_TEST_SUITE_P(AllCodes, ErrorCodeDefineTest, ::testing::Values(
    std::make_pair(Execute_Success, "execute success"),
    // CreateSurface
    std::make_pair(CreateSurface_MissingKeyword, "missing createSurface keyword"),
    std::make_pair(CreateSurface_MissingSurfaceId, "missing surfaceId in createSurface"),
    std::make_pair(CreateSurface_EmptySurfaceId, "surfaceId is empty in createSurface"),
    std::make_pair(CreateSurface_JsonError, "JSON exception in createSurface"),
    std::make_pair(CreateSurface_DuplicateSurfaceId, "surfaceId already exists"),
    // UpdateComponents
    std::make_pair(UpdateComponents_MissingKeyword, "missing updateComponents keyword"),
    std::make_pair(UpdateComponents_MissingSurfaceId, "missing surfaceId in updateComponents"),
    std::make_pair(UpdateComponents_EmptySurfaceId, "surfaceId is empty in updateComponents"),
    std::make_pair(UpdateComponents_SurfaceNotFound, "surface not found in updateComponents"),
    std::make_pair(UpdateComponents_JsonError, "JSON exception in updateComponents"),
    std::make_pair(UpdateComponents_MissingComponentsField, "missing components field in updateComponents"),
    std::make_pair(UpdateComponents_ComponentsNotArray, "components is not an array in updateComponents"),
    std::make_pair(UpdateComponents_MissingComponentEntity, "missing component entity in updateComponents"),
    std::make_pair(UpdateComponents_TemplateExpansionFailed, "template expansion failed in updateComponents"),
    // UpdateDataModel
    std::make_pair(UpdateDataModel_MissingKeyword, "missing updateDataModel keyword"),
    std::make_pair(UpdateDataModel_MissingSurfaceId, "missing surfaceId in updateDataModel"),
    std::make_pair(UpdateDataModel_EmptySurfaceId, "surfaceId is empty in updateDataModel"),
    std::make_pair(UpdateDataModel_SurfaceNotFound, "surface not found in updateDataModel"),
    std::make_pair(UpdateDataModel_JsonError, "JSON exception in updateDataModel"),
    // AppendDataModel
    std::make_pair(AppendDataModel_MissingKeyword, "missing appendDataModel keyword"),
    std::make_pair(AppendDataModel_MissingSurfaceId, "missing surfaceId in appendDataModel"),
    std::make_pair(AppendDataModel_EmptySurfaceId, "surfaceId is empty in appendDataModel"),
    std::make_pair(AppendDataModel_SurfaceNotFound, "surface not found in appendDataModel"),
    std::make_pair(AppendDataModel_JsonError, "JSON exception in appendDataModel"),
    // DeleteSurface
    std::make_pair(DeleteSurface_MissingKeyword, "missing deleteSurface keyword"),
    std::make_pair(DeleteSurface_MissingSurfaceId, "missing surfaceId in deleteSurface"),
    std::make_pair(DeleteSurface_EmptySurfaceId, "surfaceId is empty in deleteSurface"),
    std::make_pair(DeleteSurface_JsonError, "JSON exception in deleteSurface")
));

// Edge case: invalid enum value returns "unknown"
TEST(ErrorCodeDefineEdge, InvalidCode_ReturnsUnknown) {
    auto result = getExeCodeString(static_cast<AGenUIExeCode>(9999));
    EXPECT_EQ(result, "unknown");
}

// Verify success code is at value 0
TEST(ErrorCodeDefineEdge, SuccessCodeIsZero) {
    EXPECT_EQ(static_cast<int>(Execute_Success), 0);
}
