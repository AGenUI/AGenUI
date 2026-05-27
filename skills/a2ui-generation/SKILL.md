---
name: a2ui-generation
description: |
  Design and generate A2UI updateComponents and updateDataModel payloads for three modes (DTO component, non-DTO component, non-DTO page). Use when asked to create or refine A2UI cards, components, or pages from DTO/JSON/business data, generate Python transformer code for DTO-driven components, iterate on existing A2UI output files by diff, improve mobile UI quality, or validate A2UI rules such as surfaceId, root, path binding, and component-vs-page boundaries.
---

# A2UI Generation

## What This Skill Does

- Generate or refactor A2UI `updateComponents` and `updateDataModel`
- Output a unified Python transformation entry in `DTO Component` mode
- Iterate on existing files rather than rewriting everything from scratch each round
- Gate quality through design review and validation scripts

## Execution Boundary

- Do not proactively search, read, borrow from, or imitate historical examples, old artifacts, sample pages, sample JSON, sample HTML, or screenshot outputs in the repository unless the user explicitly requests it
- By default, rely only on the user's current input, currently attached files, and this skill's own documentation
- If the user provides a DTO, do not hard-code business facts

## First Principle: User Requirements First

- Global first principle: `User requirements first`
- When a feature explicitly requested by the user conflicts with a skill default rule, prioritize the user requirement
- Do not "pretend to comply with the skill"; explicitly record the conflict and apply a targeted exemption during the validation phase
- Exemptions are scoped only to items the user explicitly requested — never use them as an excuse to disable validation globally
- Choose an appropriate layout style based on the user's requirements and intent; using the same layout style and color scheme across multiple queries is forbidden

## Query / Search Term Scenario Detection

Before entering mode selection, determine whether the user input is an **information summary card** scenario:

**Trigger conditions**: The user provides a natural language search query, place name, person name, event name, or concept word (e.g., "Snow in the Forbidden City", "West Lake", "Su Dongpo") and asks for an information card.

**⚠️ CRITICAL WARNING — English model content inflation risk**: When generating cards for query/search-term inputs in English, there is a strong model tendency to produce article-style content with long paragraphs, full narrative sections, and detail-page structures. This is **always wrong** for summary cards. Treat every word below as a hard constraint, not a suggestion.

**Mandatory card shape for this scenario (non-negotiable)**:
- Structure locked to: `Title → Core attributes (1–3) → Brief summary (≤ 2 lines) → Tag group → Optional CTA`
- Main sections ≤ 3
- Body text ≤ 2 paragraphs; each paragraph ≤ 3 lines
- **FORBIDDEN**: Dynamic `List` child templates (i.e., `children.componentId`-driven template rows such as `tips_item_template`) — this is a detail-page structure, not a summary card
- **FORBIDDEN**: Full-width hero image (`aspect-ratio: 16/9` + `width: 100%`) — this is a page header, not a card element
- **FORBIDDEN**: Expanding the card into an article or content detail page (multiple long body paragraphs + full lists)
- Total card height must not exceed 2/3 of screen height

**Canonical layout patterns** (choose one; do not invent new structures):
- `Horizontal summary card`: small thumbnail on the left + title / core metrics / tags / CTA on the right (suitable for POIs, attractions, products)
- `Vertical summary card`: title section at top + core metrics row (2–3 numeric metrics side by side) + one-line summary + tags + CTA (suitable for events, people, concepts)

**Pre-generation self-check (mandatory before writing any JSON)**:
- [ ] Main sections ≤ 3?
- [ ] No dynamic List sub-templates?
- [ ] No full-width hero image?
- [ ] Body paragraphs ≤ 2, each ≤ 3 lines?
- [ ] Total height within 2/3 screen?
If any item fails → converge first or escalate to page explicitly. Never bypass.

## Mode Selection

Before starting, determine:

1. Does the user want a `component/card` or a `full page`?
2. Has the user provided a DTO?

Default to generating a component / card for the user, unless the user explicitly says they want a full page.

Then enter exactly one of the following three modes:

### Mode 1: DTO Component

Applicable when:

- The user has provided a DTO
- The user wants a component / card

Deliverables:

1. Python transformer code
2. `updateComponents` JSON
3. `updateDataModel` JSON

Unified entry function:

