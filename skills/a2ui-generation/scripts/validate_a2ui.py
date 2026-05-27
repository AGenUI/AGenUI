#!/usr/bin/env python3
"""
validate_a2ui.py

Validate an A2UI components/dataModel pair.

Preferred usage is importing `validate()` from Python code. A small CLI is also
provided for convenience.
"""

import json
import re
import sys
from pathlib import Path
from typing import Any

STRING_PATH_KEYS = {
    "items",
    "cards",
    "contents",
    "segments",
    "tips",
    "tags",
}
BUTTON_ID_RE = re.compile(r"(^|_)(button|btn)(_|$)", re.IGNORECASE)
BUTTONISH_TEXT_PATH_RE = re.compile(
    r"(^|/)(actionText|buttonText|ctaText|linkText|actionUrl|buttonUrl|ctaUrl|linkUrl)$",
    re.IGNORECASE,
)

UNSUPPORTED_STYLE_KEYS = {"flex-basis", "gap", "box-shadow", "position", "z-index"}
ALLOWED_FONT_SIZES = {"24px", "28px", "32px", "36px", "40px"}
ALLOWED_COMPONENTS = {
    "Column", "Row", "List", "Card", "Tabs", "Modal", "Divider", "Carousel",
    "Text", "RichText", "Markdown", "Image", "Icon", "Video",
    "AudioPlayer", "Lottie", "Web",
    "Button", "TextField", "CheckBox", "ChoicePicker", "Slider", "DateTimeInput",
    "Chart", "Table",
}


def _normalize_px(value: Any) -> str | None:
    if isinstance(value, str):
        candidate = value.strip()
    elif isinstance(value, (int, float)):
        if int(value) != value:
            return None
        candidate = f"{int(value)}px"
    else:
        return None

    match = re.fullmatch(r"(\d+)px", candidate)
    if not match:
        return None
    return f"{int(match.group(1))}px"


def _px_number(value: Any) -> int | None:
    normalized = _normalize_px(value)
    if normalized is None:
        return None
    return int(normalized[:-2])


def _load_overrides(path: str | None) -> dict[str, Any]:
    if not path:
        return {
            "user_requirement_first": False,
            "allow_unsupported_styles": set(),
            "allow_font_sizes": set(),
        }

    raw = json.loads(Path(path).read_text(encoding="utf-8"))
    user_requirement_first = bool(raw.get("userRequirementFirst"))

    allow_unsupported_styles = {
        item
        for item in raw.get("allowUnsupportedStyles", [])
        if isinstance(item, str) and item.strip()
    }
    allow_font_sizes = set()
    for item in raw.get("allowFontSizes", []):
        normalized = _normalize_px(item)
        if normalized:
            allow_font_sizes.add(normalized)

    return {
        "user_requirement_first": user_requirement_first,
        "allow_unsupported_styles": allow_unsupported_styles,
        "allow_font_sizes": allow_font_sizes,
    }


def _collect_binding_paths(node, location: str = "root") -> list[tuple[str, str]]:
    """Collect path-like bindings from nested JSON structures."""
    found: list[tuple[str, str]] = []

    if isinstance(node, dict):
        path_value = node.get("path")
        if isinstance(path_value, str):
            found.append((f"{location}.path", path_value))

        for key, value in node.items():
            if key in STRING_PATH_KEYS and isinstance(value, str) and value.startswith("/"):
                found.append((f"{location}.{key}", value))
            found.extend(_collect_binding_paths(value, f"{location}.{key}"))

    elif isinstance(node, list):
        for index, value in enumerate(node):
            found.extend(_collect_binding_paths(value, f"{location}[{index}]"))

    return found


