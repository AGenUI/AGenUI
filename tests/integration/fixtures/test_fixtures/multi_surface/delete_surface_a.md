# delete_surface_a

## Basic Info

| Property | Value |
|------|------|
| Category | Multi-Surface Isolation |
| Related Tests | MULTI-03 |
| Surface ID | `test-surf-multi-a` |
| Protocol Version | v0.9 |
| Paired File | `surface_a.json`, `surface_b.json` |

## Test Case Description

Deletion step data for multi-Surface isolation test: after creating two Surfaces (A and B) in the same `SurfaceManager` instance, sends this fixture's `deleteSurface` message to destroy Surface A, verifying that Surface B's component tree is not affected.

## Protocol Messages

Send 1 messages：

```json
{
  "version": "v0.9",
  "deleteSurface": {
    "surfaceId": "test-surf-multi-a"
  }
}
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| `onDeleteSurface` callback triggered | `true` |
| Surface A destroyed | state becomes destroyed or getSurface returns null |
| Surface B still exists | `getSurface(surfaceIdB)` is non-null |
| Surface B component count | 3 (consistent with `surface_b.json` expected) |
| Surface B's `text-b1` component | still exists and queryable |

## Usage

This file should be used together with `surface_a.json` and `surface_b.json`:

1. Create a single `SurfaceManager` instance
2. Send `surface_a.json` message sequence first (Create Surface A)
3. Then send `surface_b.json` message sequence (Create Surface B)
4. Send this fixture's `deleteSurface` message to destroy Surface A
5. Verify Surface A is destroyed
6. Verify Surface B's component tree is intact

## Platform Coverage

- Android: `MultiSurfaceTest.testMULTI03_deleteSurfaceADoesNotAffectSurfaceB`
- iOS: `MultiSurfaceTest.testMULTI03_deleteSurfaceADoesNotAffectSurfaceB`
- Harmony: `MultiSurfaceTest.MULTI-03_delete_surfaceA_does_not_affect_surfaceB`
