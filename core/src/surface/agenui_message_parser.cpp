#include "agenui_message_parser.h"
#include "agenui_logger_internal.h"
#include "nlohmann/json.hpp"

namespace agenui {

AGenUIExeCode AGenUIMessageParser::parseCreateSurfaceData(const std::string& jsonData, CreateSurfaceMessage& outMessage) {
    try {
        auto json = nlohmann::json::parse(jsonData);
        if (!json.contains("createSurface")) {
            AGENUI_LOG("missing createSurface field");
            return CreateSurface_MissingKeyword;
        }

        auto createSurfaceJson = json["createSurface"];
        if (!createSurfaceJson.contains("surfaceId")) {
            AGENUI_LOG("missing surfaceId");
            return CreateSurface_MissingSurfaceId;
        }
        
        outMessage.surfaceId = createSurfaceJson["surfaceId"].get<std::string>();
        // catalogId is optional
        if (createSurfaceJson.contains("catalogId")) {
            outMessage.catalogId = createSurfaceJson["catalogId"].get<std::string>();
        }
        // sendDataModel is optional
        if (createSurfaceJson.contains("sendDataModel")) {
            outMessage.sendDataModel = createSurfaceJson["sendDataModel"].get<bool>();
        }
        // animated is optional (defaults to true)
        if (createSurfaceJson.contains("animated")) {
            outMessage.animated = createSurfaceJson["animated"].get<bool>();
        }
    } catch (const nlohmann::json::exception& e) {
        return CreateSurface_JsonError;
    }
    
    return Execute_Success;
}

AGenUIExeCode AGenUIMessageParser::parseUpdateComponentsData(const std::string& jsonData, std::string& outSurfaceId, nlohmann::json& outComponentsJson) {
    try {
        auto json = nlohmann::json::parse(jsonData);
        if (!json.contains("updateComponents")) {
            AGENUI_LOG("missing updateComponents field");
            return UpdateComponents_MissingKeyword;
        }

        auto updateComponentsJson = json["updateComponents"];
        if (!updateComponentsJson.contains("surfaceId")) {
            AGENUI_LOG("missing surfaceId");
            return UpdateComponents_MissingSurfaceId;
        }
        outSurfaceId = updateComponentsJson["surfaceId"].get<std::string>();
        if (outSurfaceId.empty()) {
            return UpdateComponents_EmptySurfaceId;
        }
        outComponentsJson = updateComponentsJson;
    } catch (const nlohmann::json::exception& e) {
        return UpdateComponents_JsonError;
    }
    return Execute_Success;
}

AGenUIExeCode AGenUIMessageParser::parseUpdateDataModelData(const std::string& jsonData, std::string& outSurfaceId, nlohmann::json& outDataModelJson) {
    try {
        auto json = nlohmann::json::parse(jsonData);
        if (!json.contains("updateDataModel")) {
            AGENUI_LOG("missing updateDataModel field");
            return UpdateDataModel_MissingKeyword;
        }

        auto updateDataModelJson = json["updateDataModel"];
        if (!updateDataModelJson.contains("surfaceId")) {
            AGENUI_LOG("missing surfaceId");
            return UpdateDataModel_MissingSurfaceId;
        }
        outSurfaceId = updateDataModelJson["surfaceId"].get<std::string>();
        if (outSurfaceId.empty()) {
            return UpdateDataModel_EmptySurfaceId;
        }
        outDataModelJson = updateDataModelJson;

        return Execute_Success;
    } catch (std::exception &error) {
        return UpdateDataModel_JsonError;
    }
}

AGenUIExeCode AGenUIMessageParser::parseAppendDataModelData(const std::string& jsonData, std::string& outSurfaceId, nlohmann::json& outDataModelJson) {
    try {
        auto json = nlohmann::json::parse(jsonData);
        if (!json.contains("appendDataModel")) {
            AGENUI_LOG("missing appendDataModel field");
            return AppendDataModel_MissingKeyword;
        }

        auto appendDataModelJson = json["appendDataModel"];
        if (!appendDataModelJson.contains("surfaceId")) {
            AGENUI_LOG("missing surfaceId");
            return AppendDataModel_MissingSurfaceId;
        }
        outSurfaceId = appendDataModelJson["surfaceId"].get<std::string>();
        if (outSurfaceId.empty()) {
            return AppendDataModel_EmptySurfaceId;
        }
        outDataModelJson = appendDataModelJson;
    } catch (const nlohmann::json::exception& e) {
        return AppendDataModel_JsonError;
    }
    
    return Execute_Success;
}

AGenUIExeCode AGenUIMessageParser::parseDeleteSurfaceData(const std::string& jsonData, DeleteSurfaceMessage& outMessage) {
    try {
        auto json = nlohmann::json::parse(jsonData);
        if (!json.contains("deleteSurface")) {
            AGENUI_LOG("missing deleteSurface field");
            return DeleteSurface_MissingKeyword;
        }

        auto deleteSurfaceJson = json["deleteSurface"];
        if (!deleteSurfaceJson.contains("surfaceId")) {
            AGENUI_LOG("missing surfaceId");
            return DeleteSurface_MissingSurfaceId;
        }
        outMessage.surfaceId = deleteSurfaceJson["surfaceId"].get<std::string>();
        if (outMessage.surfaceId.empty()) {
            AGENUI_LOG("surfaceId is empty");
            return DeleteSurface_EmptySurfaceId;
        }
    } catch (const nlohmann::json::exception& e) {
        return DeleteSurface_JsonError;
    }

    return Execute_Success;
}

}  // namespace agenui
