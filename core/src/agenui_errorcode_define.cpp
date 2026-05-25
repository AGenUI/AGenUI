#include "agenui_errorcode_define.h"

namespace agenui {

std::string getExeCodeString(AGenUIExeCode code) {
    switch (code) {
        case Execute_Success:
            return "execute success";

        // CreateSurface
        case CreateSurface_MissingKeyword:
            return "missing createSurface keyword";
        case CreateSurface_MissingSurfaceId:
            return "missing surfaceId in createSurface";
        case CreateSurface_EmptySurfaceId:
            return "surfaceId is empty in createSurface";
        case CreateSurface_JsonError:
            return "JSON exception in createSurface";
        case CreateSurface_DuplicateSurfaceId:
            return "surfaceId already exists";

        // UpdateComponents
        case UpdateComponents_MissingKeyword:
            return "missing updateComponents keyword";
        case UpdateComponents_MissingSurfaceId:
            return "missing surfaceId in updateComponents";
        case UpdateComponents_EmptySurfaceId:
            return "surfaceId is empty in updateComponents";
        case UpdateComponents_SurfaceNotFound:
            return "surface not found in updateComponents";
        case UpdateComponents_JsonError:
            return "JSON exception in updateComponents";
        case UpdateComponents_MissingComponentsField:
            return "missing components field in updateComponents";
        case UpdateComponents_ComponentsNotArray:
            return "components is not an array in updateComponents";
        case UpdateComponents_MissingComponentEntity:
            return "missing component entity in updateComponents";
        case UpdateComponents_TemplateExpansionFailed:
            return "template expansion failed in updateComponents";

        // UpdateDataModel
        case UpdateDataModel_MissingKeyword:
            return "missing updateDataModel keyword";
        case UpdateDataModel_MissingSurfaceId:
            return "missing surfaceId in updateDataModel";
        case UpdateDataModel_EmptySurfaceId:
            return "surfaceId is empty in updateDataModel";
        case UpdateDataModel_SurfaceNotFound:
            return "surface not found in updateDataModel";
        case UpdateDataModel_JsonError:
            return "JSON exception in updateDataModel";

        // AppendDataModel
        case AppendDataModel_MissingKeyword:
            return "missing appendDataModel keyword";
        case AppendDataModel_MissingSurfaceId:
            return "missing surfaceId in appendDataModel";
        case AppendDataModel_EmptySurfaceId:
            return "surfaceId is empty in appendDataModel";
        case AppendDataModel_SurfaceNotFound:
            return "surface not found in appendDataModel";
        case AppendDataModel_JsonError:
            return "JSON exception in appendDataModel";

        // DeleteSurface
        case DeleteSurface_MissingKeyword:
            return "missing deleteSurface keyword";
        case DeleteSurface_MissingSurfaceId:
            return "missing surfaceId in deleteSurface";
        case DeleteSurface_EmptySurfaceId:
            return "surfaceId is empty in deleteSurface";
        case DeleteSurface_JsonError:
            return "JSON exception in deleteSurface";

        default:
            return "unknown";
    }
}

} // namespace agenui
