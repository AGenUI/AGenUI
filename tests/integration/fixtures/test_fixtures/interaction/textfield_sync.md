# textfield_sync

## Basic Info

| Property | Value |
|------|------|
| Category | Interaction Sync |
| Related Tests | SYNC-01 |
| Surface ID | `test-surf-textfield-01` |
| Protocol Version | v0.9 |

## Test Case Description

Verifies that after `TextField` component input content changes, the SDK correctly calls `submitUIDataModel` to report the new value to the engine.

Test focus:
- Data submission triggered after user types in TextField
- Reported data contains correct `surfaceId`, `componentId`, and `value` values
- TextField's `label` and `placeholder` properties render correctly

## Protocol Messages

Send 2 messages:
1. `createSurface` - Create Surface
2. `updateComponents` - Render layout with TextField

## Component Tree Structure

```
Column (root) [padding: 20px]
└── TextField (input-username) [label: "Username", placeholder: "Please enter username", value: ""]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 2 |
| Component type list | `["Column", "TextField"]` |
| All component IDs | `["root", "input-username"]` |
| `submitUIDataModel` called | `true` |
| Reported surfaceId | `"test-surf-textfield-01"` |
| Reported componentId | `"input-username"` |
| Reported data | `{ "value": "TestUser123" }` |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Stream the above 2 messages
3. Wait for Surface and component rendering to complete
4. Get the native View corresponding to TextField (EditText / UITextField)
5. Simulate text input `"TestUser123"`
6. Trigger focus change or text change event
7. Intercept `submitUIDataModel` call
8. Verify componentId and value in reported data are correct

## Notes

- This test requires a real View environment
- Android: requires spying on `ComponentEventDispatcher` or intercepting `nativeSubmitUIDataModel` JNI call
- Text change trigger timing may differ across platforms (onChange vs onEndEditing)

## Platform Coverage

- Android: Not implemented (no InteractionTest file)
- iOS: Not implemented (no InteractionTest file)
- Harmony: Not implemented (no InteractionTest file)