def _validate_binding_paths(
    comp: dict, data_root: str | None, errors: list[str]
) -> None:
    """Validate absolute/relative binding path syntax."""
    for location, path_value in _collect_binding_paths(comp, "updateComponents"):
        if not path_value:
            errors.append(f"{location}: path must not be empty")
            continue

        if "." in path_value:
            errors.append(
                f"{location}: invalid dotted path {path_value!r}; use '/' separators instead"
            )

        if path_value.startswith("/") and data_root and data_root != "/":
            if path_value != data_root and not path_value.startswith(f"{data_root}/"):
                errors.append(
                    f"{location}: absolute path {path_value!r} is outside updateDataModel.path {data_root!r}"
                )


def _looks_like_button_styles(styles: dict) -> bool:
    """Heuristic for a text block visually styled as a button."""
    if not isinstance(styles, dict):
        return False
    has_padding = any(
        key in styles
        for key in (
            "padding",
            "padding-left",
            "padding-right",
            "padding-top",
            "padding-bottom",
        )
    )
    has_shape = any(key in styles for key in ("border-radius", "border-width", "border-color"))
    has_fill = "background-color" in styles
    return has_padding and (has_shape or has_fill)


def _validate_button_patterns(components: list[dict], errors: list[str]) -> None:
    """Validate button/action structures and detect likely fake buttons."""
    button_child_ids = set()
    for component in components:
        if component.get("component") == "Button" and isinstance(component.get("child"), str):
            button_child_ids.add(component["child"])

    for component in components:
        cid = component.get("id", "")
        ctype = component.get("component", "")

        if ctype == "Button":
            action = component.get("action")
            if not isinstance(action, dict):
                errors.append(f"{cid} (Button): action must be an object")
                continue

            has_function_call = "functionCall" in action
            has_event = "event" in action
            if has_function_call == has_event:
                errors.append(
                    f"{cid} (Button): action must contain exactly one of functionCall or event"
                )
                continue

            if has_function_call:
                function_call = action.get("functionCall")
                if not isinstance(function_call, dict) or not function_call.get("call"):
                    errors.append(
                        f"{cid} (Button): functionCall must include a non-empty call"
                    )

            if has_event:
                event = action.get("event")
                if not isinstance(event, dict) or not event.get("name"):
                    errors.append(f"{cid} (Button): event must include a non-empty name")

        if ctype != "Text" or cid in button_child_ids:
            continue

        text_obj = component.get("text")
        text_path = text_obj.get("path") if isinstance(text_obj, dict) else None
        styles = component.get("styles", {})

        looks_semantically_like_button = bool(BUTTON_ID_RE.search(cid)) or (
            isinstance(text_path, str) and BUTTONISH_TEXT_PATH_RE.search(text_path)
        )
        if looks_semantically_like_button and _looks_like_button_styles(styles):
            errors.append(
                f"{cid} (Text): looks like a fake button; use Button with action instead"
            )


def _validate_style_keys_and_font_sizes(
    components: list[dict], overrides: dict[str, Any], errors: list[str]
) -> None:
    user_requirement_first = overrides.get("user_requirement_first", False)
    allowed_unsupported = overrides.get("allow_unsupported_styles", set())
    allowed_font_sizes = overrides.get("allow_font_sizes", set())

    for component in components:
        cid = component.get("id", "<unknown>")
        ctype = component.get("component", "")
        styles = component.get("styles")
        if not isinstance(styles, dict):
            continue

        for style_key in styles.keys():
            if style_key not in UNSUPPORTED_STYLE_KEYS:
                continue
            if user_requirement_first and style_key in allowed_unsupported:
                continue
            errors.append(
                f"{cid}: unsupported style key {style_key!r}; remove it or use userRequirementFirst override"
            )

        if ctype != "Text" or "font-size" not in styles:
            continue
        normalized = _normalize_px(styles.get("font-size"))
        if normalized is None:
            if not (user_requirement_first and str(styles.get("font-size")) in allowed_font_sizes):
                errors.append(
                    f"{cid} (Text): invalid font-size {styles.get('font-size')!r}; expected px string"
                )
            continue
        if normalized in ALLOWED_FONT_SIZES:
            continue
        if user_requirement_first and normalized in allowed_font_sizes:
            continue
        errors.append(
            f"{cid} (Text): font-size {normalized} is out of allowed scale {sorted(ALLOWED_FONT_SIZES)}; "
            "use userRequirementFirst override only for explicit user requirements"
        )


