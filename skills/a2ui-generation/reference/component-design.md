# Component Design

## Scope

This document covers `component/card` mode only, not full pages.

## Height Boundary

- `Component/card`: defaults to no more than `1/3` of page height
- A card is a summary-type container, not a miniaturized page

Do not expand using a page-level structure and then try to compress the height afterward.

## Card Content Budget

In card mode, defaults should be:

- Only one core conclusion area or primary visual focal point
- Only one group of the most critical supporting info
- Only one primary CTA or main status action area
- Typically no more than `2–3` main sections

Content that should not be placed in a card by default:

- Long lists
- Long tables
- Long timelines
- Long explanatory text
- Multiple large parallel sections
- Full page-level narrative chains

### Information Summary Card — Dedicated Rules (Query / Search-Term Scenario)

When the query is a search term, place name, person name, event name, or concept word, the card is an **information summary card**. The following additional rules are mandatory:

**⚠️ English model content inflation warning**: English language models tend to produce article-style, detail-page content for these queries. Every rule below is a hard constraint — not a default preference that can be overridden by "making it richer."

**Hard content budget (non-negotiable upper limits)**:
- Body text ≤ 2 paragraphs; each paragraph ≤ 3 lines
- Dynamic `List` sub-templates (`children.componentId`-driven rows such as `tips_item_template`): **FORBIDDEN** — this is a detail-page structure
- Full-width hero image (`aspect-ratio: 16/9` + `width: 100%`): **FORBIDDEN** — this is a page header element, not a card element
- Main sections ≤ 3
- Total card height must not exceed 2/3 of screen height

**Required structure**:
```
Title section (title + optional subtitle)
↓
Core attributes row (2–3 key metrics, laid out horizontally)
↓
Brief summary (Text component, ≤ 2 lines)
↓
Tag group (Row + multiple Text tag elements)
↓
CTA button (optional)
```

**Anti-patterns (must avoid)**:
- Multiple long body paragraphs (5+ lines each) + full visitor tips list + full-width hero image = article detail page, NOT a summary card
- `List` + `componentId` dynamic sub-template rendering = detail-page data-driven list, NOT a summary card
- Any structure where you could replace the card with a news article and it would still make sense = you have gone too far

For DTO component mode, additional rules apply:

- When a secondary field is missing, prefer omitting the corresponding component rather than leaving an empty placeholder
- When a key field for an entire section is missing, prefer omitting the entire section
- The final card should remain complete and compact — missing fields must not leave visible blank gaps
- When text length is uncertain, the structure must leave room for wrapping, truncation, and fallback layouts

## Card Escalation Rule

Treat any of the following as exceeding the card budget:

- Even after compression, the card fills or nearly fills a full screen
- More than `3` main sections are needed to convey the information
- The primary information requires continuous vertical scrolling to be fully seen
- After removing secondary content, the card is still too tall, too full, or too fragmented

The correct response is:

1. Converge to a summary-type card
2. If convergence still does not hold, escalate to a page
3. Do not silently deliver a page-sized large card and call it a "component"

## Mobile Width Safety

A2UI targets mobile narrow screens by default. When generating cards, avoid horizontal overflow first.

General principles:

- Do not put long text, long institution names, navigation paths, or body summaries in side-by-side multi-column structures
- Do not rely on "text being just short enough" to maintain layout
- Horizontal layouts are better suited for short labels, short numbers, button groups, and icon combinations
- When content length is uncertain, prefer `Column` or `List`
- If a local area naturally suits horizontal browsing and cannot be fully contained on a narrow screen, allow that local area to scroll horizontally to reveal more

Supplementary notes:

- "Prefer `Column`" is the default conservative strategy — it does not mean abandoning design quality
- If a local section clearly suits left-right opposition, dual-side tension, or information juxtaposition, do not downgrade it to plain vertical stacking just to be safe
- The correct approach is to first judge whether the horizontal relationship holds, then decide whether wrapping or reflow is needed

## Competitive Layouts

When a local section naturally suits layouts like "core metric left / supporting info right", "main visual left / description right", or "left-right aligned", design it as a competitive horizontal layout — not a plain left-to-right concatenation.

Goal:

- When space allows, left and right sides form tension and order
- Only when genuine competition occurs should wrapping, truncation, or vertical reflow be introduced
- Do not let a column break spontaneously due to container modeling errors when there is no actual competition

In A2UI's current capabilities, runtime breakpoints, `min-width`, `flex-basis`, or text measurement for precise decisions are not available, so heuristic decisions must be made at generation time.

