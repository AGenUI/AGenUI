# action_formatDate

## Basic Info

| Property | Value |
|------|------|
| Category | Function Call / C++ Built-in Function |
| Related Tests | SKILL-01 |
| Function Name | `formatDate` |
| Invocation | `Text.text` FunctionCall property binding |
| surfaceId | `test-surf-fc-01` |

## Test Case Description

Verifies C++ built-in function `formatDate`'s date formatting capability. Supports ISO 8601 string input, Unix timestamp (milliseconds) input, and TR-35 format pattern output.

The test binds `formatDate` FunctionCall to Text components' `text` property via `updateComponents`. After engine rendering, it asserts `textContent` contains the expected string.

```json
{
  "id": "text-01a",
  "component": "Text",
  "text": {
    "call": "formatDate",
    "args": { "value": "2025-01-15", "format": "yyyy/MM/dd" },
    "returnType": "string"
  }
}
```

## Sub-cases

### SKILL-01-a: ISO date string - yyyy/MM/dd

| Property | Value |
|------|------|
| Component ID | `text-01a` |
| Input Parameters | `{ "value": "2025-01-15", "format": "yyyy/MM/dd" }` |
| Expected Result | `textContent.contains: "2025/01/15"` |

### SKILL-01-b: Timestamp formatting (milliseconds)

| Property | Value |
|------|------|
| Component ID | `text-01b` |
| Input Parameters | `{ "value": 1736899200000, "format": "yyyy-MM-dd" }` |
| Expected Result | `textContent.contains: "2025"` (depends on local timezone, asserts year exists) |

### SKILL-01-c: Full weekday + month name - EEEE, MMMM d

| Property | Value |
|------|------|
| Component ID | `text-01c` |
| Input Parameters | `{ "value": "2025-12-15", "format": "EEEE, MMMM d" }` |
| Expected Result | `textContent.contains: "Monday, December 15"` |

### SKILL-01-d: Abbreviated weekday + month - EEE, MMM d, yyyy

| Property | Value |
|------|------|
| Component ID | `text-01d` |
| Input Parameters | `{ "value": "2025-12-15", "format": "EEE, MMM d, yyyy" }` |
| Expected Result | `textContent.contains: "Mon, Dec 15, 2025"` |

### SKILL-01-e: Two-digit year format - MM/dd/yy

| Property | Value |
|------|------|
| Component ID | `text-01e` |
| Input Parameters | `{ "value": "2025-01-15", "format": "MM/dd/yy" }` |
| Expected Result | `textContent.contains: "01/15/25"` |

### SKILL-01-f: 24-hour format - HH:mm:ss

| Property | Value |
|------|------|
| Component ID | `text-01f` |
| Input Parameters | `{ "value": "2025-01-15T14:30:45", "format": "HH:mm:ss" }` |
| Expected Result | `textContent.contains: "14:30:45"` |

### SKILL-01-g: 12-hour format + AM/PM - h:mm a

| Property | Value |
|------|------|
| Component ID | `text-01g` |
| Input Parameters | `{ "value": "2025-01-15T14:30:00", "format": "h:mm a" }` |
| Expected Result | `textContent.contains: "2:30 PM"` |

## Test Steps

1. Send `createSurface` message to initialize surface（`surfaceId: "test-surf-fc-01"`）
2. Send `updateComponents` message; each Text component's `text` property is a `formatDate` FunctionCall
3. Wait for engine rendering to complete, get each component's `textContent`
4. assert each component `textContent` contains the expected formatted string
5. verify `componentCount` equals 8, `componentIds` list is complete

## Notes

- Format patterns follow TR-35: `y`=year, `M`=month, `d`=day, `E`=weekday, `H`=24h, `h`=12h, `m`=minute, `s`=second, `a`=AM/PM
- Timestamp test case (text-01b) result depends on device local timezone; asserting year existence is sufficient
- FunctionCall's `returnType` is `"string"`; the engine renders the result as text
- This function is implemented in C++ layer, behavior is consistent across all three platforms

## Platform Coverage

- Android: Replays `action_formatDate.json` via `FixtureRunner`, asserts `textContent`
- iOS: Replays `action_formatDate.json` via `FixtureRunner`, asserts `textContent`
- Harmony: Replays `action_formatDate.json` via `FixtureRunner`, asserts `textContent`
