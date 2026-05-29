# Review Validation

## Purpose

This document unifies the review and validation process after a first draft, with the goal of:

1. Removing prose-like stacking; improving layout and visual quality
2. Ensuring readability, tappability, and usability on small screens
3. Passing both script and model validation without violating user requirements

## End-to-End Flow

After the first draft is written to disk, execute the following flow by default:

1. Read the on-disk file (review based on file, do not start a new draft)
2. Run the script validation (`scripts/validate_a2ui.py`)
3. If it fails, fix the original file directly and re-run until it passes
4. Only after the script passes for the first time, begin model design review and continue optimizing via diff
5. At least `1` model review round; if the output is still plain / prose-like / has risks, do another `2nd` round
6. If the model review modifies files, re-run the script validation; if it fails, continue fixing until it passes again
7. Only after the script passes and model review is complete can delivery happen

## Round Checklist (Every Round)

Check the following universal items every round:

- Has the mode been clearly identified: `DTO Component` / `Non-DTO Component` / `Non-DTO Page`
- Can all data paths be found in the `dataModel`
- Are all component names in the allowlist (see `reference/component-catalog.md`); no hallucinated names like Badge, Spacer, Stack, BarChart
- Do lists use template capability rather than hard-coded repeated components
- Is the layout clearly distinct from plain text stacking (not just "one row after another")
- Are atomic component capabilities being fully used to express hierarchy and emphasis
- Was the layout rationale explicitly stated before formal output, rather than skipping planning and going straight to generation
- Was at least `1` explicit design improvement completed before formal output
- At `360px` width, does any overflow, horizontal blowout, or primary information invisibility occur
- Are buttons visible and readable (text contrasts sufficiently with background, does not rely on `variant` to guess background color)
- Do images come from user materials or genuinely verifiable sources (no fabricated URLs)
- Do font sizes comply with the specification
- Are there any high-risk break candidates: short phrases, short CTAs, rating values, times, prices
- For full pages: is there obvious "collage-style" color palette jumping? Do the hero, main body sections, and night/chart/CTA areas belong to the same color band system

## Page Palette Review

When the task is `Non-DTO Page`, after the script first passes, perform an additional "page palette coherence focused review":

1. Ignore images; look only at component background colors, text colors, button colors, and tag colors
2. Judge whether the page looks like one cohesive work, not multiple templates stitched together
3. Focus checks:
   - Does the `hero` / heading disconnect from the rest of the page body
   - Is there a back-and-forth jump: "dark hero -> pure white mid-section -> dark night section -> dark CTA"
   - Has the accent color proliferated into multiple hues
   - Do `chart`, `night`, `cta` sections truly need a full dark background
4. If jarring transitions exist, fix in the following priority order:
   - Unify the full-page base color band
   - Pull dark sections back to a light-background expression
   - Reduce the number of accent colors
   - Finally, preserve one necessary dark opening area if absolutely needed

Experience thresholds:

- The full page defaults to allowing at most `1` clear dark opening area
- If the `hero` is already dark, avoid multiple independent dark large sections after it
- If the user has not explicitly requested strong contrast, default to "unified" over "contrasting"

## Mode-Specific Checks

### For `Component/Card` (DTO + Non-DTO)

- Is height within the single-screen `1/3` budget, avoiding page-sized large cards
- Are main sections converged to `2–3` or fewer; is primary info focused enough
- Is there a double card shell: `Card` outer shell exists, and inner layer adds another full visual shell
- In multi-column rows, are "protected columns" distinguished from "compressible columns"
- Do captions / unit labels / short phrases in protected columns maintain minimum readable form
- Do sections that should be left-right opposed actually anchor both sides, rather than plain concatenation
- Is there "no real competition but breaks first" (e.g. initial width budget error causing pseudo-wrapping)
- Is there "left description still full, right CTA / rating / status breaks first" — priority inversion
- Are short CTA text, rating values, times, prices, and status words being squeezed into fragments by narrow fixed-width containers
- Is the fixed-width button truly necessary; if just for alignment, should it revert to content-driven width
- Is supporting evidence over-cardsified, causing focal point fragmentation
- For multiple images in a row: is `Row + flex` fill tried first; only downgrading to `List(direction=horizontal)` when narrow-screen clipping risk is real
- Is horizontal scrolling used only for local horizontal consumption areas, not for main content areas

## Protected Content Wrap Review

For any horizontal layout, during the model review phase after the script first passes, perform an additional "protected content abnormal wrapping focused review":

1. List the protected content in the current section:
   - CTA label text
   - Status words / short badges
   - Rating values / prices / times
   - Short descriptions next to icons
2. Verify each item:
   - Is it in a narrow fixed-width container
   - Could character-by-character wrapping, single-character hanging, or punctuation on its own line occur
   - Does the protected column break first, before weak information compresses
3. If the answer is "yes", fix in the following priority order:
   - Widen the protected column
   - Lower the priority of weak information or move it to the next line
   - Change to a left-right structure that better matches role assignment
   - Only then truncate weak information

Focused anti-patterns:

- Short CTAs like "Book Now" being cut into two lines by a narrow button
- Short values like `4.9`, `22:30`, `¥268` being compressed into fragments by fixed-width columns
- Left-side long description still fully expanded while the right-side rating column or button column has already broken

### For `DTO Component` Only

- Is the Python entry fixed as `build_component_payload_from_dto`
- Do `*_components.json` and `*_datamodel.json` come directly from running Python
- Are `required` and `optional` fields clearly distinguished
- Does missing a key field allow an explicit error, and does missing an optional field correctly degrade
- When a field is missing, is it handled via "omitting the component/section" rather than leaving an empty shell
- Are low-information-value fields (generic / section words) filtered or downgraded
- When semantic meaning depends on combined fields (e.g. business status), is combination modeling done before display
- Is Python compatible with more DTOs of the same type, not just tailored to one sample
- Are `openStatus/openStatusCode/status/openTime/*` combined into readable status copy, not single-field output

## Validation Script

Script location:

- [`scripts/validate_a2ui.py`](scripts/validate_a2ui.py)

Common invocations:

- `python scripts/validate_a2ui.py components.json datamodel.json`
- `python scripts/validate_a2ui.py combined.md`
- `python scripts/validate_a2ui.py components.json datamodel.json overrides.json`
- `python scripts/validate_a2ui.py combined.md overrides.json`

## User Requirement First (Targeted Override)

When a user's explicit requirement conflicts with the default specification:

1. Satisfy the user's explicit requirement first
2. Exempt only the conflicting check items (minimum scope)
3. Keep all other checks enabled

Recommended `overrides.json`:

```json
{
  "userRequirementFirst": true,
  "allowUnsupportedStyles": ["gap"],
  "allowFontSizes": ["12px", "13px"]
}
```
