# action_toast

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / Platform Skill |
| Related Tests | SKILL-03, SKILL-04, SKILL-05 |
| Skill Name | `toast` |
| Skill Type | `platform` (registered by host App) |
| Surface ID | `test-surf-toast-01` |
| Protocol Version | v0.9 |

## Test Case Description

Verifies that platform-registered Skills (via `IFunction` interface) can be correctly routed and invoked by the SDK. Unlike C++ built-in Skills, `toast` is a platform callback function registered by the host App via `registerFunction`.

Test focus:
- After host App registers `IFunction("toast")`, clicking Button triggers Action and callback is correctly executed
- Callback parameters (`message`, `duration`) are correctly passed
- End-to-end flow of component rendering and Action binding

## Protocol Messages

Send 2 messages:
1. `createSurface` - Create Surface
2. `updateComponents` - Render Button with Action

## Component Tree Structure

```
Column (root)
└── Button (toast-btn) [action: toast({message: "Operation successful", duration: 3})]
    └── Text (toast-btn-text) - "Trigger Toast"
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 3 |
| All component IDs | `["root", "toast-btn", "toast-btn-text"]` |
| Platform callback function name | `"toast"` |
| Callback parameter `message` | `"Operation successful"` |

## Test Steps

1. Create `SurfaceManager`
2. Register `IFunction("toast")` callback interceptor
3. Stream the above 2 messages, render component tree with Button
4. Wait for Surface creation to complete
5. Simulate clicking `toast-btn` component (`performClick`)
6. Verify `IFunction.execute()` was called
7. Verify passed parameters contain `message: "Operation successful"`

## Notes

- This test requires a real View environment（Android: ActivityScenario; iOS: XCUITest）
- IFunction must be registered before sending protocol messages
- Difference between Platform Skill and C++ Skill: Platform Skills are registered by the host App via `registerFunction` API

## Platform Coverage

- Android: Not implemented (Android SkillTest only contains @Ignore cases for formatDate/formatString)
- iOS: `SkillTest.testSKILL03_platformFunctionCallbackTriggered` (known limitation: Action routing depends on UI-layer click events, callback may timeout)
- Harmony: Not implemented (Harmony SkillTest.SKILL-03 is an execute error test, not toast callback)
