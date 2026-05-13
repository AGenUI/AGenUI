# Page Design

## Scope

This document covers `full page` mode only, not components/cards.

## Page Boundary

- `Full page`: defaults to at least `2–3` screens
- A page should contain multiple content sections, not just a stretched card

## Page Structure

A full page typically combines multiple sections using `Column`, with `Divider` in between:

```json
{
  "id": "root",
  "component": "Column",
  "children": ["section1", "divider1", "section2", "divider2", "section3"]
}
```

A page should typically have:

- A clear main heading area
- At least `2–3` content sections
- Clear information hierarchy
- A rhythm that allows continuous downward scrolling, not one huge content block

## Page-Only Content

The following content is better suited for pages than cards:

- Long lists
- Long tables
- Long timelines
- Multiple large parallel sections
- Full narrative chains
- Multi-paragraph explanatory text

## Page Layout Guidance

- Pages can have richer sectioning, rhythm, and visual transitions
- Do not mechanically apply a three-column statistics card template to the page summary area
- Multiple sections are allowed inside a page, but each section should still have clear hierarchy
- If local horizontal information truly suits horizontal browsing, a local horizontal scroll container can be used
- Do not make the entire page's main content area require horizontal scrolling to be fully seen

## Pre-Output Layout Planning

Before formally outputting `updateComponents`, explicitly write out a layout rationale. Do not jump directly to generating JSON.

This pre-output layout rationale must answer at minimum:

- What main sections will the page have
- What is the visual focal point of the first screen / hero
- How the information rhythm unfolds: hook first, then explain, then expand, then close
- Which relationships suit horizontal spread, which should scroll vertically
- What roles images, charts, route ribbons, and timelines each play

Recommended minimal template:

1. `Page skeleton`
   - Example: `hero -> atmosphere guide -> main schedule -> evening closing -> practical info -> CTA`
2. `Visual focal point`
   - Example: first screen builds impact through large image; mid-section builds premium feel through whitespace and headings; closing section uses a softer accent color
3. `Information rhythm`
   - Example: overview first, then expand Day 1/Day 2, then evening scenery mood and practical reminders
4. `Key layout relationships`
   - Example: timeline with fixed info on the left and flexible copy on the right; route overview as a full ribbon, not easily-squeezed chip strings

## Explicit Improvement Before Formal Output

After completing the layout rationale above, do not directly deliver the first version as formal output. At least one explicit improvement round is required, and it must explain:

- What is not premium enough / not complete enough / not well-designed in the first version
- What the second version intends to strengthen
- Whether the improvement is reflected in layout, visual focal point, rhythm, palette, and information hierarchy — not just rewriting a few lines of copy

Default priority for improvement:

- Remove template feel; strengthen overall page coherence
- Improve premium feel and refinement, not just stacking more components
- Optimize the transition between the first screen and subsequent sections
- Optimize the primary/secondary relationship between images and text
- Optimize the presentation of routes, times, CTAs, and other high-risk horizontal information

## Page Escalation Reminder

If the user originally wanted only a component/card, but the content clearly requires:

- Multiple main sections
- Continuous vertical scrolling
- Multi-paragraph narrative explanation
- Long lists or long tables

Then explicitly suggest: this is better suited for `page` mode.