def _build_component_index(components: list[dict]) -> dict[str, dict]:
    indexed: dict[str, dict] = {}
    for component in components:
        cid = component.get("id")
        if isinstance(cid, str) and cid:
            indexed[cid] = component
    return indexed


def _child_component_ids(component: dict) -> list[str]:
    ids: list[str] = []
    child = component.get("child")
    if isinstance(child, str) and child:
        ids.append(child)

    children = component.get("children")
    if isinstance(children, list):
        for item in children:
            if isinstance(item, str) and item:
                ids.append(item)
    elif isinstance(children, dict):
        template_id = children.get("componentId")
        if isinstance(template_id, str) and template_id:
            ids.append(template_id)
    return ids


def _collect_text_descendants(
    component_id: str,
    component_index: dict[str, dict],
    depth: int = 0,
    seen: set[str] | None = None,
) -> list[dict]:
    if seen is None:
        seen = set()
    if component_id in seen or depth > 3:
        return []
    seen.add(component_id)

    component = component_index.get(component_id)
    if not isinstance(component, dict):
        return []

    collected: list[dict] = []
    if component.get("component") == "Text":
        collected.append(component)

    for child_id in _child_component_ids(component):
        collected.extend(_collect_text_descendants(child_id, component_index, depth + 1, seen.copy()))
    return collected


def _looks_like_protected_text(text_component: dict) -> bool:
    text_obj = text_component.get("text")
    text_path = text_obj.get("path") if isinstance(text_obj, dict) else None
    styles = text_component.get("styles", {})
    font_size = _px_number(styles.get("font-size")) if isinstance(styles, dict) else None

    if isinstance(text_path, str) and (
        BUTTONISH_TEXT_PATH_RE.search(text_path)
        or re.search(r"(^|/)(score|rating|price|time|status|label|summary)$", text_path, re.IGNORECASE)
    ):
        return True
    return font_size is not None and font_size >= 28


def collect_warnings(comp: dict) -> list[str]:
    warnings: list[str] = []
    components = comp.get("updateComponents", {}).get("components", [])
    component_index = _build_component_index(components)

    for component in components:
        cid = component.get("id", "<unknown>")
        ctype = component.get("component", "")
        styles = component.get("styles", {})
        width_px = _px_number(styles.get("width")) if isinstance(styles, dict) else None

        if ctype == "Button" and width_px is not None and width_px <= 220:
            child_id = component.get("child")
            text_descendants = (
                _collect_text_descendants(child_id, component_index) if isinstance(child_id, str) else []
            )
            protected_text = any(_looks_like_protected_text(text_component) for text_component in text_descendants)
            if protected_text:
                warnings.append(
                    f"{cid} (Button): fixed width {width_px}px may force protected CTA text to wrap; "
                    "prefer content-sized width unless equal-width alignment is truly required"
                )

        if ctype in {"Column", "Row", "Card"} and width_px is not None and width_px <= 160:
            child_ids = _child_component_ids(component)
            text_descendants: list[dict] = []
            for child_id in child_ids:
                text_descendants.extend(_collect_text_descendants(child_id, component_index))
            protected_count = sum(1 for text_component in text_descendants if _looks_like_protected_text(text_component))
            if protected_count >= 1 and len(child_ids) >= 1:
                warnings.append(
                    f"{cid} ({ctype}): fixed width {width_px}px is narrow for protected text content; "
                    "check for abnormal wrapping in ratings, status pills, prices, or short Chinese phrases"
                )

        if ctype == "Row":
            children = component.get("children")
            if not isinstance(children, list):
                continue
            fixed_width_button_ids: list[str] = []
            has_textual_sibling = False
            for child_id in children:
                if not isinstance(child_id, str):
                    continue
                child_component = component_index.get(child_id, {})
                child_styles = child_component.get("styles", {})
                child_width = _px_number(child_styles.get("width")) if isinstance(child_styles, dict) else None
                if child_component.get("component") == "Button" and child_width is not None and child_width <= 220:
                    fixed_width_button_ids.append(child_id)
                    continue
                text_descendants = _collect_text_descendants(child_id, component_index)
                if text_descendants:
                    has_textual_sibling = True
            if fixed_width_button_ids and has_textual_sibling:
                warnings.append(
                    f"{cid} (Row): fixed-width CTA alongside text content can create priority inversion; "
                    "ensure explanatory text compresses before the protected CTA breaks"
                )

    return warnings


