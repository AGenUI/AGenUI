"""Protocol persistence for AGenUI Studio.

Generated (custom) A2UI protocols are stored under ``~/.agenui/protocols/custom/``
as JSON files named ``{timestamp}_{short_id}.json`` (e.g. ``20260722_143052_a1b2c3.json``).

Each file contains full metadata (id, created_at, prompt, mode, provider, model)
plus the two A2UI payloads (components, datamodel). The QR-scan render sequence
served to the Playground is assembled from these payloads by ``render_sequence.py``.
"""

from __future__ import annotations

import json
import re
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any

from .config import CUSTOM_DIR


# short_id is 6 hex chars; timestamp is YYYYmmdd_HHMMSS.
_ID_RE = re.compile(r"^[0-9a-f]{6}$")


def ensure_dirs() -> None:
    """Create ~/.agenui/protocols/custom/ if missing."""
    CUSTOM_DIR.mkdir(parents=True, exist_ok=True)


def _is_valid_id(protocol_id: str) -> bool:
    """Guard against path traversal / malformed ids."""
    return bool(_ID_RE.match(protocol_id or ""))


def _find_file(protocol_id: str) -> Path | None:
    """Locate the protocol file by short_id (glob ``*_{id}.json``)."""
    if not _is_valid_id(protocol_id):
        return None
    matches = sorted(CUSTOM_DIR.glob(f"*_{protocol_id}.json"))
    return matches[0] if matches else None


def save_protocol(
    prompt: str,
    mode: str,
    provider: str,
    model: str,
    components_dict: dict[str, Any],
    datamodel_dict: dict[str, Any],
) -> dict[str, Any]:
    """Persist a generated protocol and return its metadata record."""
    ensure_dirs()

    short_id = uuid.uuid4().hex[:6]
    now = datetime.now()
    timestamp = now.strftime("%Y%m%d_%H%M%S")
    created_at = now.isoformat(timespec="seconds")

    record: dict[str, Any] = {
        "id": short_id,
        "created_at": created_at,
        "prompt": prompt,
        "mode": mode,
        "provider": provider,
        "model": model,
        "components": components_dict,
        "datamodel": datamodel_dict,
    }

    file_path = CUSTOM_DIR / f"{timestamp}_{short_id}.json"
    file_path.write_text(
        json.dumps(record, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    return record


def update_protocol(
    protocol_id: str,
    components_dict: dict[str, Any],
    datamodel_dict: dict[str, Any],
) -> dict[str, Any] | None:
    """Update the payloads of an existing protocol in place (same id/URL).

    Returns the updated record, or None if the protocol does not exist.
    """
    file_path = _find_file(protocol_id)
    if file_path is None:
        return None
    try:
        record = json.loads(file_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None

    record["components"] = components_dict
    record["datamodel"] = datamodel_dict
    file_path.write_text(
        json.dumps(record, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    return record


def load_protocol(protocol_id: str) -> dict[str, Any] | None:
    """Load the full protocol record (metadata + payloads), or None if absent."""
    file_path = _find_file(protocol_id)
    if file_path is None:
        return None
    try:
        return json.loads(file_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None


def list_protocols() -> list[dict[str, Any]]:
    """List all saved protocols (newest first) with a truncated prompt summary."""
    ensure_dirs()
    items: list[dict[str, Any]] = []
    for file_path in CUSTOM_DIR.glob("*.json"):
        try:
            record = json.loads(file_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            continue
        prompt = record.get("prompt", "") or ""
        items.append({
            "id": record.get("id"),
            "created_at": record.get("created_at"),
            "mode": record.get("mode"),
            "provider": record.get("provider"),
            "model": record.get("model"),
            "prompt_summary": prompt[:80] + ("..." if len(prompt) > 80 else ""),
        })
    items.sort(key=lambda x: x.get("created_at") or "", reverse=True)
    return items


def delete_protocol(protocol_id: str) -> bool:
    """Delete a protocol by id. Returns True if a file was removed."""
    file_path = _find_file(protocol_id)
    if file_path is None:
        return False
    try:
        file_path.unlink()
        return True
    except OSError:
        return False
