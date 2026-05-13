# A2UI Reference Index

`reference.md` serves as navigation only; it no longer contains the full mixed handbook.

Usage principles:

- Do not load all sub-documents at once by default
- Load only the `1–2` documents the current task truly needs
- For machine-verifiable rules, the [`scripts/validate_a2ui.py`](scripts/validate_a2ui.py) script is the authoritative source
- Default process order: run the script to validate and fix until passing, then do model review and optimization; if model review modifies files, re-run the script

## Read By Knowledge Domain

- Component protocol, fields, allowed values, style whitelist, font size spec:
  [`docs/component-catalog.md`](docs/component-catalog.md)
- Path binding, template binding, relative paths, list attribute binding:
  [`docs/data-binding.md`](docs/data-binding.md)
- DTO component mode, DTO discipline, unified function name, DTO deliverables:
  [`docs/dto-component-mode.md`](docs/dto-component-mode.md)
- Component/card design, height budget, content budget, multi-column text budget:
  [`docs/component-design.md`](docs/component-design.md)
- Page design, page-level structure, component-vs-page boundary:
  [`docs/page-design.md`](docs/page-design.md)
- Visual direction, interaction, buttons, image strategy, anti-patterns:
  [`docs/visual-interaction.md`](docs/visual-interaction.md)
- Review rounds, review checklist, human/machine validation boundary:
  [`docs/review-validation.md`](docs/review-validation.md)

## Read By Task

- DTO component:
  Start with [`docs/dto-component-mode.md`](docs/dto-component-mode.md) and [`docs/component-design.md`](docs/component-design.md)
- Non-DTO component:
  Start with [`docs/component-catalog.md`](docs/component-catalog.md) and [`docs/component-design.md`](docs/component-design.md)
- Non-DTO page:
  Start with [`docs/component-catalog.md`](docs/component-catalog.md), [`docs/page-design.md`](docs/page-design.md), and [`docs/visual-interaction.md`](docs/visual-interaction.md)
- Bug fix / review / iterating on existing files:
  Start with [`docs/review-validation.md`](docs/review-validation.md)

## Validation Source

Script location:

- [`scripts/validate_a2ui.py`](scripts/validate_a2ui.py)
