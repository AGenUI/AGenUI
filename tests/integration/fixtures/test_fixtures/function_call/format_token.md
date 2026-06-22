# format_token

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Built-in Function |
| Related Tests | SKILL-06 |
| Function Name | `token` |
| Invocation | `Text.text` FunctionCall property binding (nesting `token` via `formatString`) |
| surfaceId | `test-surf-fc-06` |

## Test Case Description

Verifies `token`'s design token resolution functionality. Maps token names to actual values (e.g., colors, font sizes, and other design system variables) via `TokenParser`. Parameters:
- `name` (required): token name string (e.g., `color.primary`, `font.size.body`)

Since `token`'s return value depends on the runtime registry, tests use `formatString` with nested `token` to concatenate prefix text with token values and render to Text components, using `textContent.contains` for loose assertion that prefix text exists.

```json
{
  "id": "text-06a",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "primary: ${token(name: 'color.primary')}" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-06-a: Registered color token resolution

| Property | Value |
|------|------|
| Component ID | `text-06a` |
| Template | `"primary: ${token(name: 'color.primary')}"` |
| Prerequisite | `color.primary` token must be registered before test |
| Expected Result | `textContent.contains: "primary: "` (prefix exists; token value depends on registry) |

### SKILL-06-b: Registered font size token resolution

| Property | Value |
|------|------|
| Component ID | `text-06b` |
| Template | `"body-size: ${token(name: 'font.size.body')}"` |
| Prerequisite | `font.size.body` token must be registered before test |
| Expected Result | `textContent.contains: "body-size: "` (prefix exists; token value depends on registry) |

### SKILL-06-c: Unregistered token, fallback behavior

| Property | Value |
|------|------|
| Component ID | `text-06c` |
| Template | `"unknown: ${token(name: 'nonexistent.token.xyz')}"` |
| Expected Result | `textContent.contains: "unknown: "` (returns empty string or raw token name; prefix existence passes) |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-06"`）
2. Send `updateComponents` message; each Text component's `text` property is a `formatString` (embedding `token`) FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains `"true"` or `"false"`
5. verify `componentCount` equals 4, `componentIds` list is complete

## Notes

- Resolution result depends on `TokenParser` singleton's registration state; tests use `contains` to assert prefix rather than exact value
- Behavior for unregistered tokens is determined by `TokenParser::resolve` (usually returns empty string)
- `token` function is called via `formatString`'s `${token(...)}` nesting syntax
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `format_token.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `format_token.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `format_token.json` via `FixtureRunner`, asserts `textContent`