Recommended flow:

1. First identify whether this section is "left-right opposition" rather than "plain horizontal concatenation"
2. Define the role and minimum readable form for each column
3. Try to preserve the left-right anchor relationship first, then let weak information absorb compression
4. Only switch to wrapping or vertical structure when heuristic judgment clearly shows real competition

## Protected Column Readability

A protected column must not just be "visible" — it must maintain minimum readable form.

Rules:

- Do not give a protected column an excessively narrow fixed width that causes its internal text to wrap into `2–3` lines even when there is no competition
- If a caption / unit label / supporting phrase in this column should be fully readable but is forced to break by a fixed width that is too small, this is a layout error, not valid responsive behavior
- First ensure core numbers, main status, and key phrases are displayed completely before compressing other columns

For short phrases:

- Short phrase captions should by default remain as complete word groups — do not split them into ugly broken lines
- If this column cannot hold both the core number and the phrase, prioritize reallocating column widths, adjusting information hierarchy, or rewriting the local structure
- Do not let the protected column collapse first, then misattribute the problem to "competition with the right side"

## Short CJK Phrase Integrity

Short CJK phrases, short CTAs, and short numeric phrases are high-risk break candidates by default — "broken but still visible" is not an acceptable result.

Content treated as protected by default:

- `2–8` character short phrases, e.g. "Book Now", "Open", "Patio View"
- Numeric phrases, e.g. `4.9`, `¥268`, `22:30`
- Number-plus-phrase, e.g. "Reputation 986+", "Today 30% off", "Avg. ¥268"
- Short status labels and rating summaries next to icons

Mandatory requirements:

- Do not let such content wrap character-by-character, hang a single character, or leave punctuation on its own line due to a fixed narrow width
- Do not split "Book Now" into "Book / Now", or break `4.9`, `22:30`, `¥268` into fragments
- If the phrase is in a button column, rating column, status column, price column, or right-side action column, default to protecting its integrity
- If the phrase's integrity and the current layout cannot coexist, fix the layout first — do not accept fragmented typography

Priority order:

1. Widen the protected column
2. Move, downgrade, or push weak information to the next line
3. Adjust local structure hierarchy
4. Only then consider truncating weak information

Do not mistake "short CJK phrase breaking first" for normal responsive behavior — it typically signals incorrect column width allocation.

## CTA Width Policy

A `CTA` button is a protected column — do not default to a narrow fixed width just for the sake of alignment.

Default strategy:

- Buttons prefer `padding + border-radius` for volume; do not write a fixed `width` first
- Short CTA text should naturally stretch the button width via content
- If a button sits alongside a long description, let the description absorb compression rather than letting the button text break first
- If the button label is a short phrase, do not accept character-by-character wrapping or a `2x2` grid-style layout

Only consider a fixed width when:

- The visual system explicitly requires equal-width button groups
- Alignment with other fixed-width sibling modules is required
- The fixed width has been verified to keep button text fully readable

If a fixed width is required:

- Estimate based on "minimum readable form" first — do not start with a visually appealing number
- Account for large font sizes, horizontal padding, and icon space when computing the width budget
- If the fixed width causes the button text to wrap before the description column does, treat it as a layout error and revert

## Multi-Column Text Budget

When adopting a multi-column layout for premium feel, do not let all columns compete for width without limits.

Assign a role to each column first:

- Protected column: core numbers, main status, primary button, key visuals, avatars/thumbnails, core badges
- Compressible column: descriptive text, supporting descriptions, secondary labels, supplementary notes, source attribution

Handling principles:

- Protected columns take priority for complete visibility — do not let long text squeeze them out
- Compressible columns absorb shrinkage first
- Long text defaults to wrapping first
- Use `line-clamp` only if wrapping still clearly breaks the layout
- Truncation should occur on weak information — do not truncate main titles, core status, or key CTAs first
- Do not mistake "the protected column itself is too narrow and breaks" for appropriate wrapping
- If the horizontal relationship holds, preserve left-right alignment first, then decide which column absorbs compression
- If this is plain concatenation rather than true left-right opposition, do not assume you have completed a premium horizontal layout

Implementation strategy:

