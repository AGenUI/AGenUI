# Spacing & Elevation

Defines the spacing scale and shadow elevation tokens for consistent rhythm and visual depth across all A2UI cards and pages.

## Spacing Scale

All `padding` and `margin` values should be selected from this scale. Do not invent arbitrary pixel values.

| Token | Value | Purpose |
|-------|-------|---------|
| `sp-xs` | `4px` | Micro spacing: icon-to-text inline gap |
| `sp-sm` | `8px` | Tight spacing: elements within the same logical group |
| `sp-md` | `12px` | Standard row spacing: label-to-value, line gaps |
| `sp-lg` | `16px` | Group spacing: between related sections inside a card |
| `sp-xl` | `24px` | Section spacing: between major content sections |
| `sp-2xl` | `32px` | Large section spacing: between major page blocks |

### Spacing Rules

1. **Adjacent level difference**: Do not skip more than one level between adjacent spacings (e.g., `sp-xs` should be followed by `sp-sm` or `sp-md`, not `sp-xl`)
2. **Section > internal**: Spacing between major sections must be strictly larger than spacing within a section — by at least one token level
3. **Symmetry**: Symmetric layouts (e.g., 3-column metrics) must use uniform column spacing
4. **Card shell padding**: Card outer padding defaults to `sp-xl` (24px) vertically and `sp-lg`–`sp-xl` (16–24px) horizontally

### Typical Spacing Patterns

**Metric row (horizontal multi-column):**
```json
"styles": {
  "padding": "12px 16px 12px 16px",
  "margin": "0px 0px 16px 0px"
}
```

**Card internal main section:**
```json
"styles": {
  "padding": "16px 20px 16px 20px",
  "margin": "0px 0px 24px 0px"
}
```

**Tag-to-CTA vertical spacing:**
```json
// CTA below tag row
"styles": {
  "margin": "16px 0px 0px 0px"
}
```

**Card shell padding:**
```json
"styles": {
  "padding": "24px 20px 24px 20px"
}
```

## Shadow Elevation

A2UI uses `filter: drop-shadow(...)` for visual depth. Select from these preset levels.

| Token | CSS Value | Purpose |
|-------|-----------|---------|
| `elev-0` | none | Flat / embedded elements |
| `elev-1` | `drop-shadow(0px 1px 3px rgba(0,0,0,0.06))` | Micro lift: lightweight cards, tags |
| `elev-2` | `drop-shadow(0px 4px 12px rgba(0,0,0,0.08))` | Standard: card shell (default) |
| `elev-3` | `drop-shadow(0px 8px 24px rgba(0,0,0,0.12))` | Emphasis: floating CTA, modal preview |

### Elevation Rules

1. **Card default**: `Card` as the main shell should use `elev-2` by default
2. **Max 2 levels apart**: Within a single card/page, shadow differences should not exceed 2 levels (e.g., `elev-1` and `elev-3` should not coexist)
3. **Shadow + background coordination**: Light backgrounds pair with low-opacity shadows (0.06–0.12). Dark backgrounds pair with higher-opacity shadows (0.2–0.3)
4. **Internal blocks use background color, not shadow**: Sub-blocks within a card should use `background-color` (e.g., `surface-variant`) for grouping, not additional shadows — avoid visual fragmentation

### Typical Applications

**Standard card shell:**
```json
{
  "id": "root",
  "component": "Card",
  "child": "card_body",
  "styles": {
    "filter": "drop-shadow(0px 4px 12px rgba(0, 0, 0, 0.08))",
    "margin": "16px 16px 16px 16px"
  }
}
```

**Internal color block grouping (no shadow, use background color):**
```json
{
  "id": "metric_group",
  "component": "Row",
  "children": ["metric_a", "metric_b", "metric_c"],
  "styles": {
    "background-color": "rgba(196, 148, 90, 0.10)",
    "border-radius": "16px",
    "padding": "12px 16px 12px 16px"
  }
}
```

**Floating CTA button:**
```json
{
  "id": "cta_button",
  "component": "Button",
  "child": "cta_text",
  "styles": {
    "filter": "drop-shadow(0px 2px 6px rgba(0, 0, 0, 0.10))",
    "border-radius": "24px",
    "padding": "12px 24px 12px 24px"
  }
}
```

## Border Radius System

Border radius should match content type. Use larger radii for outer containers and smaller radii for internal elements.

| Context | Radius | Notes |
|---------|--------|-------|
| Button / pill | `20px–32px` | Most rounded — capsule form |
| Card shell | `16px–24px` | Outer container |
| Internal section / group | `12px–16px` | Colored background blocks |
| Tag / badge | `8px–12px` | Small labels |
| Image | `12px–20px` | Should not exceed card radius (avoid overflow) |

### Rules

1. **Outer ≥ inner**: Card radius ≥ internal section radius ≥ tag radius
2. **Button ≥ card**: Button radius should be at least as rounded as the card shell
3. **Image ≤ card**: Image radius should not exceed card radius to avoid visual overflow
