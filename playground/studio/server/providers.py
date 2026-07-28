"""OpenAI-compatible model providers for AGenUI Studio.

All supported models (DeepSeek, Qwen, Moonshot, Zhipu, OpenAI, Gemini, Claude,
MiniMax, and any custom endpoint such as local Ollama/vLLM) expose an
OpenAI-compatible ``chat/completions`` endpoint. The differences between them
converge to four config fields (base_url, api_key, model, max_tokens), so a
single ``OpenAICompatProvider`` implementation serves every provider.

This module only talks to the LLM. It does NOT know about A2UI extraction or
validation - that orchestration lives in ``generator.py``.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Generator

from openai import OpenAI
from openai import (
    APIConnectionError,
    APIStatusError,
    APITimeoutError,
    AuthenticationError,
    PermissionDeniedError,
    RateLimitError,
)

from .config import ProviderConfig


@dataclass
class StreamToken:
    """A single incremental piece of a streaming completion.

    ``kind`` is one of:
        - ``"reasoning"``: chain-of-thought tokens emitted by reasoning models
          (e.g. GLM-5, DeepSeek-R1) — see :func:`_extract_reasoning` for the
          field names probed. These arrive long before the final answer and are
          what the UI shows while the model is "thinking".
        - ``"content"``: the final answer tokens (``delta.content``), which for
          A2UI generation is the JSON protocol text.
    """

    kind: str
    text: str


# Field names different providers use for chain-of-thought tokens in a
# streaming chat-completion delta, in priority order. ``reasoning_content``
# is the de-facto standard across the OpenAI-compatible ecosystem (DeepSeek-R1,
# GLM-4/5, Qwen-thinking, Kimi-k1.5, ...); ``reasoning`` and ``thinking`` cover
# a few other endpoints. Only non-empty *string* values are treated as
# reasoning so structured fields (e.g. OpenAI o-series' ``reasoning`` object)
# are never misread as display text.
_REASONING_FIELDS: tuple[str, ...] = ("reasoning_content", "reasoning", "thinking")


def _extract_reasoning(delta: Any) -> str:
    """Return the chain-of-thought text from a stream delta, or ``""``.

    Probes the known reasoning field names in priority order and returns the
    first non-empty string. Models that do not expose reasoning (the common
    non-reasoning case) yield ``""`` here and only ever produce ``content``
    tokens, so this is a no-op for them.
    """
    for field in _REASONING_FIELDS:
        value = getattr(delta, field, None)
        if isinstance(value, str) and value:
            return value
    return ""


# --- Reasoning on/off switch -------------------------------------------------
#
# Reasoning models default to "thinking ON", which adds a long chain-of-thought
# before the answer and badly hurts first-token latency for short A2UI card
# generation. Most providers expose an API switch to force it on/off, but the
# parameter NAME and VALUE TYPE differ per vendor, so we must discriminate:
#
#   - DeepSeek & GLM (Zhipu):  extra_body={"thinking": {"type": "enabled"|"disabled"}}
#                              (a string enum nested under ``thinking.type``)
#   - Qwen (DashScope):        extra_body={"enable_thinking": True|False}
#                              (a flat boolean)
#   - OpenAI o-series / Gemini / Claude / Moonshot / custom endpoints:
#                              no portable on/off switch we can safely send
#                              (OpenAI only has ``reasoning_effort`` which cannot
#                              fully disable reasoning and does not emit
#                              ``reasoning_content``) -> leave untouched.
#
# Detection keys off the provider name, base_url and model name so it works for
# both the preset providers and user-added custom entries.
def _reasoning_family(name: str, base_url: str, model: str) -> str:
    """Classify a provider into a reasoning-control convention.

    Returns one of ``"thinking_type"`` (DeepSeek/GLM string enum),
    ``"enable_thinking"`` (Qwen boolean) or ``"none"`` (no safe switch).
    """
    blob = f"{name} {base_url} {model}".lower()
    if "deepseek" in blob:
        return "thinking_type"
    if "bigmodel" in blob or "zhipuai" in blob or "zhipu" in blob or "glm" in blob:
        return "thinking_type"
    if "dashscope" in blob or "qwen" in blob or "qwq" in blob:
        return "enable_thinking"
    return "none"


def _reasoning_kwargs(family: str, enable: bool) -> dict[str, Any]:
    """Build the ``chat.completions.create`` kwargs that force reasoning on/off.

    ``enable`` is the user's intent (True = reason, False = skip thinking).
    Returns an empty dict for the ``"none"`` family so unknown endpoints are
    never sent a parameter they might reject.
    """
    if family == "thinking_type":
        return {"extra_body": {"thinking": {"type": "enabled" if enable else "disabled"}}}
    if family == "enable_thinking":
        return {"extra_body": {"enable_thinking": enable}}
    return {}


class ProviderError(Exception):
    """Classified provider error carrying a user-friendly message.

    Attributes:
        message: Human-readable description (already localized for the user).
        code: Stable machine-readable code, e.g. ``"auth"``, ``"rate_limit"``.
        status_code: Underlying HTTP status code if available, else ``None``.
        detail: Raw provider error message (for diagnostics), else ``None``.
    """

    def __init__(
        self,
        message: str,
        code: str,
        status_code: int | None = None,
        detail: str | None = None,
    ):
        super().__init__(message)
        self.message = message
        self.code = code
        self.status_code = status_code
        self.detail = detail


def _extract_detail(exc: Exception) -> str:
    """Pull the provider's raw error message from an openai exception.

    OpenAI-compatible providers return an error body like
    ``{"error": {"message": "...", "code": "..."}}``; surface that message so
    callers can see the real reason instead of only the classified summary.
    """
    body = getattr(exc, "body", None)
    if isinstance(body, dict):
        err_obj = body.get("error", body)
        if isinstance(err_obj, dict):
            msg = err_obj.get("message")
            if msg:
                return str(msg)
    return str(exc)


def classify_error(exc: Exception) -> ProviderError:
    """Translate a raw provider/network exception into a ProviderError.

    Mapping (see plan Part B7):
        401 / 403            -> invalid or expired API key
        429                  -> rate limited
        402 / balance        -> insufficient balance
        timeout / connection -> network failure (overseas models need proxy)
        other 5xx            -> service temporarily unavailable

    The provider's raw message is always preserved in ``detail``.
    """
    detail = _extract_detail(exc)

    # Network-level failures (no HTTP response received).
    if isinstance(exc, (APITimeoutError, TimeoutError)):
        return ProviderError(
            "Request timed out. Overseas models (OpenAI/Gemini/Claude) may "
            "require a proxy network.",
            "timeout",
            None,
            detail,
        )
    if isinstance(exc, (APIConnectionError, ConnectionError, OSError)):
        return ProviderError(
            "Network connection failed. Overseas models (OpenAI/Gemini/Claude) "
            "require a proxy network; local endpoints (Ollama/vLLM) must be "
            "running and reachable.",
            "network",
            None,
            detail,
        )

    # HTTP-level failures (a response was received).
    status = getattr(exc, "status_code", None)

    if isinstance(exc, AuthenticationError) or status in (401,):
        return ProviderError(
            "API Key is invalid or expired. Check the api_key of the "
            "corresponding provider in config.json.",
            "auth",
            status,
            detail,
        )
    if isinstance(exc, PermissionDeniedError) or status in (403,):
        return ProviderError(
            "Access denied. The API Key may lack permission for this model.",
            "auth",
            status,
            detail,
        )
    if isinstance(exc, RateLimitError) or status == 429:
        return ProviderError(
            "Too many requests. Please retry later.",
            "rate_limit",
            status,
            detail,
        )
    if status == 402:
        return ProviderError(
            "Insufficient account balance. Please top up and retry.",
            "balance",
            status,
            detail,
        )
    if status is not None and 500 <= status < 600:
        return ProviderError(
            "Model service is temporarily unavailable. Please retry later.",
            "server_error",
            status,
            detail,
        )

    # Fallback: keep the original message for diagnostics.
    return ProviderError(f"Provider error: {exc}", "unknown", status, detail)


class OpenAICompatProvider:
    """A thin wrapper over the OpenAI SDK pointing at any compatible endpoint."""

    def __init__(
        self,
        name: str,
        base_url: str,
        api_key: str,
        model: str,
        max_tokens: int = 8192,
    ):
        self.name = name
        self.base_url = base_url
        self.api_key = api_key
        self.model = model
        self.max_tokens = max_tokens
        self._client = OpenAI(base_url=base_url, api_key=api_key or "empty")

    def _messages(
        self,
        system_prompt: str,
        user_prompt: str,
        history: list[dict] | None = None,
    ) -> list[dict]:
        msgs: list[dict] = [{"role": "system", "content": system_prompt}]
        if history:
            msgs.extend(history)
        msgs.append({"role": "user", "content": user_prompt})
        return msgs

    def chat(self, system_prompt: str, user_prompt: str, timeout: float = 120) -> str:
        """Non-streaming completion. Returns the full assistant text.

        Raises:
            ProviderError: on any classified failure.
        """
        try:
            resp = self._client.chat.completions.create(
                model=self.model,
                messages=self._messages(system_prompt, user_prompt),
                max_tokens=self.max_tokens,
                stream=False,
                timeout=timeout,
            )
            return resp.choices[0].message.content or ""
        except Exception as exc:  # noqa: BLE001 - reclassify all provider errors
            raise classify_error(exc) from exc

    def chat_stream(
        self,
        system_prompt: str,
        user_prompt: str,
        timeout: float = 120,
        enable_reasoning: bool | None = None,
        history: list[dict] | None = None,
    ) -> Generator[StreamToken, None, None]:
        """Streaming completion. Yields incremental :class:`StreamToken` items.

        Reasoning models (GLM-5, DeepSeek-R1, ...) emit their chain-of-thought
        in ``delta.reasoning_content`` before the answer; those tokens are
        yielded with ``kind="reasoning"`` so callers can surface live progress
        while the model thinks. The final answer arrives in ``delta.content``
        and is yielded with ``kind="content"``. Non-reasoning models only ever
        yield ``content`` tokens.

        ``enable_reasoning`` optionally forces the model's thinking switch:
        ``True`` = reason, ``False`` = skip thinking (faster first token),
        ``None`` = leave the model default untouched. The correct vendor-specific
        parameter is chosen by :func:`_reasoning_family`; providers without a
        safe switch ignore the flag.

        The caller accumulates ``content`` tokens to reconstruct the full
        response; ``reasoning`` tokens are display-only.

        Raises:
            ProviderError: on any classified failure.
        """
        extra: dict[str, Any] = {}
        if enable_reasoning is not None:
            family = _reasoning_family(self.name, self.base_url, self.model)
            extra = _reasoning_kwargs(family, enable_reasoning)
        try:
            stream = self._client.chat.completions.create(
                model=self.model,
                messages=self._messages(system_prompt, user_prompt, history),
                max_tokens=self.max_tokens,
                stream=True,
                timeout=timeout,
                **extra,
            )
            for chunk in stream:
                if not chunk.choices:
                    continue
                delta = chunk.choices[0].delta
                reasoning = _extract_reasoning(delta)
                if reasoning:
                    yield StreamToken(kind="reasoning", text=reasoning)
                token = getattr(delta, "content", None) or ""
                if token:
                    yield StreamToken(kind="content", text=token)
        except Exception as exc:  # noqa: BLE001 - reclassify all provider errors
            raise classify_error(exc) from exc

    def test_connection(self) -> dict:
        """Lightweight connectivity check.

        Returns ``{"ok", "error", "code", "status_code", "detail"}`` where
        ``detail`` carries the provider's raw error message for diagnostics.
        """
        try:
            resp = self._client.chat.completions.create(
                model=self.model,
                messages=[{"role": "user", "content": "ping"}],
                max_tokens=16,
                stream=False,
                timeout=20,
            )
            _ = resp.choices[0].message.content
            return {"ok": True, "error": None, "code": None, "status_code": None, "detail": None}
        except Exception as exc:  # noqa: BLE001 - report any failure with full detail
            err = classify_error(exc)
            return {
                "ok": False,
                "error": err.message,
                "code": err.code,
                "status_code": err.status_code,
                "detail": err.detail,
            }


def build_provider(name: str, cfg: ProviderConfig) -> OpenAICompatProvider:
    """Construct a provider instance from a ProviderConfig entry."""
    return OpenAICompatProvider(
        name=name,
        base_url=cfg.base_url,
        api_key=cfg.api_key,
        model=cfg.model,
        max_tokens=cfg.max_tokens,
    )
