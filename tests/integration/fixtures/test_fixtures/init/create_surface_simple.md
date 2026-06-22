# create_surface_simple

## Basic Info

| Property | Value |
|------|------|
| Category | Initialization |
| Related Tests | INIT-01, INIT-03, SURFACE-01, SURFACE-02 |
| Surface ID | `test-surf-init-01` |
| Protocol Version | v0.9 |

## Test Case Description

Verifies the SDK's most basic Surface creation ability: sends a `createSurface` A2UI protocol message via `receiveTextChunk`, confirms that `ISurfaceManagerListener.onCreateSurface` callback is correctly triggered.

This is the prerequisite base test case for all other tests - if Surface creation fails, subsequent component rendering, streaming, and other tests cannot proceed.

## Protocol Messages

Send 1 messages：

```json
{
  "version": "v0.9",
  "createSurface": {
    "surfaceId": "test-surf-init-01",
    "catalogId": "https://a2ui.org/specification/v0_9/basic_catalog.json"
  }
}
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| `onCreateSurface` callback triggered | `true` |
| Callback parameter `surfaceId` | `"test-surf-init-01"` |

## Test Steps

1. Create `SurfaceManager` instance
2. Register `ISurfaceManagerListener` listener
3. Call `beginTextStream()` to start streaming
4. Call `receiveTextChunk()` to send the above JSON string
5. Call `endTextStream()` to end streaming
6. Verify listener's `onCreateSurface(surfaceId, messageId)` was called
7. Verify callback parameter `surfaceId == "test-surf-init-01"`

## Platform Coverage

- Android: `SurfaceManagerTest.INIT-03`
- iOS: `InitializationTest.testCreateSurfaceCallback`
- Harmony: `InitializationTest.INIT-03_createSurface_callback_triggered`
