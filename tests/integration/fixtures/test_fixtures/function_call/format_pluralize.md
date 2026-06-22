# format_pluralize

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Built-in Function |
| Related Tests | SKILL-05 |
| Function Name | `pluralize` |
| Invocation | `Text.text` FunctionCall property binding |
| surfaceId | `test-surf-fc-05` |

## Test Case Description

Verifies `pluralize`'s pluralization string return functionality. Matches `zero` / `one` / `two` / `other` branches based on `value`'s integer value, falling back to `other` when unmatched. Parameters:
- `value` (required): Count value (floats are rounded to integer)
- `zero` (optional): String when count is 0
- `one` (optional): String when count is 1
- `two` (optional): String when count is 2
- `other` (required): Default/fallback string

The test binds `pluralize` FunctionCall to Text components' `text` property via `updateComponents`. After engine rendering, it asserts `textContent` contains the expected string.

```json
{
  "id": "text-05a",
  "component": "Text",
  "text": {
    "call": "pluralize",
    "args": { "value": 0, "zero": "No items", "one": "1 item", "other": "items" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-05-a: count=0, uses zero branch

| Property | Value |
|------|------|
| Component ID | `text-05a` |
| Input Parameters | `{ "value": 0, "zero": "No items", "one": "1 item", "other": "items" }` |
| Expected Result | `textContent.contains: "No items"` |

### SKILL-05-b: count=1, uses one branch

| Property | Value |
|------|------|
| Component ID | `text-05b` |
| Input Parameters | `{ "value": 1, "one": "1 item", "other": "items" }` |
| Expected Result | `textContent.contains: "1 item"` |

### SKILL-05-c: count=2, uses two branch

| Property | Value |
|------|------|
| Component ID | `text-05c` |
| Input Parameters | `{ "value": 2, "two": "2 items", "other": "items" }` |
| Expected Result | `textContent.contains: "2 items"` |

### SKILL-05-d: count=5, falls back to other

| Property | Value |
|------|------|
| Component ID | `text-05d` |
| Input Parameters | `{ "value": 5, "one": "item", "other": "items" }` |
| Expected Result | `textContent.contains: "items"` |

### SKILL-05-e: count=0 but zero not provided, falls back to other

| Property | Value |
|------|------|
| Component ID | `text-05e` |
| Input Parameters | `{ "value": 0, "one": "item", "other": "items" }` |
| Expected Result | `textContent.contains: "items"` |

### SKILL-05-f: Float rounded (1.6 -> 2), uses two branch

| Property | Value |
|------|------|
| Component ID | `text-05f` |
| Input Parameters | `{ "value": 1.6, "one": "one item", "two": "two items", "other": "many items" }` |
| Expected Result | `textContent.contains: "two items"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-05"`）
2. Send `updateComponents` message; each Text component's `text` property is a `pluralize` FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains `"true"` or `"false"`
5. verify `componentCount` equals 7, `componentIds` list is complete

## Notes

- When `value` is a float, it is rounded via `std::round` before branch matching
- Branch priority: `zero` > `one` > `two` > `other` (exact match by count value)
- FunctionCall's `returnType` is `"string"`
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `format_pluralize.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `format_pluralize.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `format_pluralize.json` via `FixtureRunner`, asserts `textContent`