def validate(comp: dict, data: dict, overrides: dict[str, Any] | None = None) -> list[str]:
    """Return validation errors for an A2UI JSON pair."""
    errors: list[str] = []
    overrides = overrides or {
        "user_requirement_first": False,
        "allow_unsupported_styles": set(),
        "allow_font_sizes": set(),
    }

    for label, payload in [("updateComponents", comp), ("updateDataModel", data)]:
        if payload.get("version") != "v0.9":
            errors.append(f"{label}: version must be 'v0.9', got {payload.get('version')!r}")

    comp_sid = comp.get("updateComponents", {}).get("surfaceId", "")
    data_sid = data.get("updateDataModel", {}).get("surfaceId", "")
    if not comp_sid:
        errors.append("updateComponents: missing surfaceId")
    if not data_sid:
        errors.append("updateDataModel: missing surfaceId")
    if comp_sid and data_sid and comp_sid != data_sid:
        errors.append(f"surfaceId mismatch: components={comp_sid}, dataModel={data_sid}")

    components = comp.get("updateComponents", {}).get("components", [])
    if not components:
        errors.append("updateComponents: components array is empty")
        return errors

    ids = set()
    referenced_ids = set()
    has_root = False

    for component in components:
        cid = component.get("id", "")
        ctype = component.get("component", "")

        if ctype and ctype not in ALLOWED_COMPONENTS:
            errors.append(
                f"{cid or '?'}: unknown component '{ctype}'; "
                f"allowed: {sorted(ALLOWED_COMPONENTS)}"
            )

        if not cid:
            preview = json.dumps(component, ensure_ascii=False)[:120]
            errors.append(f"component missing id: {preview}")
            continue

        if cid in ids:
            errors.append(f"duplicate component id: {cid}")
        ids.add(cid)

        if cid == "root":
            has_root = True

        if ctype == "Text" and "text" not in component:
            errors.append(f"{cid} (Text): missing text")
        if ctype == "Image" and "url" not in component:
            errors.append(f"{cid} (Image): missing url")
        if ctype == "Button":
            if "child" not in component:
                errors.append(f"{cid} (Button): missing child")
            if "action" not in component:
                errors.append(f"{cid} (Button): missing action")
        if ctype == "Card":
            child = component.get("child")
            if not child:
                errors.append(f"{cid} (Card): missing child")
            elif isinstance(child, list):
                errors.append(f"{cid} (Card): child must be a single id, not a list")
            else:
                referenced_ids.add(child)

        children = component.get("children")
        if isinstance(children, list):
            for child in children:
                if isinstance(child, str):
                    referenced_ids.add(child)
        elif isinstance(children, dict):
            ref_id = children.get("componentId")
            if ref_id:
                referenced_ids.add(ref_id)

        tabs = component.get("tabs")
        if isinstance(tabs, list):
            for tab in tabs:
                if isinstance(tab, dict) and "child" in tab:
                    referenced_ids.add(tab["child"])

        trigger = component.get("trigger")
        if isinstance(trigger, str) and trigger:
            referenced_ids.add(trigger)

        content = component.get("content")
        if ctype == "Modal" and isinstance(content, str):
            referenced_ids.add(content)

    if not has_root:
        errors.append("missing root component with id='root'")

    missing = referenced_ids - ids
    if missing:
        errors.append(f"undefined referenced component ids: {sorted(missing)}")

    for component in components:
        if component.get("id") != "root":
            continue
        background = component.get("styles", {}).get("background-color")
        if background and str(background).lower() not in ("transparent", ""):
            errors.append(
                f"root should not set a solid background-color, got {background!r}"
            )

    data_model = data.get("updateDataModel", {})
    if "path" not in data_model:
        errors.append("updateDataModel: missing path")
    if "value" not in data_model:
        errors.append("updateDataModel: missing value")

    data_root = data_model.get("path")
    if isinstance(data_root, str):
        if not data_root.startswith("/"):
            errors.append(f"updateDataModel.path must start with '/', got {data_root!r}")
        if "." in data_root:
            errors.append(
                f"updateDataModel.path uses dotted syntax {data_root!r}; use '/' separators instead"
            )
    else:
        data_root = None

    _validate_binding_paths(comp, data_root, errors)
    _validate_button_patterns(components, errors)
    _validate_style_keys_and_font_sizes(components, overrides, errors)

    return errors


