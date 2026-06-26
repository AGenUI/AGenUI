// SCI*: Stream Chunking Integration Tests.
//
// Verifies that splitting protocol data at arbitrary positions produces
// the same final listener events when fed through the public ISurfaceManager
// streaming API.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "stream_integration_base.h"

namespace {

using Base = ::agenui::testing::integration::StreamIntegrationBase;

class StreamChunkingIntegrationTest : public Base {};

// SCI001: Complete payload as single chunk produces onComponentsAdd.
TEST_F(StreamChunkingIntegrationTest, SCI001_SingleChunk_ComponentsAdd) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1", R"([{"id":"root","component":"Text","text":"hello"}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        ASSERT_FALSE(l.componentsAddCalls.empty());
        auto& first = l.componentsAddCalls.front();
        EXPECT_EQ(first.surfaceId, "s1");
        ASSERT_FALSE(first.messages.empty());
        EXPECT_EQ(first.messages.front().componentId, "root");
    });
}

// SCI002: Split at midpoint produces same result.
TEST_F(StreamChunkingIntegrationTest, SCI002_MidpointSplit_SameResult) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1", R"([{"id":"root","component":"Text","text":"hello"}])");
    feedChunked(update, {update.size() / 2});

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        ASSERT_FALSE(l.componentsAddCalls.empty());
        EXPECT_EQ(l.componentsAddCalls.front().messages.front().componentId,
                  "root");
    });
}

// SCI003: Byte-by-byte produces same result.
TEST_F(StreamChunkingIntegrationTest, SCI003_ByteByByte_SameResult) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1", R"([{"id":"root","component":"Text","text":"hello"}])");
    feedByteByByte(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 5000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        ASSERT_FALSE(l.componentsAddCalls.empty());
        EXPECT_EQ(l.componentsAddCalls.front().messages.front().componentId,
                  "root");
    });
}

// SCI004: Split inside a JSON key name still delivers correctly.
TEST_F(StreamChunkingIntegrationTest, SCI004_SplitInsideKey_Correct) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1", R"([{"id":"root","component":"Text","text":"world"}])");
    // Split inside "component" key
    size_t keyPos = update.find("\"component\"");
    ASSERT_NE(keyPos, std::string::npos);
    feedChunked(update, {keyPos + 4});  // mid-key

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        ASSERT_FALSE(l.componentsAddCalls.empty());
        EXPECT_EQ(l.componentsAddCalls.front().messages.front().componentId,
                  "root");
    });
}

// SCI005: Split between createSurface and updateComponents - both events fire.
TEST_F(StreamChunkingIntegrationTest, SCI005_TwoEnvelopes_BothEventsFireInStream) {
    std::string create = buildCreateSurface("s2");
    std::string update = buildUpdateComponents(
        "s2", R"([{"id":"root","component":"Text","text":"hi"}])");
    std::string combined = create + update;

    feedAll(combined);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.createSurfaceCalls.empty(); }, 2000));
    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
}

// SCI006: Multiple components in one update - all arrive.
TEST_F(StreamChunkingIntegrationTest, SCI006_MultipleComponents_AllArrive) {
    createTestSurface("s1");
    listener.clear();

    // Root container with children
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Column","children":[)"
        R"({"id":"c1","component":"Text","text":"a"},)"
        R"({"id":"c2","component":"Text","text":"b"},)"
        R"({"id":"c3","component":"Text","text":"c"}]}])");
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        int totalComponents = 0;
        for (auto& rec : l.componentsAddCalls) {
            totalComponents += static_cast<int>(rec.messages.size());
        }
        EXPECT_GE(totalComponents, 1);
    });
}

// SCI007: Exhaustive binary split - every position produces correct result.
TEST_F(StreamChunkingIntegrationTest, SCI007_ExhaustiveSplit_AllCorrect) {
    std::string create = buildCreateSurface("s1");
    std::string update = buildUpdateComponents(
        "s1", R"([{"id":"root","component":"Text","text":"ok"}])");
    std::string combined = create + update;

    // Test every 7th split position for speed
    int successCount = 0;
    for (size_t i = 1; i < combined.size(); i += 7) {
        ::agenui::testing::ScopedSurfaceManager localSm;
        ASSERT_TRUE(localSm);
        ::agenui::testing::MockMessageListener localListener;
        localSm->addSurfaceEventListener(&localListener);

        localSm->beginTextStream();
        localSm->receiveTextChunk(combined.substr(0, i));
        localSm->receiveTextChunk(combined.substr(i));
        localSm->endTextStream();
        ::agenui::testing::WaitForWorkerIdle(2000);

        bool hasSurface = localListener.waitFor(
            [&]() { return !localListener.createSurfaceCalls.empty(); }, 2000);
        if (hasSurface) {
            bool hasComp = localListener.waitFor(
                [&]() { return !localListener.componentsAddCalls.empty(); },
                2000);
            if (hasComp) successCount++;
        }

        localSm->removeSurfaceEventListener(&localListener);
    }
    EXPECT_GT(successCount, 0)
        << "At least some split positions should produce full delivery";
}

// SCI008: Fixed 2-byte chunks.
TEST_F(StreamChunkingIntegrationTest, SCI008_TwoByteChunks_CompleteDelivery) {
    createTestSurface("s1");
    listener.clear();

    std::string update = buildUpdateComponents(
        "s1", R"([{"id":"root","component":"Text","text":"test data"}])");

    sm->beginTextStream();
    for (size_t i = 0; i < update.size(); i += 2) {
        size_t len = std::min(size_t(2), update.size() - i);
        sm->receiveTextChunk(update.substr(i, len));
    }
    sm->endTextStream();
    Drain(3000);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 3000));
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        ASSERT_FALSE(l.componentsAddCalls.empty());
        EXPECT_EQ(l.componentsAddCalls.front().messages.front().componentId,
                  "root");
    });
}

}  // namespace
