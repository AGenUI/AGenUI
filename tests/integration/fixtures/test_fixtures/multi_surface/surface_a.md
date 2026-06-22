# surface_a

## Basic Info

| Property | Value |
|------|------|
| Category | Multi-Surface Isolation |
| Related Tests | MULTI-01, MULTI-02, MULTI-03 |
| Surface ID | `test-surf-multi-a` |
| Protocol Version | v0.9 |
| Paired File | `surface_b.json` |

## Test Case Description

Multi-Surface isolation test (Side A): creates two independent Surfaces (A and B) in the same `SurfaceManager` instance, verifying their component trees are isolated from each other.

Surface A renders a layout containing a Button, for component isolation comparison with Surface B (which only contains Text).

Test focus:
- Surface A's component tree contains only its own components
- Surface B's components (e.g., `text-b1`) do not appear in Surface A's query results
- Deleting Surface A does not affect Surface B's component tree

## Component Tree Structure

```
Column (root) [padding: 20px]
└── Button (btn-a) [background: #FF6B35, 80px]
    └── Text (btn-a-text) - "Surface A Button" [#FFFFFF]
```

## Expected Results

| Assertion | Expected Value |
|--------|--------|
| Total components | 3 |
| Component type list | `["Column", "Button", "Text"]` |
| Root component ID | `"root"` |
| All component IDs | `["root", "btn-a", "btn-a-text"]` |
| Surface B's components not in A | `btn-a` does not appear in Surface B's component tree |

## Usage

This file should be used together with `surface_b.json`:

1. Create a single `SurfaceManager` instance
2. Send `surface_a.json` message sequence first (Create Surface A)
3. Then send `surface_b.json` message sequence (Create Surface B)
4. Get component trees for both Surfaces
5. Verify component trees are isolated from each other
6. (MULTI-03) Delete Surface A, verify Surface B is unaffected

## Platform Coverage

- Android: `MultiSurfaceTest.MULTI-01/02/03`
- iOS: `MultiSurfaceTest.testMULTI01/02/03`
- Harmony: `MultiSurfaceTest.MULTI-01/02/03`
