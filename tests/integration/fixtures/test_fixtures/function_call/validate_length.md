# validate_length

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Validation Function |
| Related Tests | SKILL-09 |
| Function Name | `length` |
| Return Type | `boolean` |
| Invocation | `formatString` embeds `${length(...)}` to convert boolean to string rendered in `Text.text` |
| surfaceId | `test-surf-fc-09` |

## Test Case Description

Verifies `length`'s string length range validation. Checks whether the string `value`'s length satisfies the `[min, max]` constraint (closed interval). Both `min` and `max` are optional parameters.

Since `length` returns `boolean`, the test uses `formatString`'s `${length(...)}` nesting syntax to convert the result to a string rendered in Text components, then asserts `"true"` or `"false"` via `textContent.contains`.

```json
{
  "id": "text-09a",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "${length(value: 'hello', min: 3, max: 10)}" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-09-a: Length satisfies min and max -> true

| Property | Value |
|------|------|
| Component ID | `text-09a` |
| Input Parameters | `length(value: 'hello', min: 3, max: 10)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-09-b: Length equals min boundary -> true

| Property | Value |
|------|------|
| Component ID | `text-09b` |
| Input Parameters | `length(value: 'abc', min: 3, max: 10)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-09-c: Length equals max boundary -> true

| Property | Value |
|------|------|
| Component ID | `text-09c` |
| Input Parameters | `length(value: '1234567890', min: 1, max: 10)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-09-d: Length below min -> false

| Property | Value |
|------|------|
| Component ID | `text-09d` |
| Input Parameters | `length(value: 'ab', min: 3, max: 10)` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-09-e: Length exceeds max -> false

| Property | Value |
|------|------|
| Component ID | `text-09e` |
| Input Parameters | `length(value: '12345678901', min: 1, max: 10)` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-09-f: Only min specified, length satisfies -> true

| Property | Value |
|------|------|
| Component ID | `text-09f` |
| Input Parameters | `length(value: 'long enough', min: 5)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-09-g: Only max specified, length satisfies -> true

| Property | Value |
|------|------|
| Component ID | `text-09g` |
| Input Parameters | `length(value: 'short', max: 20)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-09-h: No min/max constraint, always -> true

| Property | Value |
|------|------|
| Component ID | `text-09h` |
| Input Parameters | `length(value: 'any string')` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-09-i: Empty string, min=0 -> true

| Property | Value |
|------|------|
| Component ID | `text-09i` |
| Input Parameters | `length(value: '', min: 0)` |
| Expected Result | `textContent.contains: "true"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-09"`）
2. Send `updateComponents` message; each Text component's `text` property is a `formatString` (embedding `length`) FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains `"true"` or `"false"`
5. verify `componentCount` equals 10, `componentIds` list is complete

## Notes

- Length is based on byte count (`std::string::length()`); multi-byte UTF-8 characters are counted by bytes
- Boundaries are inclusive (closed interval)
- `formatString` nesting syntax: `"${length(value: ...)}"` renders boolean as `"true"` or `"false"`
- When used directly in component properties, `returnType` is `"boolean"`, no `formatString` wrapping needed
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `validate_length.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `validate_length.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `validate_length.json` via `FixtureRunner`, asserts `textContent`
