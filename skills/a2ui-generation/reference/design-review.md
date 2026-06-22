# Design Quality Review

## Purpose

After the validation script passes for the first time, perform this structured design review on the on-disk output. The goal is to elevate the UI from "correct but bland" to "polished and visually compelling" by auditing four dimensions: color palette, layout structure, decorative detail, and theme appropriateness.

## How to Use

1. Read the generated `*_components.json` file
2. Walk through each audit dimension below
3. For every issue found, record:
   - **Component**: which component id is affected
   - **Problem**: what specifically is wrong
   - **Fix**: the concrete style change to apply
4. Apply all fixes to the on-disk file via diff edits
5. Re-run `scripts/validate_a2ui.py` after changes

## Dimension 1: Color Palette Audit

Check these in order:

- **Generic grey-blue trap**: Is the entire card using only `#333333` / `#666666` / `#999999` text + `#007AFF` accent? If so, the palette has no personality — it must be redesigned with domain-appropriate colors
- **Single accent syndrome**: Count how many distinct accent colors are used. If there is only 1, related but different elements (e.g., multiple metrics, chart segments, tags) cannot be visually distinguished — add 2–3 coordinated accent colors
- **Hero impact**: Is the hero number / key metric using a visually striking color that contrasts sharply with the surrounding text? If it blends in with body text, boost it with a saturated primary color
- **Text contrast**: Check `color` values against their container `background-color`. Dark text on dark background or light text on light background is a critical error
- **Color layer depth**: Count how many distinct background colors exist in the card. If there is only 1 (usually white), there is no depth — add at least one `surface-variant` layer (a tinted background for the hero area or metric section)

## Dimension 2: Layout Structure Audit

- **Focal point existence**: Can you immediately identify the single most important element in the card? If everything is the same size and weight, nothing stands out — enlarge the hero element (`40px`), or give it a distinctive background container
- **Pure vertical stacking**: If the component tree is only `Column > Column > Column > Text`, there is zero spatial tension. Check whether any content naturally forms horizontal relationships (metrics side-by-side, icon + label, image + text) and convert those to `Row`
- **Visual grouping**: Are related items visually grouped? If 3 metrics are in a Row but have no shared visual container (background, border, spacing gap), they look disconnected. Add a container with `surface-variant` background or consistent spacing
- **Rhythm**: Is there alternation between visually dense areas and breathing space? If all gaps are the same size (`margin: 0px 0px 16px 0px` everywhere), the card feels monotonous. Vary section gaps: larger between major sections (`24px–32px`), smaller within sections (`6px–8px`)
- **Component expressiveness**: Is any section using only `Text` where `Chart`, `RichText`, `Table`, or `Image` would better match the data type?
- **Image alignment**: Check that `Image` components inside `Row` or `Column` containers use appropriate `fit` (`cover` for fill, `contain` for preserve aspect ratio) and that parent containers set `align: "center"` or `align: "stretch"` to prevent off-center rendering. Single images should be horizontally centered; image strips should use uniform height + `align: "stretch"`

## Dimension 3: Decorative Detail Audit

- **Border-radius uniformity**: If all containers use the same `border-radius` (or none), the card lacks shape variety. Use larger radius (`16px–20px`) for hero/highlight containers and smaller radius (`8px–12px`) for tags and secondary elements
- **Surface layers**: Does the card use any background color other than the root card white? If not, add at least one `surface-variant` container (a slightly tinted background) around the hero area or a key section to create visual depth
- **Drop-shadow usage**: Key containers (hero area, action buttons, floating elements) benefit from subtle shadow: `filter: drop-shadow(0px 2px 12px rgba(0, 0, 0, 0.06))`. If nothing has shadow, the card feels flat
- **Padding adequacy**: Check inner containers for padding. If content is pressed against container edges (`padding: 0px`), it looks cramped. Minimum recommended: `16px` for small containers, `20px–28px` for main containers
- **Color layer count**: A polished card should have at least 3 color layers: (1) card background, (2) section/container background, (3) accent/primary colors. If only 2 layers exist, the card is under-decorated

