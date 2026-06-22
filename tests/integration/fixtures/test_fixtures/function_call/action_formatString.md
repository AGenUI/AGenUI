# action_formatString

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Built-in Function |
| Related Tests | SKILL-02 |
| Function Name | `formatString` |
| Invocation | `Text.text` FunctionCall property binding, nesting other functions via `${...}` in `args.value` |
| surfaceId | `test-surf-fc-02` |

## Test Case Description

Verifies `formatString`'s string template interpolation functionality, and the engine's ability to evaluate nested function calls (`${funcName(...)}`) within `args.value` strings.

Tests bind `formatString` FunctionCall to Text component's `text` property via `updateComponents`, asserting `textContent` contains the expected string after engine rendering.

```json
{
  "id": "text-02c",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "Today is ${formatDate(value: '2025-12-15', format: 'EEEE, MMMM d')}." },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-02-a: Plain string direct return

| Property | Value |
|------|------|
| Component ID | `text-02a` |
| Input Parameters | `{ "value": "Hello, AGenUI!" }` |
| Expected Result | `textContent.contains: "Hello, AGenUI!"` |

### SKILL-02-b: Number string return

| Property | Value |
|------|------|
| Component ID | `text-02b` |
| Input Parameters | `{ "value": "42" }` |
| Expected Result | `textContent.contains: "42"` |

### SKILL-02-c: Nested formatDate expression

| Property | Value |
|------|------|
| Component ID | `text-02c` |
| Template | `"Today is ${formatDate(value: '2025-12-15', format: 'EEEE, MMMM d')}."` |
| Expected Result | `textContent.contains: "Today is Monday, December 15."` |

### SKILL-02-d: Nested formatNumber expression

| Property | Value |
|------|------|
| Component ID | `text-02d` |
| Template | `"Total: ${formatNumber(value: 1234567.89, decimals: 2)}"` |
| Expected Result | `textContent.contains: "Total: 1,234,567.89"` |

### SKILL-02-e: Nested pluralize expression

| Property | Value |
|------|------|
| Component ID | `text-02e` |
| Template | `"You have ${pluralize(value: 3, one: 'item', other: 'items')} in cart."` |
| Expected Result | `textContent.contains: "You have items in cart."` |

### SKILL-02-f: Empty string input

| Property | Value |
|------|------|
| Component ID | `text-02f` |
| Input Parameters | `{ "value": "" }` |
| Expected Result | `textContent` is empty string (component renders as empty) |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-02"`）
2. Send `updateComponents` message; each Text component's `text` property is a `formatString` FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains `"true"` or `"false"`
5. verify `componentCount` equals 7, `componentIds` list is complete

## Notes

- The `${funcName(...)}` syntax in `args.value` is pre-evaluated by the **engine layer**; `formatString` receives the already-interpolated string
- Nested call mechanism is handled by the engine, including JSON Pointer paths (`${/path}`) and function calls (`${funcName(...)}`)
- FunctionCall's `returnType` is `"string"`
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `action_formatString.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `action_formatString.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `action_formatString.json` via `FixtureRunner`, asserts `textContent`
