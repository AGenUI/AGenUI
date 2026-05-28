# Rizzcharts - A2UI 电商仪表盘 Agent（Go）

[English](README.md)

一个演示用的 Agent 服务，通过 A2UI 协议可视化电商数据（销售分布、区域异常值），基于 A2A（Agent-to-Agent）JSON-RPC 协议提供服务。

## 前置条件

- Go 1.22+
- 一个兼容 OpenAI 接口的 API Key（Gemini、OpenAI 或任何 LiteLLM 兼容的服务商）

## 配置

在本目录下创建 `.env` 文件：

```bash
# 方案一：使用 Gemini（默认）
GEMINI_API_KEY=your-gemini-api-key
GEMINI_MODEL=gemini-2.5-flash

# 方案二：使用 OpenAI 兼容接口
OPENAI_API_KEY=your-api-key
OPENAI_API_BASE=https://api.openai.com/v1
LITELLM_MODEL=gpt-4o
```

## 运行

```bash
cd samples/go/rizzcharts
go run .
```

服务启动后监听 `http://localhost:10002`。

## 接口

| 接口 | 说明 |
|------|------|
| `GET /.well-known/agent-card.json` | A2A Agent Card（Agent 能力与元信息） |
| `POST /` | A2A JSON-RPC 端点（支持 `message/send` 方法） |

## 使用示例

### 获取 Agent Card

```bash
curl http://localhost:10002/.well-known/agent-card.json | jq .
```

### 发送消息

```bash
curl -X POST http://localhost:10002/ \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "message/send",
    "params": {
      "message": {
        "messageId": "msg-1",
        "role": "user",
        "parts": [{"kind": "text", "text": "show my sales breakdown by product category for q3"}],
        "kind": "message"
      }
    }
  }'
```

### 示例查询

- `"show my sales breakdown by product category for q3"` — 返回 A2UI Chart 组件载荷（环形图/饼图）
- `"show revenue trends yoy by month"` — 返回 A2UI Chart 组件载荷
- `"were there any outlier stores in the northeast region"` — 返回 A2UI Map 组件载荷

## 架构

```
main.go      - HTTP 服务器，A2A 协议处理
agent.go     - LLM 交互，工具调用循环
tools.go     - 模拟数据工具（get_sales_data、get_store_sales）
```

Agent 使用 A2UI Go SDK 实现以下流程：
1. 构建包含 A2UI schema 和组件目录定义的系统提示词
2. 通过 OpenAI 兼容接口调用 LLM，附带工具定义
3. 执行工具调用并将结果反馈给 LLM
4. 从 LLM 响应中解析 `<a2ui-json>` 块
5. 以 A2A data parts 形式返回 A2UI 消息

## 支持的能力

| 能力 | 说明 |
|------|------|
| `view_sales_by_category` | 按品类的销售饼图 |
| `view_regional_outliers` | 区域销售异常值地图 |