## Dimension 4: Theme Appropriateness Audit

Match the color temperature and mood to the content domain. These are directional guidelines, not fixed values:

| Domain | Color direction | Avoid |
|--------|----------------|-------|
| Food / lifestyle | Warm tones: orange, amber, coral, warm reds | Cold blue, clinical grey |
| Tech / digital | Cool tones: blue, indigo, cyan, electric purple | Warm earthy tones |
| Nature / travel | Organic tones: green, teal, earth brown, warm gold | Neon, synthetic colors |
| Finance / business | Stable tones: navy, slate blue, warm gold accents | Playful bright colors |
| Health / fitness | Energetic tones: green, orange, teal, vibrant coral | Dark muted palettes |
| Education / knowledge | Calm tones: deep blue, sage green, warm neutral | Aggressive neon |
| Entertainment / social | Vibrant tones: magenta, coral, electric blue, orange | Dull corporate colors |

**How to apply**: If the card is about food but uses cold blue as primary → flag it. If it's about finance but uses playful coral → flag it. Suggest 2–3 specific hex values that better match the domain.

## Review Output Template

After completing all 4 dimensions, summarize findings as:

```
Design Review Results:
[1] {component_id}: {problem} → {fix}
[2] {component_id}: {problem} → {fix}
...
Issues found: {N}
```

If 0 issues found across all dimensions, state: `Design review passed — no improvements needed.`

Apply all fixes, then re-run the validation script before delivery.

## Pre-Output Design Checklist

Before generating the formal output, evaluate the layout plan against these 6 dimensions. Each dimension should be answered with a brief yes/no + one-line rationale.

### 1. Visual Focal Point
- Is there exactly one clear visual anchor (hero number, main image, or dominant title)?
- Are there no more than 1 element competing for primary attention?

### 2. Information Hierarchy
- Are primary / secondary / tertiary layers differentiated by at least one full font-size tier?
- Does secondary information use a lighter color (`on-surface-variant` or `opacity < 0.7`)?

### 3. Color Tension
- Is the `primary` palette color applied to at least one key element (CTA, hero number, or tag)?
- Are all accent colors drawn from the same palette (no cross-palette contamination)?
- Are there no more than 2 distinct hue families in the card?

### 4. Whitespace Rhythm
- Is spacing between major sections strictly larger than spacing within sections (by ≥ 1 token level)?
- Is the card shell padded sufficiently (≥ `sp-xl` / 24px vertically) so shadows are not clipped?

### 5. Horizontal Tension
- Is there at least one meaningful left-right relationship (metrics side-by-side, icon + label, image + text)?
- If all content is vertical stacking, is there a reason no horizontal relationship exists?

### 6. Decoration Level
- Does the card shell use an elevation token (at minimum `elev-1`, default `elev-2`)?
- Does the card use at least one color block (`surface-variant` or `primary-container`) for section grouping?
- Are border radii consistent with the radius system (outer ≥ inner, button most rounded)?

### 7. Expressiveness Level
- Is any section using plain `Text` where `Chart`, `RichText`, `Table`, `Image`, or `Markdown` would better represent the data?
- If numerical comparisons exist, is at least one expressed via `Chart` rather than listed as text?
- If the data is tabular, is `Table` used rather than simulated rows of `Text`?

### Fix Priority Order

If multiple dimensions fail, fix in this order:
1. **Visual Focal Point** — no focal point = user doesn't know where to look
2. **Information Hierarchy** — hierarchy混乱 = information cannot be scanned and understood
3. **Color Tension** — no accent = visually flat
4. **Decoration Level** — no decoration = lacks refinement
5. **Expressiveness Level** — plain text where richer components fit = wasted protocol capability
6. **Whitespace Rhythm** — uniform spacing = no breathing room
7. **Horizontal Tension** — all vertical = lacks spatial tension
