/** SSE client for POST /api/generate using @microsoft/fetch-event-source.
 *
 * The browser's native EventSource only supports GET, so we use fetch-based
 * SSE to send the prompt in a POST body. Supports aborting via AbortSignal.
 */

import { fetchEventSource } from "@microsoft/fetch-event-source";
import type {
  DoneEvent,
  ErrorEvent,
  ReasoningEvent,
  StageEvent,
  TokenEvent,
} from "@/types";

export interface GenerateCallbacks {
  onStage: (stage: StageEvent) => void;
  onToken: (token: TokenEvent) => void;
  onReasoning: (reasoning: ReasoningEvent) => void;
  onDone: (done: DoneEvent) => void;
  onError: (error: ErrorEvent) => void;
}

export interface ChatMessage {
  role: "user" | "assistant";
  content: string;
}

export async function startGeneration(
  prompt: string,
  mode: string,
  provider: string | null,
  reasoning: boolean,
  callbacks: GenerateCallbacks,
  signal: AbortSignal,
  history: ChatMessage[] = [],
): Promise<void> {
  await fetchEventSource("/api/generate", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ prompt, mode, stream: true, provider, reasoning, history }),
    signal,
    // Keep the stream alive even when the tab is backgrounded.
    openWhenHidden: true,
    onmessage(ev) {
      let data: unknown;
      try {
        data = JSON.parse(ev.data);
      } catch {
        return; // ignore malformed frames
      }
      switch (ev.event) {
        case "stage":
          callbacks.onStage(data as StageEvent);
          break;
        case "token":
          callbacks.onToken(data as TokenEvent);
          break;
        case "reasoning":
          callbacks.onReasoning(data as ReasoningEvent);
          break;
        case "done":
          callbacks.onDone(data as DoneEvent);
          break;
        case "error":
          callbacks.onError(data as ErrorEvent);
          break;
        default:
          break;
      }
    },
    onerror(err) {
      // Surface as an error event and stop (do not let fetchEventSource retry).
      callbacks.onError({
        message: err instanceof Error ? err.message : String(err),
        code: "stream_error",
      });
      throw err;
    },
  });
}
