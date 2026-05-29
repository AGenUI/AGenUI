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
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

STRING_PATH_KEYS = {
    "items",
    "cards",
    "contents",
    "segments",
    "tips",
    "tags",
}
BUTTON_ID_RE = re.compile(r"(^|_)(button|btn)(_|$)", re.IGNORECASE)
PLACEHOLDER_URL_RE = re.compile(
    r"https?://(example\.com|placeholder\.com|via\.placeholder\.com|picsum\.photos|placehold\.(co|it|jp)|dummyimage\.com|fakeimg\.pl|loremflickr\.com)",
    re.IGNORECASE,
)
BUTTONISH_TEXT_PATH_RE = re.compile(
    r"(^|/)(actionText|buttonText|ctaText|linkText|actionUrl|buttonUrl|ctaUrl|linkUrl)$",
    re.IGNORECASE,
)


ALLOWED_COMPONENTS = {
    "Column", "Row", "List", "Card", "Tabs", "Modal", "Divider", "Carousel",
    "Text", "RichText", "Markdown", "Image", "Icon", "Video",
    "AudioPlayer", "Lottie", "Web",
    "Button", "TextField", "CheckBox", "ChoicePicker", "Slider", "DateTimeInput",
    "Chart", "Table",
}

ALLOWED_COMMON_STYLE_KEYS = {
    "width", "height",
    "padding", "padding-inline-start", "padding-left", "padding-inline-end", "padding-right",
    "padding-block-start", "padding-top", "padding-block-end", "padding-bottom",
    "margin", "margin-inline-start", "margin-left", "margin-inline-end", "margin-right",
    "margin-block-start", "margin-top", "margin-block-end", "margin-bottom",
    "background", "background-color", "background-image",
    "border-radius", "border-color", "border-style", "border-width",
    "opacity", "overflow", "display", "visibility",
    "flex-grow", "flex-shrink", "flex-wrap",
    "justify-content", "align-items", "align-content", "align-self",
    "aspect-ratio", "filter", "box-shadow",
}
ALLOWED_TEXT_STYLE_KEYS = {
    "color", "font-size", "font-weight", "font-family", "line-height",
    "text-align", "line-clamp", "text-overflow",
    "text-decoration", "text-decoration-line", "text-decoration-style",
    "text-decoration-color", "text-decoration-thickness",
}

SHORTHAND_FOUR_VALUE_RE = re.compile(r"^\d+px\s+\d+px\s+\d+px\s+\d+px$")
SINGLE_PX_RE = re.compile(r"^\d+px$")
COLOR_RE = re.compile(
    r"^("
    r"#[0-9a-fA-F]{3}"
    r"|#[0-9a-fA-F]{6}"
    r"|#[0-9a-fA-F]{8}"
    r"|rgba?\(\s*\d+\s*,\s*\d+\s*,\s*\d+\s*(,\s*[\d.]+\s*)?\)"
    r"|transparent"
    r"|(linear|radial|conic)-gradient\(.+\)"
    r")$"
)
COLOR_STYLE_KEYS = {"background-color", "background", "color", "border-color", "text-decoration-color"}

STYLE_ENUMS: dict[str, set[str]] = {
    "overflow":      {"hidden", "visible"},
    "display":       {"none", "flex"},
    "visibility":    {"visible", "hidden"},
    "flex-wrap":     {"nowrap", "wrap", "wrap-reverse"},
    "justify-content": {"flex-start", "flex-end", "center", "space-between", "space-around", "space-evenly"},
    "align-items":   {"flex-start", "flex-end", "center", "stretch", "baseline"},
    "align-content": {"flex-start", "flex-end", "center", "space-between", "space-around", "stretch"},
    "align-self":    {"auto", "flex-start", "flex-end", "center", "stretch", "baseline"},
    "border-style":  {"solid"},
    "text-overflow": {"clip", "head", "middle", "ellipsis"},
    "text-decoration-line":  {"none", "underline", "line-through"},
    "text-decoration-style": {"solid", "dashed", "dotted", "double", "wavy"},
}

