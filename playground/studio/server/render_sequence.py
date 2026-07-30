"""Build the A2UI render sequence served to the Playground via QR-scan.

Both the iOS and Android Playground download a protocol by URL and expect a
single-line JSON array of exactly three A2UI events::

    [ createSurface, updateComponents, updateDataModel ]

They feed the three elements to the renderer in order (``beginTextStream`` ->
three ``receiveTextChunk`` calls -> ``endTextStream``). Neither preset samples
nor generated protocols store a ``createSurface`` event, so this module derives
one from the ``surfaceId`` carried inside ``updateComponents`` and packages the
three events into the array the clients consume.

Android reads only the first line of the response (``reader.readLine()``), so
the array must be serialized as a single-line JSON document; the server relies
on Starlette's compact ``JSONResponse`` for that.
"""

from __future__ import annotations

from typing import Any

# Standard A2UI basic component catalog (matches the iOS menu flow and the
# Android stability engine).
DEFAULT_CATALOG_ID = "https://a2ui.org/specification/v0_9/basic_catalog.json"


def build_render_sequence(
    components: dict[str, Any] | None,
    datamodel: dict[str, Any] | None,
) -> list[dict[str, Any]] | None:
    """Build ``[createSurface, updateComponents, updateDataModel]`` for QR-scan.

    ``components`` is the full ``updateComponents`` event object (it carries the
    ``surfaceId`` used to synthesize ``createSurface``). ``datamodel`` is the
    full ``updateDataModel`` event object, or None when the protocol has none —
    in which case an empty object placeholder keeps the array at three elements
    (iOS requires ``count >= 3`` and Android reads ``index 2`` unconditionally).

    Returns None when ``components`` is missing or has no ``surfaceId`` (the
    caller should treat that as a malformed protocol).
    """
    surface_id = (components or {}).get("updateComponents", {}).get("surfaceId")
    if not surface_id:
        return None

    create_surface: dict[str, Any] = {
        "version": (components or {}).get("version", "v0.9"),
        "createSurface": {
            "surfaceId": surface_id,
            "catalogId": DEFAULT_CATALOG_ID,
            "sendDataModel": True,
        },
    }

    return [create_surface, components, datamodel if datamodel is not None else {}]
