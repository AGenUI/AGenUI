# 01_split_at_key_boundary

## Basic Info

| Property | Value |
|------|------|
| Category | Stream / Static Chunks |
| Related Tests | STREAM-07 |
| Surface ID | `test-surf-split-01` |
| Chunking Strategy | split in the middle of a JSON key name |
| Total Chunks | 2 |

## Test Case Description

Pre-built static chunking scenario: manually splits data at JSON key name boundaries to verify the SDK's streaming parser correctly handles JSON key tokens spanning chunks.

Unlike dynamic chunking (automatically split by chunkSize), this test's split points are carefully designed  —  —precisely at `"surfaceId"` and `"updateComponents"` and other key names, testing the parser's ability to handle tokens crossing chunk boundaries.

## File Structure

```
01_split_at_key_boundary/
├── meta.json      # Metadata: surfaceId, chunking strategy, expected results
├── chunk_01.txt   # First chunk (split at key boundary)
└── chunk_02.txt   # Second chunk (remaining content)
```

## Chunk Contents

**chunk_01.txt（201 bytes):**
```
{"version":"v0.9","createSurface":{"surfaceId":"test-surf-split-01","catalogId":"https://a2ui.org/specification/v0_9/basic_catalog.json"}}{"version":"v0.9","updateComponents":{"surf
```
Note: splits at the `"surf` position of `"surfaceId"`, i.e., in the middle of a key's value string.

**chunk_02.txt（208 bytes):**
```
aceId":"test-surf-split-01","components":[{"id":"root","component":"Column","children":["split-text"],"align":"stretch"},{"id":"split-text","component":"Text","text":"Split boundary test","variant":"body"}]}}
```

## Component Tree Structure

```
Column (root)
└── Text (split-text) - "Split boundary test" [body]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 2 |
| All component IDs | `["root", "split-text"]` |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Call `beginTextStream()` to start stream
3. Call `receiveTextChunk(chunk_01 content)` to send first chunk
4. Call `receiveTextChunk(chunk_02 content)` to send second chunk
5. Call `endTextStream()` to end stream
6. Verify `onCreateSurface` callback fires
7. Verify final component tree matches the result of sending complete JSON at once

## Design Intent

This is a manually crafted boundary test to ensure:
- Parser does not error when JSON string tokens span chunks
- Completeness after chunk reassembly is not affected by split position
- Complements automatic chunking tests: automatic chunking covers random positions, this test covers known high-risk boundaries

## Platform Coverage

- Android: `StreamTest.testSTREAM07_staticChunks_keyBoundary`
- iOS: `StreamTest.testSTREAM07_staticChunksKeyBoundary`
- Harmony: `StreamTest.STREAM-07_static_chunks_key_boundary`
