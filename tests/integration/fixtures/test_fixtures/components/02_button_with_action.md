# 02_button_with_action

## Basic Info

| Property | Value |
|------|------|
| Category | Component Render |
| Related Tests | COMP-02, SURFACE-03 |
| Surface ID | `test-surf-btn-01` |
| Protocol Version | v0.9 |

## Test Case Description

Button with Action test case: verifies that after binding a `functionCall` type Action to a `Button` component, the SDK correctly parses the Action configuration.

Test focus:
- Column → Button → Text parent-child relationships correctly established
- Button's `action.functionCall` configuration is correctly parsed
- Action's `call` name and `args` parameters are correctly accessible
- Button component supports `child` (single child) reference

## Protocol Messages

Send 2 messages：

**Message 1 - Create Surface：**
```json
{
  "version": "v0.9",
  "createSurface": {
    "surfaceId": "test-surf-btn-01",
    "catalogId": "https://a2ui.org/specification/v0_9/basic_catalog.json"
  }
}
```

**Message 2 - Update Components：**
```json
{
  "version": "v0.9",
  "updateComponents": {
    "surfaceId": "test-surf-btn-01",
    "components": [
      { "id": "root", "component": "Column", "children": ["btn-submit"], "align": "stretch", "styles": { "padding": "20px" } },
      { "id": "btn-submit", "component": "Button", "child": "btn-label", "styles": { "width": "670px", "height": "80px", "padding": "18px 24px", "border-radius": "16px" }, "action": { "functionCall": { "call": "toast", "args": { "message": "Operation successful", "duration": 3 } } } },
      { "id": "btn-label", "component": "Text", "text": "Submit", "styles": { "font-size": "32px", "text-align": "center", "color": "#FFFFFF" } }
    ]
  }
}
```

## Component Tree Structure

```
Column (root)
└── Button (btn-submit) [action: toast("Operation successful")]
    └── Text (btn-label) - "Submit"
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 3 |
| Component type list | `["Column", "Button", "Text"]` |
| Root component ID | `"root"` |
| All component IDs | `["root", "btn-submit", "btn-label"]` |
| `btn-submit` Action type | `"functionCall"` |
| `btn-submit` Action call | `"toast"` |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Stream the above 2 messages
3. Wait for `onCreateSurface` to get Surface
4. Get component tree, verify structure and count
5. Get `btn-submit` component, verify Action configuration exists
6. Verify Action's `call` is `"toast"`

## Platform Coverage

- Android: `ComponentRenderTest.COMP-02`
- iOS: `ComponentRenderTest.testButtonWithActionRender`
- Harmony: `ComponentRenderTest.COMP-02_button_with_action_render`
