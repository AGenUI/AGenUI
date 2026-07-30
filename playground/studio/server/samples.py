"""Preset sample protocols for AGenUI Studio.

Samples live under ``~/.agenui/protocols/presets/`` using the same directory
layout as the Playground stories: ``<Name>/updateComponents.json`` plus an
optional ``updateDataModel.json``.

On first run the presets directory is seeded from the app bundle's
``samples/protocols/`` so users get a ready-made gallery.
Seeding only happens when the presets directory is empty (e.g. after
``--update`` clears it), so anything the user adds later is preserved.
"""

from __future__ import annotations

import json
import shutil
from pathlib import Path
from typing import Any

from .config import SAMPLES_DIR
from .presets.loader import PRESETS_DIR  # repo seed source

# Optional reference rendering image stored alongside each preset's payloads.
RENDERING_NAME = "rendering.png"


def _read_json(path: Path) -> Any | None:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None


def ensure_samples() -> None:
    """Seed ``~/.agenui/protocols/presets/`` from the app bundle when empty.

    Idempotent: only copies when the presets directory has no preset dirs yet.
    """
    SAMPLES_DIR.mkdir(parents=True, exist_ok=True)
    if any(p.is_dir() for p in SAMPLES_DIR.iterdir()):
        return
    if not PRESETS_DIR.is_dir():
        return
    for entry in PRESETS_DIR.iterdir():
        if not entry.is_dir():
            continue
        if not (entry / "updateComponents.json").exists():
            continue
        shutil.copytree(entry, SAMPLES_DIR / entry.name, dirs_exist_ok=True)


def _resolve_sample_dir(sample_id: str) -> Path | None:
    """Resolve a sample id to its directory, guarding against path traversal."""
    if not sample_id or "/" in sample_id or "\\" in sample_id or ".." in sample_id:
        return None
    candidate = (SAMPLES_DIR / sample_id).resolve()
    if candidate.parent != SAMPLES_DIR.resolve():
        return None
    if not candidate.is_dir():
        return None
    return candidate


def list_samples() -> list[dict[str, Any]]:
    """List available samples (sorted by name)."""
    ensure_samples()
    items: list[dict[str, Any]] = []
    for entry in sorted(SAMPLES_DIR.iterdir(), key=lambda p: p.name.lower()):
        if not entry.is_dir():
            continue
        if not (entry / "updateComponents.json").exists():
            continue
        items.append(
            {
                "id": entry.name,
                "name": entry.name,
                "has_rendering": (entry / RENDERING_NAME).is_file(),
            }
        )
    return items


def load_sample(sample_id: str) -> dict[str, Any] | None:
    """Load a sample in the same shape as a protocol record.

    Returns ``{"id", "name", "components", "datamodel"}`` or None if not found.
    ``datamodel`` is None when the sample has no updateDataModel.json.
    """
    sample_dir = _resolve_sample_dir(sample_id)
    if sample_dir is None:
        return None

    components = _read_json(sample_dir / "updateComponents.json")
    if components is None:
        return None

    datamodel_path = sample_dir / "updateDataModel.json"
    datamodel = _read_json(datamodel_path) if datamodel_path.exists() else None

    return {
        "id": sample_id,
        "name": sample_id,
        "components": components,
        "datamodel": datamodel,
        "has_rendering": (sample_dir / RENDERING_NAME).is_file(),
    }


def get_rendering_path(sample_id: str) -> Path | None:
    """Return the path to a sample's ``rendering.png``, or None when absent."""
    sample_dir = _resolve_sample_dir(sample_id)
    if sample_dir is None:
        return None
    rendering = sample_dir / RENDERING_NAME
    return rendering if rendering.is_file() else None
