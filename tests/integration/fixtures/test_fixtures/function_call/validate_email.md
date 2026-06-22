# validate_email

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Validation Function |
| Related Tests | SKILL-11 |
| Function Name | `email` |
| Return Type | `boolean` |
| Invocation | `formatString` embeds `${email(...)}` to convert boolean to string rendered in `Text.text` |
| surfaceId | `test-surf-fc-11` |

## Test Case Description

Verifies `email`'s email address format validation. Requires a string `value` to validate; the engine internally uses regex to verify email format and returns validity (`true`/`false`).

Since `email` returns `boolean`, the test uses `formatString`'s `${email(...)}` nesting syntax to convert the result to a string rendered in Text components, then asserts `"true"` or `"false"` via `textContent.contains`.

```json
{
  "id": "text-11a",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "${email(value: 'user@example.com')}" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-11-a: Valid email address -> true

| Property | Value |
|------|------|
| Component ID | `text-11a` |
| Input Parameters | `email(value: 'user@example.com')` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-11-b: Valid email with subdomain -> true

| Property | Value |
|------|------|
| Component ID | `text-11b` |
| Input Parameters | `email(value: 'user.name+tag@sub.domain.org')` |
| Expected Result | `textContent.contains: "true"` |

### SKILL-11-c: Missing @ symbol -> false

| Property | Value |
|------|------|
| Component ID | `text-11c` |
| Input Parameters | `email(value: 'userexample.com')` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-11-d: Missing domain part -> false

| Property | Value |
|------|------|
| Component ID | `text-11d` |
| Input Parameters | `email(value: 'user@')` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-11-e: Missing top-level domain suffix -> false

| Property | Value |
|------|------|
| Component ID | `text-11e` |
| Input Parameters | `email(value: 'user@example')` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-11-f: Empty string -> false

| Property | Value |
|------|------|
| Component ID | `text-11f` |
| Input Parameters | `email(value: '')` |
| Expected Result | `textContent.contains: "false"` |

### SKILL-11-g: Contains space -> false

| Property | Value |
|------|------|
| Component ID | `text-11g` |
| Input Parameters | `email(value: 'user @example.com')` |
| Expected Result | `textContent.contains: "false"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-11"`）
2. Send `updateComponents` message; each Text component's `text` property is a `formatString` (embedding `email`) FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains `"true"` or `"false"`
5. verify `componentCount` equals 8, `componentIds` list is complete

## Rendering Integration Usage

```json
{
  "id": "submit-btn",
  "component": "Button",
  "enabled": {
    "call": "email",
    "args": { "value": { "path": "/form/email" } },
    "returnType": "boolean"
  }
}
```

## Notes

- `email` function is a wrapper around `regex` with a built-in email pattern; no external pattern needed
- Validation uses full match (entire format must conform to RFC-style email specification)
- `formatString` nesting syntax: `"${email(value: ...)}"` renders boolean as `"true"` or `"false"`
- When used directly in component properties (`Button.enabled`), `returnType` is `"boolean"`, no `formatString` wrapping needed
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `validate_email.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `validate_email.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `validate_email.json` via `FixtureRunner`, asserts `textContent`
