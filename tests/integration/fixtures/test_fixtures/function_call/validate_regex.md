# validate_regex

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Validation Function |
| Related Tests | SKILL-10 |
| Function Name | `regex` |
| Return Type | `boolean` |
| Invocation | `formatString` embeds `${regex(...)}` to convert boolean to string rendered in `Text.text` |
| surfaceId | `test-surf-fc-10` |

## Test Case Description

Verifies `regex`'s full-match regex validation (using `std::regex_match`). Requires a string `value` to validate and a regex `pattern`. Full match means the entire string must match the pattern, not just a partial match.

Since `regex` returns `boolean`, the test uses `formatString`'s `${regex(...)}` nesting syntax to convert the result to a string rendered in Text components, then asserts `"true"` or `"false"` via `textContent.contains`.

```json
{
  "id": "text-10a",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "${regex(value: '13812345678', pattern: '^1[3-9]\\d{9}$')}" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-10-a: Phone number regex match success -> true

| Property | Value |
|------|------|
| Component ID | `text-10a` |
| Input Parameters | `regex(value: '13812345678', pattern: '^1[3-9]\\d{9}$')` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-10-b: Phone number regex mismatch -> false

| Property | Value |
|------|------|
| Component ID | `text-10b` |
| Input Parameters | `regex(value: '12345678901', pattern: '^1[3-9]\\d{9}$')` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-10-c: Pure digit string match -> true

| Property | Value |
|------|------|
| Component ID | `text-10c` |
| Input Parameters | `regex(value: '123456', pattern: '^\\d+$')` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-10-d: Contains non-digit characters -> false

| Property | Value |
|------|------|
| Component ID | `text-10d` |
| Input Parameters | `regex(value: '123abc', pattern: '^\\d+$')` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-10-e: Email format match -> true

| Property | Value |
|------|------|
| Component ID | `text-10e` |
| Input Parameters | `regex(value: 'user@example.com', pattern: '^[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}$')` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-10-f: Email format mismatch (no @) -> false

| Property | Value |
|------|------|
| Component ID | `text-10f` |
| Input Parameters | `regex(value: 'userexample.com', pattern: '^[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}$')` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-10-g: Empty string matches empty regex -> true

| Property | Value |
|------|------|
| Component ID | `text-10g` |
| Input Parameters | `regex(value: '', pattern: '^$')` |
| Expected Result | `textContent.contains: "true"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-10"`）
2. Send `updateComponents` message; each Text component's `text` property is a `formatString` (embedding `regex`) FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains `"true"` or `"false"`
5. verify `componentCount` equals 8, `componentIds` list is complete

## Notes

- Uses `std::regex_match` (full match), not `std::regex_search` (partial match)
- Backslashes in JSON strings require double escaping: `\\d` represents regex `\d`
- `formatString` nesting syntax: `"${regex(value: ..., pattern: ...)}"` renders boolean as `"true"` or `"false"`
- When used directly in component properties, `returnType` is `"boolean"`, no `formatString` wrapping needed
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `validate_regex.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `validate_regex.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `validate_regex.json` via `FixtureRunner`, asserts `textContent`
