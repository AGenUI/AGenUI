# validate_numeric

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Validation Function |
| Related Tests | SKILL-08 |
| Function Name | `numeric` |
| Return Type | `boolean` |
| Invocation | `formatString` embeds `${numeric(...)}` to convert boolean to string rendered in `Text.text` |
| surfaceId | `test-surf-fc-08` |

## Test Case Description

Verifies `numeric`'s numerical range validation. Checks whether `value` is within `[min, max]` range (closed interval). Both `min` and `max` are optional parameters.

Since `numeric` returns `boolean`, the test uses `formatString`'s `${numeric(...)}` nesting syntax to convert the result to a string rendered in Text components, then asserts `"true"` or `"false"` via `textContent.contains`.

```json
{
  "id": "text-08a",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "${numeric(value: 5, min: 0, max: 10)}" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-08-a: Value within min-max range -> true

| Property | Value |
|------|------|
| Component ID | `text-08a` |
| Input Parameters | `numeric(value: 5, min: 0, max: 10)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-08-b: Value equals min boundary -> true

| Property | Value |
|------|------|
| Component ID | `text-08b` |
| Input Parameters | `numeric(value: 0, min: 0, max: 10)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-08-c: Value equals max boundary -> true

| Property | Value |
|------|------|
| Component ID | `text-08c` |
| Input Parameters | `numeric(value: 10, min: 0, max: 10)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-08-d: Value below min -> false

| Property | Value |
|------|------|
| Component ID | `text-08d` |
| Input Parameters | `numeric(value: -1, min: 0, max: 10)` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-08-e: Value exceeds max -> false

| Property | Value |
|------|------|
| Component ID | `text-08e` |
| Input Parameters | `numeric(value: 11, min: 0, max: 10)` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-08-f: Only min specified, value satisfies -> true

| Property | Value |
|------|------|
| Component ID | `text-08f` |
| Input Parameters | `numeric(value: 100, min: 1)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-08-g: Only max specified, value satisfies -> true

| Property | Value |
|------|------|
| Component ID | `text-08g` |
| Input Parameters | `numeric(value: 50, max: 100)` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-08-h: No min/max constraint, always -> true

| Property | Value |
|------|------|
| Component ID | `text-08h` |
| Input Parameters | `numeric(value: 99999)` |
| Expected Result | `textContent.contains: "true"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-08"`）
2. Send `updateComponents` message; each Text component's `text` property is a `formatString` (embedding `numeric`) FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains `"true"` or `"false"`
5. verify `componentCount` equals 9, `componentIds` list is complete

## Notes

- Boundaries are inclusive (`value >= min` and `value <= max`)
- `formatString` nesting syntax: `"${numeric(value: ...)}"` renders boolean as `"true"` or `"false"`
- When used directly in component properties, `returnType` is `"boolean"`, no `formatString` wrapping needed
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `validate_numeric.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `validate_numeric.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `validate_numeric.json` via `FixtureRunner`, asserts `textContent`
