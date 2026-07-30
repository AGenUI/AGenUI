"""FastAPI application for AGenUI Studio.

Exposes the local A2UI generation agent over HTTP:
    - System   : health, server-info (LAN IP for QR codes)
    - Config   : list providers, update config.json, test connection
    - Generate : POST /api/generate (SSE streaming or plain JSON)
    - Protocols: CRUD over ~/.agenui/protocols/ (+ /raw for Playground QR scan)
    - Presets  : read-only example payloads from the Playground stories
    - Static   : serves the frontend build (Chapter 2) when present

All routes are prefixed with /api so the static frontend can own the root path.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from . import render_sequence, samples, storage
from .config import (
    ProviderConfig,
    get_available_providers,
    get_lan_ip,
    load_config,
    save_config,
)
from .generator import GenerationEvent, generate_a2ui_stream, generate_a2ui_sync
from .providers import build_provider


app = FastAPI(title="AGenUI Studio", version="0.1.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.on_event("startup")
def _startup() -> None:
    """Seed the samples gallery and ensure storage dirs exist on boot."""
    storage.ensure_dirs()
    samples.ensure_samples()


# --------------------------------------------------------------------------- #
# Request models
# --------------------------------------------------------------------------- #
class ChatMessage(BaseModel):
    role: str  # "user" | "assistant"
    content: str


class GenerateRequest(BaseModel):
    prompt: str
    mode: str = "component"  # "component" | "page"
    stream: bool = True
    provider: str | None = None
    # Force the model's reasoning/thinking switch. Default off so reasoning
    # models (GLM/DeepSeek/Qwen) skip the slow chain-of-thought for fast card
    # generation; the UI exposes a toggle to turn it back on. Providers without
    # a safe switch ignore this flag (see providers._reasoning_family).
    reasoning: bool = False
    # Multi-turn history: prior user/assistant messages for protocol refinement.
    history: list[ChatMessage] = []


class ProviderUpdate(BaseModel):
    base_url: str | None = None
    api_key: str | None = None
    model: str | None = None
    max_tokens: int | None = None


class ConfigUpdateRequest(BaseModel):
    active: str | None = None
    set_providers: dict[str, ProviderUpdate] | None = None
    remove_providers: list[str] | None = None


class TestConnectionRequest(BaseModel):
    name: str


# --------------------------------------------------------------------------- #
# Helpers
# --------------------------------------------------------------------------- #
def _resolve_provider(name: str | None = None):
    """Resolve a provider by explicit name, else the active one, else the first
    configured provider with an api_key. Returns None when nothing is usable."""
    cfg = load_config()

    if name:
        pc = cfg.providers.get(name)
        if pc and pc.api_key:
            return build_provider(name, pc)
        return None

    if cfg.active:
        pc = cfg.providers.get(cfg.active)
        if pc and pc.api_key:
            return build_provider(cfg.active, pc)

    for pname, pc in cfg.providers.items():
        if pc.api_key:
            return build_provider(pname, pc)

    return None


def _sse(event: GenerationEvent) -> str:
    """Serialize a GenerationEvent into an SSE frame (single-line data)."""
    payload = json.dumps(event.data, ensure_ascii=False)
    return f"event: {event.type}\ndata: {payload}\n\n"


# --------------------------------------------------------------------------- #
# System
# --------------------------------------------------------------------------- #
@app.get("/api/health")
def health() -> dict[str, Any]:
    return {"status": "ok", "service": "agenui-studio"}


@app.get("/api/server-info")
def server_info(request: Request) -> dict[str, Any]:
    cfg = load_config()
    lan_ip = get_lan_ip()
    port = request.url.port or cfg.port
    return {
        "lan_ip": lan_ip,
        "port": port,
        "base_url": f"http://{lan_ip}:{port}",
    }


# --------------------------------------------------------------------------- #
# Config
# --------------------------------------------------------------------------- #
@app.get("/api/providers")
def list_providers() -> dict[str, Any]:
    cfg = load_config()
    return {
        "active": cfg.active,
        "providers": get_available_providers(cfg),
    }


@app.get("/api/config/all")
def get_all_config() -> dict[str, Any]:
    """Return ALL providers (including those without api_key) with full key values.

    Used by the configuration modal so users can edit any provider.
    """
    cfg = load_config()
    providers: list[dict[str, Any]] = []
    for name, pc in cfg.providers.items():
        providers.append({
            "name": name,
            "base_url": pc.base_url,
            "api_key": pc.api_key,
            "model": pc.model,
            "max_tokens": pc.max_tokens,
        })
    return {"active": cfg.active, "providers": providers}


@app.post("/api/config")
def update_config(req: ConfigUpdateRequest) -> dict[str, Any]:
    cfg = load_config()

    if req.set_providers:
        for name, upd in req.set_providers.items():
            existing = cfg.providers.get(name)
            if existing is not None:
                if upd.base_url is not None:
                    existing.base_url = upd.base_url
                if upd.api_key is not None:
                    existing.api_key = upd.api_key
                if upd.model is not None:
                    existing.model = upd.model
                if upd.max_tokens is not None:
                    existing.max_tokens = upd.max_tokens
            else:
                cfg.providers[name] = ProviderConfig(
                    base_url=upd.base_url or "",
                    api_key=upd.api_key or "",
                    model=upd.model or "",
                    max_tokens=upd.max_tokens or 8192,
                )

    if req.remove_providers:
        for name in req.remove_providers:
            cfg.providers.pop(name, None)
            if cfg.active == name:
                cfg.active = None

    if req.active is not None:
        cfg.active = req.active

    save_config(cfg)
    return {
        "ok": True,
        "active": cfg.active,
        "providers": get_available_providers(cfg),
    }


@app.post("/api/config/test")
def test_connection(req: TestConnectionRequest) -> dict[str, Any]:
    cfg = load_config()
    pc = cfg.providers.get(req.name)
    if pc is None or not pc.api_key:
        return {
            "ok": False,
            "error": f"Provider '{req.name}' is not configured or missing api_key",
        }
    provider = build_provider(req.name, pc)
    return provider.test_connection()


# --------------------------------------------------------------------------- #
# Generation
# --------------------------------------------------------------------------- #
@app.post("/api/generate")
def generate(req: GenerateRequest):
    prompt = (req.prompt or "").strip()
    if not prompt:
        return JSONResponse(
            status_code=400,
            content={"success": False, "message": "prompt is required", "code": "bad_request"},
        )

    mode = req.mode if req.mode in ("component", "page") else "component"
    history_msgs = [{"role": m.role, "content": m.content} for m in req.history] if req.history else None
    provider = _resolve_provider(req.provider)
    if provider is None:
        return JSONResponse(
            status_code=400,
            content={
                "success": False,
                "message": (
                    "No provider available. Set an api_key in ~/.agenui/config.json "
                    "or via POST /api/config."
                ),
                "code": "no_provider",
            },
        )

    if not req.stream:
        return generate_a2ui_sync(provider, prompt, mode, req.reasoning, history_msgs)

    def event_stream():
        for event in generate_a2ui_stream(provider, prompt, mode, req.reasoning, history_msgs):
            yield _sse(event)

    return StreamingResponse(
        event_stream(),
        media_type="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "X-Accel-Buffering": "no",
        },
    )


# --------------------------------------------------------------------------- #
# Protocols
# --------------------------------------------------------------------------- #
@app.get("/api/protocols")
def list_protocols() -> dict[str, Any]:
    return {"protocols": storage.list_protocols()}


@app.get("/api/protocols/{protocol_id}")
def get_protocol(protocol_id: str):
    record = storage.load_protocol(protocol_id)
    if record is None:
        return JSONResponse(status_code=404, content={"error": "protocol not found"})
    return record


@app.get("/api/protocols/{protocol_id}/raw")
def get_protocol_raw(protocol_id: str):
    """Render sequence [createSurface, updateComponents, updateDataModel] for QR scan."""
    record = storage.load_protocol(protocol_id)
    if record is None:
        return JSONResponse(status_code=404, content={"error": "protocol not found"})
    sequence = render_sequence.build_render_sequence(
        record.get("components"), record.get("datamodel")
    )
    if sequence is None:
        return JSONResponse(
            status_code=500, content={"error": "protocol missing surfaceId"}
        )
    return sequence


@app.delete("/api/protocols/{protocol_id}")
def delete_protocol(protocol_id: str) -> dict[str, Any]:
    deleted = storage.delete_protocol(protocol_id)
    return {"deleted": deleted}


class ProtocolUpdateRequest(BaseModel):
    components: dict[str, Any]
    datamodel: dict[str, Any] | None = None


@app.put("/api/protocols/{protocol_id}")
def update_protocol(protocol_id: str, req: ProtocolUpdateRequest):
    """Update an existing protocol's payloads in place (QR URL stays valid)."""
    record = storage.update_protocol(protocol_id, req.components, req.datamodel)
    if record is None:
        return JSONResponse(status_code=404, content={"error": "protocol not found"})
    return {"ok": True, "id": record.get("id")}


