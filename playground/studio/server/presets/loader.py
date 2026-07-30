"""Load A2UI preset examples from the samples/protocols directory.

Presets live under ``samples/protocols/<Name>/`` and each
contains an ``updateComponents.json`` (and optionally ``updateDataModel.json``).
They are exposed read-only so the Studio UI can offer one-tap example payloads
and the Playground can load them by URL.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


# Repo root / samples / protocols
PRESETS_DIR = (
    Path(__file__).resolve().parents[4]
    / "samples"
    / "protocols"
)


def _read_json(path: Path) -> Any | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None


def _resolve_preset_dir(preset_id: str) -> Path | None:
    """Resolve a preset id to its directory, guarding against path traversal."""
    if not preset_id or "/" in preset_id or "\\" in preset_id or ".." in preset_id:
        return None
    candidate = (PRESETS_DIR / preset_id).resolve()
    if candidate.parent != PRESETS_DIR.resolve():
        return None
    if not candidate.is_dir():
        return None
    return candidate


def list_presets() -> list[dict[str, Any]]:
    """List available presets (sorted by name)."""
    if not PRESETS_DIR.is_dir():
        return []
    items: list[dict[str, Any]] = []
    for entry in sorted(PRESETS_DIR.iterdir(), key=lambda p: p.name.lower()):
        if not entry.is_dir():
            continue
        if not (entry / "updateComponents.json").exists():
            continue
        items.append({"id": entry.name, "name": entry.name})
    return items


def load_preset(preset_id: str) -> dict[str, Any] | None:
    """Load a preset in the same shape as a raw protocol.

    Returns ``{"id", "name", "components", "datamodel"}`` or None if not found.
    ``datamodel`` is None when the preset has no updateDataModel.json.
    """
    preset_dir = _resolve_preset_dir(preset_id)
    if preset_dir is None:
        return None

    components = _read_json(preset_dir / "updateComponents.json")
    if components is None:
        return None

    datamodel_path = preset_dir / "updateDataModel.json"
    datamodel = _read_json(datamodel_path) if datamodel_path.exists() else None

    return {
        "id": preset_id,
        "name": preset_id,
        "components": components,
        "datamodel": datamodel,
    }
