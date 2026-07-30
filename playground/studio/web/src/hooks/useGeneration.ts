/** Generation lifecycle state machine.
 *
 * idle --generate--> generating --done--> done
 * generating --error--> error
 * generating --stop--> idle (aborts the SSE stream)
 *
 * Tokens accumulate in a buffer and are re-parsed (throttled ~100ms) into
 * thinking text + streaming protocol JSON. On `done`, the editor is replaced
 * with the clean, formatted protocol returned by the backend.
 */

import { useCallback, useRef, useState } from "react";
import { startGeneration } from "@/api/sse";
import type { ChatMessage } from "@/api/sse";
import { parseStream } from "@/lib/protocolParser";
import type {
  DoneEvent,
  ErrorEvent,
  GenerationStatus,
  StageName,
} from "@/types";

export interface GenerationState {
  status: GenerationStatus;
  currentStage: StageName | null;
  model: string | null;
  prompt: string;
  /** Chain-of-thought streamed by reasoning models (display-only). */
  reasoning: string;
  thinking: string;
  componentsText: string;
  datamodelText: string;
  /** Epoch ms when the current round started (null when idle). */
  startedAt: number | null;
  done: DoneEvent | null;
  error: ErrorEvent | null;
}

const INITIAL: GenerationState = {
  status: "idle",
  currentStage: null,
  model: null,
  prompt: "",
  reasoning: "",
  thinking: "",
  componentsText: "",
  datamodelText: "",
  startedAt: null,
  done: null,
  error: null,
};

const PARSE_INTERVAL_MS = 100;

export function useGeneration() {
  const [state, setState] = useState<GenerationState>(INITIAL);
  const bufferRef = useRef("");
  const reasoningRef = useRef("");
  const throttleRef = useRef<number | null>(null);
  const abortRef = useRef<AbortController | null>(null);
  const abortedRef = useRef(false);

  const flushParsed = useCallback(() => {
    const parsed = parseStream(bufferRef.current);
    setState((s) => ({
      ...s,
      reasoning: reasoningRef.current,
      thinking: parsed.thinking,
      componentsText: parsed.components,
      datamodelText: parsed.datamodel,
    }));
  }, []);

  const scheduleParse = useCallback(() => {
    if (throttleRef.current != null) return;
    throttleRef.current = window.setTimeout(() => {
      throttleRef.current = null;
      flushParsed();
    }, PARSE_INTERVAL_MS);
  }, [flushParsed]);

  const clearThrottle = useCallback(() => {
    if (throttleRef.current != null) {
      window.clearTimeout(throttleRef.current);
      throttleRef.current = null;
    }
  }, []);

  const generate = useCallback(
    (prompt: string, mode: string, provider: string | null, reasoning: boolean, history: ChatMessage[] = []) => {
      // Reset for a new round.
      bufferRef.current = "";
      reasoningRef.current = "";
      abortedRef.current = false;
      clearThrottle();
      abortRef.current?.abort();
      const controller = new AbortController();
      abortRef.current = controller;

      setState({
        ...INITIAL,
        status: "generating",
        prompt,
        startedAt: Date.now(),
      });

      startGeneration(
        prompt,
        mode,
        provider,
        reasoning,
        {
          onStage: (stage) => {
            setState((s) => ({
              ...s,
              currentStage: stage.stage,
              model: stage.model ?? s.model,
            }));
          },
          onToken: (token) => {
            bufferRef.current += token.content;
            scheduleParse();
          },
          onReasoning: (reasoning) => {
            reasoningRef.current += reasoning.content;
            scheduleParse();
          },
          onDone: (done) => {
            clearThrottle();
            setState((s) => ({
              ...s,
              status: "done",
              currentStage: null,
              done,
              // Replace streaming text with the clean, formatted protocol.
              componentsText: done.components
                ? JSON.stringify(done.components, null, 2)
                : s.componentsText,
              datamodelText: done.datamodel
                ? JSON.stringify(done.datamodel, null, 2)
                : s.datamodelText,
            }));
          },
          onError: (error) => {
            clearThrottle();
            // A user-initiated stop surfaces as an abort; treat it as idle.
            if (abortedRef.current) {
              setState((s) => ({ ...s, status: "idle", currentStage: null }));
              return;
            }
            setState((s) => ({
              ...s,
              status: "error",
              currentStage: null,
              error,
            }));
          },
        },
        controller.signal,
        history,
      ).catch(() => {
        // fetchEventSource rejects after onerror; state already handled there.
      });
    },
    [clearThrottle, scheduleParse],
  );

  const stop = useCallback(() => {
    abortedRef.current = true;
    abortRef.current?.abort();
    // fetch-event-source resolves (does NOT call onerror) when the external
    // signal is aborted, so we must flip the status here ourselves; relying
    // on onError would leave the round stuck in "generating".
    clearThrottle();
    setState((s) => ({ ...s, status: "idle", currentStage: null }));
  }, [clearThrottle]);

  /** Reset to the pristine idle state to start a brand-new conversation.
   * Aborts any in-flight stream and clears the prompt, thinking text and
   * result so nothing from the previous round lingers. */
  const reset = useCallback(() => {
    abortedRef.current = true;
    abortRef.current?.abort();
    clearThrottle();
    bufferRef.current = "";
    reasoningRef.current = "";
    setState(INITIAL);
  }, [clearThrottle]);

  /** Editor onChange handlers (manual edits after generation completes). */
  const setComponentsText = useCallback((text: string) => {
    setState((s) => ({ ...s, componentsText: text }));
  }, []);
  const setDatamodelText = useCallback((text: string) => {
    setState((s) => ({ ...s, datamodelText: text }));
  }, []);

  return {
    ...state,
    isGenerating: state.status === "generating",
    generate,
    stop,
    reset,
    setComponentsText,
    setDatamodelText,
  };
}
