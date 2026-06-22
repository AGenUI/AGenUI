# mixed_func_in_action

## Basic Info

| Property | Value |
|------|------|
| Category | Mixed Scenario / FunctionCall Used in Action/Component Props |
| Related Tests | SCENE-04 |
| Core Mechanism | `FunctionCall` as dynamic value source for component properties (`Text.text`, `Button.enabled`) or Action parameters (`toast.message`) |
| Return Type | `string` / `boolean` |
| surfaceId | `test-surf-scene-04` |

## Test Case Description

Verifies `FunctionCall` capability when mounted to component properties and Action parameters. When property types are `DynamicString` or `DynamicBoolean`, a `FunctionCall` object can directly replace a static literal.

The test first injects context data via `updateDataModel`, then configures Text (`text` property) and Button (`enabled` property) components via `updateComponents`. After engine rendering, it asserts `textContent` and `componentProps`.

**Core Mechanism**:
- `DynamicString` properties (e.g., `Text.text`, `toast.message`): can be string literals, `DataBinding`, or `FunctionCall` returning `string`
- `DynamicBoolean` properties (e.g., `Button.enabled`): can be boolean literals, `DataBinding`, `FunctionCall`, or logic expressions
- Action dynamic parameters (e.g., `toast`) also support `FunctionCall` as values

```json
{
  "id": "btn-s4g",
  "component": "Button",
  "enabled": {
    "call": "email",
    "args": { "value": { "path": "/form/validEmail" } },
    "returnType": "boolean"
  },
  "action": { "functionCall": { "call": "toast", "args": { "message": "Form submitted!", "duration": 2 } } }
}
```

## Sub-cases

### SCENE-04-a: Text.text uses formatString FunctionCall

| Property | Value |
|------|------|
| Component ID | `text-s4a` |
| Scenario | Text component displays static welcome message |
| Function | `formatString(value: "Welcome, Alice!")` |
| Expected Result | `textContent.contains: "Welcome, Alice!"` |

### SCENE-04-b: Text.text uses formatDate FunctionCall + DataBinding

| Property | Value |
|------|------|
| Component ID | `text-s4b` |
| Scenario | Text component displays activity deadline from context |
| Context | `{ "activity": { "deadline": "2025-12-31" } }` |
| Function | `formatDate(value: /activity/deadline, format: "yyyy/MM/dd")` |
| Expected Result | `textContent.contains: "2025/12/31"` |

### SCENE-04-c: Text.text uses formatCurrency FunctionCall + DataBinding

| Property | Value |
|------|------|
| Component ID | `text-s4c` |
| Scenario | Text component displays product price in USD |
| Context | `{ "product": { "price": 1299.0 } }` |
| Function | `formatCurrency(value: /product/price, currency: "USD")` |
| Expected Result | `textContent.contains: "USD 1,299.00"` |

### SCENE-04-d: Text.text uses pluralize FunctionCall + DataBinding

| Property | Value |
|------|------|
| Component ID | `text-s4d` |
| Scenario | Text displays cart item count, shows singular form when count=1 |
| Context | `{ "cart": { "count": 1 } }` |
| Function | `pluralize(value: /cart/count, one: "item in cart", other: "items in cart")` |
| Expected Result | `textContent.contains: "item in cart"` |

### SCENE-04-e: Text.text uses formatNumber FunctionCall + DataBinding

| Property | Value |
|------|------|
| Component ID | `text-s4e` |
| Scenario | Text displays user score |
| Context | `{ "user": { "score": 98765 } }` |
| Function | `formatNumber(value: /user/score, decimals: 0)` |
| Expected Result | `textContent.contains: "98,765"` |

### SCENE-04-f: Button.enabled binds email FunctionCall (valid email -> button enabled)

| Property | Value |
|------|------|
| Component ID | `btn-s4f` |
| Scenario | Submit button is clickable only when email is valid |
| Context | `{ "form": { "validEmail": "valid@test.com" } }` |
| Function | `email(value: /form/validEmail)` |
| Expected Result | `componentProps.enabled: true` |

### SCENE-04-g: Button.enabled binds email FunctionCall (invalid email -> button disabled)

| Property | Value |
|------|------|
| Component ID | `btn-s4g` |
| Scenario | Submit button is disabled when email is invalid |
| Context | `{ "form": { "invalidEmail": "bad-email" } }` |
| Function | `email(value: /form/invalidEmail)` |
| Expected Result | `componentProps.enabled: false` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-scene-04"`）
2. Send `updateDataModel` message to inject context data (`activity`, `product`, `cart`, `user`, `form` fields)
3. Send `updateComponents` message to configure Text (`text` FunctionCall) and Button (`enabled` FunctionCall) components
4. Wait for engine rendering to complete
5. Assert Text component's `textContent` contains expected string
6. Assert Button component's `enabled` property value (`true`/`false`)
7. verify `componentCount` equals 8, `componentIds` list is complete

## Notes

- `Text.text` is `DynamicString`; FunctionCall's `returnType` must be `"string"`
- `Button.enabled` is `DynamicBoolean`; FunctionCall's `returnType` must be `"boolean"`
- `toast` action's `message` field is `DynamicString`, supports `FunctionCall`
- FunctionCall's `returnType` must match the property's expected type (string/boolean)
- Combining `DataBinding` with logic expressions enables complete form validation and dynamic UI scenarios

## Platform Coverage

- Android: Replays `mixed_func_in_action.json` via `FixtureRunner`, asserts `textContent` and `componentProps`
- iOS: Replays `mixed_func_in_action.json` via `FixtureRunner`, asserts `textContent` and `componentProps`
- Harmony: Replays `mixed_func_in_action.json` via `FixtureRunner`, asserts `textContent` and `componentProps`