COMPONENT_ENUMS: dict[str, dict[str, set[str]]] = {
    "Column":       {"justify": {"start", "center", "end", "spaceBetween", "spaceAround", "spaceEvenly", "stretch"},
                     "align":   {"start", "center", "end", "stretch"}},
    "Row":          {"justify": {"start", "center", "end", "spaceBetween", "spaceAround", "spaceEvenly", "stretch"},
                     "align":   {"start", "center", "end", "stretch"}},
    "List":         {"direction": {"vertical", "horizontal"},
                     "align":     {"start", "center", "end", "stretch"}},
    "Text":         {"variant": {"h1", "h2", "h3", "h4", "h5", "body", "caption"}},
    "RichText":     {"variant": {"h1", "h2", "h3", "h4", "h5", "body", "caption"}},
    "Image":        {"variant": {"icon", "avatar", "smallFeature", "mediumFeature", "largeFeature", "header"},
                     "fit":     {"contain", "cover", "fill", "none", "scaleDown"}},
    "Button":       {"variant": {"primary", "borderless"}},
    "TextField":    {"variant": {"shortText", "longText", "number", "obscured"}},
    "ChoicePicker": {"variant": {"mutuallyExclusive", "multipleSelection"}},
    "Divider":      {"axis":    {"horizontal", "vertical"}},
    "Lottie":       {"variant": {"icon", "small", "medium", "large"}},
}

COMPONENT_REQUIRED_FIELDS: dict[str, list[str]] = {
    "Carousel":    ["content"],
    "Tabs":        ["tabs"],
    "Modal":       ["trigger", "content"],
    "RichText":    ["text"],
    "Markdown":    ["content"],
    "Video":       ["url"],
    "AudioPlayer": ["url"],
    "Lottie":      ["url"],
    "Web":         ["source"],
    "Icon":        ["name"],
    "Chart":       ["data"],
    "Table":       ["columns", "rows"],
}

