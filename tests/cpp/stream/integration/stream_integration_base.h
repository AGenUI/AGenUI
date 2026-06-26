// Shared fixture for stream integration tests.
//
// Exercises the public ISurfaceManager API (beginTextStream / receiveTextChunk
// / endTextStream) and verifies behavior through IAGenUIMessageListener
// callbacks — simulating real third-party SDK consumption.

#pragma once

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "support/mock_message_listener.h"
#include "support/scoped_surface_manager.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace agenui {
namespace testing {
namespace integration {

using MockMessageListener = ::agenui::testing::MockMessageListener;

class StreamIntegrationBase : public ::testing::Test {
protected:
    ::agenui::testing::ScopedSurfaceManager sm;
    ::agenui::testing::MockMessageListener listener;

    void SetUp() override {
        ASSERT_TRUE(sm) << "ScopedSurfaceManager creation failed";
        sm->addSurfaceEventListener(&listener);
    }

    void TearDown() override {
        if (sm) sm->removeSurfaceEventListener(&listener);
    }

    // --- Surface lifecycle ---

    void createTestSurface(const std::string& surfaceId) {
        auto json = buildCreateSurface(surfaceId);
        sm->beginTextStream();
        sm->receiveTextChunk(json);
        sm->endTextStream();
        ASSERT_TRUE(listener.waitFor(
            [&]() { return !listener.createSurfaceCalls.empty(); }, 2000))
            << "createTestSurface timed out waiting for onCreateSurface";
        Drain();  // Ensure all post-createSurface initialization completes
    }

    // --- Feed helpers ---

    void feedAll(const std::string& data) {
        sm->beginTextStream();
        sm->receiveTextChunk(data);
        sm->endTextStream();
        Drain();
    }

    void feedChunked(const std::string& data,
                     const std::vector<size_t>& splits) {
        sm->beginTextStream();
        size_t prev = 0;
        for (size_t pos : splits) {
            if (pos > prev && pos <= data.size()) {
                sm->receiveTextChunk(data.substr(prev, pos - prev));
                prev = pos;
            }
        }
        if (prev < data.size()) {
            sm->receiveTextChunk(data.substr(prev));
        }
        sm->endTextStream();
        Drain();
    }

    void feedByteByByte(const std::string& data) {
        sm->beginTextStream();
        for (char c : data) {
            sm->receiveTextChunk(std::string(1, c));
        }
        sm->endTextStream();
        Drain(5000);
    }

    // --- Protocol JSON builders ---

    static std::string buildCreateSurface(const std::string& surfaceId) {
        return R"({"version":"v0.9","createSurface":{"surfaceId":")" +
               surfaceId +
               R"(","catalogId":"https://a2ui.org/specification/v0_9/standard_catalog.json","theme":{},"sendDataModel":false,"animated":true}})";
    }

    static std::string buildUpdateComponents(
        const std::string& surfaceId,
        const std::string& componentsArrayJson) {
        return R"({"version":"v0.9","updateComponents":{"surfaceId":")" +
               surfaceId +
               R"(","components":)" + componentsArrayJson + R"(}})";
    }

    static std::string buildUpdateDataModel(const std::string& surfaceId,
                                            const std::string& path,
                                            const std::string& valueJson) {
        return R"({"version":"v0.9","updateDataModel":{"surfaceId":")" +
               surfaceId + R"(","path":")" + path + R"(","value":)" +
               valueJson + R"(}})";
    }

    // --- Result collection ---

    // Collect full content from listener callbacks by assembling
    // initial value + streaming deltas from component JSON.
    std::string collectComponentContent(const std::string& contentField,
                                        const std::string& appendField) {
        std::string result;

        // From componentsAdd calls
        listener.withLock([&](MockMessageListener& l) {
            for (auto& rec : l.componentsAddCalls) {
                for (auto& msg : rec.messages) {
                    auto j = nlohmann::json::parse(msg.component, nullptr, false);
                    if (j.is_discarded()) continue;
                    if (j.contains(contentField) && j[contentField].is_string()) {
                        result = j[contentField].get<std::string>();
                    }
                }
            }
            // From componentsUpdate calls
            for (auto& rec : l.componentsUpdateCalls) {
                for (auto& msg : rec.messages) {
                    auto j = nlohmann::json::parse(msg.component, nullptr, false);
                    if (j.is_discarded()) continue;
                    if (j.contains(contentField) && j[contentField].is_string()) {
                        result = j[contentField].get<std::string>();
                    } else if (j.contains(appendField) && j[appendField].is_string()) {
                        result += j[appendField].get<std::string>();
                    }
                }
            }
        });
        return result;
    }

    int countComponentsAdd() {
        int count = 0;
        listener.withLock([&](MockMessageListener& l) {
            count = static_cast<int>(l.componentsAddCalls.size());
        });
        return count;
    }

    int countComponentsUpdate() {
        int count = 0;
        listener.withLock([&](MockMessageListener& l) {
            count = static_cast<int>(l.componentsUpdateCalls.size());
        });
        return count;
    }

    bool hasAppendField(const std::string& appendField) {
        bool found = false;
        listener.withLock([&](MockMessageListener& l) {
            for (auto& rec : l.componentsUpdateCalls) {
                for (auto& msg : rec.messages) {
                    auto j = nlohmann::json::parse(msg.component, nullptr, false);
                    if (!j.is_discarded() && j.contains(appendField)) {
                        found = true;
                        return;
                    }
                }
                if (found) return;
            }
        });
        return found;
    }

    void Drain(int ms = 2000) {
        ::agenui::testing::WaitForWorkerIdle(ms);
    }
};

}  // namespace integration
}  // namespace testing
}  // namespace agenui
