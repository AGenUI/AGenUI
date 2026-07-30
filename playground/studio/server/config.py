"""Configuration management for AGenUI Studio.

Config file: ~/.agenui/config.json (JSON format, similar to Claude Code settings.json).
Supports multiple OpenAI-compatible providers with base_url, api_key, model fields.
"""

from __future__ import annotations

import json
import os
import socket
import stat
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


CONFIG_DIR = Path.home() / ".agenui"
CONFIG_FILE = CONFIG_DIR / "config.json"
PROTOCOLS_DIR = CONFIG_DIR / "protocols"
# Model-generated (custom) protocols and presets live in sub-directories.
CUSTOM_DIR = PROTOCOLS_DIR / "custom"
SAMPLES_DIR = PROTOCOLS_DIR / "presets"

# Preset template for first-run config generation.
# api_key left empty for user to fill.
PRESET_TEMPLATE: dict[str, dict[str, Any]] = {
    # --- Domestic (China) ---
    "deepseek": {
        "base_url": "https://api.deepseek.com/v1",
        "model": "deepseek-chat",
        "max_tokens": 8192,
    },
    "qwen": {
        "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "model": "qwen-plus",
        "max_tokens": 8192,
    },
    "moonshot": {
        "base_url": "https://api.moonshot.cn/v1",
        "model": "kimi-k2-0711-preview",
        "max_tokens": 8192,
    },
    "zhipuai": {
        "base_url": "https://open.bigmodel.cn/api/paas/v4",
        "model": "glm-4.6",
        "max_tokens": 8192,
    },
    "minimax": {
        "base_url": "https://api.minimaxi.com/v1",
        "model": "MiniMax-Text-01",
        "max_tokens": 8192,
        "status": "pending_verification",
    },
    # --- International ---
    "openai": {
        "base_url": "https://api.openai.com/v1",
        "model": "gpt-4.1",
        "max_tokens": 8192,
    },
    "gemini": {
        "base_url": "https://generativelanguage.googleapis.com/v1beta/openai/",
        "model": "gemini-2.5-pro",
        "max_tokens": 8192,
    },
    "anthropic": {
        "base_url": "https://api.anthropic.com/v1/",
        "model": "claude-sonnet-4-5",
        "max_tokens": 8192,
    },
    "openrouter": {
        "base_url": "https://openrouter.ai/api/v1",
        "model": "anthropic/claude-sonnet-4",
        "max_tokens": 8192,
    },
}


@dataclass
class ProviderConfig:
    """Configuration for a single OpenAI-compatible provider."""

    base_url: str
    api_key: str
    model: str
    max_tokens: int = 8192

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "ProviderConfig":
        return cls(
            base_url=data.get("base_url", ""),
            api_key=data.get("api_key", ""),
            model=data.get("model", ""),
            max_tokens=data.get("max_tokens", 8192),
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "base_url": self.base_url,
            "api_key": self.api_key,
            "model": self.model,
            "max_tokens": self.max_tokens,
        }


@dataclass
class ServerConfig:
    """Top-level server configuration."""

    active: str | None = None
    providers: dict[str, ProviderConfig] = field(default_factory=dict)
    host: str = "0.0.0.0"
    port: int = 8765


def _generate_default_config() -> dict[str, Any]:
    """Generate the default config template with preset providers (api_key empty)."""
    providers: dict[str, Any] = {}
    for name, preset in PRESET_TEMPLATE.items():
        providers[name] = {
            "base_url": preset["base_url"],
            "api_key": "",
            "model": preset["model"],
            "max_tokens": preset["max_tokens"],
        }
    return {
        "active": None,
        "providers": providers,
        "server": {"host": "0.0.0.0", "port": 8765},
    }


def load_config() -> ServerConfig:
    """Load config from ~/.agenui/config.json.

    If the file does not exist, generates a default template and saves it.
    """
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)

    if not CONFIG_FILE.exists():
        default = _generate_default_config()
        _write_config_file(default)

    try:
        raw = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        raw = _generate_default_config()

    providers: dict[str, ProviderConfig] = {}
    for name, pdata in raw.get("providers", {}).items():
        if isinstance(pdata, dict):
            providers[name] = ProviderConfig.from_dict(pdata)

    server_section = raw.get("server", {})
    return ServerConfig(
        active=raw.get("active"),
        providers=providers,
        host=server_section.get("host", "0.0.0.0"),
        port=server_section.get("port", 8765),
    )


def save_config(cfg: ServerConfig) -> None:
    """Persist config to ~/.agenui/config.json with chmod 600."""
    data: dict[str, Any] = {
        "active": cfg.active,
        "providers": {name: pc.to_dict() for name, pc in cfg.providers.items()},
        "server": {"host": cfg.host, "port": cfg.port},
    }
    _write_config_file(data)


def _write_config_file(data: dict[str, Any]) -> None:
    """Write config JSON and set file permission to 600."""
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    CONFIG_FILE.write_text(
        json.dumps(data, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    os.chmod(CONFIG_FILE, stat.S_IRUSR | stat.S_IWUSR)  # 0o600


def get_available_providers(cfg: ServerConfig) -> list[dict[str, Any]]:
    """Return providers that have a non-empty api_key, with masked key display."""
    result: list[dict[str, Any]] = []
    for name, pc in cfg.providers.items():
        if not pc.api_key:
            continue
        result.append({
            "name": name,
            "model": pc.model,
            "base_url": pc.base_url,
            "max_tokens": pc.max_tokens,
            "api_key_display": mask_api_key(pc.api_key),
            "is_active": name == cfg.active,
        })
    return result


def mask_api_key(key: str) -> str:
    """Mask API key for display, showing only last 4 characters."""
    if not key or len(key) <= 4:
        return "****"
    return f"{key[:3]}...{key[-4:]}"


def _get_wifi_ip() -> str | None:
    """Try to get the WiFi interface IP (en0 on macOS, wlan0 on Linux).

    When VPN or virtual interfaces alter the routing table, the socket-connect
    method may return a non-LAN-routable IP.  The WiFi interface IP is what
    phones on the same network can actually reach.
    """
    import platform
    import subprocess

    try:
        if platform.system() == "Darwin":
            out = subprocess.check_output(
                ["ipconfig", "getifaddr", "en0"], text=True, stderr=subprocess.DEVNULL
            ).strip()
            if out:
                return out
        else:
            # Linux: prefer wlan0, fall back to first non-loopback from hostname -I
            for cmd in (
                ["hostname", "-I"],
                ["ip", "-4", "addr", "show", "wlan0"],
            ):
                try:
                    out = subprocess.check_output(
                        cmd, text=True, stderr=subprocess.DEVNULL
                    ).strip()
                    for token in out.split():
                        if token[0].isdigit() and not token.startswith("127."):
                            return token.split("/")[0]
                except (OSError, subprocess.CalledProcessError):
                    continue
    except (OSError, subprocess.CalledProcessError):
        pass
    return None


def get_lan_ip() -> str:
    """Detect the LAN IP reachable by phones on the same WiFi network.

    Prefers the WiFi interface address; falls back to the default-route
    socket trick when no WiFi interface is found.
    """
    wifi_ip = _get_wifi_ip()
    if wifi_ip:
        return wifi_ip
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
        finally:
            s.close()
    except OSError:
        return "127.0.0.1"
