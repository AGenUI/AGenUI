# validate_required

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Validation Function |
| Related Tests | SKILL-07 |
| Function Name | `required` |
| Return Type | `boolean` |
| Invocation | `formatString` embeds `${required(...)}` to convert boolean to string rendered in `Text.text` |
| surfaceId | `test-surf-fc-07` |

## Test Case Description

Verifies `required`'s non-empty validation. Checks whether `value` is null, undefined, empty string, empty array, or empty object — returns `false` for any of these, otherwise returns `true`.

Since `required` returns `boolean`, the test uses `formatString`'s `${required(...)}` nesting syntax to convert the result to a string rendered in Text components, then asserts `"true"` or `"false"` via `textContent.contains`.

```json
{
  "id": "text-07a",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "${required(value: 'hello')}" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-07-a: Non-empty string -> true

| Property | Value |
|------|------|
| Component ID | `text-07a` |
| Input Parameters | `required(value: 'hello')` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-07-b: Empty string -> false

| Property | Value |
|------|------|
| Component ID | `text-07b` |
| Input Parameters | `required(value: '')` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-07-c: null value -> false

| Property | Value |
|------|------|
| Component ID | `text-07c` |
| Input Parameters | `required(value: null)` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-07-d: Empty array -> false

| Property | Value |
|------|------|
| Component ID | `text-07d` |
| Input Parameters | `required(value: [])` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-07-e: Non-empty array -> true

| Property | Value |
|------|------|
| Component ID | `text-07e` |
| Input Parameters | `required(value: [1,2,3])` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-07-f: Number 0 -> true (0 is a valid value)

| Property | Value |
|------|------|
| Component ID | `text-07f` |
| Input Parameters | `required(value: 0)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-07-g: Boolean false -> true (false is a valid value)

| Property | Value |
|------|------|
| Component ID | `text-07g` |
| Input Parameters | `required(value: false)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-07-h: Empty object -> false

| Property | Value |
|------|------|
| Component ID | `text-07h` |
| Input Parameters | `required(value: {})` |
| Expected Result | `textContent.contains: "false"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-07"`）
2. Send `updateComponents` message; each Text component's `text` property is a `formatString` (embedding `required`) FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains `"true"` or `"false"`
5. verify `componentCount` equals 9, `componentIds` list is complete

## Rendering Integration Usage

```json
{
  "id": "submit-btn",
  "component": "Button",
  "enabled": {
    "call": "required",
    "args": { "value": { "path": "/form/username" } },
    "returnType": "boolean"
  }
}
```

## Notes

- Number `0` and boolean `false` are both treated as valid values (non-empty), return `true`
- `formatString` nesting syntax: `"${required(value: ...)}"` renders boolean as `"true"` or `"false"`
- When used directly in component properties (`Button.enabled`), `returnType` is `"boolean"`, no `formatString` wrapping needed
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `validate_required.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `validate_required.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `validate_required.json` via `FixtureRunner`, asserts `textContent`
