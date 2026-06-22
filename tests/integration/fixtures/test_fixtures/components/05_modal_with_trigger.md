# 05_modal_with_trigger

## Basic Info

| Property | Value |
|------|------|
| Category | Component Render |
| Related Tests | COMP-05 |
| Surface ID | `test-surf-modal-01` |
| Protocol Version | v0.9 |

## Test Case Description

Modal with trigger test case: verifies that the `Modal` component's `trigger` and `content` reference relationships are correctly parsed by the SDK.

Test focus:
- Modal component references trigger button via `trigger` field
- Modal component references popup content via `content` field
- `visible: false` initially hidden state
- Inter-component references without parent-child relationship (trigger/content are sibling references)

## Protocol Messages

Send 2 messages, including an `updateComponents` message with 5 components.

## Component Tree Structure

```
Column (root) [padding: 20px]
├── Button (trigger-btn) [300x80, border-radius: 12px]
│   └── Text (trigger-btn-text) - "Open Modal"
└── Modal (modal-dialog) [trigger: "trigger-btn", content: "modal-body", visible: false]
    └── Text (modal-body) - "This is modal content for testing." [body]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 5 |
| Component type list | `["Column", "Button", "Text", "Modal", "Text"]` |
| Root component ID | `"root"` |
| All component IDs | `["root", "trigger-btn", "trigger-btn-text", "modal-dialog", "modal-body"]` |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Stream the above 2 messages
3. Wait for `onCreateSurface` to get Surface
4. Get component tree, verify total components is 5
5. Verify Modal component's `trigger` property references `"trigger-btn"`
6. Verify Modal component's `content` property references `"modal-body"`

## Design Intent

Modal is a special component type that does not use the conventional `child`/`children` parent-child references, but instead references other components via `trigger` and `content` properties. This test case ensures the SDK can correctly handle this non-standard reference pattern.

## Platform Coverage

- Android: `ComponentRenderTest.COMP-05`
- iOS: `ComponentRenderTest.testModalWithTriggerRender`
- Harmony: `ComponentRenderTest.COMP-05_modal_with_trigger_render`