```python
def build_component_payload_from_dto(dto: dict) -> tuple[dict, dict]:
    ...
```

### Mode 2: Non-DTO Component

Applicable when:

- The user has not provided a DTO
- The user wants a component / card

Deliverables:

1. `updateComponents` JSON
2. `updateDataModel` JSON

Mandatory order (must not be reversed):

1. Output UI layout (`updateComponents`) first
2. Output data (`updateDataModel`) second

### Mode 3: Non-DTO Page

Applicable when:

- The user has not provided a DTO
- The user wants a full page

Deliverables:

1. `updateComponents` JSON
2. `updateDataModel` JSON

Mandatory order (must not be reversed):

1. Output UI layout (`updateComponents`) first
2. Output data (`updateDataModel`) second

Supplementary rules:

- Only the three modes above are defined by default
- If the user provides a DTO but explicitly wants a full page, do not silently apply one of these modes; ask first whether to design the page as DTO-driven

## Read Only What You Need

Do not read all sub-documents by default. Load only what the current task requires:

| Task type | Required docs | Load on demand |
| --- | --- | --- |
| DTO component | [`docs/dto-component-mode.md`](docs/dto-component-mode.md), [`docs/component-design.md`](docs/component-design.md) | [`docs/component-catalog.md`](docs/component-catalog.md), [`docs/data-binding.md`](docs/data-binding.md), [`docs/visual-interaction.md`](docs/visual-interaction.md) |
| Non-DTO component | [`docs/component-catalog.md`](docs/component-catalog.md), [`docs/component-design.md`](docs/component-design.md) | [`docs/data-binding.md`](docs/data-binding.md), [`docs/visual-interaction.md`](docs/visual-interaction.md) |
| Non-DTO page | [`docs/component-catalog.md`](docs/component-catalog.md), [`docs/page-design.md`](docs/page-design.md), [`docs/visual-interaction.md`](docs/visual-interaction.md) | [`docs/data-binding.md`](docs/data-binding.md), [`docs/review-validation.md`](docs/review-validation.md) |
| Bug fix / review / iterating on existing artifacts | [`docs/review-validation.md`](docs/review-validation.md) | Whichever doc is directly related to the issue |

Additional notes:

- Load [`docs/component-catalog.md`](docs/component-catalog.md) when you need to verify atomic components, charts, fields, allowed values, or style whitelists
- Load [`docs/data-binding.md`](docs/data-binding.md) when you need to verify path binding, template binding, or relative path rules
- Load [`docs/component-design.md`](docs/component-design.md) when you need to verify component height budgets, card content budgets, or multi-column text budgets
- Load [`docs/page-design.md`](docs/page-design.md) when you need to verify full-page structure, layout composition, or page-level sectioning

## Output Persistence

Final artifacts should be written to files by default, and the user should be told the paths explicitly.

Priority order:

1. If the user specifies a directory or filename, save according to that
2. If the user provides an existing artifact directory, prefer saving near that context
3. Otherwise choose a clear, sensible, easy-to-find location

Default file naming:

- `*_components.json`
- `*_datamodel.json`
- `*_transformer.py` or `*_vo.py`

Non-DTO mode write order (mandatory):

1. Generate and save `*_components.json` first
2. Generate and save `*_datamodel.json` second

To save tokens:

- Write the first draft to disk immediately after generation
- If the user continues modifying, iterate on the existing file by default
- Each round of changes should edit the file and work from a diff — do not repaste the entire JSON in the conversation

## Workflow

1. Read user input; confirm mode, data source, interaction requirements, and visual constraints
2. Load only the sub-documents the current task truly needs
3. Before formal output, explicitly list the layout rationale: at minimum describe the main sections, visual focal point, information rhythm, key horizontal relationships, and the role of images
4. Based on that layout rationale, draft an internal first version, then perform at least `1` explicit design improvement before proceeding to formal output
5. Output the first draft formally and write it to disk immediately (non-DTO mode: components before datamodel, mandatory)
6. Run [`scripts/validate_a2ui.py`](scripts/validate_a2ui.py) immediately; if it fails, fix the file directly and re-run until it passes
7. Only after the script passes for the first time, perform `1–2` rounds of model-level design review and improvement on the on-disk files
8. During model review rounds: address palette coherence, information hierarchy, horizontal relationships, and premium feel; also perform a dedicated "protected content abnormal wrapping" check on all horizontal layouts — prioritize checking whether short phrases, CTAs, ratings, times, and prices are being squeezed and broken by narrow fixed widths
9. If the model review modifies files, re-run the validation script; deliver only after it passes again
10. At delivery, clearly state the output file paths; if placeholder links were used, explicitly remind the user to replace them

