# Expressiveness Toolkit

> **Related**: For choosing *which component* to use (Chart vs Text, Table vs Text list), see **Component Expressiveness Hierarchy** in [`reference/component-design.md`](component-design.md). This document covers visual richness techniques *within* chosen components.

How to create visual richness within A2UI protocol constraints. Each technique below uses only protocol-supported capabilities.

## 1. Inline Color Mixing (RichText)

`RichText` supports a subset of HTML tags for inline color, emphasis, and formatting within a single text block.

Supported tags: `<font>`, `<color>`, `<a>`, `<br>`, `<blockquote>`, `<i>`, `<u>`, `<strike>`, `<sub>`, `<sup>`, `<strong>`, `<b>`, `<small>`, `<img>`.

**Use cases**: Highlight price numbers, embed status labels in body text, emphasize brand names.

```json
{
  "id": "rich_description",
  "component": "RichText",
  "text": {"path": "/card/description"},
  "variant": "body",
  "styles": {
    "color": "#6B6B6B",
    "line-height": 1.5
  }
}
```

Corresponding dataModel value can contain HTML:
```
"<font color='#C4945A'><b>¥268</b></font>/night, breakfast included"
```

**Rules**:
- Do not use `RichText` for buttons or large color blocks — it is a text enhancement tool
- Inline color should come from the selected palette's `primary` token
- Use `<b>` or `<strong>` for emphasis, not just color — combine both for maximum impact

## 2. Color Block Grouping (background-color + border-radius)

Use `Column/Row + background-color + border-radius + padding` to create colored section blocks.

This is the core means of internal card layering — do **not** nest `Card` components; use color blocks instead.

```json
{
  "id": "highlight_section",
  "component": "Column",
  "children": ["hl_title", "hl_body"],
  "styles": {
    "background-color": "rgba(196, 148, 90, 0.08)",
    "border-radius": "16px",
    "padding": "16px 20px 16px 20px"
  }
}
```

**Rules**:
- Use `rgba(r,g,b,0.06–0.15)` transparency for section backgrounds, not solid fills
- Text inside color blocks should use `on-surface` color for readability
- Maximum 2 color blocks per card to avoid visual fragmentation
- Color block backgrounds should use `rgba` transparency of the card's primary accent color (e.g. `rgba(r,g,b, 0.08–0.15)`), not solid fills

## 3. Icon + Text Metric Groups

Use built-in `Icon` components to support information expression. Pair with text for a complete metric display.

**Typical pattern**: Icon + value + label, three-piece set.

```json
{
  "id": "metric_row",
  "component": "Row",
  "children": ["metric_icon", "metric_value_col"],
  "align": "center"
}
{
  "id": "metric_icon",
  "component": "Icon",
  "name": "star",
  "styles": {
    "width": "28px",
    "height": "28px",
    "margin": "0px 8px 0px 0px"
  }
}
{
  "id": "metric_value_col",
  "component": "Column",
  "children": ["metric_num", "metric_label"]
}
```

**Common icon recommendations**:
- Rating/reviews: `star`, `starHalf`, `starOff`
- Location/navigation: `locationOn`, `place`
- Time/schedule: `calendarToday`, `event`
- Contact/communication: `phone`, `mail`, `send`
- Status/alert: `info`, `warning`, `check`, `error`
- Actions: `search`, `settings`, `share`, `favorite`, `favoriteOff`
- Commerce: `shoppingCart`, `payment`
- Media: `play`, `pause`, `camera`, `photo`

## 4. Opacity Layering

`opacity` creates visual fade effects: secondary text, decorative separators, overlay layers.

```json
{
  "id": "source_attribution",
  "component": "Text",
  "text": {"path": "/card/source"},
  "variant": "caption",
  "styles": {
    "color": "#6B6B6B",
    "opacity": 0.6,
    "font-size": "24px"
  }
}
```

**Common usage**:
- Source attribution / footnotes: `opacity: 0.5–0.7`
- Decorative separators (thin line): `opacity: 0.3` + `background-color`
- Background decorative element fade: `opacity: 0.05–0.1`
- Disabled/inactive elements: `opacity: 0.4`

## 5. Pseudo-Gradient Effects

A2UI does not support CSS gradients, but subtle gradient effects can be simulated using adjacent blocks with slightly different colors from the same palette.

```json
// Upper section
{"id": "top", "component": "Column", "styles": {"background-color": "#F5F0EB"}}

// Lower section (subtle variation in same tone family)
{"id": "bottom", "component": "Column", "styles": {"background-color": "#EDE6DF"}}
```

Use two adjacent colors from the same hue family with subtle brightness difference.

**Rules**:
- Adjacent block colors must be from the same hue family
- The color difference should be subtle (noticeable but not jarring)
- Do not alternate light/dark/light/dark — this creates a "stitched template" feel
- Use pseudo-gradients for section transitions, not for the entire card

## 6. Decorative Separators

Use `Divider` or thin styled components to create visual breaks between sections.

**Using Divider component:**
```json
{
  "id": "section_divider",
  "component": "Divider",
  "axis": "horizontal"
}
```

**Using styled thin component (for colored dividers):**
```json
{
  "id": "accent_divider",
  "component": "Column",
  "styles": {
    "height": "2px",
    "background-color": "rgba(196, 148, 90, 0.30)",
    "margin": "16px 0px 16px 0px"
  }
}
```

## Anti-Patterns

- **No color layering at all**: A card with only one background color (usually white) has no depth — add at least one `surface-variant` or `primary-container` section
- **Over-decoration**: More than 3 different decorative techniques (shadow + color block + gradient + icon) in a small card = visual chaos
- **Wrong tool for the job**: Using `RichText` to create colored backgrounds, or using `Icon` where `Text` with emoji would suffice
- **Hardcoded values**: Every color, spacing, and radius value should come from the defined tokens, not arbitrary numbers
