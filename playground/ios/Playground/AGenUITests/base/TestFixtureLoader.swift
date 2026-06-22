//
//  TestFixtureLoader.swift
//  AGenUITests
//
//  测试数据加载工具
//
//  从 Bundle 中的 test_fixtures/ 目录读取 Phase 1 生成的 JSON 测试用例文件。
//  iOS Build Phase 脚本在编译时将 playground/resource/test_fixtures 复制到 Bundle 中。
//
//  用法示例：
//    let json = try TestFixtureLoader.loadMessagesAsString("components/02_button_with_action.json")
//    let fixture = try TestFixtureLoader.loadFixture("components/02_button_with_action.json")
//    let surfaceId = fixture["surfaceId"] as? String
//

import Foundation

class TestFixtureLoader {

    private static let fixtureBase = "test_fixtures"

    // MARK: - 原始文本读取

    /// 读取 test_fixtures/ 下的文件原始文本内容
    ///
    /// - Parameter relativePath: 相对于 test_fixtures/ 的路径，如 "components/01_text_only.json"
    /// - Returns: 文件完整文本
    static func readRawText(_ relativePath: String) throws -> String {
        // 尝试在 Bundle 中找到 test_fixtures 子路径
        let components = relativePath.components(separatedBy: "/")
        guard !components.isEmpty else {
            throw TestFixtureError.fileNotFound(relativePath)
        }

        // 先尝试 Bundle.main，再尝试 Bundle(for: TestFixtureLoader.self)
        let bundles = [Bundle(for: TestFixtureLoader.self), Bundle.main]

        for bundle in bundles {
            if let url = bundle.url(forResource: (components.last ?? "").replacingOccurrences(of: ".json", with: ""),
                                    withExtension: "json",
                                    subdirectory: fixtureBase + "/" + components.dropLast().joined(separator: "/")) {
                return try String(contentsOf: url, encoding: .utf8)
            }
        }

        // 备选：直接按路径查找
        for bundle in bundles {
            if let basePath = bundle.resourcePath {
                let fullPath = basePath + "/" + fixtureBase + "/" + relativePath
                if FileManager.default.fileExists(atPath: fullPath) {
                    return try String(contentsOfFile: fullPath, encoding: .utf8)
                }
            }
        }

        throw TestFixtureError.fileNotFound(relativePath)
    }

    // MARK: - JSON fixture 加载

    /// 加载 fixture JSON 文件，返回完整字典（含 messages / expect 字段）
    ///
    /// - Parameter relativePath: 相对路径，如 "components/02_button_with_action.json"
    /// - Returns: 解析后的字典
    static func loadFixture(_ relativePath: String) throws -> [String: Any] {
        let text = try readRawText(relativePath)
        guard let data = text.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw TestFixtureError.parseError(relativePath)
        }
        return json
    }

    /// 从 fixture 的 messages 数组中，将所有消息拼接为单个连续字符串
    ///
    /// A2UI 协议支持多条 JSON 消息拼接在一起流式传入。
    ///
    /// - Parameter relativePath: fixture 相对路径
    /// - Returns: 拼接后的 JSON 字符串（每条消息之间无分隔符）
    static func loadMessagesAsString(_ relativePath: String) throws -> String {
        let fixture = try loadFixture(relativePath)
        guard let messages = fixture["messages"] as? [[String: Any]] else {
            throw TestFixtureError.missingField("messages", relativePath)
        }
        return try messages.map { message -> String in
            guard let data = try? JSONSerialization.data(withJSONObject: message),
                  let str = String(data: data, encoding: .utf8) else {
                throw TestFixtureError.parseError(relativePath)
            }
            return str
        }.joined()
    }

    /// 从 fixture 的 payload 数组（流式用例）中拼接为单个字符串
    ///
    /// - Parameter relativePath: fixture 相对路径
    /// - Returns: 拼接后的 JSON 字符串
    static func loadPayloadAsString(_ relativePath: String) throws -> String {
        let fixture = try loadFixture(relativePath)
        guard let payload = fixture["payload"] as? [[String: Any]] else {
            throw TestFixtureError.missingField("payload", relativePath)
        }
        return try payload.map { item -> String in
            guard let data = try? JSONSerialization.data(withJSONObject: item),
                  let str = String(data: data, encoding: .utf8) else {
                throw TestFixtureError.parseError(relativePath)
            }
            return str
        }.joined()
    }

    /// 便捷方法：读取 fixture 的 surfaceId 字段
    static func getSurfaceId(_ relativePath: String) throws -> String {
        let fixture = try loadFixture(relativePath)
        guard let surfaceId = fixture["surfaceId"] as? String else {
            throw TestFixtureError.missingField("surfaceId", relativePath)
        }
        return surfaceId
    }

    /// 便捷方法：读取 fixture 的 expect 对象
    static func getExpect(_ relativePath: String) throws -> [String: Any] {
        let fixture = try loadFixture(relativePath)
        guard let expect = fixture["expect"] as? [String: Any] else {
            throw TestFixtureError.missingField("expect", relativePath)
        }
        return expect
    }

    // MARK: - 流式工具

    /// 将字符串按指定 chunkSize 分片，返回片段数组
    ///
    /// - Parameters:
    ///   - fullJson: 完整 JSON 字符串
    ///   - chunkSize: 每片字符数，<= 0 表示一次性发送全部
    /// - Returns: 分片后的字符串数组
    static func splitIntoChunks(_ fullJson: String, chunkSize: Int) -> [String] {
        guard chunkSize > 0 else { return [fullJson] }
        var chunks: [String] = []
        var index = fullJson.startIndex
        while index < fullJson.endIndex {
            let end = fullJson.index(index, offsetBy: chunkSize, limitedBy: fullJson.endIndex) ?? fullJson.endIndex
            chunks.append(String(fullJson[index..<end]))
            index = end
        }
        return chunks
    }
}

// MARK: - 错误类型

enum TestFixtureError: Error, CustomStringConvertible {
    case fileNotFound(String)
    case parseError(String)
    case missingField(String, String)

    var description: String {
        switch self {
        case .fileNotFound(let path):
            return "测试 fixture 文件未找到：test_fixtures/\(path)"
        case .parseError(let path):
            return "测试 fixture JSON 解析失败：test_fixtures/\(path)"
        case .missingField(let field, let path):
            return "测试 fixture 缺少字段 '\(field)'：test_fixtures/\(path)"
        }
    }
}
