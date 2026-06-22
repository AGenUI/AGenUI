# 01_button_simple

## Basic Info

| Property | Value |
|------|------|
| Category | Stream |
| Related Tests | STREAM-01, STREAM-02, STREAM-03, STREAM-04 |
| Surface ID | `test-surf-stream-01` |
| Protocol Version | v0.9 |

## Test Case Description

Basic streaming test case: uses a simple Button component as payload to verify that under different `chunkSize` chunking strategies, the SDK's streaming parser correctly processes chunked data, producing a component tree identical to the single-send complete JSON result.

## Chunking Strategy Sub-cases

| Sub-case ID | Description | chunkSize | Notes |
|-----------|------|-----------|------|
| STREAM-01 | Small chunk streaming | 10  bytes | High-frequency slicing, simulates slow network |
| STREAM-02 | Large chunk streaming | 500  bytes | Low-frequency slicing, simulates fast network |
| STREAM-03 | Single character streaming | 1  bytes | Extreme scenario, transmits only 1 character at a time |
| STREAM-04 | Single send | json.length | No chunking, serves as baseline comparison |

## Payload (Complete Message Sequence)

JSON string containing 2 A2UI protocol messages (concatenated):

```json
{"version":"v0.9","createSurface":{"surfaceId":"test-surf-stream-01",...}}
{"version":"v0.9","updateComponents":{"surfaceId":"test-surf-stream-01","components":[...]}}
```

## Component Tree Structure

```
Column (root)
└── Button (stream-btn) [auto x 80px, border-radius: 12px]
    └── Text (stream-btn-text) - "Stream Button" [28px, center]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 3 |
| Component type list | `["Column", "Button", "Text"]` |
| All component IDs | `["root", "stream-btn", "stream-btn-text"]` |

## Test Steps (Per Sub-case)

1. Create `SurfaceManager` and register listener
2. Serialize payload to single string (two JSON messages concatenated)
3. Call `beginTextStream()` to start streaming
4. Split string into multiple fragments by `chunkSize`
5. Call `receiveTextChunk(chunk)` sequentially to send each fragment
6. Call `endTextStream()` to end streaming
7. Wait for `onCreateSurface` callback
8. Get component tree, verify it matches expected result exactly

## Design Intent

Verifies that the SDK's internal JSON streaming parser correctly handles:
- Splitting at any byte position (including middle of JSON key/value)
- Accumulated concatenation in single-character extreme scenario
- Multiple JSON messages in concatenated format without separators

## Platform Coverage

- Android: `StreamTest.testSTREAM01/02/03/04`
- iOS: `StreamTest.testSTREAM01/02/03/04`
- Harmony: `StreamTest.STREAM-01/02/03/04`
