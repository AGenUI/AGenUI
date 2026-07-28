/** Shared TypeScript types mirroring the AGenUI Studio backend API. */

export interface Provider {
  name: string;
  model: string;
  base_url: string;
  max_tokens: number;
  api_key_display: string;
  is_active: boolean;
}

export interface ProvidersResponse {
  active: string | null;
  providers: Provider[];
}

/** Full provider config (with raw api_key) used by the configuration modal. */
export interface ConfigProvider {
  name: string;
  base_url: string;
  api_key: string;
  model: string;
  max_tokens: number;
}

export interface ServerInfo {
  lan_ip: string;
  port: number;
  base_url: string;
}

/** A2UI protocol payloads (opaque JSON from the generator). */
export type A2uiPayload = Record<string, unknown>;

export interface ProtocolSummary {
  id: string;
  created_at: string;
  mode: string;
  provider: string;
  model: string;
  prompt_summary: string;
}

export interface ProtocolRecord {
  id: string;
  created_at: string;
  prompt: string;
  mode: string;
  provider: string;
  model: string;
  components: A2uiPayload;
  datamodel: A2uiPayload | null;
}

export interface PresetSummary {
  id: string;
  name: string;
  /** Whether a reference rendering.png exists for this preset. */
  has_rendering: boolean;
}

export interface PresetRecord {
  id: string;
  name: string;
  components: A2uiPayload;
  datamodel: A2uiPayload | null;
  /** Whether a reference rendering.png exists for this preset. */
  has_rendering: boolean;
}

/** SSE event payloads from POST /api/generate. */
export type StageName =
  | "building_prompt"
  | "calling_model"
  | "extracting"
  | "validating"
  | "saving";

export interface StageEvent {
  stage: StageName;
  model?: string;
}

export interface TokenEvent {
  content: string;
}

/** Chain-of-thought token from a reasoning model (GLM-5, DeepSeek-R1, ...).
 * Streamed long before the final answer; display-only (never part of the
 * A2UI payload). */
export interface ReasoningEvent {
  content: string;
}

export interface DoneEvent {
  success: boolean;
  protocol_id: string;
  protocol_url: string;
  components: A2uiPayload;
  datamodel: A2uiPayload | null;
  validation_passed: boolean;
  validation_errors: string[];
  validation_warnings: string[];
}

export interface ErrorEvent {
  message: string;
  code: string;
  status_code?: number | null;
  detail?: string | null;
  raw_response?: string;
}

/** Overall generation lifecycle state. */
export type GenerationStatus = "idle" | "generating" | "done" | "error";

/** Snapshot of a finished (or stopped) generation round, kept for history. */
export interface RoundSnapshot {
  id: string;
  prompt: string;
  model: string | null;
  reasoning: string;
  thinking: string;
  done: DoneEvent | null;
  error: ErrorEvent | null;
}
