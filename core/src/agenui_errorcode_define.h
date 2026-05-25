#pragma once

#include <string>

namespace agenui {

enum AGenUIExeCode {
    Execute_Success = 0,

    // CreateSurface
    CreateSurface_MissingKeyword,
    CreateSurface_MissingSurfaceId,
    CreateSurface_EmptySurfaceId,
    CreateSurface_JsonError,
    CreateSurface_DuplicateSurfaceId,

    // UpdateComponents
    UpdateComponents_MissingKeyword,
    UpdateComponents_MissingSurfaceId,
    UpdateComponents_EmptySurfaceId,
    UpdateComponents_SurfaceNotFound,
    UpdateComponents_JsonError,
    UpdateComponents_MissingComponentsField,
    UpdateComponents_ComponentsNotArray,
    UpdateComponents_MissingComponentEntity,
    UpdateComponents_TemplateExpansionFailed,

    // UpdateDataModel
    UpdateDataModel_MissingKeyword,
    UpdateDataModel_MissingSurfaceId,
    UpdateDataModel_EmptySurfaceId,
    UpdateDataModel_SurfaceNotFound,
    UpdateDataModel_JsonError,

    // AppendDataModel
    AppendDataModel_MissingKeyword,
    AppendDataModel_MissingSurfaceId,
    AppendDataModel_EmptySurfaceId,
    AppendDataModel_SurfaceNotFound,
    AppendDataModel_JsonError,

    // DeleteSurface
    DeleteSurface_MissingKeyword,
    DeleteSurface_MissingSurfaceId,
    DeleteSurface_EmptySurfaceId,
    DeleteSurface_JsonError,
};

/**
 * @brief Returns the string description for an error code.
 * @param code Error code enum value
 * @return Corresponding string, or "unknown" if not found
 */
std::string getExeCodeString(AGenUIExeCode code);

} // namespace agenui
