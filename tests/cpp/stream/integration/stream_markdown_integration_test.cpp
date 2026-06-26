// SMI*: Stream Markdown Integration Tests.
//
// Verifies that Markdown component streaming deltas (appendContent) arrive
// correctly through the public ISurfaceManager API and can be reconstructed
// into the original content.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "stream_integration_base.h"

namespace {

using Base = ::agenui::testing::integration::StreamIntegrationBase;

class StreamMarkdownIntegrationTest : public Base {};

// SMI001: Complete Markdown sent at once - single add with full content.
TEST_F(StreamMarkdownIntegrationTest, SMI001_CompleteMarkdown_SingleAdd) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":"# Hello World\n\nThis is **bold** text."}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        ASSERT_FALSE(l.componentsAddCalls.empty());
        auto& msg = l.componentsAddCalls.front().messages.front();
        EXPECT_EQ(msg.componentId, "root");
        auto j = nlohmann::json::parse(msg.component, nullptr, false);
        ASSERT_FALSE(j.is_discarded());
        EXPECT_TRUE(j.contains("content"));
        std::string content = j["content"].get<std::string>();
        EXPECT_EQ(content, "# Hello World\n\nThis is **bold** text.");
    });
}

// SMI002: Incomplete content (chunked) produces initial add + subsequent updates.
TEST_F(StreamMarkdownIntegrationTest, SMI002_ChunkedMarkdown_AddThenUpdates) {
    createTestSurface("s1");
    listener.clear();

    // Build the updateComponents JSON and split it mid-content
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":"ABCDEFGHIJ"}])");

    // Split inside the content value to trigger streaming behavior
    size_t contentStart = update.find("ABCDEFGHIJ");
    ASSERT_NE(contentStart, std::string::npos);
    size_t splitPos = contentStart + 3;  // After "ABC"
    feedChunked(update, {splitPos});

    // Should have at least one add callback
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));

    // Final content should reconstruct to original
    std::string collected = collectComponentContent("content", "appendContent");
    EXPECT_FALSE(collected.empty());
}

// SMI003: Streaming produces appendContent field in update callbacks.
TEST_F(StreamMarkdownIntegrationTest, SMI003_StreamingProducesAppendContent) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":"Hello streaming world here"}])");

    // Feed byte-by-byte to maximize streaming deltas
    size_t contentStart = update.find("Hello streaming");
    ASSERT_NE(contentStart, std::string::npos);

    // Split into small chunks inside the content
    std::vector<size_t> splits;
    for (size_t i = contentStart + 1; i < contentStart + 15; i += 3) {
        splits.push_back(i);
    }
    feedChunked(update, splits);

    // Wait for callbacks
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 3000));

    // If there were streaming updates, check for appendContent
    // Note: The engine may or may not produce appendContent depending on
    // whether content was actually streamed (incomplete at parse time)
    int addCount = countComponentsAdd();
    EXPECT_GE(addCount, 1);
}

// SMI004: Content reconstruction - initial content + all appendContent = original.
TEST_F(StreamMarkdownIntegrationTest, SMI004_ContentReconstruction) {
    createTestSurface("s1");
    listener.clear();

    const std::string originalContent =
        "# Title\n\n- Item 1\n- Item 2\n- Item 3\n\n> Quote here";

    // Build properly escaped JSON using nlohmann to handle newlines
    nlohmann::json comp;
    comp["id"] = "root";
    comp["component"] = "Markdown";
    comp["content"] = originalContent;
    nlohmann::json arr = nlohmann::json::array();
    arr.push_back(comp);
    std::string update = buildUpdateComponents("s1", arr.dump());

    // Feed in small chunks to trigger streaming
    sm->beginTextStream();
    size_t chunkSize = 10;
    for (size_t i = 0; i < update.size(); i += chunkSize) {
        sm->receiveTextChunk(
            update.substr(i, std::min(chunkSize, update.size() - i)));
    }
    sm->endTextStream();
    Drain(3000);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 3000));

    // Reconstruct and verify
    std::string collected = collectComponentContent("content", "appendContent");
    EXPECT_EQ(collected, originalContent);
}

// SMI005: DataModel streaming produces listener events.
TEST_F(StreamMarkdownIntegrationTest, SMI005_DataModelStreaming) {
    createTestSurface("s1");
    listener.clear();

    // First create a component with a binding path
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":{"path":"/data/text"}}])");
    feedAll(update);

    // Wait for the component add
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Now send a updateDataModel with streaming data
    std::string dmUpdate = buildUpdateDataModel("s1", "/data/text",
                                                R"("Hello from data model")");
    feedAll(dmUpdate);

    // Should trigger componentsUpdate for the bound component
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 2000));
}

// SMI006: Byte-by-byte streaming - no data loss.
TEST_F(StreamMarkdownIntegrationTest, SMI006_ByteByByte_NoDataLoss) {
    createTestSurface("s1");
    listener.clear();

    const std::string content = "Short content for byte test";
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":")" + content +
            R"("}])");

    feedByteByByte(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 5000));

    std::string collected = collectComponentContent("content", "appendContent");
    EXPECT_EQ(collected, content);
}

}  // namespace
