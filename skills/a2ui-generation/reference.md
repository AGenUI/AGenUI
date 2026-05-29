# A2UI Reference Index

`reference.md` serves as navigation only; it no longer contains the full mixed handbook.

Usage principles:

- Do not load all sub-documents at once by default
- Load only the `1–2` documents the current task truly needs
- For machine-verifiable rules, the [`scripts/validate_a2ui.py`](scripts/validate_a2ui.py) script is the authoritative source
- Default process order: run the script to validate and fix until passing, then do model review and optimization; if model review modifies files, re-run the script

## Read By Knowledge Domain

- Component protocol, fields, allowed values, style whitelist, font size spec:
  [`reference/component-catalog.md`](reference/component-catalog.md)
- Path binding, template binding, relative paths, list attribute binding:
  [`reference/data-binding.md`](reference/data-binding.md)
- DTO component mode, DTO discipline, unified function name, DTO deliverables:
  [`reference/dto-component-mode.md`](reference/dto-component-mode.md)
- Component/card design, height budget, content budget, multi-column text budget:
  [`reference/component-design.md`](reference/component-design.md)
- Page design, page-level structure, component-vs-page boundary:
  [`reference/page-design.md`](reference/page-design.md)
- Visual direction, interaction, buttons, image strategy, anti-patterns:
  [`reference/visual-interaction.md`](reference/visual-interaction.md)
- Design quality review, palette/layout/decoration/theme audit:
  [`reference/design-review.md`](reference/design-review.md)

- Spacing scale, shadow elevation, border radius system:
  [`reference/spacing-elevation.md`](reference/spacing-elevation.md)
- Expressiveness techniques: inline color, color blocks, icons, opacity, pseudo-gradients:
  [`reference/expressiveness-toolkit.md`](reference/expressiveness-toolkit.md)
- Review rounds, review checklist, human/machine validation boundary:
  [`reference/review-validation.md`](reference/review-validation.md)

## Read By Task

- DTO component:
  Start with [`reference/dto-component-mode.md`](reference/dto-component-mode.md) and [`reference/component-design.md`](reference/component-design.md)
- Non-DTO component:
  Start with [`reference/component-catalog.md`](reference/component-catalog.md) and [`reference/component-design.md`](reference/component-design.md)
- Non-DTO page:
  Start with [`reference/component-catalog.md`](reference/component-catalog.md), [`reference/page-design.md`](reference/page-design.md), and [`reference/visual-interaction.md`](reference/visual-interaction.md)
- Bug fix / review / iterating on existing files:
  Start with [`reference/review-validation.md`](reference/review-validation.md)

## Validation Source

Script location:

- [`scripts/validate_a2ui.py`](scripts/validate_a2ui.py)
