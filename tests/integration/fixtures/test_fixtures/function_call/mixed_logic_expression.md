# mixed_logic_expression

## Basic Info

| Property | Value |
|------|------|
| Category | Mixed Scenario / LogicExpression + FunctionCall |
| Related Tests | SCENE-03 |
| Core Mechanism | Combines multiple validation functions via `&&`/`\|\|`/`!` logic expressions in `formatString`'s `args.value` template |
| Return Type | `string` (boolean results converted to string via formatString) |
| surfaceId | `test-surf-scene-03` |

## Test Case Description

Verifies `formatString`'s ability to combine logic expressions (`&&`, `||`, `!`) with multiple validation functions (`required`, `email`, `length`, `numeric`, `regex`). Commonly used to build complex form validation logic and render results to Text components.

The test first injects context data via `updateDataModel`, then binds `formatString` FunctionCall with logic expressions to Text components' `text` property via `updateComponents`. After engine rendering, it asserts `textContent` contains expected results.

**Core Mechanism**:
- `&&`: Returns `true` only when all sub-conditions are `true` (short-circuit evaluation)
- `||`: Returns `true` when any sub-condition is `true` (short-circuit evaluation)
- `!`: Negates a single condition
- Logic expressions are embedded in `formatString`'s `args.value` template using `${expr}` syntax

```json
{
  "id": "text-s3a",
  "component": "Text",
  "text": {
    "call": "formatString",
    "args": { "value": "${required(value: ${/form/email}) && email(value: ${/form/email})}" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SCENE-03-a: AND logic, both validations pass -> true

| Property | Value |
|------|------|
| Component ID | `text-s3a` |
| Context | `{ "form": { "email": "user@example.com" } }` |
| Logic | `required(/form/email) && email(/form/email)` |
| Expected Result | `textContent.contains: "true"` |

### SCENE-03-b: AND logic, one validation fails -> false

| Property | Value |
|------|------|
| Component ID | `text-s3b` |
| Context | `{ "form": { "invalidEmail": "invalid-email" } }` |
| Logic | `required(/form/invalidEmail) && email(/form/invalidEmail)` |
| Expected Result | `textContent.contains: "false"` (invalid email format, AND short-circuits to false) |

### SCENE-03-c: OR logic, at least one passes -> true

| Property | Value |
|------|------|
| Component ID | `text-s3c` |
| Context | `{ "form": { "val": "user@example.com" } }` |
| Logic | `email(/form/val) \|\| regex(/form/val, '^\\d+$')` |
| Expected Result | `textContent.contains: "true"` (email branch is true, OR evaluates to true) |

### SCENE-03-d: NOT logic, negates false -> true

| Property | Value |
|------|------|
| Component ID | `text-s3d` |
| Context | `{ "form": { "emptyVal": "" } }` |
| Logic | `!required(/form/emptyVal)` |
| Expected Result | `textContent.contains: "true"` (empty string required=false, NOT -> true) |

### SCENE-03-e: Three-condition AND, valid username -> true

| Property | Value |
|------|------|
| Component ID | `text-s3e` |
| Context | `{ "form": { "username": "Alice" } }` |
| Logic | `required(/form/username) && length(min:2, max:20) && regex('^[a-zA-Z]+$')` |
| Expected Result | `textContent.contains: "true"` |

### SCENE-03-f: Compound nesting, AND combined with OR

| Property | Value |
|------|------|
| Component ID | `text-s3f` |
| Context | `{ "form": { "age": "25" } }` |
| Logic | `(required && numeric(min:0, max:150)) \|\| (required && email)` |
| Expected Result | `textContent.contains: "true"` (age=25 satisfies first branch) |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-scene-03"`）
2. Send `updateDataModel` message to inject context data (`form` field)
3. Send `updateComponents` message; each Text component's `text` property is a `formatString` FunctionCall with logic expressions
4. Wait for engine rendering to complete, get each component's `textContent`
5. assert each component `textContent` contains `"true"` or `"false"`
6. verify `componentCount` equals 7, `componentIds` list is complete

## Notes

- `&&`/`||`/`!` logic expressions are embedded in `formatString`'s `args.value` template string using `${...}` syntax
- Short-circuit evaluation: `&&` stops at first `false`, `||` stops at first `true`
- Most common usage: render multi-condition validation results to Text components, or bind directly to boolean properties like `Button.enabled`

## Platform Coverage

- Android: Replays `mixed_logic_expression.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `mixed_logic_expression.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `mixed_logic_expression.json` via `FixtureRunner`, asserts `textContent`
