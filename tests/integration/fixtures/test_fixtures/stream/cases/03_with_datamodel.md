# 03_with_datamodel

## Basic Info

| Property | Value |
|------|------|
| Category | Stream |
| Related Tests | STREAM-06 |
| Surface ID | `test-surf-stream-03` |
| Protocol Version | v0.9 |

## Test Case Description

Complete streaming test case: verifies the full streaming transmission of three A2UI protocol messages (`createSurface` + `updateComponents` + `updateDataModel`). Tests the SDK's streaming parsing capability for DataModel messages.

## Chunking Strategy Sub-cases

| Sub-case ID | Description | chunkSize | Notes |
|-----------|------|-----------|------|
| STREAM-03-dm-small | Small chunk with DataModel | 10  bytes | Three messages concatenated then chunked |
| STREAM-03-dm-full | Single send of all three messages | json.length | Baseline comparison |

## Payload (Complete Message Sequence)

Contains 3 A2UI protocol messages:

**Message 1 - createSurface：**
```json
{"version":"v0.9","createSurface":{"surfaceId":"test-surf-stream-03","catalogId":"...","sendDataModel":false}}
```

**Message 2 - updateComponents：**
```json
{"version":"v0.9","updateComponents":{"surfaceId":"test-surf-stream-03","components":[
  {"id":"root","component":"Column","children":["dm-title","dm-value-text"],...},
  {"id":"dm-title","component":"Text","text":"DataModel Test","variant":"h2"},
  {"id":"dm-value-text","component":"Text","text":"${/user/name}","variant":"body"}
]}}
```

**Message 3 - updateDataModel：**
```json
{"version":"v0.9","updateDataModel":{"surfaceId":"test-surf-stream-03","value":{"user":{"name":"Alice","age":28}}}}
```

## Component Tree Structure

```
Column (root) [padding: 20px]
├── Text (dm-title) - "DataModel Test" [h2]
└── Text (dm-value-text) - "${/user/name}" → "Alice" [body, DataBinding]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 3 |
| Component type list | `["Column", "Text", "Text"]` |
| All component IDs | `["root", "dm-title", "dm-value-text"]` |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Concatenate 3 JSON messages into a single string
3. Stream in chunks by chunkSize
4. `endTextStream()` to end
5. Verify `onCreateSurface` fires
6. Verify component tree contains 3 components
7. (Optional) Verify `dm-value-text` display text is `"Alice"` after DataModel binding

## Design Intent

- Verifies the SDK's streaming JSON parser handles 3 consecutive JSON messages (concatenated without separators)
- Verifies correctness of `updateDataModel` messages under streaming
- `sendDataModel: false` ensures DataModel is not sent automatically with createSurface; requires explicit updateDataModel message

## Platform Coverage

- Android: `StreamTest.testSTREAM06_withDataModel_chunkSize20`
- iOS: `StreamTest.testSTREAM06_withDataModelSmallChunk`
- Harmony: `StreamTest.STREAM-06_dataModel_small_chunk`
