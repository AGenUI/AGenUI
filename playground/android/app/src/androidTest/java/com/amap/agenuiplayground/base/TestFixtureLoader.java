package com.amap.agenuiplayground.base;

import android.content.Context;
import android.content.res.AssetManager;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

/**
 * 测试数据加载工具
 *
 * <p>从 {@code assets/test_fixtures/} 目录读取 Phase 1 生成的 JSON 测试用例文件。
 * Android build.gradle 已将 {@code playground/resource} 加入 assets srcDirs，
 * 因此路径前缀为 {@code test_fixtures/}（相对于 resource 目录）。
 *
 * <p>用法示例：
 * <pre>
 *   String json = loader.loadMessagesAsString("components/02_button_with_action.json");
 *   JSONObject fixture = loader.loadFixture("components/02_button_with_action.json");
 *   String surfaceId = fixture.getString("surfaceId");
 *   JSONObject expect = fixture.getJSONObject("expect");
 * </pre>
 */
public class TestFixtureLoader {

    private static final String FIXTURE_BASE = "test_fixtures/";

    private final AssetManager assetManager;

    public TestFixtureLoader(Context context) {
        this.assetManager = context.getAssets();
    }

    // ==================== 原始文本读取 ====================

    /**
     * 读取 test_fixtures/ 下的文件原始文本内容。
     *
     * @param relativePath 相对于 test_fixtures/ 的路径，如 {@code "components/01_text_only.json"}
     * @return 文件完整文本
     * @throws IOException 文件不存在或读取失败
     */
    public String readRawText(String relativePath) throws IOException {
        String fullPath = FIXTURE_BASE + relativePath;
        try (InputStream is = assetManager.open(fullPath);
             BufferedReader reader = new BufferedReader(
                     new InputStreamReader(is, StandardCharsets.UTF_8))) {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
            return sb.toString().trim();
        }
    }

    // ==================== JSON fixture 加载 ====================

    /**
     * 加载 fixture JSON 文件，返回完整 JSONObject（含 messages / expect 字段）。
     *
     * @param relativePath 相对路径，如 {@code "components/02_button_with_action.json"}
     * @return 解析后的 JSONObject
     * @throws IOException   文件读取失败
     * @throws JSONException JSON 解析失败
     */
    public JSONObject loadFixture(String relativePath) throws IOException, JSONException {
        return new JSONObject(readRawText(relativePath));
    }

    /**
     * 从 fixture 的 {@code messages} 数组中，将所有消息拼接为单个连续字符串。
     *
     * <p>这是 {@code receiveTextChunk()} 的推荐输入格式：
     * A2UI 协议支持多条 JSON 消息拼接在一起流式传入。
     *
     * @param relativePath fixture 相对路径
     * @return 拼接后的 JSON 字符串（每条消息之间无分隔符）
     * @throws IOException   文件读取失败
     * @throws JSONException JSON 解析失败
     */
    public String loadMessagesAsString(String relativePath) throws IOException, JSONException {
        JSONObject fixture = loadFixture(relativePath);
        JSONArray messages = fixture.getJSONArray("messages");
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < messages.length(); i++) {
            sb.append(messages.get(i).toString());
        }
        return sb.toString();
    }

    /**
     * 从 fixture 的 {@code payload} 数组（流式用例）中拼接为单个字符串。
     *
     * @param relativePath fixture 相对路径
     * @return 拼接后的 JSON 字符串
     * @throws IOException   文件读取失败
     * @throws JSONException JSON 解析失败
     */
    public String loadPayloadAsString(String relativePath) throws IOException, JSONException {
        JSONObject fixture = loadFixture(relativePath);
        JSONArray payload = fixture.getJSONArray("payload");
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < payload.length(); i++) {
            sb.append(payload.get(i).toString());
        }
        return sb.toString();
    }

    /**
     * 便捷方法：读取 fixture 的 {@code surfaceId} 字段。
     */
    public String getSurfaceId(String relativePath) throws IOException, JSONException {
        return loadFixture(relativePath).getString("surfaceId");
    }

    /**
     * 便捷方法：读取 fixture 的 {@code expect} 对象。
     */
    public JSONObject getExpect(String relativePath) throws IOException, JSONException {
        return loadFixture(relativePath).getJSONObject("expect");
    }

    /**
     * 便捷方法：返回 fixture 的 {@code messages} JSONArray。
     *
     * <p>建议与 {@link #loadMessagesAsString} 区分使用：
     * <ul>
     *   <li>{@link #loadMessagesAsString} 拼接为字符串，适用于单条消息或不含 multiMessage 的 fixture</li>
     *   <li>{@link #getMessages} 返回原始数组，适用于逐条发送策略（多消息序列）</li>
     * </ul>
     *
     * @param relativePath fixture 相对路径
     * @return 消息 JSONArray（每个元素为完整的协议 JSON 对象）
     * @throws IOException   文件读取失败
     * @throws JSONException JSON 解析失败
     */
    public JSONArray getMessages(String relativePath) throws IOException, JSONException {
        return loadFixture(relativePath).getJSONArray("messages");
    }

    // ==================== 流式工具 ====================

    /**
     * 将字符串按指定 chunkSize 分片，返回片段数组。
     *
     * @param fullJson  完整 JSON 字符串
     * @param chunkSize 每片字符数，{@code -1} 表示一次性发送全部
     * @return 分片后的字符串数组
     */
    public static String[] splitIntoChunks(String fullJson, int chunkSize) {
        if (chunkSize <= 0) {
            return new String[]{fullJson};
        }
        int len = fullJson.length();
        int count = (len + chunkSize - 1) / chunkSize;
        String[] chunks = new String[count];
        for (int i = 0; i < count; i++) {
            int start = i * chunkSize;
            int end = Math.min(start + chunkSize, len);
            chunks[i] = fullJson.substring(start, end);
        }
        return chunks;
    }
}
