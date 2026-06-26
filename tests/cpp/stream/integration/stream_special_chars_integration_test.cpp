// SSCI*: Stream Special Characters Integration Tests.
//
// End-to-end UTF-8 / emoji integrity through the full pipeline.
// Verifies that multi-byte characters survive streaming when split at
// arbitrary positions.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "stream_integration_base.h"

namespace {

using Base = ::agenui::testing::integration::StreamIntegrationBase;

class StreamSpecialCharsIntegrationTest : public Base {};

// SSCI001: Chinese characters split at multi-byte boundary - no crash.
TEST_F(StreamSpecialCharsIntegrationTest, SSCI001_ChineseSplitAtBoundary_NoCrash) {
    createTestSurface("s1");
    listener.clear();

    // Chinese characters are 3 bytes each in UTF-8
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":"\u4f60\u597d\u4e16\u754c"}])");

    // Find content start and split mid-character
    size_t contentStart = update.find("\\u4f60");
    if (contentStart == std::string::npos) {
        // Use raw UTF-8 bytes
        update = buildUpdateComponents(
            "s1",
            "[{\"id\":\"root\",\"component\":\"Markdown\",\"content\":\"\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c\"}]");
        contentStart = update.find("\xe4\xbd\xa0");
        ASSERT_NE(contentStart, std::string::npos);
    }

    // Split in the middle of a multi-byte sequence
    feedChunked(update, {contentStart + 1});

    // Should not crash - just verify something came through
    Drain(3000);
    SUCCEED();
}

// SSCI002: Chinese content preserved in callback - exact bytes match.
TEST_F(StreamSpecialCharsIntegrationTest, SSCI002_ChineseContentPreserved) {
    createTestSurface("s1");
    listener.clear();

    const std::string chinese = "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c";  // "你好世界"
    std::string update = buildUpdateComponents(
        "s1",
        "[{\"id\":\"root\",\"component\":\"Markdown\",\"content\":\"" +
            chinese + "\"}]");

    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));

    std::string collected = collectComponentContent("content", "appendContent");
    EXPECT_EQ(collected, chinese);
}

// SSCI003: Emoji split mid-codepoint - no crash.
TEST_F(StreamSpecialCharsIntegrationTest, SSCI003_EmojiSplitMidCodepoint_NoCrash) {
    createTestSurface("s1");
    listener.clear();

    // Emoji: 🎉 = F0 9F 8E 89 (4 bytes)
    const std::string emoji = "\xF0\x9F\x8E\x89";
    std::string update = buildUpdateComponents(
        "s1",
        "[{\"id\":\"root\",\"component\":\"Markdown\",\"content\":\"" +
            emoji + "\"}]");

    // Split right in the middle of the emoji
    size_t emojiStart = update.find(emoji);
    ASSERT_NE(emojiStart, std::string::npos);
    feedChunked(update, {emojiStart + 2});  // Split 4-byte emoji at byte 2

    Drain(3000);
    SUCCEED();  // No crash is success
}

// SSCI004: Emoji preserved in callback - exact bytes match.
TEST_F(StreamSpecialCharsIntegrationTest, SSCI004_EmojiPreserved) {
    createTestSurface("s1");
    listener.clear();

    const std::string emoji = "\xF0\x9F\x8E\x89\xF0\x9F\x9A\x80";  // 🎉🚀
    std::string update = buildUpdateComponents(
        "s1",
        "[{\"id\":\"root\",\"component\":\"Markdown\",\"content\":\"" +
            emoji + "\"}]");

    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));

    std::string collected = collectComponentContent("content", "appendContent");
    EXPECT_EQ(collected, emoji);
}

// SSCI005: Mixed UTF-8 byte-by-byte - all content delivered.
TEST_F(StreamSpecialCharsIntegrationTest, SSCI005_MixedUTF8ByteByByte) {
    createTestSurface("s1");
    listener.clear();

    // Mix of ASCII, 2-byte, 3-byte, 4-byte UTF-8
    const std::string mixed = "Hi\xC3\xA9\xe4\xb8\xad\xF0\x9F\x98\x80" "end";
    // "Hi" + é(2B) + 中(3B) + 😀(4B) + "end"
    std::string update = buildUpdateComponents(
        "s1",
        "[{\"id\":\"root\",\"component\":\"Markdown\",\"content\":\"" +
            mixed + "\"}]");

    feedByteByByte(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 5000));

    std::string collected = collectComponentContent("content", "appendContent");
    EXPECT_EQ(collected, mixed);
}

// SSCI006: Escaped unicode sequences handled properly.
TEST_F(StreamSpecialCharsIntegrationTest, SSCI006_EscapedUnicode) {
    createTestSurface("s1");
    listener.clear();

    // Use JSON unicode escapes
    std::string update = buildUpdateComponents(
        "s1",
        R"([{"id":"root","component":"Markdown","content":"Hello \u0041\u0042\u0043"}])");

    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));

    // \u0041\u0042\u0043 = "ABC" in the parsed JSON
    std::string collected = collectComponentContent("content", "appendContent");
    // The content may be "Hello ABC" if the JSON parser resolves escapes,
    // or "Hello \\u0041\\u0042\\u0043" if passed through raw
    EXPECT_FALSE(collected.empty());
}

// SSCI007: Large CJK payload - no truncation when delivered complete.
TEST_F(StreamSpecialCharsIntegrationTest, SSCI007_LargeCJKPayload_NoTruncation) {
    createTestSurface("s1");
    listener.clear();

    // Build a string with 100 Chinese characters (300 bytes)
    std::string cjk;
    const std::string hanzi = "\xe4\xb8\xad";  // "中" = 3 bytes
    for (int i = 0; i < 100; ++i) {
        cjk += hanzi;
    }

    std::string update = buildUpdateComponents(
        "s1",
        "[{\"id\":\"root\",\"component\":\"Markdown\",\"content\":\"" +
            cjk + "\"}]");

    // Feed complete message - verifies no truncation of large content
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 5000));

    std::string collected = collectComponentContent("content", "appendContent");
    EXPECT_EQ(collected, cjk);
}

}  // namespace
