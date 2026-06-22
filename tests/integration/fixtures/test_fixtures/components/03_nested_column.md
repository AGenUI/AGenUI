# 03_nested_column

## Basic Info

| Property | Value |
|------|------|
| Category | Component Render |
| Related Tests | COMP-03 |
| Surface ID | `test-surf-col-01` |
| Protocol Version | v0.9 |

## Test Case Description

Nested Column layout test case: verifies the SDK correctly builds deep component trees with multi-level container nesting.

Test focus:
- Outer Column contains 2 inner Column sub-containers
- Each inner Column contains 2 Text child components
- 3-level deep component tree correctly established
- All 7 components (3 Column + 4 Text) are traversable

## Protocol Messages

Send 2 messages：

**Message 1 - Create Surface：**
```json
{
  "version": "v0.9",
  "createSurface": {
    "surfaceId": "test-surf-col-01",
    "catalogId": "https://a2ui.org/specification/v0_9/basic_catalog.json"
  }
}
```

**Message 2 - Update Components：**

A flat list of 7 components with parent-child relationships established via `children` fields.

## Component Tree Structure

```
Column (root) [padding: 16px, gap: 12px]
├── Column (col-a) [background: #F0F0F0]
│   ├── Text (text-a1) - "Section A - Title" [h3]
│   └── Text (text-a2) - "Section A - Description" [body]
└── Column (col-b) [background: #E8F4FD]
    ├── Text (text-b1) - "Section B - Title" [h3]
    └── Text (text-b2) - "Section B - Description" [body]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 7 |
| Component type list | `["Column", "Column", "Text", "Text", "Column", "Text", "Text"]` |
| Root component ID | `"root"` |
| Root component type | `"Column"` |
| All component IDs | `["root", "col-a", "text-a1", "text-a2", "col-b", "text-b1", "text-b2"]` |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Stream the above 2 messages
3. Wait for `onCreateSurface` to get Surface
4. Get component tree, verify total count is 7
5. Verify root component has 2 children (col-a, col-b)
6. Verify each sub-Column has 2 Text child components

## Design Intent

This test case covers the SDK's handling of nested containers, ensuring `children` references are not lost or corrupted in multi-level nesting. Complementary to `04_card_complex` (which tests `child` single child component reference).

## Platform Coverage

- Android: `ComponentRenderTest.COMP-03`
- iOS: `ComponentRenderTest.testNestedColumnRender`
- Harmony: `ComponentRenderTest.COMP-03_nested_column_render`
