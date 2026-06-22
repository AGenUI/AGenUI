# surface_b

## Basic Info

| Property | Value |
|------|------|
| Category | Multi-Surface Isolation |
| Related Tests | MULTI-01, MULTI-02, MULTI-03 |
| Surface ID | `test-surf-multi-b` |
| Protocol Version | v0.9 |
| Paired File | `surface_a.json` |

## Test Case Description

Multi-Surface isolation test (Side B): the second Surface created in the same `SurfaceManager` instance, renders only Text components, for component type isolation comparison with Surface A (which contains Button).

Test focus:
- Surface B's component tree contains only its own components (2 Text)
- Surface A's components (e.g., `btn-a`) do not appear in Surface B's query results
- The two Surfaces have completely independent component namespaces

## Component Tree Structure

```
Column (root) [padding: 20px]
├── Text (text-b1) - "Surface B - Title" [h2]
└── Text (text-b2) - "Surface B - only text components, no Button" [body]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 3 |
| Component type list | `["Column", "Text", "Text"]` |
| Root component ID | `"root"` |
| All component IDs | `["root", "text-b1", "text-b2"]` |
| Surface A's components not in B | `text-b1` does not appear in Surface A's component tree |

## Usage

This file should be used together with `surface_a.json` (see `surface_a.md` for complete test steps).

## Design Intent

Surface A contains Button and Surface B only contains Text, in order to:
1. Distinguish the content of two Surfaces through different component types
2. Verify `getComponentTree()` returns component type lists that don't get mixed up
3. After deleting Surface A, Button-type components should completely disappear, while Surface B's Text is unaffected

## Platform Coverage

- Android: `MultiSurfaceTest.MULTI-01/02/03`
- iOS: `MultiSurfaceTest.testMULTI01/02/03`
- Harmony: `MultiSurfaceTest.MULTI-01/02/03`
