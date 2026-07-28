"""Data models for the A2UI model benchmark harness."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class ExpectedConstraints:
    """Scenario-level expectations used for pass/fail checks."""

    surface_id: str | None = None
    min_components: int = 0
    required_component_types: list[str] = field(default_factory=list)
    required_bindings: list[str] = field(default_factory=list)
    forbidden_component_types: list[str] = field(default_factory=list)
    requires_transformer: bool = False
    style_overrides: dict[str, Any] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, data: dict[str, Any] | None) -> "ExpectedConstraints":
        if not data:
            return cls()
        return cls(
            surface_id=data.get("surface_id"),
            min_components=data.get("min_components", 0),
            required_component_types=data.get("required_component_types", []),
            required_bindings=data.get("required_bindings", []),
            forbidden_component_types=data.get("forbidden_component_types", []),
            requires_transformer=data.get("requires_transformer", False),
            style_overrides=data.get("style_overrides", {}),
        )


@dataclass
class Scenario:
    """A single benchmark scenario (prompt + optional data + expectations)."""

    id: str
    name: str
    description: str
    mode: str  # "component" or "page"
    dto: bool
    difficulty: str
    tags: list[str]
    prompt: str
    data_file: Path | None
    data_file_content: dict[str, Any] | None
    expected: ExpectedConstraints
    dimensions: dict[str, str] = field(default_factory=dict)
    track: str = "default"
    subset: str = ""
    prompt_zh: str = ""
    prompt_en: str = ""
    design_image: Path | None = None
    design_roi_box: list[int] | None = None
    golden_spec_path: Path | None = None
    golden_spec_content: dict[str, Any] | None = None
    task_goal: str = ""
    interaction_type: str = ""
    page_type: str = ""
    must_have_elements: list[str] = field(default_factory=list)
    must_not_break_regions: list[str] = field(default_factory=list)
    golden_layout_regions: list[dict[str, Any]] = field(default_factory=list)
    round_instructions: list[str] = field(default_factory=list)
    edit_source_case_id: str = ""
    edit_source_components: Path | None = None
    edit_source_components_content: dict[str, Any] | None = None
    edit_source_datamodel: Path | None = None
    edit_source_datamodel_content: dict[str, Any] | None = None
    allowed_component_edit_paths: list[str] = field(default_factory=list)
    allowed_datamodel_edit_paths: list[str] = field(default_factory=list)
    expected_component_edit_values: dict[str, Any] = field(default_factory=dict)
    expected_datamodel_edit_values: dict[str, Any] = field(default_factory=dict)
    score_weights_override: dict[str, float] = field(default_factory=dict)
    expected_anchors: list[dict[str, Any]] = field(default_factory=list)


@dataclass
class JudgeScore:
    """LLM-as-judge scores for a single generation."""

    layout_quality: int | None = None
    visual_hierarchy: int | None = None
    data_binding_correctness: int | None = None
    prompt_adherence: int | None = None
    overall: int | None = None
    rationale: str = ""
    issues: list[str] = field(default_factory=list)
    raw_response: str = ""
    parse_error: str | None = None

    def average(self) -> float | None:
        scores = [
            self.layout_quality,
            self.visual_hierarchy,
            self.data_binding_correctness,
            self.prompt_adherence,
            self.overall,
        ]
        valid = [s for s in scores if s is not None]
        return sum(valid) / len(valid) if valid else None


@dataclass
class GenerationResult:
    """Result of one (scenario, provider, model) generation run."""

    scenario_id: str
    scenario_name: str
    scenario_prompt: str
    provider: str
    model: str
    mode: str
    dto: bool
    track: str = "default"
    subset: str = ""
    repeat_index: int = 1
    repeat_total: int = 1

    # Generation metadata
    latency_seconds: float | None = None
    prompt_tokens: int | None = None
    completion_tokens: int | None = None
    total_tokens: int | None = None
    tokens_estimated: bool = False
    cost_usd: float | None = None

    # Output payloads
    components_json: str | None = None
    datamodel_json: str | None = None
    components_dict: dict[str, Any] | None = None
    datamodel_dict: dict[str, Any] | None = None
    extraction_error: str | None = None

    # Validation
    validation_passed: bool = False
    error_count: int = 0
    warning_count: int = 0
    validation_errors: list[str] = field(default_factory=list)
    validation_warnings: list[str] = field(default_factory=list)

    # Scenario expectation checks
    scenario_checks_passed: bool = False
    scenario_check_failures: list[str] = field(default_factory=list)

    # Judge
    judge_scores: JudgeScore = field(default_factory=JudgeScore)

    # Render and iteration-1 scoring
    render_requested: bool = False
    render_success: bool = False
    render_error: str | None = None
    screenshot_path: str | None = None
    raw_screenshot_path: str | None = None
    render_region_box: list[int] | None = None
    screenshot_is_render_roi: bool = False
    design_image_path: str | None = None
    render_latency_seconds: float | None = None
    first_token_seconds: float | None = None
    blank_screen_detected: bool = False
    gate_level: str = "not_evaluated"
    gate_reasons: list[str] = field(default_factory=list)
    failure_tags: list[str] = field(default_factory=list)
    single_run_usable: bool = False
    objective_scores: dict[str, Any] = field(default_factory=dict)
    fidelity_scores: dict[str, Any] = field(default_factory=dict)
    anchor_scores: dict[str, Any] = field(default_factory=dict)
    iteration_history: list[dict[str, Any]] = field(default_factory=list)
    multi_round_convergence_score: float | None = None
    multi_round_evaluation: dict[str, Any] = field(default_factory=dict)
    local_edit_stability_score: float | None = None
    local_edit_target_score: float | None = None
    local_edit_evaluation: dict[str, Any] = field(default_factory=dict)
    final_case_score: float | None = None

    # File paths
    output_paths: dict[str, str] = field(default_factory=dict)

    # Provider-specific metadata
    provider_metadata: dict[str, Any] = field(default_factory=dict)


@dataclass
class RunMeta:
    """Metadata for an entire benchmark run."""

    run_id: str
    started_at: str
    dataset_path: str
    providers: list[dict[str, str]]
    max_rounds: int
    judge_model: str
    finished_at: str | None = None
    repeat_runs: int = 1
    track_summary: dict[str, Any] = field(default_factory=dict)
    subset_summary: dict[str, Any] = field(default_factory=dict)
    leaderboards: dict[str, Any] = field(default_factory=dict)
    toc_score: float | None = None
    tod_score: float | None = None
    overall_score: float | None = None
    hard_fail_rate: float | None = None
    severe_fail_rate: float | None = None
