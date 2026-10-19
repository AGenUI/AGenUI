"""Build system prompts for A2UI generation, mirroring a2ui_gen.py logic."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import re
from pathlib import Path
from types import ModuleType
from typing import Any, TYPE_CHECKING

if TYPE_CHECKING:
    from ..models import Scenario


FULL_PROMPT_PROFILE = "full"
MULTIMODAL_SKILL_PROMPT_PROFILE = "multimodal_skill_v1"
# Compatibility alias for callers that imported the previous constant name.
MULTIMODAL_COMPACT_PROMPT_PROFILE = MULTIMODAL_SKILL_PROMPT_PROFILE

MULTIMODAL_TASK_INSTRUCTIONS = """
# Multimodal Design Task

Recreate the attached UI design as valid A2UI v0.9. Preserve the visible
first-screen structure, business text, hierarchy, spacing, colors, imagery, and
controls. Prefer faithful visible content over inventing content that is not in
the reference. Think through the layout silently; the response format below
does not permit analysis or a layout rationale.
"""

_COMMON_SKILL_SOURCES: tuple[tuple[str, tuple[str, ...] | None], ...] = (
    ("SKILL.md", ("Non-Negotiables",)),
    ("reference/component-catalog.md", None),
    ("reference/data-binding.md", None),
    (
        "reference/visual-interaction.md",
        ("Real Interaction", "Image Sourcing", "URL Authenticity", "Anti-Patterns To Avoid"),
    ),
)

_COMPONENT_SKILL_SOURCES: tuple[tuple[str, tuple[str, ...] | None], ...] = (
    (
        "reference/component-design.md",
        ("Card Shell Guidance", "Image Strip Fill Rule", "Compatibility Driven Layout"),
    ),
)

_PAGE_SKILL_SOURCES: tuple[tuple[str, tuple[str, ...] | None], ...] = (
    (
        "reference/page-design.md",
        ("Page Structure", "Page Layout Guidance"),
    ),
)


def _multimodal_skill_sources(
    is_page: bool,
) -> tuple[tuple[str, tuple[str, ...] | None], ...]:
    return _COMMON_SKILL_SOURCES + (
        _PAGE_SKILL_SOURCES if is_page else _COMPONENT_SKILL_SOURCES
    )


def _extract_markdown_sections(text: str, headings: tuple[str, ...], source: str) -> str:
    """Extract complete Markdown sections by exact heading text."""
    lines = text.splitlines()
    parsed: list[tuple[int, int, str]] = []
    for index, line in enumerate(lines):
        match = re.match(r"^(#{1,6})\s+(.+?)\s*$", line)
        if match:
            parsed.append((index, len(match.group(1)), match.group(2)))

    sections: list[str] = []
    for heading in headings:
        matches = [item for item in parsed if item[2] == heading]
        if len(matches) != 1:
            raise ValueError(
                f"Expected exactly one heading {heading!r} in {source}, found {len(matches)}"
            )
        start, level, _ = matches[0]
        end = len(lines)
        for next_start, next_level, _ in parsed:
            if next_start > start and next_level <= level:
                end = next_start
                break
        sections.append("\n".join(lines[start:end]).strip())
    return "\n\n".join(sections)


def _read_skill_fragment(
    skill_dir: Path,
    relative_path: str,
    headings: tuple[str, ...] | None,
) -> str:
    path = skill_dir / relative_path
    if not path.exists():
        raise FileNotFoundError(f"Skill prompt source not found: {path}")
    text = path.read_text(encoding="utf-8")
    if headings is None:
        return text.strip()
    return _extract_markdown_sections(text, headings, relative_path)


def _load_skill_validator(skill_dir: Path) -> ModuleType:
    path = skill_dir / "scripts" / "validate_a2ui.py"
    if not path.exists():
        raise FileNotFoundError(f"A2UI validator not found: {path}")
    spec = importlib.util.spec_from_file_location("_a2ui_prompt_validator", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load A2UI validator: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _code_values(values: Any) -> str:
    return " | ".join(f"`{value}`" for value in sorted(values))


def _render_validator_contract(skill_dir: Path) -> str:
    """Render exact allowlists and enums from the skill's executable validator."""
    validator = _load_skill_validator(skill_dir)
    required_names = (
        "ALLOWED_COMPONENTS",
        "ALLOWED_COMMON_STYLE_KEYS",
        "ALLOWED_TEXT_STYLE_KEYS",
        "STYLE_ENUMS",
        "COMPONENT_ENUMS",
        "COMPONENT_REQUIRED_FIELDS",
        "ALLOWED_ICON_NAMES",
    )
    missing = [name for name in required_names if not hasattr(validator, name)]
    if missing:
        raise RuntimeError(f"A2UI validator contract is missing: {', '.join(missing)}")

    lines = [
        "# Validator-Enforced Contract",
        "",
        "This contract is generated directly from `scripts/validate_a2ui.py`.",
        "",
        f"- Allowed components: {_code_values(validator.ALLOWED_COMPONENTS)}",
        f"- Common style keys: {_code_values(validator.ALLOWED_COMMON_STYLE_KEYS)}",
        "- Text-only style keys (only `Text` and `RichText`): "
        f"{_code_values(validator.ALLOWED_TEXT_STYLE_KEYS)}",
        f"- Allowed icon names: {_code_values(validator.ALLOWED_ICON_NAMES)}",
        "",
        "Component attribute enums:",
    ]
    for component in sorted(validator.COMPONENT_ENUMS):
        for attribute in sorted(validator.COMPONENT_ENUMS[component]):
            values = validator.COMPONENT_ENUMS[component][attribute]
            lines.append(f"- `{component}.{attribute}`: {_code_values(values)}")

    lines.extend(["", "Style value enums:"])
    for attribute in sorted(validator.STYLE_ENUMS):
        lines.append(
            f"- `styles.{attribute}`: {_code_values(validator.STYLE_ENUMS[attribute])}"
        )

    lines.extend(["", "Required component fields:"])
    for component in sorted(validator.COMPONENT_REQUIRED_FIELDS):
        fields = validator.COMPONENT_REQUIRED_FIELDS[component]
        lines.append(f"- `{component}`: {_code_values(fields)}")
    return "\n".join(lines)


