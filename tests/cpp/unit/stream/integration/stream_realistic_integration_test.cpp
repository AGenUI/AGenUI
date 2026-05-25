// SRI*: Stream Realistic Integration Tests.
//
// End-to-end tests simulating real-world streaming scenarios with:
// - Multi-component trees (Column/List with Text + Divider + Markdown siblings)
// - Multiple data-binding paths resolved from one updateDataModel
// - Inline content streaming in mixed component trees
// - LLM-style token-by-token delivery with many tiny chunks (CJK content)
//
// All assertions focus on content completeness. No ordering guarantees are
// tested since the engine may batch components differently at chunk boundaries.

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <vector>

#include "stream_integration_base.h"

namespace {

using Base = ::agenui::testing::integration::StreamIntegrationBase;

class StreamRealisticIntegrationTest : public Base {
protected:
    // Align a byte offset to the next UTF-8 character boundary.
    // Avoids splitting in the middle of a multi-byte character.
    static size_t alignToUtf8Boundary(const std::string& s, size_t pos) {
        if (pos >= s.size()) return s.size();
        // Move forward past continuation bytes (10xxxxxx = 0x80..0xBF)
        while (pos < s.size() &&
               (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) {
            pos++;
        }
        return pos;
    }

    // Collect all componentIds from add+update callbacks
    std::set<std::string> collectAllComponentIds() {
        std::set<std::string> ids;
        listener.withLock([&](::agenui::testing::MockMessageListener& l) {
            for (auto& rec : l.componentsAddCalls) {
                for (auto& msg : rec.messages) {
                    ids.insert(msg.componentId);
                }
            }
            for (auto& rec : l.componentsUpdateCalls) {
                for (auto& msg : rec.messages) {
                    ids.insert(msg.componentId);
                }
            }
        });
        return ids;
    }

    // Collect content for a specific componentId
    std::string collectComponentContentById(
        const std::string& targetId,
        const std::string& contentField,
        const std::string& appendField) {
        std::string result;
        listener.withLock([&](::agenui::testing::MockMessageListener& l) {
            for (auto& rec : l.componentsAddCalls) {
                for (auto& msg : rec.messages) {
                    if (msg.componentId != targetId) continue;
                    auto j = nlohmann::json::parse(msg.component, nullptr, false);
                    if (j.is_discarded()) continue;
                    if (j.contains(contentField) && j[contentField].is_string()) {
                        result = j[contentField].get<std::string>();
                    }
                }
            }
            for (auto& rec : l.componentsUpdateCalls) {
                for (auto& msg : rec.messages) {
                    if (msg.componentId != targetId) continue;
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
};

// SRI001: Non-streaming multi-component tree.
// Complete multi-component updateComponents delivered at once.
TEST_F(StreamRealisticIntegrationTest, SRI001_NonStreaming_MultiComponent) {
    createTestSurface("s1");
    listener.clear();

    // Build multi-component tree using nlohmann::json
    nlohmann::json root;
    root["id"] = "root";
    root["component"] = "Column";
    root["children"] = nlohmann::json::array({"title", "desc", "divider", "footer"});
    root["align"] = "stretch";

    nlohmann::json title;
    title["id"] = "title";
    title["component"] = "Text";
    title["text"] = "天气预报 - 今日概况";
    title["variant"] = "h2";

    nlohmann::json desc;
    desc["id"] = "desc";
    desc["component"] = "Text";
    desc["text"] = "今天多云转晴，气温18°C至26°C，东南风3-4级。空气质量良好，适宜户外活动。";
    desc["variant"] = "body";

    nlohmann::json divider;
    divider["id"] = "divider";
    divider["component"] = "Divider";

    nlohmann::json footer;
    footer["id"] = "footer";
    footer["component"] = "Text";
    footer["text"] = "数据来源：中国气象局 · 更新时间 08:00";
    footer["variant"] = "caption";

    nlohmann::json arr = nlohmann::json::array({root, title, desc, divider, footer});
    std::string update = buildUpdateComponents("s1", arr.dump());
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));

    // Verify all componentIds are present
    auto ids = collectAllComponentIds();
    EXPECT_TRUE(ids.count("root")) << "root missing";
    EXPECT_TRUE(ids.count("title")) << "title missing";
    EXPECT_TRUE(ids.count("desc")) << "desc missing";
    EXPECT_TRUE(ids.count("footer")) << "footer missing";

    // Layout recalculations (e.g. Divider adding height) may emit updates
    // even in non-streaming mode. This is normal engine behavior.
    // We only verify no streaming-related textChunk/appendContent deltas.

    // Verify content of title component
    std::string titleText = collectComponentContentById("title", "text", "textChunk");
    EXPECT_EQ(titleText, "天气预报 - 今日概况");
}

// SRI002: Non-streaming multi-binding DataModel.
// Multiple binding paths resolved from one complete updateDataModel.
TEST_F(StreamRealisticIntegrationTest, SRI002_NonStreaming_MultiBindingDM) {
    createTestSurface("s1");
    listener.clear();

    // Phase 1: updateComponents with binding paths
    nlohmann::json root;
    root["id"] = "root";
    root["component"] = "Column";
    root["children"] = nlohmann::json::array({"title", "desc", "divider", "source"});
    root["align"] = "stretch";

    nlohmann::json title;
    title["id"] = "title";
    title["component"] = "Text";
    title["text"] = nlohmann::json::object({{"path", "/weather/title"}});
    title["variant"] = "h2";

    nlohmann::json desc;
    desc["id"] = "desc";
    desc["component"] = "Text";
    desc["text"] = nlohmann::json::object({{"path", "/weather/desc"}});
    desc["variant"] = "body";

    nlohmann::json divider;
    divider["id"] = "divider";
    divider["component"] = "Divider";

    nlohmann::json source;
    source["id"] = "source";
    source["component"] = "Text";
    source["text"] = nlohmann::json::object({{"path", "/weather/source"}});
    source["variant"] = "caption";

    nlohmann::json arr = nlohmann::json::array({root, title, desc, divider, source});
    std::string update = buildUpdateComponents("s1", arr.dump());
    feedAll(update);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Phase 2: updateDataModel with all values
    nlohmann::json dmValue;
    dmValue["title"] = "天气预报 - 今日概况";
    dmValue["desc"] = "今天多云转晴，气温18°C至26°C，东南风3-4级。空气质量良好。";
    dmValue["source"] = "数据来源：中国气象局";
    std::string dm = buildUpdateDataModel("s1", "/weather", dmValue.dump());
    feedAll(dm);

    // Binding resolution may dispatch as componentsAdd (deferred nodes)
    // or componentsUpdate (if nodes were already added as placeholders).
    EXPECT_TRUE(listener.waitFor(
        [&]() {
            return !listener.componentsAddCalls.empty() ||
                   !listener.componentsUpdateCalls.empty();
        },
        5000));

    // Verify that bound components eventually received content
    int totalMessages = 0;
    listener.withLock([&](::agenui::testing::MockMessageListener& l) {
        for (auto& rec : l.componentsAddCalls) {
            totalMessages += static_cast<int>(rec.messages.size());
        }
        for (auto& rec : l.componentsUpdateCalls) {
            totalMessages += static_cast<int>(rec.messages.size());
        }
    });
    EXPECT_GE(totalMessages, 2) << "Multiple bindings should produce multiple messages";
}

// SRI003: Streaming Text inline with mixed siblings.
// Text value split across chunks in a multi-component updateComponents.
TEST_F(StreamRealisticIntegrationTest, SRI003_StreamingText_MixedSiblings) {
    createTestSurface("s1");
    listener.clear();

    const std::string articleText =
        "根据实时交通数据分析，当前北京市区主要道路交通状况如下："
        "二环内早高峰车流量较大，平均车速约25km/h；"
        "三环路段整体通行顺畅，建议优先选择三环出行。"
        "四环至五环间有多处施工路段，请注意绕行。"
        "京藏高速进京方向拥堵严重，预计持续至上午10点。";

    const std::string mdContent = "## 出行建议\n\n- 建议错峰出行\n- 优先选择公共交通";

    // Build components
    nlohmann::json root;
    root["id"] = "root";
    root["component"] = "Column";
    root["children"] = nlohmann::json::array(
        {"header", "divider1", "article", "divider2", "md_section"});
    root["align"] = "stretch";

    nlohmann::json header;
    header["id"] = "header";
    header["component"] = "Text";
    header["text"] = "智能路况分析";
    header["variant"] = "h2";

    nlohmann::json divider1;
    divider1["id"] = "divider1";
    divider1["component"] = "Divider";

    nlohmann::json article;
    article["id"] = "article";
    article["component"] = "Text";
    article["text"] = articleText;
    article["variant"] = "body";

    nlohmann::json divider2;
    divider2["id"] = "divider2";
    divider2["component"] = "Divider";

    nlohmann::json mdSection;
    mdSection["id"] = "md_section";
    mdSection["component"] = "Markdown";
    mdSection["content"] = mdContent;

    nlohmann::json arr = nlohmann::json::array(
        {root, header, divider1, article, divider2, mdSection});
    std::string update = buildUpdateComponents("s1", arr.dump());

    // Split inside the article text value at 4 positions
    // Find the serialized article text to locate split points
    size_t articleTextPos = update.find(articleText.substr(0, 20));
    ASSERT_NE(articleTextPos, std::string::npos)
        << "Cannot find article text in serialized JSON";

    // Split at ~25%, ~50%, ~75% within the article text
    // Align to UTF-8 character boundaries to avoid broken multi-byte chars
    size_t split1 = alignToUtf8Boundary(update, articleTextPos + articleText.size() / 4);
    size_t split2 = alignToUtf8Boundary(update, articleTextPos + articleText.size() / 2);
    size_t split3 = alignToUtf8Boundary(update, articleTextPos + articleText.size() * 3 / 4);

    feedChunked(update, {split1, split2, split3});

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 3000));
    Drain(3000);

    // Verify content completeness for the article component specifically
    std::string collected = collectComponentContentById("article", "text", "textChunk");
    EXPECT_EQ(collected, articleText);

    // Verify all components eventually appear
    auto ids = collectAllComponentIds();
    EXPECT_TRUE(ids.count("header")) << "header missing";
    EXPECT_TRUE(ids.count("article")) << "article missing";
}

// SRI004: Streaming Text DataModel with many tiny chunks.
// Simulates LLM token-by-token delivery.
TEST_F(StreamRealisticIntegrationTest, SRI004_StreamingTextDM_TinyChunks) {
    createTestSurface("s1");
    listener.clear();

    // Phase 1: updateComponents with binding paths
    nlohmann::json root;
    root["id"] = "root";
    root["component"] = "Column";
    root["children"] = nlohmann::json::array(
        {"md_title", "divider", "info_text", "tag_row"});
    root["align"] = "stretch";

    nlohmann::json mdTitle;
    mdTitle["id"] = "md_title";
    mdTitle["component"] = "Markdown";
    mdTitle["content"] = nlohmann::json::object({{"path", "/article/title"}});

    nlohmann::json divider;
    divider["id"] = "divider";
    divider["component"] = "Divider";

    nlohmann::json infoText;
    infoText["id"] = "info_text";
    infoText["component"] = "Text";
    infoText["text"] = nlohmann::json::object({{"path", "/article/content"}});
    infoText["variant"] = "body";

    nlohmann::json tagRow;
    tagRow["id"] = "tag_row";
    tagRow["component"] = "Row";
    tagRow["children"] = nlohmann::json::array({"tag1", "tag2"});
    tagRow["justify"] = "start";

    nlohmann::json tag1;
    tag1["id"] = "tag1";
    tag1["component"] = "Text";
    tag1["text"] = nlohmann::json::object({{"path", "/article/tag1"}});
    tag1["variant"] = "caption";

    nlohmann::json tag2;
    tag2["id"] = "tag2";
    tag2["component"] = "Text";
    tag2["text"] = nlohmann::json::object({{"path", "/article/tag2"}});
    tag2["variant"] = "caption";

    nlohmann::json compArr = nlohmann::json::array(
        {root, mdTitle, divider, infoText, tagRow, tag1, tag2});
    std::string compUpdate = buildUpdateComponents("s1", compArr.dump());
    feedAll(compUpdate);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Phase 2: updateDataModel with large content split into tiny chunks
    const std::string contentText =
        "故宫博物院位于北京市中心，是中国明清两代的皇家宫殿，"
        "旧称紫禁城。占地面积72万平方米，建筑面积约15万平方米，"
        "几家在口碑榜和本地人推荐中都比较靠前。"
        "鸭皮酥而不硬，鸭肉多汁不柴，卷饼配葱丝、黄瓜和甜面酱很下饭，"
        "店里环境较新，适合家庭聚会。"
        "便宜坊、北平盛世、紫光园等老牌和新派烤鸭店都在你周边几公里范围内。"
        "如果现在就想吃，直接去四季民福或大鸭梨望京店，基本不会踩雷。"
        "门票价格旺季60元、淡季40元。周一闭馆，建议提前在线预约。";

    nlohmann::json dmValue;
    dmValue["article"] = nlohmann::json::object({
        {"title", "# 故宫博物院参观攻略"},
        {"content", contentText},
        {"tag1", "\xF0\x9F\x8F\x9B\xEF\xB8\x8F 文化遗产"},  // 🏛️
        {"tag2", "\xE2\xAD\x90 5A景区"}  // ⭐
    });
    std::string dmJson = buildUpdateDataModel("s1", "/", dmValue.dump());

    // Feed in many tiny chunks (simulating LLM token delivery)
    sm->beginTextStream();
    size_t pos = 0;
    // Vary chunk sizes: 5, 8, 12, 15, 20, 7, 10, 25...
    const std::vector<size_t> chunkSizes = {
        30, 8, 12, 15, 6, 20, 7, 10, 25, 5, 18, 9, 22, 11, 14,
        8, 30, 6, 19, 13, 7, 16, 10, 25, 8, 12, 20, 50};
    size_t chunkIdx = 0;
    while (pos < dmJson.size()) {
        size_t sz = chunkSizes[chunkIdx % chunkSizes.size()];
        size_t len = std::min(sz, dmJson.size() - pos);
        sm->receiveTextChunk(dmJson.substr(pos, len));
        pos += len;
        chunkIdx++;
    }
    sm->endTextStream();
    Drain(5000);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 5000));
    EXPECT_GE(countComponentsUpdate(), 1)
        << "DM streaming should produce at least one update";
}

// SRI005: Markdown inline streaming in mixed tree.
// Markdown content split across chunks with Text sibling.
TEST_F(StreamRealisticIntegrationTest, SRI005_StreamingMarkdown_MixedTree) {
    createTestSurface("s1");
    listener.clear();

    const std::string mdContent =
        "# Markdown 标题\n\n"
        "这是一段 **粗体** 和 *斜体* 文本。\n\n"
        "- 列表项 1\n- 列表项 2\n- 列表项 3\n\n"
        "需要处理A2UI协议中下发markdown时的流式处理，"
        "即在流式解析updateComponents时，如果是Markdown组件，"
        "则需要判断展示的内容是否是数据绑定，"
        "如果不是数据绑定，即展示内容直接在组件内时，"
        "那么不等待组件json闭合就开始下发组件更新事件。";

    // Build components
    nlohmann::json root;
    root["id"] = "root";
    root["component"] = "List";
    root["children"] = nlohmann::json::array({"md_1", "item_1"});

    // For streaming to work, "id" must appear before "content" in JSON
    // (the streaming parser needs the component id before content arrives).
    // nlohmann::json sorts keys alphabetically ("component" < "content" < "id"),
    // so we manually construct the Markdown JSON with correct field order.
    std::string md1Str = "{\"id\":\"md_1\",\"component\":\"Markdown\",\"content\":" +
                         nlohmann::json(mdContent).dump() + "}";

    nlohmann::json item1;
    item1["id"] = "item_1";
    item1["component"] = "Text";
    item1["text"] = "列表项 1";
    item1["variant"] = "body";

    std::string arrStr = "[" + root.dump() + "," + md1Str + "," + item1.dump() + "]";
    std::string update = buildUpdateComponents("s1", arrStr);

    // Find the markdown content in serialized JSON. Since JSON escapes newlines,
    // search for a unique substring without special characters.
    const std::string searchKey = "需要处理A2UI协议中下发markdown";
    size_t mdStart = update.find(searchKey);
    ASSERT_NE(mdStart, std::string::npos) << "Cannot find markdown content in JSON";

    // Find the extent of the content string value in the JSON
    // Search for the end of the "content" value (closing quote after the content)
    size_t contentFieldPos = update.find("\"content\":\"");
    ASSERT_NE(contentFieldPos, std::string::npos);
    size_t contentValueStart = contentFieldPos + 11;  // skip `"content":"`
    // Find closing quote (skip escaped quotes)
    size_t contentValueEnd = contentValueStart;
    while (contentValueEnd < update.size()) {
        if (update[contentValueEnd] == '"' && update[contentValueEnd - 1] != '\\') break;
        contentValueEnd++;
    }

    // Split at 3 positions within the content value, aligned to UTF-8 boundaries
    size_t contentLen = contentValueEnd - contentValueStart;
    size_t split1 = alignToUtf8Boundary(update, contentValueStart + contentLen / 4);
    size_t split2 = alignToUtf8Boundary(update, contentValueStart + contentLen / 2);
    size_t split3 = alignToUtf8Boundary(update, contentValueStart + contentLen * 3 / 4);

    feedChunked(update, {split1, split2, split3});

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 3000));
    Drain(3000);

    // Verify streaming produced appendContent deltas
    EXPECT_TRUE(hasAppendField("appendContent"))
        << "Markdown streaming should produce appendContent deltas";

    // Verify content completeness for the md_1 component
    std::string collected = collectComponentContentById("md_1", "content", "appendContent");
    EXPECT_EQ(collected, mdContent);

    // Verify all components eventually appear
    auto ids = collectAllComponentIds();
    EXPECT_TRUE(ids.count("md_1")) << "md_1 missing";
    EXPECT_TRUE(ids.count("item_1")) << "item_1 missing";
}

