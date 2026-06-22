# 02_nested_layout

## Basic Info

| Property | Value |
|------|------|
| Category | Stream |
| Related Tests | STREAM-05, STREAM-08 |
| Surface ID | `test-surf-stream-02` |
| Protocol Version | v0.9 |

## Test Case Description

Advanced streaming test case: uses a nested layout (Column containing Card and multiple Text) as payload to verify the integrity of complex component structures after chunked transmission. Also serves as the data source for the **Mid-stream reset** scenario (STREAM-08), testing `beginTextStream()` reset behavior.

## Chunking Strategy Sub-cases

| Sub-case ID | Description | chunkSize | Special Behavior |
|-----------|------|-----------|----------|
| STREAM-05 | Nested layout + small chunk | 10~50  bytes | None |
| STREAM-08 | Mid-stream reset | 10~20  bytes | Calls `beginTextStream()` after sending 50 bytes to restart |

## Payload (Complete Message Sequence)

Contains 2 messages with 6 components total:

```json
{"version":"v0.9","createSurface":{"surfaceId":"test-surf-stream-02",...}}
{"version":"v0.9","updateComponents":{"surfaceId":"test-surf-stream-02","components":[6 components]}}
```

## Component Tree Structure

```
Column (root) [padding: 16px, gap: 12px]
├── Card (layout-card) [padding: 20px, border-radius: 12px]
│   └── Column (card-content)
│       ├── Text (card-title) - "Nested Layout Card" [h2]
│       └── Text (card-desc) - "This card tests nested layout streaming." [body]
└── Text (layout-footer) - "Footer text below card" [caption]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 6 |
| Component type list | `["Column", "Card", "Column", "Text", "Text", "Text"]` |
| All component IDs | `["root", "layout-card", "card-content", "card-title", "card-desc", "layout-footer"]` |

## Mid-stream Reset Scenario (STREAM-08)

Test flow:
1. `beginTextStream()` — start first transmission
2. Send first 50 bytes as chunk (JSON is incomplete at this point)
3. `beginTextStream()` — **Mid-stream reset**, discards previously incomplete data
4. Resend complete payload from beginning
5. `endTextStream()` — end
6. Verify final component tree is correct (only the last complete transmission takes effect)

This scenario simulates network reconnection or server re-sending.

## Design Intent

- Verifies completeness of complex nested structures (Card → Column → Text) under streaming
- Verifies `beginTextStream()` reset semantics: discards previously cached incomplete data
- Ensures SDK does not produce incorrect component trees from residual partial data

## Platform Coverage

- Android: `StreamTest.testSTREAM05_nestedLayout_chunkSize50` / `testSTREAM08_resetMidStream`
- iOS: `StreamTest.testSTREAM05_nestedLayoutLargeChunk` / `testSTREAM08_resetMidStreamProducesCorrectResult`
- Harmony: `StreamTest.STREAM-05_nested_layout_small_chunk` / `STREAM-08_midway_reset_and_resend`
