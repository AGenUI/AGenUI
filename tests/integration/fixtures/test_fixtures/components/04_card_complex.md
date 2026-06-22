# 04_card_complex

## Basic Info

| Property | Value |
|------|------|
| Category | Component Render |
| Related Tests | COMP-04 |
| Surface ID | `test-surf-card-01` |
| Protocol Version | v0.9 |

## Test Case Description

Complex Card component test case: verifies the complete component tree construction when `Card` acts as a container with an embedded `Column` layout (containing title Text, body Text, and action Button).

Test focus:
- `Card` uses `child` (single child component reference) to contain content
- Multi-level nesting: Column → Card → Column → [Text, Text, Button → Text]
- Button bound to `openUrl` type functionCall Action
- All 7 components are traversable
- Style properties (drop-shadow, border-radius, etc.) do not affect component tree structure

## Protocol Messages

Send 2 messages, including an `updateComponents` message with 7 components.

## Component Tree Structure

```
Column (root) [padding: 20px, background: #F5F5F5]
└── Card (card-wrapper) [padding: 24px, border-radius: 16px, shadow]
    └── Column (card-inner) [gap: 12px]
        ├── Text (card-title) - "Card Title" [bold, 32px]
        ├── Text (card-body) - "This is the card body text..." [#666, line-clamp: 3]
        └── Button (card-btn) [action: openUrl("https://a2ui.org/")]
            └── Text (card-btn-text) - "Learn More" [#0066CC]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 7 |
| Component type list | `["Column", "Card", "Column", "Text", "Text", "Button", "Text"]` |
| Root component ID | `"root"` |
| All component IDs | `["root", "card-wrapper", "card-inner", "card-title", "card-body", "card-btn", "card-btn-text"]` |
| `card-btn` Action type | `"functionCall"` |
| `card-btn` Action call | `"openUrl"` |

## Test Steps

1. Create `SurfaceManager` and register listener
2. Stream the above 2 messages
3. Wait for `onCreateSurface` to get Surface
4. Get component tree, verify total count is 7
5. Verify Card component references inner Column via `child`
6. Verify Button's Action is configured as `openUrl`

## Design Intent

This test case verifies the mixed use of `child` (single child component) and `children` (multiple child components) reference patterns, and that complex style properties (shadow, border-radius, etc.) do not affect component tree correctness.

## Platform Coverage

- Android: `ComponentRenderTest.COMP-04`
- iOS: `ComponentRenderTest.testCardComplexRender`
- Harmony: `ComponentRenderTest.COMP-04_card_complex_render`
