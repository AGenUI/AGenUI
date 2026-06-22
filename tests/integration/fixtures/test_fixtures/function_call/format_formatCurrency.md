# format_formatCurrency

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Built-in Function |
| Related Tests | SKILL-04 |
| Function Name | `formatCurrency` |
| Invocation | `Text.text` FunctionCall property binding |
| surfaceId | `test-surf-fc-04` |

## Test Case Description

Verifies `formatCurrency`'s currency formatting functionality. Output format is `<currency> <number>`, supports multiple currency codes, thousands separator control, and decimal precision control. Parameters:
- `value` (required): number to format
- `currency` (required): currency code (e.g., `USD`, `CNY`, `EUR`)
- `decimals` (optional, default 2): decimal places
- `grouping` (optional, default true): whether to add thousands separator

The test binds `formatCurrency` FunctionCall to Text components' `text` property via `updateComponents`. After engine rendering, it asserts `textContent` contains the expected string.

```json
{
  "id": "text-04a",
  "component": "Text",
  "text": {
    "call": "formatCurrency",
    "args": { "value": 1234.5, "currency": "USD" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-04-a: USD currency format — thousands separator + 2 decimal places

| Property | Value |
|------|------|
| Component ID | `text-04a` |
| Input Parameters | `{ "value": 1234.5, "currency": "USD" }` |
| Expected Result | `textContent.contains: "USD 1,234.50"` |

### SKILL-04-b: CNY currency format

| Property | Value |
|------|------|
| Component ID | `text-04b` |
| Input Parameters | `{ "value": 9999.99, "currency": "CNY" }` |
| Expected Result | `textContent.contains: "CNY 9,999.99"` |

### SKILL-04-c: EUR currency format

| Property | Value |
|------|------|
| Component ID | `text-04c` |
| Input Parameters | `{ "value": 500, "currency": "EUR" }` |
| Expected Result | `textContent.contains: "EUR 500.00"` |

### SKILL-04-d: decimals=0, no decimal part

| Property | Value |
|------|------|
| Component ID | `text-04d` |
| Input Parameters | `{ "value": 1000, "currency": "USD", "decimals": 0 }` |
| Expected Result | `textContent.contains: "USD 1,000"` |

### SKILL-04-e: grouping=false, disable thousands separator

| Property | Value |
|------|------|
| Component ID | `text-04e` |
| Input Parameters | `{ "value": 1234567.89, "currency": "USD", "grouping": false }` |
| Expected Result | `textContent.contains: "USD 1234567.89"` |

### SKILL-04-f: Negative currency value

| Property | Value |
|------|------|
| Component ID | `text-04f` |
| Input Parameters | `{ "value": -250.75, "currency": "USD" }` |
| Expected Result | `textContent.contains: "USD -250.75"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-04"`）
2. Send `updateComponents` message, each Text component's `text` property is a `formatCurrency` FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains the currency code and formatted number
5. verify `componentCount` equals 7, `componentIds` list is complete

## Notes

- Output format is `<currency> <number>`, with a space between currency code and number
- Currency code is the user-supplied string, engine does not perform ISO 4217 validation
- FunctionCall's `returnType` is `"string"`
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `format_formatCurrency.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `format_formatCurrency.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `format_formatCurrency.json` via `FixtureRunner`, asserts `textContent`
