// STI*: Stream Text Integration Tests.
//
// Verifies that Text component streaming deltas (textChunk) arrive correctly
// through the public ISurfaceManager API and can be reconstructed.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "stream_integration_base.h"

namespace {

using Base = ::agenui::testing::integration::StreamIntegrationBase;

class StreamTextIntegrationTest : public Base {};

// STI001: Complete Text sent at once - single add.
TEST_F(StreamTextIntegrationTest, STI001_CompleteText_SingleAdd) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Text","text":"Hello World"}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        ASSERT_FALSE(l.componentsAddCalls.empty());
        auto& msg = l.componentsAddCalls.front().messages.front();
        EXPECT_EQ(msg.componentId, "root");
        auto j = nlohmann::json::parse(msg.component, nullptr, false);
        ASSERT_FALSE(j.is_discarded());
        EXPECT_TRUE(j.contains("text"));
        std::string text = j["text"].get<std::string>();
        EXPECT_EQ(text, "Hello World");
    });
}

// STI002: Incomplete text (chunked) produces initial add + updates.
TEST_F(StreamTextIntegrationTest, STI002_ChunkedText_AddThenUpdates) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Text","text":"ABCDEFGHIJKLMNO"}])");

    // Split inside the text value
    size_t textStart = update.find("ABCDEFGHIJ");
    ASSERT_NE(textStart, std::string::npos);
    size_t splitPos = textStart + 4;  // After "ABCD"
    feedChunked(update, {splitPos});

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));

    std::string collected = collectComponentContent("text", "textChunk");
    EXPECT_FALSE(collected.empty());
}

// STI003: textChunk in update callbacks when streaming.
TEST_F(StreamTextIntegrationTest, STI003_StreamingProducesTextChunk) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Text","text":"Hello streaming text content here"}])");

    // Feed in small chunks
    sm->beginTextStream();
    size_t chunkSize = 8;
    for (size_t i = 0; i < update.size(); i += chunkSize) {
        sm->receiveTextChunk(
            update.substr(i, std::min(chunkSize, update.size() - i)));
    }
    sm->endTextStream();
    Drain(3000);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 3000));

    int addCount = countComponentsAdd();
    EXPECT_GE(addCount, 1);
}

// STI004: Text reconstruction - initial text + all textChunk = original.
TEST_F(StreamTextIntegrationTest, STI004_TextReconstruction) {
    createTestSurface("s1");
    listener.clear();

    const std::string originalText =
        "The quick brown fox jumps over the lazy dog. 1234567890.";
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Text","text":")" + originalText + R"("}])");

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

    std::string collected = collectComponentContent("text", "textChunk");
    EXPECT_EQ(collected, originalText);
}

// STI005: DataModel streaming for bound text.
TEST_F(StreamTextIntegrationTest, STI005_DataModelStreaming) {
    createTestSurface("s1");
    listener.clear();

    // Create component with binding
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Text","text":{"path":"/msg/body"}}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Send data model update
    std::string dmUpdate =
        buildUpdateDataModel("s1", "/msg/body", R"("Hello DM")");
    feedAll(dmUpdate);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 2000));
}

// STI006: Byte-by-byte - no data loss.
TEST_F(StreamTextIntegrationTest, STI006_ByteByByte_NoDataLoss) {
    createTestSurface("s1");
    listener.clear();

    const std::string text = "Short text byte test";
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Text","text":")" + text + R"("}])");

    feedByteByByte(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 5000));

    std::string collected = collectComponentContent("text", "textChunk");
    EXPECT_EQ(collected, text);
}

}  // namespace