def extract_json_blocks(text: str) -> list[dict]:
    """Extract JSON code fences from markdown-like text."""
    pattern = r"```(?:json)?\s*\n(.*?)\n\s*```"
    blocks = re.findall(pattern, text, re.DOTALL)
    parsed = []
    for block in blocks:
        try:
            parsed.append(json.loads(block.strip()))
        except json.JSONDecodeError:
            continue
    return parsed


def load_payloads(path1: str, path2: str | None = None) -> tuple[dict, dict]:
    """Load payloads from one markdown file or two JSON files."""
    if path2 is None:
        text = Path(path1).read_text(encoding="utf-8")
        blocks = extract_json_blocks(text)
        if len(blocks) < 2:
            raise ValueError("could not extract two JSON blocks from the input file")
        return blocks[0], blocks[1]

    with open(path1, "r", encoding="utf-8") as f:
        comp = json.load(f)
    with open(path2, "r", encoding="utf-8") as f:
        data = json.load(f)
    return comp, data


def main() -> int:
    if len(sys.argv) not in (2, 3, 4):
        print("Usage: python validate_a2ui.py <combined.md>")
        print("   or: python validate_a2ui.py <components.json> <datamodel.json>")
        print("Optional:")
        print("   python validate_a2ui.py <combined.md> <overrides.json>")
        print("   python validate_a2ui.py <components.json> <datamodel.json> <overrides.json>")
        return 1

    try:
        overrides: dict[str, Any] = {
            "user_requirement_first": False,
            "allow_unsupported_styles": set(),
            "allow_font_sizes": set(),
        }

        if len(sys.argv) == 2:
            comp, data = load_payloads(sys.argv[1])
        elif len(sys.argv) == 3:
            if sys.argv[1].endswith(".md"):
                comp, data = load_payloads(sys.argv[1])
                overrides = _load_overrides(sys.argv[2])
            else:
                comp, data = load_payloads(sys.argv[1], sys.argv[2])
        else:
            if sys.argv[1].endswith(".md"):
                comp, data = load_payloads(sys.argv[1])
                overrides = _load_overrides(sys.argv[2])
            else:
                comp, data = load_payloads(sys.argv[1], sys.argv[2])
                overrides = _load_overrides(sys.argv[3])
    except Exception as exc:  # pragma: no cover
        print(f"Failed to load input: {exc}")
        return 1

    errors = validate(comp, data, overrides=overrides)
    if errors:
        print(f"Found {len(errors)} problem(s):")
        for error in errors:
            print(f" - {error}")
        return 1

    warnings = collect_warnings(comp)
    if warnings:
        print(f"Found {len(warnings)} warning(s):")
        for warning in warnings:
            print(f" - {warning}")

    count = len(comp.get("updateComponents", {}).get("components", []))
    surface_id = comp.get("updateComponents", {}).get("surfaceId", "N/A")
    print("A2UI validation passed")
    print(f"components: {count}")
    print(f"surfaceId: {surface_id}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
