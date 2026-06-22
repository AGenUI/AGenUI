# delete_surface_simple

## Basic Info

| Property | Value |
|------|------|
| Category | Initialization |
| Related Tests | SURFACE-02 |
| Surface ID | `test-surf-init-01` |
| Protocol Version | v0.9 |
| Prerequisite | `create_surface_simple.json` |

## Test Case Description

Verifies the SDK's Surface destruction capability: sends an A2UI protocol message containing `deleteSurface` via `receiveTextChunk`, and confirms that the `ISurfaceManagerListener.onDeleteSurface` callback is correctly triggered.

This test case requires first using `create_surface_simple.json` to Create Surface, then sending this fixture's `deleteSurface` message to destroy it.

## Protocol Messages

Send 1 messages：

```json
{
  "version": "v0.9",
  "deleteSurface": {
    "surfaceId": "test-surf-init-01"
  }
}
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| `onDeleteSurface` callback triggered | `true` |
| Callback parameter `surfaceId` | `"test-surf-init-01"` |
| Surface state (iOS) | `.destroyed` |
| `getSurface()` return value (Android/Harmony) | `null` |

## Test Steps

1. Create `SurfaceManager` instance
2. Register `ISurfaceManagerListener` listener
3. Use `create_surface_simple.json` to Create Surface (prerequisite step)
4. Call `beginTextStream()` to start streaming
5. Call `receiveTextChunk()` to send the above `deleteSurface` JSON string
6. Call `endTextStream()` to end streaming
7. Verify listener's `onDeleteSurface` was called
8. Verify Surface state change or reference cleanup

## Platform Coverage

- Android: `SurfaceLifecycleTest.testSURFACE02_deleteSurfaceCallbackAndGetSurfaceNull`
- iOS: `SurfaceLifecycleTest.testSURFACE02_deleteSurfaceCallbackTriggered`
- Harmony: `SurfaceLifecycleTest.SURFACE-02_onDeleteSurface_callback`

## Platform Verification Differences

| Platform | Verification Method |
|------|----------|
| iOS | Assert `Surface.state == .destroyed` |
| Android | Assert `getSurface(surfaceId) == null` |
| Harmony | Assert `getSurface(surfaceId) == null` |

> Note: iOS currently fails the `.destroyed` assertion because the SDK's `SurfaceManager.onDeleteSurface` does not call `surface.destroy()` (known SDK defect), which does not affect fixture data correctness.
