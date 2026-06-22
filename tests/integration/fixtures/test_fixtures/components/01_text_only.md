# 01_text_only

## Basic Info

| Property | Value |
|------|------|
| Category | Component Render |
| Related Tests | COMP-01, SURFACE-03 |
| Surface ID | `test-surf-text-01` |
| Protocol Version | v0.9 |

## Test Case Description

Pure text rendering test case: verifies that a `Column` container containing 3 `Text` child components results in the SDK correctly building the component tree.

Test focus:
- Component count is correct (4: 1 Column + 3 Text)
- Component types correctly identified
- Parent-child relationships (children references) correctly established
- All component IDs are queryable

## Protocol Messages

Send 2 messages:

**Message 1 - Create Surface:**
```json
{
  "version": "v0.9",
  "createSurface": {
    "surfaceId": "test-surf-text-01",
    "catalogId": "https://a2ui.org/specification/v0_9/basic_catalog.json"
  }
}
```

**Message 2 - Update Components:**
```json
{
  "version": "v0.9",
  "updateComponents": {
    "surfaceId": "test-surf-text-01",
    "components": [
      { "id": "root", "component": "Column", "children": ["text-title", "text-body", "text-caption"], "align": "stretch" },
      { "id": "text-title", "component": "Text", "text": "Hello AGenUI", "variant": "h1" },
      { "id": "text-body", "component": "Text", "text": "This is a body text for testing.", "variant": "body" },
      { "id": "text-caption", "component": "Text", "text": "Caption text", "variant": "caption" }
    ]
  }
}
```

## Component Tree Structure

```
Column (root)
├── Text (text-title) - "Hello AGenUI" [h1]
├── Text (text-body) - "This is a body text for testing." [body]
└── Text (text-caption) - "Caption text" [caption]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 4 |
| Component type list | `["Column", "Text", "Text", "Text"]` |
| Root component ID | `"root"` |
| Root component type | `"Column"` |
| All component IDs | `["root", "text-title", "text-body", "text-caption"]` |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Stream the above 2 messages (beginTextStream → receiveTextChunk → endTextStream)
3. Wait for `onCreateSurface` callback to get Surface instance
4. Call `surface.getComponent()` to get root component
5. Traverse component tree, verify component count, types, and IDs

## Platform Coverage

- Android: `ComponentRenderTest.COMP-01`
- iOS: `ComponentRenderTest.testTextOnlyRender`
- Harmony: `ComponentRenderTest.COMP-01_text_only_render`
