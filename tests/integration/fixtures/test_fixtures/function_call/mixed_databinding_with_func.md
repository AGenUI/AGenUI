# mixed_databinding_with_func

## Basic Info

| Property | Value |
|------|------|
| Category | Mixed Scenario / DataBinding + FunctionCall |
| Related Tests | SCENE-02 |
| Core Mechanism | FunctionCall `args` values use `DataBinding` (`{ "path": "..." }`) to dynamically retrieve context values |
| Return Type | `string` / `boolean` (validation functions converted to string via formatString) |
| surfaceId | `test-surf-scene-02` |

## Test Case Description

Verifies `FunctionCall` combined with `DataBinding`. Function arguments are no longer static literals but dynamically bound to context data values via JSON Pointer paths (e.g., `/event/date`).

The test first injects context data via `updateDataModel`, then binds FunctionCall to Text components' `text` property via `updateComponents`. After engine rendering, it asserts `textContent` contains expected results.

**Core Mechanism**:
- `DataBinding` format: `{ "path": "/json/pointer/path" }` (used in FunctionCall args)
- The engine resolves context data at the `path` before executing the function, then passes the resolved value as function input
- Context data is injected via the `updateDataModel` message

```json
{
  "id": "text-s2a",
  "component": "Text",
  "text": {
    "call": "formatDate",
    "args": { "value": { "path": "/event/date" }, "format": "yyyy/MM/dd" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SCENE-02-a：DataBinding → formatDate

| Property | Value |
|------|------|
| Component ID | `text-s2a` |
| Context | `{ "event": { "date": "2025-06-18" } }` |
| DataBinding Path | `/event/date` |
| Format | `yyyy/MM/dd` |
| Expected Result | `textContent.contains: "2025/06/18"` |

### SCENE-02-b：DataBinding → formatNumber

| Property | Value |
|------|------|
| Component ID | `text-s2b` |
| Context | `{ "order": { "amount": 9876543.21 } }` |
| DataBinding Path | `/order/amount` |
| Extra Params | `decimals: 2` |
| Expected Result | `textContent.contains: "9,876,543.21"` |

### SCENE-02-c：DataBinding → formatCurrency

| Property | Value |
|------|------|
| Component ID | `text-s2c` |
| Context | `{ "cart": { "total": 299.99 } }` |
| DataBinding Path | `/cart/total` |
| Currency | `USD` |
| Expected Result | `textContent.contains: "USD 299.99"` |

### SCENE-02-d：DataBinding → pluralize

| Property | Value |
|------|------|
| Component ID | `text-s2d` |
| Context | `{ "inbox": { "count": 5 } }` |
| DataBinding Path | `/inbox/count` |
| Extra Params | `one: "message", other: "messages"` |
| Expected Result | `textContent.contains: "messages"` |

### SCENE-02-e: DataBinding -> required (non-empty value -> true)

| Property | Value |
|------|------|
| Component ID | `text-s2e` |
| Context | `{ "form": { "username": "alice" } }` |
| DataBinding Path | `/form/username` |
| Expected Result | `textContent.contains: "true"` |

### SCENE-02-f: DataBinding -> required (empty string -> false)

| Property | Value |
|------|------|
| Component ID | `text-s2f` |
| Context | `{ "form": { "emptyField": "" } }` |
| DataBinding Path | `/form/emptyField` |
| Expected Result | `textContent.contains: "false"` |

### SCENE-02-g: DataBinding -> email (valid email -> true)

| Property | Value |
|------|------|
| Component ID | `text-s2g` |
| Context | `{ "form": { "validEmail": "test@example.com" } }` |
| DataBinding Path | `/form/validEmail` |
| Expected Result | `textContent.contains: "true"` |

### SCENE-02-h: DataBinding -> email (invalid email -> false)

| Property | Value |
|------|------|
| Component ID | `text-s2h` |
| Context | `{ "form": { "invalidEmail": "not-an-email" } }` |
| DataBinding Path | `/form/invalidEmail` |
| Expected Result | `textContent.contains: "false"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-scene-02"`）
2. Send `updateDataModel` message to inject context data (`event`, `order`, `cart`, `inbox`, `form` fields)
3. Send `updateComponents` message; each Text component's `text` property is a FunctionCall with DataBinding parameters
4. Wait for engine rendering to complete, get each component's `textContent`
5. assert each component `textContent` contains `"true"` or `"false"`
6. verify `componentCount` equals 9, `componentIds` list is complete

## Notes

- DataBinding format in FunctionCall `args`: `{ "path": "/json/pointer/path" }` (object format)
- DataBinding format in `formatString` templates: `${/path/to/value}` (interpolation syntax) — different from the above
- JSON Pointer paths follow RFC 6901 (starts with `/`, levels separated by `/`)
- If the path points to a nonexistent value, engine behavior depends on platform implementation (usually returns null or throws error)
- Validation functions (required/email) have their boolean results converted to strings via `formatString` nesting before assertion

## Platform Coverage

- Android: Replays `mixed_databinding_with_func.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `mixed_databinding_with_func.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `mixed_databinding_with_func.json` via `FixtureRunner`, asserts `textContent`