- Give visual columns, number columns, and button columns stable width or stronger layout priority
- Give text columns higher compressibility, e.g. allow `flex-shrink`
- Text columns prefer multiple lines rather than forcing everything on one line
- Secondary descriptions, footnotes, source attributions, and supplementary labels can use `line-clamp: 1–2`
- If the main title must be truncated, first confirm no lower-priority content is competing for width on the same line
- If a row contains an image, badge, long title, and supporting text simultaneously, do not force all of them into a single horizontal layer
- For "left metric / right description" layouts, prefer left-right anchoring, strong-vs-weak column division, and wrapping when necessary — not mechanical concatenation of a fixed small left block and a plain right list
- For button columns, rating columns, and status columns, default to avoiding narrow fixed widths; when content includes short phrases or numeric phrases, natural width or a larger budget is even more important

Anti-patterns:

- Right-side CTA with a fixed narrow width, causing "Book Now" to split into two lines or four squares
- Rating column simultaneously carrying star icon, score, and summary, with a narrow fixed width causing `4.9` or "Reputation 986+" to fragment
- Left-side long description still fully expanded while the right-side protected column — rating or button — breaks first
- **Tag group and CTA button placed in the same Row, causing the CTA to be pushed outside the card's right boundary and clipped invisible**

## Tag Group and CTA Layout Isolation Rule

**FORBIDDEN: Placing a tag group and a CTA button inside the same Row.**

This is the most common root cause of button overflow and clipping. When there are >= 2 tags, even if you set flex-wrap: wrap and flex-grow: 1 on the tag group, and flex-shrink: 0 on the CTA, if the total tag width + CTA width > card width, the CTA will still overflow the card's right boundary and be clipped invisible.

Reason: **flex-wrap only controls wrapping among direct children inside that container — it has no effect on sibling nodes (like cta_button) in the outer Row.**

Typical wrong structure (forbidden — no flex property can fix this):

```
bottom_row (Row, justify: spaceBetween)
  ├── tag_group (Row, flex-grow: 1, flex-wrap: wrap)  ← expands to fill remaining space
  │     ├── tag_c9 / tag_stem / tag_public
  └── cta_button (flex-shrink: 0)  ← pushed outside right boundary, clipped invisible
```

**Correct structure (mandatory)**: Separate tag group and CTA into independent rows using Column:

```
bottom_section (Column)
  ├── tag_row (Row, justify: start, flex-wrap: wrap)
  │     ├── tag_1 / tag_2 / tag_3
  └── cta_button (margin-top: 12px, width: 100%)
```

**Only exception**: When there is exactly 1 tag with very short text (<= 4 chars) and overflow has been confirmed not to occur, same-row placement may be considered. Otherwise, always use separate rows.

## Evidence Vs Badge

Do not turn semantically different information into the same type of small block.

How to differentiate:

- Badge / tag / pill: short words, short phrases, weak decorative info — suitable for capsule/pill style
- Evidence / supporting sentence: sentence-level proof, credible corroboration, supplementary explanation — defaults to a supporting info column, not a badge wall

Rules:

- Sentence-level evidence should not default to heavy rounded-corner blocks — avoid making the supporting area a secondary visual center
- If the right side carries supporting evidence, prefer presenting it as an ordered supporting info area, not a group of competing mini-cards
- To strengthen hierarchy, prefer arrangement, whitespace, font size, and color layering — not a pile of independent colored blocks adding visual weight

## Card Shell Guidance

- A single card defaults to one main card shell
- Use lightweight sectioning inside the main card — no card-within-card
- If `Card` is already used as the main shell, its immediate children must not add a full visual shell (`background-color + border-radius + drop-shadow`)
- `root` handles layout only — do not assign card background to it
- Small cards need appropriate whitespace on the outside — borders and shadows must not touch the edge
- If the card body uses a prominent `drop-shadow(...)`, outer whitespace must not be a token amount
- Cards should be composable content blocks, not standalone panels with an outer background plate

## Missing Data Behavior

For missing fields in DTO component mode, the default behavior should be "structural pruning", not "style hiding":

- Missing text field: omit the corresponding text component
- Missing image field: omit the corresponding image component and its wrapper
- Missing badge / tag field: omit the corresponding badge area
- Missing entire info group: omit the entire section

The goal is for the final structure to be naturally compact — not held together by empty values, empty containers, or hidden styles.

## Image Strip Fill Rule

When a card needs "multiple images in a row" and the images are the primary visual, fill must be prioritized over mere visibility.

Mandatory requirements:

- In an image strip that needs to fill horizontally, prefer letting `Row` carry `Image` directly — do not wrap in an extra `Card` or container by default
- `Image` defaults to `fit: cover` — avoid thumbnails floating in large empty slots
- Do not use `variant` (e.g. `smallFeature`, `mediumFeature`) on `Image` in a fill strip, as the renderer may apply built-in sizes that prevent filling the parent layout
- A2UI style sizes support `px` only — do not use `%`, `vw`, or similar units to express fill

