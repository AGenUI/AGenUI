"""A2UI generation orchestrator for AGenUI Studio.

This module wires together the reused benchmark building blocks (prompt building,
JSON extraction, A2UI validation) with a BYOK provider, and emits a stream of
``GenerationEvent`` objects that the server forwards to the browser over SSE.

Reused (read-only, NOT modified) from ``test/a2ui_benchmark``:
    - generation/prompt_builder.py : build_system_prompt / build_user_prompt
    - generation/extractor.py      : extract_json_blocks / parse_json_pair
    - validation/validator.py      : validate_payloads

Generation loop (see plan Part B5):
    building_prompt -> calling_model (stream tokens) -> extracting -> validating
    -> saving -> done
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Generator

from .benchmark.generation.extractor import (
    extract_json_blocks,
    parse_json_pair,
)
from .benchmark.generation.prompt_builder import (
    build_system_prompt,
    build_user_prompt,
)
from .benchmark.validation.validator import validate_payloads

from . import storage
from .providers import OpenAICompatProvider, ProviderError


# Repo root / skills / a2ui-generation (shared read-only with the benchmark).
SKILL_DIR = Path(__file__).resolve().parents[3] / "skills" / "a2ui-generation"


# Instruction wrapped around the user message on refinement turns (i.e. when a
# conversation history carrying a previous protocol is present). Without it the
# model treats a follow-up as a brand-new request and regenerates from scratch,
# producing a result that diverges heavily from the original protocol. New
# conversations have no history, so their prompt is sent through untouched.
REFINEMENT_INSTRUCTION = """\
The conversation above contains a previously generated A2UI protocol: the
assistant's most recent message holds it as two JSON code blocks (the first is
updateComponents, the second is updateDataModel).

The user now wants to refine that existing protocol. You MUST:
1. Treat the previous protocol as the baseline. Keep its structure, components,
   data bindings, content and styling UNCHANGED, except for the specific
   modifications requested below.
2. Apply ONLY the changes described in the user's request. Do not redesign,
   re-layout, or rewrite unrelated parts.
3. Output the COMPLETE updated protocol - both the full updateComponents block
   and the full updateDataModel block - not just the changed fragments.

