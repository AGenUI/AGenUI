# checkbox_sync

## Basic Info

| Property | Value |
|------|------|
| Category | Interaction Sync |
| Related Tests | SYNC-02 |
| Surface ID | `test-surf-checkbox-01` |
| Protocol Version | v0.9 |

## Test Case Description

Verifies that after `CheckBox` component state changes, the SDK correctly calls `submitUIDataModel` to report the new state to the engine.

Test focus:
- Data submission triggered when CheckBox is toggled from `false` to `true`
- Data submission triggered when CheckBox is toggled from `true` to `false`
- Reported data contains correct `surfaceId`, `componentId`, and `checked` values

## Protocol Messages

Send 2 messages:
1. `createSurface` - Create Surface
2. `updateComponents` - Render layout with CheckBox

## Component Tree Structure

```
Column (root) [padding: 20px]
└── CheckBox (cb-agree) - "I agree to the terms and conditions" [value: false]
```

## Sub-cases

### SYNC-02-a: CheckBox toggled from false to true

| Property | Value |
|------|------|
| Initial Value | `false` |
| Toggle To | `true` |
| Reported surfaceId | `"test-surf-checkbox-01"` |
| Reported componentId | `"cb-agree"` |
| Reported data | `{ "checked": true }` |

### SYNC-02-b: CheckBox toggled from true to false

| Property | Value |
|------|------|
| Initial Value | `true` |
| Toggle To | `false` |
| Reported surfaceId | `"test-surf-checkbox-01"` |
| Reported componentId | `"cb-agree"` |
| Reported data | `{ "checked": false }` |

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 2 |
| Component type list | `["Column", "CheckBox"]` |
| All component IDs | `["root", "cb-agree"]` |
| `submitUIDataModel` called | `true` |
| `checked` field in reported data | Consistent with post-operation state |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Stream the above 2 messages
3. Wait for Surface and component rendering to complete
4. Get the native View corresponding to CheckBox
5. Simulate toggle operation (performClick / setChecked)
6. Intercept `submitUIDataModel` call
7. Verify componentId and checked values in reported data are correct

## Notes

- This test requires a real View environment（Android: ActivityScenario with real View hierarchy）
- Requires spy/intercept of `ComponentEventDispatcher.submitUIDataModel()` or intercepting `nativeSubmitUIDataModel` JNI call
- The two sub-cases must be executed separately; cannot toggle consecutively in the same instance

## Platform Coverage

- Android: Not implemented (no InteractionTest file)
- iOS: Not implemented (no InteractionTest file)
- Harmony: Not implemented (no InteractionTest file)
