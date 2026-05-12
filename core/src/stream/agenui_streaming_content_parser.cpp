#include "agenui_streaming_content_parser.h"
#include "agenui_logger_internal.h"
#include <cstring>
#include "nlohmann/json.hpp"
#include "module/agenui_thread_manager.h"
#include "surface/agenui_surface_coordinator.h"

namespace agenui {

    StreamingContentParser::StreamingContentParser(SurfaceCoordinator* coordinator)
        : _coordinator(coordinator) {
        _markdownPlugin = std::unique_ptr<MarkdownStreamPlugin>(new MarkdownStreamPlugin());
        _textPlugin = std::unique_ptr<TextStreamPlugin>(new TextStreamPlugin());
        _compositePlugin = std::unique_ptr<CompositeStreamPlugin>(new CompositeStreamPlugin());
        _compositePlugin->addPlugin(_markdownPlugin.get());
        _compositePlugin->addPlugin(_textPlugin.get());
        _extractor.setPlugin(_compositePlugin.get());
    }

    StreamingContentParser::~StreamingContentParser() {
        stop();
    }

    bool StreamingContentParser::start() {
        return true;
    }

    void StreamingContentParser::stop() {
    }

    void StreamingContentParser::setQueryContent(const std::string &content) {
        _queryContent = content;
    }

    void StreamingContentParser::processDataBeginning() {
        AGENUI_LOG("processing begin");
        resetState();

        AGENUI_PERFORMANCE_LOG("stream_begin", "");
    }

    void StreamingContentParser::processDataAssembling(const std::string& data) {
        AGENUI_PERFORMANCE_LOG("stream_assembling_begin", "%s", data.c_str());
        AGENUI_LOG("%s", data.c_str());
        _extractor.appendData(data);
        auto results = _extractor.driveParser();
        dispatchParseResults(results);
        
        AGENUI_PERFORMANCE_LOG("stream_assembling_end", "%s", data.c_str());
    }

    void StreamingContentParser::processDataEnding() {
        AGENUI_LOG("processing end");
        resetState();
        
        AGENUI_PERFORMANCE_LOG("stream_end", "");
    }

    void StreamingContentParser::dispatchParseResults(const std::vector<ProtocolStreamExtractor::ParseResult>& results) {
        for (const auto& result : results) {
            if (result.type == ProtocolStreamExtractor::ParseResult::Type::NormalEvent) {
                processNormalEvent(result);
            } else {
                sendSingleComponentUpdate(result.componentJson, result.surfaceId, result.version);
            }
        }
    }

    void StreamingContentParser::processNormalEvent(const ProtocolStreamExtractor::ParseResult& result) {
        if (!_coordinator) {
            return;
        }

        AGenUIExeCode ret = ExeCode_Parse_success;
        const std::string& data = result.eventJson;
        if (result.eventType == ProtocolStreamExtractor::EventType::CreateSurface) {
            ret = _coordinator->createSurface(data);
        } else if (result.eventType == ProtocolStreamExtractor::EventType::UpdateDataModel) {
            ret = _coordinator->updateDataModel(data);
        } else if (result.eventType == ProtocolStreamExtractor::EventType::AppendDataModel) {
            ret = _coordinator->appendDataModel(data);
        } else if (result.eventType == ProtocolStreamExtractor::EventType::DeleteSurface) {
            ret = _coordinator->deleteSurface(data);
        }
        if (ret != ExeCode_Parse_success) {
            AGENUI_LOG("ret:%s, type:%d, data:%s", getExeCodeString(ret).c_str(), result.eventType, data.c_str());
        }
    }

    void StreamingContentParser::sendSingleComponentUpdate(const std::string& componentJson, const std::string& surfaceId, const std::string& version) {
        if (!_coordinator) {
            return;
        }

        std::string updateJson = "{";
        if (!version.empty()) {
            updateJson += "\"version\":\"" + version + "\",";
        }
        updateJson += "\"updateComponents\":{";
        updateJson += "\"surfaceId\":\"" + surfaceId + "\",";
        updateJson += "\"components\":[" + componentJson + "]";
        updateJson += "}}";
        AGenUIExeCode ret = _coordinator->updateComponents(updateJson);
        if (ret != ExeCode_Parse_success) {
            AGENUI_LOG("ret:%s, data:%s", getExeCodeString(ret).c_str(), updateJson.c_str());
        }
    }

    void StreamingContentParser::resetState() {
        _extractor.reset();
    }

} // namespace agenui