def build_prompt_source_metadata(
    skill_dir: Path,
    is_page: bool,
    prompt_profile: str,
) -> dict[str, Any]:
    """Return source paths and a content hash for skill-backed prompts."""
    if prompt_profile != MULTIMODAL_SKILL_PROMPT_PROFILE:
        return {}

    source_specs = _multimodal_skill_sources(is_page)
    source_paths = [relative_path for relative_path, _ in source_specs]
    source_paths.append("scripts/validate_a2ui.py")
    digest = hashlib.sha256()
    for relative_path in source_paths:
        path = skill_dir / relative_path
        if not path.exists():
            raise FileNotFoundError(f"Skill prompt source not found: {path}")
        digest.update(relative_path.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return {
        "skill_prompt_hash": digest.hexdigest(),
        "skill_prompt_sources": source_paths,
    }


def _build_multimodal_skill_parts(skill_dir: Path, is_page: bool) -> list[str]:
    parts = [MULTIMODAL_TASK_INSTRUCTIONS, _render_validator_contract(skill_dir)]
    for relative_path, headings in _multimodal_skill_sources(is_page):
        fragment = _read_skill_fragment(skill_dir, relative_path, headings)
        parts.append(f"# Skill Source: {relative_path}\n\n{fragment}")
    return parts

OUTPUT_INSTRUCTIONS = """
## OUTPUT FORMAT (CRITICAL - follow exactly)

You MUST output exactly TWO JSON code blocks. The FIRST block is
updateComponents, the SECOND block is updateDataModel. Output ONLY
the two fenced code blocks, no explanatory text before or after.

Example structure:

```json
{
  "version": "v0.9",
  "updateComponents": {
    "surfaceId": "my_card",
    "components": [
      { "id": "root", "component": "Card", "child": "body" },
      { "id": "body", "component": "Column", "children": ["title", "desc"] },
      { "id": "title", "component": "Text", "text": { "path": "/page/title" }, "variant": "h2" },
      { "id": "desc", "component": "Text", "text": { "path": "/page/desc" }, "variant": "body" }
    ]
  }
}
```

```json
{
  "version": "v0.9",
  "updateDataModel": {
    "surfaceId": "my_card",
    "path": "/page",
    "value": {
      "title": "Hello World",
      "desc": "This is a sample card"
    }
  }
}
```

CRITICAL RULES for components array:
- ALL components must be in the top-level "components" array
- "children" must be an array of STRING IDs, NOT inline objects
- The only exception is List with dynamic children: {"path": "/data/items", "componentId": "item_tpl"}
- Every component referenced in "children" must have a matching entry in the components array
- Use "styles" (plural) not "style" for component style objects
"""

SKILL_BACKED_OUTPUT_INSTRUCTIONS = """
## OUTPUT FORMAT (CRITICAL - follow exactly)

Output exactly TWO JSON code blocks fenced with `json` and no other text. The
first block must contain the complete `updateComponents` object. The second
block must contain the matching `updateDataModel` object.
"""

PLACEHOLDER_IMAGE_INSTRUCTIONS = """
## IMAGE PLACEHOLDER MODE

Use placeholder images from the http://iph.href.lu service.
Format: http://iph.href.lu/{width}x{height}?text={label}

Choose appropriate dimensions based on the image's role:
  - Cover/hero image: http://iph.href.lu/400x200?text=Cover
  - Avatar/icon: http://iph.href.lu/80x80?text=Avatar
  - Product thumbnail: http://iph.href.lu/200x200?text=Product
  - Poster: http://iph.href.lu/300x400?text=Poster
  - Banner: http://iph.href.lu/600x150?text=Banner

The {label} should be a short descriptive word (in English or Chinese).
Always bind the URL via {"path": "..."} in updateComponents and provide
the placeholder URL string in updateDataModel value.
"""

DATA_FILE_INSTRUCTIONS = """
## DATA-DRIVEN LAYOUT MODE

The user has provided a sample data file. You MUST design the layout
around this data and bind component properties to the provided fields.

Rules:
- Use the exact field names and structure from the sample data.
- Bind Text fields with {"path": "/data/<fieldName>"}.
- Bind Image source URLs with {"path": "/data/<imageFieldName>"}.
- If the data contains nested objects, bind deeper paths like
  "/data/author/name" or "/data/location/city".
- If the data contains an array, use a List component with
  {"path": "/data/<arrayField>", "componentId": "<item_template_id>"}
  and define the item template component separately.
- In updateDataModel, set "path": "/data" and "value": to the sample
  data object EXACTLY as provided (do not invent values for fields that
  already exist; you may trim large arrays to 1-3 representative items
  if needed for brevity).
- Do NOT ignore image URLs in the data; prefer Image components for
  those fields so the rendered result looks authentic.
"""


def build_system_prompt(
    skill_dir: Path,
    is_page: bool = False,
    allow_placeholder_images: bool = False,
    data_file_content: dict[str, Any] | None = None,
    prompt_profile: str = FULL_PROMPT_PROFILE,
) -> str:
    """Build the full prompt or the skill-backed prompt used with design images."""
    if not skill_dir.exists():
        raise FileNotFoundError(f"Skill directory not found: {skill_dir}")

    skill_md = skill_dir / "SKILL.md"
    if not skill_md.exists():
        raise FileNotFoundError(f"SKILL.md not found in {skill_dir}")

    if prompt_profile not in {FULL_PROMPT_PROFILE, MULTIMODAL_SKILL_PROMPT_PROFILE}:
        raise ValueError(f"Unsupported prompt profile: {prompt_profile}")

    parts: list[str] = []

    if prompt_profile == MULTIMODAL_SKILL_PROMPT_PROFILE:
        parts.extend(_build_multimodal_skill_parts(skill_dir, is_page))
        if allow_placeholder_images:
            parts.append(PLACEHOLDER_IMAGE_INSTRUCTIONS)
        if data_file_content is not None:
            parts.append(DATA_FILE_INSTRUCTIONS)
        parts.append(SKILL_BACKED_OUTPUT_INSTRUCTIONS)
        return "\n\n---\n\n".join(parts)

    # 1. Load SKILL.md (strip YAML frontmatter)
    raw = skill_md.read_text(encoding="utf-8")
    if raw.startswith("---"):
        end = raw.find("---", 3)
        if end != -1:
            raw = raw[end + 3:].strip()
    parts.append(f"# A2UI Generation Skill\n\n{raw}")

    # 2. Load reference docs based on mode
    docs_dir = skill_dir / "reference"
    if not docs_dir.exists():
        # Fallback to legacy "docs" directory
        docs_dir = skill_dir / "docs"
    if docs_dir.exists():
        if is_page:
            doc_files = [
                "component-catalog.md",
                "data-binding.md",
                "page-design.md",
                "visual-interaction.md",
            ]
        else:
            doc_files = [
                "component-catalog.md",
                "data-binding.md",
                "component-design.md",
                "visual-interaction.md",
            ]

        for fname in doc_files:
            fpath = docs_dir / fname
            if fpath.exists():
                content = fpath.read_text(encoding="utf-8")
                parts.append(f"# Reference: {fname}\n\n{content}")

    # 3. Optional placeholder images
    if allow_placeholder_images:
        parts.append(PLACEHOLDER_IMAGE_INSTRUCTIONS)

    # 4. Optional data-driven layout instructions
    if data_file_content is not None:
        parts.append(DATA_FILE_INSTRUCTIONS)

    # 5. Output format instructions (always last)
    parts.append(OUTPUT_INSTRUCTIONS)

    return "\n\n---\n\n".join(parts)


def build_user_prompt(
    scenario_or_prompt,
    include_expected_constraints: bool = False,
) -> str:
    """Build user message, optionally with DTO binding hints.

    Accepts either a Scenario object (preferred) or a raw prompt string
    (for backward compatibility).

    For DTO scenarios with required_bindings, appends path hints since
    the model must match the provided sample data field names exactly.
    Multimodal providers may explicitly include structural acceptance
    constraints. Text-only providers retain the original prompt behavior.
    """
    from ..models import Scenario

    if isinstance(scenario_or_prompt, str):
        return scenario_or_prompt.strip()

    scenario: Scenario = scenario_or_prompt
    parts = [scenario.prompt.strip()]

    if scenario.data_file_content is not None:
        sample_json = json.dumps(
            scenario.data_file_content,
            indent=2,
            ensure_ascii=False,
        )
        parts.append(f"\n### 输入数据\n\n```json\n{sample_json}\n```")

    # Only inject binding path hints for DTO scenarios where paths must
    # match the provided sample data structure
    if scenario.dto and scenario.expected and scenario.expected.required_bindings:
        bindings_str = ", ".join(scenario.expected.required_bindings)
        parts.append(
            f"\nRequired data binding paths (use these exact paths from the sample data): {bindings_str}"
        )

    if include_expected_constraints and scenario.expected:
        constraints: list[str] = []
        if scenario.expected.min_components:
            constraints.append(
                f"Minimum component count: {scenario.expected.min_components}."
            )
        if scenario.expected.required_component_types:
            required = ", ".join(scenario.expected.required_component_types)
            constraints.append(f"Required component types: {required}.")
        if scenario.expected.forbidden_component_types:
            forbidden = ", ".join(scenario.expected.forbidden_component_types)
            constraints.append(f"Forbidden component types: {forbidden}.")
        if constraints:
            parts.append(
                "\nMultimodal acceptance constraints (must all be satisfied):\n- "
                + "\n- ".join(constraints)
            )

    return "\n".join(parts)