Recommended implementation:

- Image strip container: default to `Row` (fill-style primary visual); switch to `List(direction=horizontal)` only when narrow-screen clipping risk is real
- Image component: plain `Image`
- Image style: at minimum include a uniform `height` (px) + `flex-grow: 1` + `flex-shrink: 1` + necessary border-radius/clipping styles
- Spacing: use `margin` (four values) to control gaps between image items

Standard non-scrolling image strip (preferred):

```json
{"id": "image_strip", "component": "Row", "children": ["image_0", "image_1", "image_2"], "align": "stretch"}
{"id": "image_0", "component": "Image", "url": {"path": "/card/images/0"}, "fit": "cover", "styles": {"height": "220px", "flex-grow": 1, "flex-shrink": 1, "margin": "0px 8px 0px 0px", "border-radius": "18px", "overflow": "hidden"}}
```

Standard horizontal-scroll image strip (fallback):

```json
{"id": "image_strip", "component": "List", "children": ["image_0", "image_1", "image_2"], "direction": "horizontal", "align": "start"}
{"id": "image_0", "component": "Image", "url": {"path": "/card/images/0"}, "fit": "cover", "styles": {"width": "312px", "height": "252px", "margin": "0px 12px 0px 0px", "border-radius": "20px"}}
```

Selection strategy (mandatory):

1. Use `Row + Image(fit: cover) + flex-grow/flex-shrink` for a fill row first (default)
2. If this causes static clipping on the target narrow screen, the last image gets swallowed, or the visible width is clearly insufficient, switch to `List(horizontal)` (fallback)
3. Do not default to horizontal scrolling just because there are multiple images — scrolling is a risk mitigation, not a default style

Validation and fallback strategy:

1. Preferred approach: `Row + Image(fit: cover) + flex` — verify that it truly fills
2. If images are clipped and cannot be fully shown, switch to `List(horizontal)` and assign a fixed `px` width based on image count (define explicit widths for 1/2/3/4 images)
3. If the renderer has unstable `flex` behavior, do not keep relying on auto-stretch — use predictable fixed-width strategy directly
4. Regardless of the approach, ensure:
   - Images remain on one row
   - Layout is predictable as image count changes
   - No static clipping, large empty slots, or thumbnail feel

## Compatibility Driven Layout

Because the DTO component Python will handle more DTOs of the same type, the component structure itself must also be compatible:

- Do not build layout on the assumption that "every field happens to exist and every text happens to be short"
- Explicitly layer fields into required / optional
- Design the layout-collapse path for when optional fields are missing
- For high-volatility fields like main title, subtitle, institution name, source, and tags, prepare multi-line or truncation strategies by default

## Component Expressiveness Hierarchy

When data can be represented by multiple component types, prefer the one with higher expressiveness over plain `Text`.

| Tier | Components | When to use |
|---|---|---|
| Tier 1 — Visual | `Chart`, `Image`, `Carousel`, `Video`, `Lottie` | Numerical comparison → Chart; visual content → Image/Carousel; animation → Lottie |
| Tier 2 — Structured | `Card`, `RichText`, `Table`, `Markdown` | Formatted emphasis → RichText; tabular data → Table; rich text blocks → Markdown |
| Tier 3 — Basic | `Text`, `Icon`, `Image`,`Divider`, `Button` | Single labels, icons, separators — essential but low expressiveness on their own |

Rules:

- During layout planning, check: "Is any section using Tier 3 where Tier 1–2 would better serve the data?"
- Do not upgrade when data does not support it — a single number does not need a Chart; a short label does not need RichText
- Tier 1 components require explicit `styles.height` (no intrinsic auto-sizing)
- Card mode: at most 1 Tier 1 component as visual focal point
- This hierarchy supplements, not overrides, content budget and height boundary rules

## Chart Height Specification

Chart has no intrinsic height; the renderer needs an explicit `styles.height` to allocate canvas space.

| `chartType` | Recommended height | Rationale |
|---|---|---|
| `donut` | `300px` – `400px` | Compact, roughly square; `300px` for cards, `400px` for pages |
| `bar` | `400px` – `500px` | Needs vertical space for bars + axis labels |
| `line` | `400px` – `500px` | Needs vertical space for trend lines + axis labels |
| `bar_grouped` | `400px` – `500px` | Same as bar, may need slightly more for legend |

In card mode, prefer the lower end of each range. In page mode, use the upper end. If the Chart is the primary visual focal point of a card, use the upper end regardless of mode.
