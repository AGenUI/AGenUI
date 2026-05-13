# Visual Interaction

## Default Visual Direction

Unless the user explicitly requests minimal, simple, plain, or understated, the default visual direction should be:

- Refined
- Beautiful
- Visually striking
- Layered
- Suited for mobile card-style presentation
- Light-background card + accent color

Do not default to "fewest components" or "lowest style cost" — actively create stronger design expression within what the protocol allows.

## Page Palette Coherence

For `full page` tasks, the palette should first satisfy "the whole page is one visual language" before considering whether individual sections are eye-catching enough.

Priority principles:

- Start by establishing a full-page main color band, e.g. "warm off-white base + low-saturation blue-grey accent" or "light grey-white base + golden-brown accent"
- If the `hero` / heading area uses dark color, it can only be used once as a clear, restrained opening — subsequent sections need a smooth return, not repeated dark-light jumps
- If the main body of the page after the hero is light-background, prefer pulling the `hero` into the same light-background system too, letting the image itself deliver the visual impact rather than relying on an extra large dark block
- Slight brightness changes between sections are allowed, but they should feel like layering on the same sheet of paper, not different templates stitched together
- Default to `1` primary accent color + `1` secondary accent color; do not change the main color scheme per section
- Tags, badges, route ribbons, buttons, and callouts should reuse the same accent color logic, not each go their own way

Experience rules for page mode:

- If you can immediately divide the page into "dark section", "light section", "another dark section", it is likely not cohesive
- If you ignore all image areas and look only at background colors and text colors, the page should still look like one continuous work
- `chart`, `night`, `cta`, and other sections that are easily made into full dark blocks: try light-background expression first; preserve a large dark block only when the user explicitly needs strong contrast or a night atmosphere requires it
- A page needs rhythm, not stacked contrast; color rhythm should be continuous, gentle, and predictable

## Real Interaction

Elements that are designed to be clickable must use real interaction by default:

- Use `Button + functionCall` for navigation / opening links
- Use `Button + event` for business actions / host integration / continued response
- Do not output fake buttons; do not let `Text` or plain containers pretend to be clickable

If the user has not provided a specific URL / schema / event but the design still requires a real button:

- Still output a real `Button` first
- A placeholder `functionCall.openUrl` may be used temporarily
- The `url` can use an obvious placeholder link, e.g. `https://example.com/todo`
- Do not degrade to a fake button just because a real link is missing
- The final reply must remind the user to replace with the real link, schema, or event

Button visibility safety net:

- Do not assume `variant` will always provide a readable background color on all renderers
- If button label text uses a light color (e.g. white), explicitly provide a dark background (e.g. `background-color`) on the button container, or switch to dark label text
- A button should at minimum have a clickable form: both `padding` and `border-radius` are recommended
- Before delivery, manually check: CTA label text is clearly visible against the actual background, not blending into it

## Image Sourcing

When the design requires image decoration:

- If the user's provided materials already contain usable images, prioritize making full use of them
- If the user has not provided images but the visual clearly needs them, search online for suitable real images before using them
- Do not fabricate non-existent image URLs
- If no reliable, suitable, accessible image can be found, switch to an image-free approach rather than filling in a random URL

## Anti-Patterns To Avoid

Avoid the following structures by default:

- Using a `Text` styled to look like a button as a CTA without a `Button.action`
- Turning an entry that should be clickable into explanatory text with no real event response
- Button text color relying on `variant` guesswork, making button labels invisible
- Adding a whole extra grey / light / host-background backing plate on the outermost layer just to make the main card stand out
- After wrapping in `Card`, adding another full visual shell (`background-color + border + border-radius + drop-shadow`) to the immediate child layer
- Each badge, icon area, metric area, and callout area inside the main card being wrapped in its own separate `Card`
- Making every small module into an independent mini-card for "refinement", fragmenting the visual focal point
- Mechanically turning sentence-level supporting evidence, credible corroboration, and supplementary notes into a row or column of heavy capsule blocks, causing the supporting area to steal the main focal point
- Downgrading all horizontal relationships to plain vertical stacking the moment a narrow-screen risk is perceived, sacrificing spatial tension and design quality that could otherwise hold
- Page hero is a large dark block, then it suddenly cuts to pure white, then cuts to another dark block — creating a "template-stitching" feel
- Giving `hero`, `chart`, `night`, `cta` each a completely different color palette to manufacture "premium feel"
- Tags, buttons, badges, and description blocks each using a different hue, leaving the page without a unified visual thread
- **Placing a tag group (2 or more tags) and a CTA button in the same Row — even with flex-wrap: wrap on the tag group and flex-shrink: 0 on the CTA — causing the CTA to be pushed outside the card's right boundary and clipped invisible**
