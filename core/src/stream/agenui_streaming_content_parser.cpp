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
        AGENUI_PERFORMANCE_LOG("stream_assembling_begin", "");
        AGENUI_LOG("%s", data.c_str());
        _extractor.appendData(data);
        auto results = _extractor.driveParser();
        dispatchParseResultsBatched(results);

        AGENUI_PERFORMANCE_LOG("stream_assembling_end", "");
    }

    void StreamingContentParser::processDataEnding() {
        AGENUI_LOG("processing end");
        resetState();
        
        AGENUI_PERFORMANCE_LOG("stream_end", "");
    }

    void StreamingContentParser::dispatchParseResultsBatched(const std::vector<ProtocolStreamExtractor::ParseResult>& results) {
        // Find component protocols sharing the same surfaceId from streaming parse results,
        // merge them into a single batch for component parsing, layout calculation, etc.
        // This reduces overhead of protocol component parsing, JSON deserialization and layout computation.
        size_t resultCursor = 0;
        const size_t resultCount = results.size();
        while (resultCursor < resultCount) {
            const auto& head = results[resultCursor];
            if (head.type == ProtocolStreamExtractor::ParseResult::Type::NormalEvent) {
                processNormalEvent(head);
                ++resultCursor;
                continue;
            }
            // Collect contiguous ComponentUpdate results with the same surfaceId into one batch.
            // (Logically allows receiving and parsing two updateComponents events with different surfaceIds.)
            size_t batchIndex = resultCursor + 1;
            while (batchIndex < resultCount) {
                const auto& cur = results[batchIndex];
                if (cur.type != ProtocolStreamExtractor::ParseResult::Type::ComponentUpdate) {
                    break;
                }
                if (cur.surfaceId != head.surfaceId) {
                    break;
                }
                ++batchIndex;
            }
            if (batchIndex - resultCursor == 1) {
                // Fast path: keep behavior identical to legacy single-component path.
                const auto& singleContent = results[resultCursor];
                sendSingleComponentUpdate(singleContent.componentJson, singleContent.surfaceId, singleContent.version);
            } else {
                sendBatchedComponentUpdate(results, resultCursor, batchIndex);
            }
            resultCursor = batchIndex;
        }
    }

    void StreamingContentParser::sendBatchedComponentUpdate(
        const std::vector<ProtocolStreamExtractor::ParseResult>& results,
        size_t start, size_t end) {
        if (!_coordinator || start >= end) {
            return;
        }
        const auto& first = results[start];
        std::string updateJson;
        // Pre-reserve a reasonable capacity; component JSONs can be large.
        size_t reserveBytes = 64 + first.surfaceId.size() + first.version.size();
        for (size_t cursor = start; cursor < end; ++cursor) {
            reserveBytes += results[cursor].componentJson.size() + 2;
        }
        updateJson.reserve(reserveBytes);
        updateJson += "{";
        if (!first.version.empty()) {
            updateJson += "\"version\":\"";
            updateJson += first.version;
            updateJson += "\",";
        }
        updateJson += "\"updateComponents\":{\"surfaceId\":\"";
        updateJson += first.surfaceId;
        updateJson += "\",\"components\":[";
        for (size_t k = start; k < end; ++k) {
            if (k > start) updateJson += ",";
            updateJson += results[k].componentJson;
        }
        updateJson += "]}}";
        AGenUIExeCode ret = _coordinator->updateComponents(updateJson);
        if (ret != Execute_Success) {
            AGENUI_LOG("ret:%s, batch:%zu", getExeCodeString(ret).c_str(), end - start);
        }
    }

    void StreamingContentParser::processNormalEvent(const ProtocolStreamExtractor::ParseResult& result) {
        if (!_coordinator) {
            return;
        }

        AGenUIExeCode ret = Execute_Success;
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
        if (ret != Execute_Success) {
            AGENUI_LOG("ret:%s, type:%d, data:%s", getExeCodeString(ret).c_str(), result.eventType, data.c_str());
        }
    }

    void StreamingContentParser::sendSingleComponentUpdate(const std::string& componentJson, const std::string& surfaceId, const std::string& version) {
        if (!_coordinator) {
            return;
        }

        std::string updateJson;
        updateJson.reserve(64 + surfaceId.size() + version.size() + componentJson.size());
        updateJson += "{";
        if (!version.empty()) {
            updateJson += "\"version\":\"";
            updateJson += version;
            updateJson += "\",";
        }
        updateJson += "\"updateComponents\":{\"surfaceId\":\"";
        updateJson += surfaceId;
        updateJson += "\",\"components\":[";
        updateJson += componentJson;
        updateJson += "]}}";
        AGenUIExeCode ret = _coordinator->updateComponents(updateJson);
        if (ret != Execute_Success) {
            AGENUI_LOG("ret:%s, data:%s", getExeCodeString(ret).c_str(), updateJson.c_str());
        }
    }

    void StreamingContentParser::resetState() {
        _extractor.reset();
    }

} // namespace agenui
