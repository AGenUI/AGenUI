# Rizzcharts - A2UI Ecommerce Dashboard Agent (Go)

[中文文档](README.zh-CN.md)

A demo agent server that visualizes ecommerce data (sales breakdowns, regional outliers) via the A2UI protocol, served over A2A (Agent-to-Agent) JSON-RPC.

## Prerequisites

- Go 1.22+
- An OpenAI-compatible API key (Gemini, OpenAI, or any LiteLLM-compatible provider)

## Configuration

Create a `.env` file in this directory:

```bash
# Option 1: Gemini (default)
GEMINI_API_KEY=your-gemini-api-key
GEMINI_MODEL=gemini-2.5-flash

# Option 2: OpenAI-compatible API
OPENAI_API_KEY=your-api-key
OPENAI_API_BASE=https://api.openai.com/v1
LITELLM_MODEL=gpt-4o
```

## Run

```bash
cd samples/go/rizzcharts
go run .
```

The server starts on `http://localhost:10002`.

## Endpoints

| Endpoint | Description |
|----------|-------------|
| `GET /.well-known/agent-card.json` | A2A Agent Card (agent capabilities & metadata) |
| `POST /` | A2A JSON-RPC endpoint (supports `message/send`) |

## Usage Examples

### Fetch Agent Card

```bash
curl http://localhost:10002/.well-known/agent-card.json | jq .
```

### Send a Message

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

### Example Queries

- `"show my sales breakdown by product category for q3"` — Returns A2UI Chart component payload (doughnut/pie)
- `"show revenue trends yoy by month"` — Returns A2UI Chart component payload
- `"were there any outlier stores in the northeast region"` — Returns A2UI Map component payload

## Architecture

```
main.go      - HTTP server, A2A protocol handling
agent.go     - LLM interaction, tool-calling loop
tools.go     - Mock data tools (get_sales_data, get_store_sales)
```

The agent uses the A2UI Go SDK to:
1. Build a system prompt with A2UI schema and catalog definitions
2. Call an LLM via OpenAI-compatible API with tool definitions
3. Execute tools and feed results back to the LLM
4. Parse `<a2ui-json>` blocks from the LLM response
5. Return A2UI messages as A2A data parts

## Supported Skills

| Skill | Description |
|-------|-------------|
| `view_sales_by_category` | Pie chart of sales by product category |
| `view_regional_outliers` | Map showing regional sales outliers |