When a "user explicitly required item conflicts with a rule":

- Use `overrides.json` to exempt only the conflicting checks at minimum scope
- Exempt only the relevant check; do not disable other validations as collateral

## Non-Negotiables

Only truly irreplaceable business invariants that scripts cannot fully substitute are listed here:

- Non-DTO mode must produce UI layout (`updateComponents`) before data (`updateDataModel`)
- Do not output fake buttons; clickable elements must use a real `Button + action`
- Real buttons must have visible label text; do not rely solely on `variant` to guess the runtime background color
- DTO fields must pass a semantic validity check before display; non-empty does not equal informative
- When semantic meaning depends on a combination of fields (e.g., status + time), perform the combination mapping first, then decide whether to display or omit
- `Component/card` mode must not silently become page-like; do not deliver a near-full-screen large card
- Component mode content should converge first; escalate to page only when convergence fails, and do so explicitly
- **Card hard gate (self-check before generating any JSON)**: main sections ≤ 3; no dynamic `List` sub-templates (i.e., `children.componentId`-driven template rows); no full-width hero image (`aspect-ratio: 16/9` + `width: 100%`); body paragraphs ≤ 2; if any item fails, converge first or escalate to page explicitly — bypassing is not allowed
- **Information summary card scenario (query/search-term driven)**: structure is locked to `Title → Core attributes (1–3) → Brief summary (≤ 2 lines) → Tag group → CTA`; expanding into article narrative structure is forbidden
- **Tag group and CTA must be in separate rows**: when a card bottom has both a tag group (>= 2 tags) and a CTA button, they must be separated using Column (tag row first, CTA below); placing them in the same Row is forbidden — even flex-wrap and flex-shrink cannot prevent the CTA from overflowing and being clipped
- **⚠️ English model content inflation guard**: When query input is in English, the model tends to generate long-form article content. For every card task, actively resist generating: multiple long body paragraphs, full visitor tip lists, narrative sections, or any structure that resembles a detail page. If you find yourself writing more than 2 paragraphs of body text, stop and converge immediately
- A single card defaults to one main card shell; a full visual shell (`background + border-radius + drop-shadow`) inside a `Card` is forbidden
- A row of multiple images defaults to `Row + flex` fill; downgrade to a horizontal scroll container (`List direction=horizontal`) only when narrow-screen clipping risk is real
- Protected content — short phrases, CTAs, rating values, prices, times — must not wrap or fragment due to a fixed narrow width
- `CTA` buttons default to content-driven width via `padding + border-radius`; do not write a fixed narrow width unless explicit alignment requirements exist and readability has been verified
- When design requires images, do not fabricate non-existent image URLs
- After the first draft, always iterate on the on-disk file; do not regenerate the entire artifact each round
- Before formal output, the layout rationale must be explicitly listed; skipping layout planning and jumping straight to JSON is not allowed
- The layout rationale must cover at minimum: main sections, visual focal point, content rhythm, key component relationships, and the role of images/charts
- Before formal output, at least `1` explicit improvement round is required; the first version in your head must not be delivered directly as the final first draft
- This explicit improvement round defaults to prioritizing premium feel, design quality, and overall completeness — not just literal additions

## Resources

- Sub-document index: [`reference.md`](reference.md)
- Component catalog: [`docs/component-catalog.md`](docs/component-catalog.md)
- Data binding: [`docs/data-binding.md`](docs/data-binding.md)
- DTO mode: [`docs/dto-component-mode.md`](docs/dto-component-mode.md)
- Component design: [`docs/component-design.md`](docs/component-design.md)
- Page design: [`docs/page-design.md`](docs/page-design.md)
- Visual & interaction: [`docs/visual-interaction.md`](docs/visual-interaction.md)
- Review & validation: [`docs/review-validation.md`](docs/review-validation.md)
- Validation script: [`scripts/validate_a2ui.py`](scripts/validate_a2ui.py)