User's refinement request:
{user_request}"""


@dataclass
class GenerationEvent:
    """A single SSE event pushed from the server to the browser.

    type is one of: "stage" | "token" | "reasoning" | "done" | "error".

    ``token`` carries the final-answer text (the A2UI JSON); ``reasoning``
    carries a reasoning model's chain-of-thought (display-only, streamed long
    before the answer so the UI can show live "thinking" progress).
    """

    type: str
    data: dict[str, Any] = field(default_factory=dict)


def _stage(name: str, **extra: Any) -> GenerationEvent:
    return GenerationEvent(type="stage", data={"stage": name, **extra})


def _split_combined_payload(
    comp_dict: dict | None, data_dict: dict | None
) -> tuple[dict | None, dict | None]:
    """Split a single combined payload back into the expected pair.

    On refinement turns some models collapse the two required blocks into one
    object shaped like {"updateComponents": ..., "updateDataModel": ...} (often
    echoing the format they saw in the chat history). The two-block extractor
    yields that object as ``comp_dict`` with ``data_dict`` still None; recover
    by splitting it so the turn still succeeds.
    """
    if data_dict is not None or not isinstance(comp_dict, dict):
        return comp_dict, data_dict
    if "updateComponents" in comp_dict and "updateDataModel" in comp_dict:
        version = comp_dict.get("version", "v0.9")
        return (
            {"version": version, "updateComponents": comp_dict["updateComponents"]},
            {"version": version, "updateDataModel": comp_dict["updateDataModel"]},
        )
    return comp_dict, data_dict


def _attempt(full_text: str) -> dict[str, Any]:
    """Run extract -> parse -> validate over a raw model response.

    Returns a dict with keys: components, datamodel, validation, error, raw.
    ``components``/``datamodel`` are None when extraction/parsing failed.
    """
    comp_json, data_json = extract_json_blocks(full_text)
    comp_dict, data_dict, parse_error = parse_json_pair(comp_json, data_json)

    # Recover from a combined single-object response (see helper docstring).
    comp_dict, data_dict = _split_combined_payload(comp_dict, data_dict)
    if comp_dict is not None and data_dict is not None:
        parse_error = None

    if comp_dict is None or data_dict is None:
        return {
            "components": None,
            "datamodel": None,
            "validation": None,
            "error": parse_error or "Failed to extract A2UI JSON from model output",
            "raw": full_text,
        }

    validation = validate_payloads(comp_dict, data_dict)
    return {
        "components": comp_dict,
        "datamodel": data_dict,
        "validation": validation,
        "error": None,
        "raw": full_text,
    }


def _has_payload(result: dict[str, Any]) -> bool:
    return result.get("components") is not None and result.get("datamodel") is not None


def generate_a2ui_stream(
    provider: OpenAICompatProvider,
    user_prompt: str,
    mode: str = "component",
    enable_reasoning: bool | None = None,
    history: list[dict] | None = None,
) -> Generator[GenerationEvent, None, None]:
    """Generate an A2UI protocol, yielding progress events as they happen.

    Tokens are streamed to the caller as the model produces them. After the
    stream completes, the response is extracted and validated. A parseable
    result is always saved and returned (with its validation report); only a
    total extraction failure yields an ``error`` event. No automatic retry is
    performed.

    ``enable_reasoning`` is forwarded to the provider to force the model's
    thinking switch on/off (``None`` keeps the model default).

    ``history`` is an optional list of prior chat messages (user/assistant
    dicts) enabling multi-turn refinement of a protocol.
    """
    try:
        yield _stage("building_prompt")
        is_page = mode == "page"
        system_prompt = build_system_prompt(
            SKILL_DIR,
            is_page=is_page,
            allow_placeholder_images=True,
        )
        user_message = build_user_prompt(user_prompt)
        # On refinement turns, explicitly frame the request as an incremental
        # modification of the previous protocol so the model preserves the rest
        # (a bare follow-up prompt would otherwise be treated as a fresh
        # generation and diverge heavily from the original).
        if history:
            user_message = REFINEMENT_INSTRUCTION.format(user_request=user_message)

        yield _stage("calling_model", model=provider.model)
        full_text = ""
        for tok in provider.chat_stream(
            system_prompt, user_message, enable_reasoning=enable_reasoning,
            history=history,
        ):
            if tok.kind == "reasoning":
                # Chain-of-thought: display-only, does not enter the payload.
                yield GenerationEvent(type="reasoning", data={"content": tok.text})
            else:
                full_text += tok.text
                yield GenerationEvent(type="token", data={"content": tok.text})

        yield _stage("extracting")
        yield _stage("validating")
        result = _attempt(full_text)

        if not _has_payload(result):
            yield GenerationEvent(
                type="error",
                data={
                    "message": (
                        "Failed to extract a valid A2UI protocol from the model "
                        "output. Please refine your prompt and try again."
                    ),
                    "code": "extraction_failed",
                    "raw_response": result.get("raw", ""),
                },
            )
            return

        yield _stage("saving")
        record = storage.save_protocol(
            prompt=user_prompt,
            mode=mode,
            provider=provider.name,
            model=provider.model,
            components_dict=result["components"],
            datamodel_dict=result["datamodel"],
        )

        validation = result.get("validation") or {}
        yield GenerationEvent(
            type="done",
            data={
                "success": True,
                "protocol_id": record["id"],
                "protocol_url": f"/api/protocols/{record['id']}/raw",
                "components": result["components"],
                "datamodel": result["datamodel"],
                "validation_passed": validation.get("validation_passed", False),
                "validation_errors": validation.get("validation_errors", []),
                "validation_warnings": validation.get("validation_warnings", []),
            },
        )

    except ProviderError as exc:
        yield GenerationEvent(
            type="error",
            data={
                "message": exc.message,
                "code": exc.code,
                "status_code": exc.status_code,
                "detail": exc.detail,
            },
        )
    except FileNotFoundError as exc:
        yield GenerationEvent(
            type="error",
            data={"message": f"Skill resources missing: {exc}", "code": "config"},
        )
    except Exception as exc:  # noqa: BLE001 - surface any unexpected failure
        yield GenerationEvent(
            type="error",
            data={"message": f"Unexpected error: {exc}", "code": "internal"},
        )


def generate_a2ui_sync(
    provider: OpenAICompatProvider,
    user_prompt: str,
    mode: str = "component",
    enable_reasoning: bool | None = None,
    history: list[dict] | None = None,
) -> dict[str, Any]:
    """Non-streaming wrapper (for curl / testing). Returns the final event data."""
    final: dict[str, Any] = {
        "success": False,
        "message": "Generation produced no result",
        "code": "internal",
    }
    for event in generate_a2ui_stream(provider, user_prompt, mode, enable_reasoning, history):
        if event.type in ("done", "error"):
            final = event.data
    return final