# --------------------------------------------------------------------------- #
# Presets (samples under ~/.agenui/protocols/presets/)
# --------------------------------------------------------------------------- #
@app.get("/api/presets")
def list_presets() -> dict[str, Any]:
    return {"presets": samples.list_samples()}


@app.get("/api/presets/{preset_id}")
def get_preset(preset_id: str):
    preset = samples.load_sample(preset_id)
    if preset is None:
        return JSONResponse(status_code=404, content={"error": "preset not found"})
    return preset


@app.get("/api/presets/{preset_id}/raw")
def get_preset_raw(preset_id: str):
    """Render sequence [createSurface, updateComponents, updateDataModel] for QR scan."""
    preset = samples.load_sample(preset_id)
    if preset is None:
        return JSONResponse(status_code=404, content={"error": "preset not found"})
    sequence = render_sequence.build_render_sequence(
        preset.get("components"), preset.get("datamodel")
    )
    if sequence is None:
        return JSONResponse(
            status_code=500, content={"error": "preset missing surfaceId"}
        )
    return sequence


@app.get("/api/presets/{preset_id}/rendering")
def get_preset_rendering(preset_id: str):
    """Serve the preset's reference rendering image (rendering.png)."""
    path = samples.get_rendering_path(preset_id)
    if path is None:
        return JSONResponse(status_code=404, content={"error": "rendering not found"})
    return FileResponse(path, media_type="image/png")