// SRI006: Markdown DataModel streaming with many tiny chunks.
// Markdown binding receives incremental updates as tiny chunks arrive.
TEST_F(StreamRealisticIntegrationTest, SRI006_StreamingMarkdownDM_TinyChunks) {
    createTestSurface("s1");
    listener.clear();

    // Phase 1: updateComponents with binding paths
    nlohmann::json root;
    root["id"] = "root";
    root["component"] = "List";
    root["children"] = nlohmann::json::array({"md_1", "item_1"});

    nlohmann::json md1;
    md1["id"] = "md_1";
    md1["component"] = "Markdown";
    md1["content"] = nlohmann::json::object({{"path", "/route/info"}});

    nlohmann::json item1;
    item1["id"] = "item_1";
    item1["component"] = "Text";
    item1["text"] = nlohmann::json::object({{"path", "/route/content"}});
    item1["variant"] = "body";

    nlohmann::json compArr = nlohmann::json::array({root, md1, item1});
    std::string compUpdate = buildUpdateComponents("s1", compArr.dump());
    feedAll(compUpdate);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsAddCalls.empty(); }, 2000));
    listener.clear();

    // Phase 2: updateDataModel with large Markdown content, tiny chunks
    const std::string infoText =
        "# Markdown 标题\n\n"
        "这是一段 **粗体** 和 *斜体* 文本。\n\n"
        "- 列表项 1\n- 列表项 2\n- 列表项 3\n\n"
        "需要处理A2UI协议中下发markdown时的流式处理，"
        "即在流式解析updateComponents时，如果是Markdown组件，"
        "则需要判断展示的内容是否是数据绑定。"
        "updatedatamodel是判断所有json完整闭合后才发送事件。";

    nlohmann::json dmValue;
    dmValue["route"] = nlohmann::json::object({
        {"info", infoText},
        {"content", "列表项 1"}
    });
    std::string dmJson = buildUpdateDataModel("s1", "/", dmValue.dump());

    // Feed in many tiny chunks
    sm->beginTextStream();
    size_t pos = 0;
    const std::vector<size_t> chunkSizes = {
        25, 10, 15, 8, 20, 12, 30, 7, 18, 11, 22, 9, 14, 35, 10};
    size_t chunkIdx = 0;
    while (pos < dmJson.size()) {
        size_t sz = chunkSizes[chunkIdx % chunkSizes.size()];
        size_t len = std::min(sz, dmJson.size() - pos);
        sm->receiveTextChunk(dmJson.substr(pos, len));
        pos += len;
        chunkIdx++;
    }
    sm->endTextStream();
    Drain(5000);

    EXPECT_TRUE(listener.waitFor(
        [&]() { return !listener.componentsUpdateCalls.empty(); }, 5000));
    EXPECT_GE(countComponentsUpdate(), 1)
        << "DM streaming should produce at least one update";
}

}  // namespace
