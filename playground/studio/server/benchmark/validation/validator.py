"""Validate generated A2UI payloads and check scenario expectations."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

# Make the skill scripts importable
_SKILL_SCRIPTS = Path(__file__).resolve().parents[5] / "skills" / "a2ui-generation" / "scripts"
if str(_SKILL_SCRIPTS) not in sys.path:
    sys.path.insert(0, str(_SKILL_SCRIPTS))

from validate_a2ui import collect_warnings, validate  # type: ignore[import-not-found]

from ..models import ExpectedConstraints, Scenario


def validate_payloads(
    components_dict: dict[str, Any] | None,
    datamodel_dict: dict[str, Any] | None,
    overrides: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Run validate_a2ui against a component/dataModel pair.

    Returns a dict with passed, error_count, warning_count, errors, warnings.
    """
    if not components_dict or not datamodel_dict:
        return {
            "validation_passed": False,
            "error_count": 1,
            "warning_count": 0,
            "validation_errors": ["Missing components or dataModel payload"],
            "validation_warnings": [],
        }

    errors = validate(components_dict, datamodel_dict, overrides)
    warnings = collect_warnings(components_dict)

    return {
        "validation_passed": len(errors) == 0,
        "error_count": len(errors),
        "warning_count": len(warnings),
        "validation_errors": errors,
        "validation_warnings": warnings,
    }


def check_scenario_expectations(
    scenario: Scenario,
    components_dict: dict[str, Any] | None,
    datamodel_dict: dict[str, Any] | None,
) -> dict[str, Any]:
    """Check scenario-specific expectations beyond schema validation."""
    failures: list[str] = []
    expected = scenario.expected

    if not components_dict or not datamodel_dict:
        return {
            "scenario_checks_passed": False,
            "scenario_check_failures": ["Missing payloads"],
        }

    comp_root = components_dict.get("updateComponents", {})
    data_root = datamodel_dict.get("updateDataModel", {})
    components = comp_root.get("components", [])
    component_types = {c.get("component") for c in components if isinstance(c, dict)}

    # surfaceId consistency check (both payloads must use the same surfaceId)
    comp_sid = comp_root.get("surfaceId")
    data_sid = data_root.get("surfaceId")
    if comp_sid and data_sid and comp_sid != data_sid:
        failures.append(
            f"surfaceId inconsistency: updateComponents uses {comp_sid!r}, "
            f"updateDataModel uses {data_sid!r}"
        )

    # Minimum component count
    if expected.min_components and len(components) < expected.min_components:
        failures.append(
            f"component count {len(components)} < expected {expected.min_components}"
        )

    # Required component types
    for ctype in expected.required_component_types:
        if ctype not in component_types:
            failures.append(f"missing required component type: {ctype}")

    # Forbidden component types
    for ctype in expected.forbidden_component_types:
        if ctype in component_types:
            failures.append(f"forbidden component type present: {ctype}")

    # Required binding paths
    # Required binding paths (only enforce for DTO scenarios where paths must
    # match the provided sample data structure; for Non-DTO the model creates
    # both bindings and data freely)
    if expected.required_bindings and scenario.dto:
        binding_paths = _collect_binding_paths(components_dict)
        for path in expected.required_bindings:
            if path not in binding_paths:
                failures.append(f"missing required binding path: {path}")

    return {
        "scenario_checks_passed": len(failures) == 0,
        "scenario_check_failures": failures,
    }


def _collect_binding_paths(node: Any, location: str = "root") -> set[str]:
    """Collect all absolute path strings used in component bindings."""
    found: set[str] = set()
    string_path_keys = {"items", "cards", "contents", "segments", "tips", "tags"}

    if isinstance(node, dict):
        path_value = node.get("path")
        if isinstance(path_value, str):
            found.add(path_value)
        for key, value in node.items():
            if key in string_path_keys and isinstance(value, str) and value.startswith("/"):
                found.add(value)
            found.update(_collect_binding_paths(value, f"{location}.{key}"))
    elif isinstance(node, list):
        for index, value in enumerate(node):
            found.update(_collect_binding_paths(value, f"{location}[{index}]"))

    return found
