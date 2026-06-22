# format_formatNumber

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Built-in Function |
| Related Tests | SKILL-03 |
| Function Name | `formatNumber` |
| Invocation | `Text.text` FunctionCall property binding |
| surfaceId | `test-surf-fc-03` |

## Test Case Description

Verifies `formatNumber`'s number formatting functionality. Supports thousands separator grouping, decimal precision control, negative numbers, zero values, etc. Parameters:
- `value` (required): number to format
- `decimals` (optional, default 2): decimal places
- `grouping` (optional, default true): whether to add thousands separator

Tests bind `formatNumber` FunctionCall to Text component's `text` property via `updateComponents`, asserting `textContent` contains the expected string after engine rendering.

```json
{
  "id": "text-03a",
  "component": "Text",
  "text": {
    "call": "formatNumber",
    "args": { "value": 1234567.89 },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-03-a: Default format — thousands separator + 2 decimal places

| Property | Value |
|------|------|
| Component ID | `text-03a` |
| Input Parameters | `{ "value": 1234567.89 }` |
| Expected Result | `textContent.contains: "1,234,567.89"` |

### SKILL-03-b: Integer, decimals=0

| Property | Value |
|------|------|
| Component ID | `text-03b` |
| Input Parameters | `{ "value": 1000, "decimals": 0 }` |
| Expected Result | `textContent.contains: "1,000"` |

### SKILL-03-c: Decimal precision decimals=4

| Property | Value |
|------|------|
| Component ID | `text-03c` |
| Input Parameters | `{ "value": 3.14159, "decimals": 4 }` |
| Expected Result | `textContent.contains: "3.1416"` |

### SKILL-03-d: Disable thousands grouping grouping=false

| Property | Value |
|------|------|
| Component ID | `text-03d` |
| Input Parameters | `{ "value": 1234567.89, "grouping": false }` |
| Expected Result | `textContent.contains: "1234567.89"`(no comma) |

### SKILL-03-e: Negative number formatting

| Property | Value |
|------|------|
| Component ID | `text-03e` |
| Input Parameters | `{ "value": -9876.5, "decimals": 1 }` |
| Expected Result | `textContent.contains: "-9,876.5"` |

### SKILL-03-f: Zero value formatting

| Property | Value |
|------|------|
| Component ID | `text-03f` |
| Input Parameters | `{ "value": 0, "decimals": 2 }` |
| Expected Result | `textContent.contains: "0.00"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-03"`）
2. Send `updateComponents` message, each Text component's `text` property is a `formatNumber` FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains the expected formatted string
5. verify `componentCount` equals 7, `componentIds` list is complete

## Notes

- Thousands separator is English comma `,`
- Rounding rules determined by `std::fixed` + `std::setprecision`
- FunctionCall's `returnType` is `"string"`
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `format_formatNumber.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `format_formatNumber.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `format_formatNumber.json` via `FixtureRunner`, asserts `textContent`
