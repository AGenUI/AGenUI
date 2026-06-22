# mixed_formatString_nested

## Basic Info

| Property | Value |
|------|------|
| Category | Mixed Scenario / Nested Function Call |
| Related Tests | SCENE-01 |
| Core Mechanism | Nesting other function calls via `${funcName(...)}` in `formatString`'s `args.value` template |
| Return Type | `string` |
| surfaceId | `test-surf-scene-01` |

## Test Case Description

Verifies the ability of `formatString` to be combined with other formatting functions (`formatDate`, `formatNumber`, `formatCurrency`, `pluralize`, `token`) via nesting.

Tests bind various nested `formatString` FunctionCall to Text component's `text` property via `updateComponents`, asserting `textContent` contains the expected interpolation result after engine rendering.

**Core Mechanism:**
- `${funcName(...)}` syntax in `args.value` is pre-evaluated and replaced with strings by the **engine layer** before being passed to `formatString`
- Tests write template strings with nested calls directly in `args.value`, no step-by-step processing needed

```json
{
  "id": "text-s1a",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "Today is ${formatDate(value: '2025-12-15', format: 'EEEE, MMMM d')}." },
    "returnType": "string"
  }
}
```

## Sub-cases

### SCENE-01-a: Nested formatDate

| Property | Value |
|------|------|
| Component ID | `text-s1a` |
| Template | `"Today is ${formatDate(value: '2025-12-15', format: 'EEEE, MMMM d')}."` |
| Expected Result | `textContent.contains: "Today is Monday, December 15."` |

### SCENE-01-b: Nested formatNumber

| Property | Value |
|------|------|
| Component ID | `text-s1b` |
| Template | `"Price is ${formatNumber(value: 1234567.89, decimals: 2)}."` |
| Expected Result | `textContent.contains: "Price is 1,234,567.89."` |

### SCENE-01-c: Nested formatCurrency

| Property | Value |
|------|------|
| Component ID | `text-s1c` |
| Template | `"Total: ${formatCurrency(value: 99.9, currency: 'CNY')}"` |
| Expected Result | `textContent.contains: "Total: CNY 99.90"` |

### SCENE-01-d: Nested pluralize

| Property | Value |
|------|------|
| Component ID | `text-s1d` |
| Template | `"You have ${pluralize(value: 3, one: 'item', other: 'items')} in cart."` |
| Expected Result | `textContent.contains: "You have items in cart."` |

### SCENE-01-e: Multiple expressions nested simultaneously

| Property | Value |
|------|------|
| Component ID | `text-s1e` |
| Template | `"${formatDate(value: '2025-01-01', format: 'yyyy')} year has ${formatNumber(value: 365, decimals: 0)} days."` |
| Expected Result | `textContent.contains: "2025"` and `contains: "365"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-scene-01"`）
2. Send `updateComponents` message, each Text component's `text` property is a `formatString` FunctionCall with nested calls
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains the expected interpolated string
5. verify `componentCount` equals 6, `componentIds` list is complete

## Notes

- Nested expressions in `args.value` are evaluated by the engine layer, `formatString` receives the already-interpolated string
- Nested call output depends on each sub-function's implementation; sub-function test cases should pass individually first
- `token` return value depends on the runtime-registered token table, expected to use `contains` prefix for loose matching

## Platform Coverage

- Android: Replays `mixed_formatString_nested.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `mixed_formatString_nested.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `mixed_formatString_nested.json` via `FixtureRunner`, asserts `textContent`