# --------------------------------------------------------------------------- #
# Static frontend (Chapter 2). Serve the built SPA when present, otherwise a
# minimal status page so the root URL is never a bare 404.
# --------------------------------------------------------------------------- #
STATIC_DIR = Path(__file__).resolve().parent / "static"

if (STATIC_DIR / "index.html").exists():

    @app.get("/", include_in_schema=False)
    def spa_index() -> FileResponse:
        # Serve the SPA entry HTML with no-cache so browsers always pick up the
        # latest built bundle reference. The hashed assets under /assets are
        # content-addressed and remain safely cacheable via the static mount.
        return FileResponse(
            STATIC_DIR / "index.html",
            media_type="text/html",
            headers={"Cache-Control": "no-cache, no-store, must-revalidate"},
        )

    app.mount("/", StaticFiles(directory=str(STATIC_DIR), html=True), name="static")
else:

    @app.get("/", include_in_schema=False)
    def root() -> HTMLResponse:
        return HTMLResponse(
            "<!doctype html><meta charset='utf-8'>"
            "<title>AGenUI Studio</title>"
            "<body style='font-family:system-ui;padding:40px;max-width:720px;margin:auto'>"
            "<h1>AGenUI Studio</h1>"
            "<p>The local A2UI generation agent is running.</p>"
            "<ul>"
            "<li>API docs: <a href='/docs'>/docs</a></li>"
            "<li>Health: <a href='/api/health'>/api/health</a></li>"
            "<li>Server info: <a href='/api/server-info'>/api/server-info</a></li>"
            "<li>Providers: <a href='/api/providers'>/api/providers</a></li>"
            "<li>Presets: <a href='/api/presets'>/api/presets</a></li>"
            "</ul>"
            "<p>The web UI will be served here once built.</p>"
            "</body>"
        )