ALLOWED_ICON_NAMES = {
    "accountCircle", "add", "arrowBack", "arrowForward", "attachFile",
    "calendarToday", "call", "camera", "check", "close",
    "delete", "download", "edit", "error", "event",
    "favorite", "favoriteOff", "folder", "help",
    "home", "info", "locationOn", "lock", "lockOpen",
    "mail", "menu", "moreHoriz", "moreVert", "notifications",
    "notificationsOff", "payment", "person", "phone",
    "photo", "print", "refresh",
    "search", "send", "settings", "share", "shoppingCart",
    "star", "starHalf", "starOff",
    "upload", "visibility", "visibilityOff", "warning",
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
        }

    raw = json.loads(Path(path).read_text(encoding="utf-8"))
    user_requirement_first = bool(raw.get("userRequirementFirst"))

    allow_unsupported_styles = {
        item
        for item in raw.get("allowUnsupportedStyles", [])
        if isinstance(item, str) and item.strip()
    }

    return {
        "user_requirement_first": user_requirement_first,
        "allow_unsupported_styles": allow_unsupported_styles,
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


def _validate_style_keys_and_values(
    components: list[dict], overrides: dict[str, Any], errors: list[str]
) -> None:
    user_requirement_first = overrides.get("user_requirement_first", False)
    allowed_extra_keys = overrides.get("allow_unsupported_styles", set())

    for component in components:
        cid = component.get("id", "<unknown>")
        ctype = component.get("component", "")
        styles = component.get("styles")
        if not isinstance(styles, dict):
            continue

        is_text = ctype in {"Text", "RichText"}
        allowed_keys = ALLOWED_COMMON_STYLE_KEYS | ALLOWED_TEXT_STYLE_KEYS if is_text else ALLOWED_COMMON_STYLE_KEYS

        for style_key, style_value in styles.items():
            if style_key not in allowed_keys:
                if user_requirement_first and style_key in allowed_extra_keys:
                    continue
                if not is_text and style_key in ALLOWED_TEXT_STYLE_KEYS:
                    errors.append(
                        f"{cid} ({ctype}): text-only style key {style_key!r} "
                        f"is not allowed on non-Text components"
                    )
                else:
                    errors.append(
                        f"{cid}: unknown style key {style_key!r}; "
                        f"not in the allowed style whitelist"
                    )
                continue

            if not isinstance(style_value, str):
                continue

            if style_key in {"padding", "margin"}:
                if not SHORTHAND_FOUR_VALUE_RE.match(style_value):
                    errors.append(
                        f"{cid}: {style_key} shorthand must use 4 px values "
                        f"(e.g. '20px 20px 20px 20px'), got {style_value!r}"
                    )

            elif style_key in {"border-radius", "border-width"}:
                if not SINGLE_PX_RE.match(style_value):
                    errors.append(
                        f"{cid}: {style_key} must be a single px value "
                        f"(e.g. '16px'), got {style_value!r}"
                    )

            elif style_key in COLOR_STYLE_KEYS:
                value_stripped = style_value.strip()
                if not COLOR_RE.match(value_stripped):
                    errors.append(
                        f"{cid}: {style_key} must be #RGB, #RRGGBB, #RRGGBBAA, "
                        f"rgb(...), rgba(...), transparent, or gradient(...), got {style_value!r}"
                    )

            if style_key == "font-weight":
                _FONT_WEIGHT_KEYWORDS = {"normal", "bold"}
                _FONT_WEIGHT_NUMERICS = {str(n) for n in range(100, 1000, 100)}
                if style_value not in _FONT_WEIGHT_KEYWORDS and style_value not in _FONT_WEIGHT_NUMERICS:
                    errors.append(
                        f"{cid}: style font-weight value {style_value!r} "
                        f"is not valid; allowed: 'normal', 'bold', or numeric 100-900"
                    )
            elif style_key in STYLE_ENUMS:
                allowed_values = STYLE_ENUMS[style_key]
                if style_value not in allowed_values:
                    errors.append(
                        f"{cid}: style {style_key} value {style_value!r} "
                        f"is not valid; allowed: {sorted(allowed_values)}"
                    )

        if is_text and "font-size" in styles:
            normalized = _normalize_px(styles.get("font-size"))
            if normalized is None:
                errors.append(
                    f"{cid} ({ctype}): invalid font-size {styles.get('font-size')!r}; "
                    f"expected a px value (e.g. '16px')"
                )


def _validate_component_enums(components: list[dict], errors: list[str]) -> None:
    for component in components:
        cid = component.get("id", "<unknown>")
        ctype = component.get("component", "")
        enum_defs = COMPONENT_ENUMS.get(ctype)
        if not enum_defs:
            continue
        for attr_name, allowed_values in enum_defs.items():
            value = component.get(attr_name)
            if value is None:
                continue
            if not isinstance(value, str):
                errors.append(
                    f"{cid} ({ctype}): {attr_name} must be a string, got {type(value).__name__}"
                )
                continue
            if value not in allowed_values:
                errors.append(
                    f"{cid} ({ctype}): {attr_name} value {value!r} "
                    f"is not valid; allowed: {sorted(allowed_values)}"
                )


def _validate_required_fields(components: list[dict], errors: list[str]) -> None:
    for component in components:
        cid = component.get("id", "<unknown>")
        ctype = component.get("component", "")
        required = COMPONENT_REQUIRED_FIELDS.get(ctype)
        if not required:
            continue
        for field in required:
            if field not in component:
                errors.append(f"{cid} ({ctype}): missing required field '{field}'")


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

        if ctype == "Chart":
            height_px = _px_number(styles.get("height")) if isinstance(styles, dict) else None
            chart_type = component.get("chartType", "")
            if height_px is None:
                rec = "300px–400px" if chart_type == "donut" else "400px–500px"
                warnings.append(
                    f"{cid} (Chart): no styles.height specified; "
                    f"Chart has no intrinsic height and needs an explicit value "
                    f"(recommended for {chart_type or 'unknown'}: {rec})"
                )

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


def _collect_all_urls(node: Any, path: str = "") -> list[tuple[str, str]]:
    """Walk a data value tree and collect all URL strings."""
    found: list[tuple[str, str]] = []
    if isinstance(node, str) and re.match(r"https?://", node, re.IGNORECASE):
        found.append((path, node))
    elif isinstance(node, dict):
        for k, v in node.items():
            found.extend(_collect_all_urls(v, f"{path}/{k}"))
    elif isinstance(node, list):
        for i, v in enumerate(node):
            found.extend(_collect_all_urls(v, f"{path}[{i}]"))
    return found


def _check_url_reachable(url: str, timeout: int = 5) -> tuple[bool, str]:
    """Send HTTP GET request with Range header to verify URL serves real media."""
    try:
        req = Request(url, method="GET")
        req.add_header("User-Agent", "A2UI-Validator/1.0")
        req.add_header("Range", "bytes=0-1023")
        with urlopen(req, timeout=timeout) as resp:
            if resp.status >= 400:
                return False, f"HTTP {resp.status}"
            ct = resp.headers.get("Content-Type", "")
            if ct.startswith("text/html"):
                return False, "server returned HTML instead of media (likely soft 404)"
            return True, ""
    except HTTPError as e:
        return False, f"HTTP {e.code}"
    except URLError as e:
        return False, str(e.reason)
    except Exception as e:
        return False, str(e)


def collect_data_warnings(data: dict) -> list[str]:
    """Warn about placeholder or unreachable URLs in updateDataModel values."""
    warnings: list[str] = []
    value = data.get("updateDataModel", {}).get("value", {})
    url_entries = _collect_all_urls(value)
    if not url_entries:
        return warnings

    unreachable_count = 0
    for data_path, url in url_entries:
        if PLACEHOLDER_URL_RE.search(url):
            warnings.append(
                f"dataModel{data_path}: placeholder URL detected ({url}); "
                f"URLs must be real and loadable — omit the component if no real URL is available"
            )
            unreachable_count += 1
            continue

        ok, reason = _check_url_reachable(url)
        if not ok:
            unreachable_count += 1
            warnings.append(
                f"dataModel{data_path}: URL unreachable ({url}); {reason} — "
                f"the resource may fail to load at runtime"
            )

    if unreachable_count == len(url_entries) and len(url_entries) >= 2:
        warnings.append(
            "all URLs in dataModel are unreachable — you may be offline; "
            "re-run with network access to get accurate results"
        )

    return warnings


def validate(comp: dict, data: dict, overrides: dict[str, Any] | None = None) -> list[str]:
    """Return validation errors for an A2UI JSON pair."""
    errors: list[str] = []
    overrides = overrides or {
        "user_requirement_first": False,
        "allow_unsupported_styles": set(),
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

        if ctype == "Chart":
            chart_type = component.get("chartType", "")
            if chart_type not in {"bar", "line", "donut", "bar_grouped"}:
                errors.append(
                    f"{cid} (Chart): chartType must be one of "
                    f"'bar', 'line', 'donut', 'bar_grouped', got {chart_type!r}"
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
    _validate_style_keys_and_values(components, overrides, errors)
    _validate_component_enums(components, errors)
    _validate_required_fields(components, errors)

    for component in components:
        cid = component.get("id", "<unknown>")
        if component.get("component") == "Icon":
            name = component.get("name")
            if isinstance(name, str) and name not in ALLOWED_ICON_NAMES:
                errors.append(
                    f"{cid} (Icon): name {name!r} is not in the allowed icon set; "
                    f"see component-catalog.md for the full list"
                )

        ctype = component.get("component", "")
        children = component.get("children")
        if ctype == "Row" and isinstance(children, list) and len(children) > 3:
            errors.append(
                f"{cid} (Row): {len(children)} direct children exceeds the maximum of 3 "
                f"for horizontal layout — split into multiple rows or switch to vertical layout"
            )
        if ctype == "List" and component.get("direction") == "horizontal":
            if isinstance(children, list) and len(children) > 3:
                errors.append(
                    f"{cid} (List horizontal): {len(children)} static children exceeds the "
                    f"maximum of 3 — split into multiple rows or switch to vertical layout"
                )
            elif isinstance(children, dict):
                binding_path = children.get("path")
                if isinstance(binding_path, str) and data_root and binding_path.startswith(data_root):
                    relative = binding_path[len(data_root):].lstrip("/")
                    node = data_model.get("value", {})
                    for segment in relative.split("/"):
                        if isinstance(node, dict):
                            node = node.get(segment)
                        else:
                            node = None
                            break
                    if isinstance(node, list) and len(node) > 3:
                        errors.append(
                            f"{cid} (List horizontal): data-driven children at "
                            f"{binding_path!r} has {len(node)} items, exceeds the maximum "
                            f"of 3 — reduce data items or switch to vertical layout"
                        )

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

    warnings = collect_warnings(comp) + collect_data_warnings(data)
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
