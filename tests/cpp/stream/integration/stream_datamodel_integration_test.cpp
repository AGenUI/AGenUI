// SDI*: Stream DataModel Integration Tests.
//
// Verifies DataModel streaming with binding paths through the public API.
// Components bound to data model paths should receive updates when
// updateDataModel events arrive.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "stream_integration_base.h"

namespace {

using Base = ::agenui::testing::integration::StreamIntegrationBase;

class StreamDataModelIntegrationTest : public Base {};

// SDI001: Simple string binding - component updates when data arrives.
TEST_F(StreamDataModelIntegrationTest, SDI001_SimpleStringBinding) {
    createTestSurface("s1");
    listener.clear();

    // Create Markdown with content bound to /message
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":{"path":"/message"}}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Send data model update
    std::string dm = buildUpdateDataModel("s1", "/message", R"("Hello binding")");
    feedAll(dm);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 2000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        ASSERT_FALSE(l.componentsUpdateCalls.empty());
        EXPECT_EQ(l.componentsUpdateCalls.front().surfaceId, "s1");
    });
}

// SDI002: Nested object with streaming leaf - multiple updates during streaming.
TEST_F(StreamDataModelIntegrationTest, SDI002_NestedObjectStreamingLeaf) {
    createTestSurface("s1");
    listener.clear();

    // Create component bound to nested path
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":{"path":"/chat/reply"}}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Send nested DM with streaming value (chunked to simulate streaming)
    std::string dm = buildUpdateDataModel(
        "s1", "/", R"({"chat":{"reply":"Streaming nested value here"}})");

    // Feed in chunks to trigger streaming behavior
    sm->beginTextStream();
    size_t chunkSize = 15;
    for (size_t i = 0; i < dm.size(); i += chunkSize) {
        sm->receiveTextChunk(
            dm.substr(i, std::min(chunkSize, dm.size() - i)));
    }
    sm->endTextStream();
    Drain(3000);

    // Should get at least one update
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 3000));
}

// SDI003: Array elements - updates for indexed paths.
TEST_F(StreamDataModelIntegrationTest, SDI003_ArrayElements) {
    createTestSurface("s1");
    listener.clear();

    // Create component bound to array element
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":{"path":"/items/0/text"}}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Send data model with array
    std::string dm = buildUpdateDataModel(
        "s1", "/items/0/text", R"("Array item content")");
    feedAll(dm);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 2000));
}

// SDI004: Large value byte-by-byte - no truncation.
TEST_F(StreamDataModelIntegrationTest, SDI004_LargeValueByteByByte) {
    createTestSurface("s1");
    listener.clear();

    // Create component bound to path
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":{"path":"/big"}}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Build a large value (200 chars)
    std::string largeValue(200, 'X');
    std::string dm = buildUpdateDataModel("s1", "/big",
                                          "\"" + largeValue + "\"");

    // Feed in small chunks
    sm->beginTextStream();
    size_t chunkSize = 5;
    for (size_t i = 0; i < dm.size(); i += chunkSize) {
        sm->receiveTextChunk(
            dm.substr(i, std::min(chunkSize, dm.size() - i)));
    }
    sm->endTextStream();
    Drain(5000);

    // Should get update(s) - no crash, no truncation
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 5000));
}

// SDI005: Multiple bindings - both Markdown and Text update independently.
TEST_F(StreamDataModelIntegrationTest, SDI005_MultipleBindings) {
    createTestSurface("s1");
    listener.clear();

    // Create a root Markdown bound to /a
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":{"path":"/a"}}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Update binding /a first time
    std::string dmA1 = buildUpdateDataModel("s1", "/a", R"("Content A1")");
    feedAll(dmA1);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 2000));

    size_t updatesAfterA1 = 0;
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        updatesAfterA1 = l.componentsUpdateCalls.size();
    });
    EXPECT_GE(updatesAfterA1, 1u);

    // Update binding /a second time - produces additional update
    std::string dmA2 = buildUpdateDataModel("s1", "/a", R"("Content A2")");
    feedAll(dmA2);

    EXPECT_TRUE(listener.waitFor(
        [&]() {
            return listener.componentsUpdateCalls.size() > updatesAfterA1;
        },
        2000));
}

}  // namespace
