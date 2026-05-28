# A2UI Go Agent SDK

[English](README.md)

`agent_sdks/go` 目录包含 A2UI Agent SDK 的 Go 语言实现。

## A2UI Go Agent SDK 是什么？

A2UI（Agent-to-UI）协议定义了一套标准，让 AI Agent 能够生成富交互式用户界面。这个 Go SDK 使服务端 Agent 能够：

- **生成结构化 UI 响应** — 产出符合 A2UI 协议的消息（createSurface、updateComponents、updateDataModel、deleteSurface），可被任何合规的渲染器（Android、iOS、HarmonyOS、Web）直接展示。
- **流式增量解析** — 在 LLM 逐 token 生成的过程中，实时解析并产出部分 UI 组件，实现低延迟的渐进式渲染。
- **校验输出** — 对 A2UI 消息进行 JSON Schema 校验和协议规则检查（拓扑结构、可达性、循环引用、递归深度），在到达客户端之前捕获错误。
- **管理组件 Catalog** — 加载内置或自定义的组件目录，自动生成 LLM system prompt，告知模型有哪些可用的 UI 组件及其用法。

简而言之，它是 LLM 后端与任意 A2UI 兼容前端渲染器之间的桥梁。

## 安装

```bash
go get github.com/AGenUI/AGenUI/agent_sdks/go
```

## 使用方法

完整的使用示例请参考 [Rizzcharts 示例](../../samples/go/rizzcharts/)。

## 核心组件

### 根包 `a2ui`

* **`a2ui.go`**：包入口。定义 `Version`、`SystemPromptOptions` 以及用于生成 LLM 系统提示词的 `InferenceStrategy` 接口。

### Schema 管理 (`schema/`)

* **`manager.go`**：`A2uiSchemaManager` 负责加载规范 schema、管理组件目录、为 LLM 生成系统提示词。
* **`validator.go`**：`A2uiValidator` 对 A2UI 消息进行 JSON Schema 校验和协议规则检查（拓扑结构、可达性、循环引用、递归深度）。
* **`catalog.go`**：`A2uiCatalog` 和 `CatalogConfig`，用于管理组件库。
* **`catalog_provider.go`**：`A2uiCatalogProvider` 接口，提供 `FileSystemCatalogProvider` 和 `RawCatalogProvider` 实现。
* **`common_modifiers.go`**：Schema 修改器工具，用于裁剪和定制组件 schema。
* **`assets.go`**：通过 `go:embed` 打包的 JSON Schema 资源。
* **`utils.go`**：拓扑分析和 ref 字段提取的共享工具函数。

### 解析器 (`parser/`)

* **`parser.go`**：`ParseResponse` 用于同步解析完整的 LLM 响应。
* **`streaming.go`**：`A2uiStreamParser` 流式增量解析器，支持 JSON 自动修复、组件级别的增量输出和校验。
* **`streaming_v09.go`**：v0.9 版本的流式解析逻辑（`createSurface`、`updateComponents`、`updateDataModel`、`deleteSurface`）。
* **`payload_fixer.go`**：自动修复 LLM 输出中常见的 JSON 问题（尾逗号、未引用的 key 等）。
* **`response_part.go`**：`ResponsePart` 结构体，表示解析后的输出（文本 + A2UI JSON）。

### 基础组件目录 (`basiccatalog/`)

* **`basiccatalog.go`**：`BundledCatalogProvider`，提供标准 A2UI 基础组件目录（Button、Text、Row、Column 等）。

## 运行测试

1. 进入目录：

   ```bash
   cd agent_sdks/go
   ```

2. 运行所有测试：

   ```bash
   go test ./...
   ```
