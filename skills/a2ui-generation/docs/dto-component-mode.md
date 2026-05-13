# DTO Component Mode

## When To Use

Enter this mode only when both of the following conditions are met:

- The user has provided a DTO
- The user wants a `component/card`

If the user provides a DTO but wants a full page, do not default into this mode — ask first.

## Deliverables

Default deliverables are three items:

1. Python transformer code
2. `updateComponents` JSON
3. `updateDataModel` JSON

Important notes:

- In this mode, Python is the single source of truth
- Both JSON files must be produced by running the Python code
- Do not deliver Python on one side and hand-write a separate independent JSON on the other

## Unified Python Entry

The unified entry function name for external delivery must be:

```python
def build_component_payload_from_dto(dto: dict) -> tuple[dict, dict]:
    ...
```

Rules:

- The function name is fixed: `build_component_payload_from_dto`
- The return value is fixed: `(update_components, update_datamodel)`
- Additional private helper functions for VO, intermediate mapping, or utility logic are allowed
- Do not change the external delivery entry function name

## Python As Single Source Of Truth

In DTO component mode, the final delivery chain must be:

`DTO -> Python -> updateComponents/updateDataModel`

Not:

`DTO -> Python`

plus a separate fork:

`DTO -> Manually hand-written JSON pair`

Mandatory requirements:

- Write the Python transformation logic first
- Run Python to produce the two JSONs
- The final on-disk `*_components.json` and `*_datamodel.json` must come from Python's output
- If Python is modified, re-run it to refresh both JSONs
- Do not manually maintain a final JSON that drifts from Python

## Compatibility First

In DTO component mode, the generated Python is not a one-off script — it will be used to process more DTOs of the same type.

Design it as a reusable transformer, not something that barely works for the current single DTO.

Mandatory requirements:

- For every field, consider compatibility strategies for missing values, empty values, unstable types, and minor naming variations
- For optional fields, default to graceful degradation — do not let the entire transformer fail lightly
- For required fields, explicit errors are allowed, but the error must identify which key field is missing
- Do not treat the current sample DTO as the only fixed structure

## DTO Data Discipline

When the user provides a DTO:

- All display values must come directly from the DTO, or from deterministic transformations of DTO fields
- Hard-coding business copy, summaries, marketing language, or supplementary facts is forbidden
- Allowed processing: concatenation, enum mapping, formatting, list reorganization
- Every dataModel display field should be traceable to its DTO source
- A field being non-empty does not equal it being informative; apply semantic validity filtering

Semantic validity filtering (mandatory):

- For status/label short text, if it is a generic word, placeholder word, group name, or section name, treat it as "low information value" and omit it
- Only display values that provide clear business meaning (can help in judging, deciding, or understanding the current object's state)

Field combination semantics (mandatory):

- Do not treat every field as an independently displayable unit; first determine whether it is a "label field", "status field", or "detail field"
- If a field is just a label (e.g. `openStatus = "Business Hours"`), combine it with the corresponding status/detail fields before displaying
- For business hours information, prefer combining the following fields to generate the final copy rather than displaying any single field alone:
  - Status signal: `status/opentimeStatus`, `openStatusCode`
  - Time detail: `status/openTime/shortOpenTime`, `status/openTime/normalTimeText`, `openTime`
  - Label field: `openStatus` (used only as semantic label or can be omitted)
- Recommended output: combined semantic copy like `"Open · 10:00–23:30"`, `"Closed · Opens tomorrow at 10:00"` — avoid displaying just "Business Hours"

## Auto Hide By Omission

In DTO component mode, "auto-hide" capability is supported, but the implementation is not runtime conditional rendering — it is conditional generation at the Python transformation phase.

Correct approach:

- If a DTO field is missing, empty, or does not constitute a valid display value, do not write the corresponding component
- Also do not write the corresponding dataModel field
- If the key fields for an entire section are all missing, do not output the entire section at all

This means "auto-hide" translates to:

- Not adding that component id to the parent component's `children`
- Not generating that component definition
- Not generating that component's corresponding data

Do not use the following pseudo-solutions:

- Generating the component but with an empty string
- Using `display: none` or `visibility: hidden` for data-driven hiding
- Keeping empty containers, empty sections, or empty titles and expecting the layout to collapse on its own

Recommended pattern:

```python
def build_component_payload_from_dto(dto: dict) -> tuple[dict, dict]:
    components = []
    root_children = []
    data_value = {}

    subtitle = dto.get("subtitle")
    if subtitle:
        components.append(
            {"id": "subtitle", "component": "Text", "text": {"path": "/card/subtitle"}}
        )
        root_children.append("subtitle")
        data_value["subtitle"] = subtitle

    # Same pattern for other fields — omit if missing
    ...
```

## Required Vs Optional Fields

When writing a transformer, first divide fields into two categories:

### Required Fields

These fields may raise an error when missing:

- Fields without which the card's primary semantics cannot be identified
- Fields without which the main title / main status / primary visual / primary CTA cannot be formed
- Fields whose absence leaves the card with no clear meaning

Requirements:

- Error messages must explicitly identify the missing field or missing field group
- Do not use vague errors like "invalid data"
- Do not continue assembling a semantically broken card when a key field is missing

### Optional Fields

These fields default to graceful handling when missing:

- Do not generate the corresponding component
- Do not generate the corresponding data
- Downgrade to a more compact layout when necessary

Do not escalate optional field absence into an error.

## Overlong Text Handling

Because this Python will process more DTOs of the same type, overlong text must be considered by default.

Requirements:

- Do not assume titles, subtitles, labels, descriptions, sources, or institution names will always be short
- Design convergence strategies for long text at the structural design stage
- Treat "text overlong" as normal input, not exceptional input

Default handling direction:

- Main title: prefer preserving, allow wrapping; if it still affects structure, apply controlled truncation
- Subtitle, description, source, supplementary copy: prefer wrapping or `line-clamp`
- Badge/tag: reduce the number on one row when necessary, or omit weak labels entirely
- Multi-column layout: the column containing long text is the compressible column — do not push other columns off screen

Key point:

- When text is overlong, prioritize adjusting component structure and text styles
- Do not fail the entire DTO transformation just because some text is too long

## DTO Component Workflow

1. Map out the DTO's field structure, hierarchy, and reusable fields
2. Mark which fields are required and which are optional
3. Mark which non-empty fields have low information value (generic / placeholder / section words), and which fields need to be combined before they carry semantic meaning
4. Design the component structure, reserving fallback space for missing fields, low-info fields, combined fields, and long text
5. Write the Python mapping to convert the DTO to final payload; omit at component level or section level when fields are missing, have low info value, or combination fails
6. Run Python to directly produce `updateComponents` and `updateDataModel`
7. Write the Python output to two JSON files on disk
8. Run the validation script immediately; if it fails, fix the Python or artifacts directly and re-run until passing
9. Only after the script passes, perform model-level design review and optimization
10. If the model review produces changes, re-run the validation script until it passes again

Do not skip steps `5–6` and hand-write the final JSON from imagination.

## DTO Reminders

- Do not write business values into components
- If the DTO is missing something, treat it as missing — do not invent business facts
- When a DTO field is missing, prefer omitting the component and data rather than leaving an empty shell component
- Both JSONs are the output of running Python, not a second source of truth maintained in parallel with Python
- This Python must be compatible with more DTOs of the same type — do not hard-code the current input as the only structure
- When optional fields are missing, default to degradation; when key fields are missing, explicit errors are allowed
- When text is overlong, prioritize layout/style convergence — do not casually fail on overlong text as an exception
- If the design requires a button but the DTO provides no real link, do not degrade to a fake button
