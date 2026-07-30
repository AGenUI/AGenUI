"""Extract updateComponents and updateDataModel JSON blocks from model output."""

from __future__ import annotations

import json
import re


def extract_json_blocks(text: str) -> tuple[str | None, str | None]:
    """Extract updateComponents and updateDataModel JSON from model response.

    Returns (components_json_str, datamodel_json_str). Either may be None if extraction fails.
    """
    if not text:
        return None, None

    # Strategy 1: find ```json ... ``` blocks
    pattern = r"```json\s*\n(.*?)```"
    matches = re.findall(pattern, text, re.DOTALL)

    if len(matches) >= 2:
        return matches[0].strip(), matches[1].strip()

    if len(matches) == 1:
        first = matches[0].strip()
        remaining = text[text.find(first) + len(first):]
        second = _extract_first_json_object(remaining)
        return first, second

    # Strategy 2: find top-level JSON objects directly
    objects = _extract_all_json_objects(text)
    if len(objects) >= 2:
        return objects[0], objects[1]
    if len(objects) == 1:
        return objects[0], None

    return None, None


def _extract_first_json_object(text: str) -> str | None:
    """Extract the first balanced top-level JSON object from text."""
    objects = _extract_all_json_objects(text)
    return objects[0] if objects else None


def _extract_all_json_objects(text: str) -> list[str]:
    """Extract all balanced top-level JSON objects from text."""
    objects: list[str] = []
    depth = 0
    start: int | None = None

    for i, ch in enumerate(text):
        if ch == "{":
            if depth == 0:
                start = i
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0 and start is not None:
                candidate = text[start:i + 1].strip()
                try:
                    json.loads(candidate)
                    objects.append(candidate)
                except json.JSONDecodeError:
                    pass
                start = None

    return objects


def parse_json_pair(
    components_json: str | None,
    datamodel_json: str | None,
) -> tuple[dict | None, dict | None, str | None]:
    """Parse JSON strings into dicts; return error message if parsing fails."""
    components_dict: dict | None = None
    datamodel_dict: dict | None = None
    errors: list[str] = []

    if components_json:
        try:
            components_dict = json.loads(components_json)
        except json.JSONDecodeError as e:
            errors.append(f"updateComponents JSON parse error: {e}")
    else:
        errors.append("updateComponents JSON missing")

    if datamodel_json:
        try:
            datamodel_dict = json.loads(datamodel_json)
        except json.JSONDecodeError as e:
            errors.append(f"updateDataModel JSON parse error: {e}")
    else:
        errors.append("updateDataModel JSON missing")

    error = "; ".join(errors) if errors else None
    return components_dict, datamodel_dict, error
