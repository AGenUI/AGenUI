/** Conversation area: user prompts + assistant cards (stages, thinking, results). */

import { useEffect, useRef } from "react";
import { ErrorCard } from "./ErrorCard";
import { StageStepper } from "./StageStepper";
import { ThinkingProcess } from "./ThinkingProcess";
import { ValidationBanner } from "./ValidationBanner";
import { WaitingIndicator } from "./WaitingIndicator";
import { SparkleIcon, StopIcon } from "@/components/icons";
import type {
  DoneEvent,
  ErrorEvent,
  GenerationStatus,
  RoundSnapshot,
  StageName,
} from "@/types";

interface RoundView {
  id: string;
  prompt: string;
  model: string | null;
  currentStage: StageName | null;
  reasoning: string;
  thinking: string;
  startedAt: number | null;
  done: DoneEvent | null;
  error: ErrorEvent | null;
  live: boolean;
  status: GenerationStatus;
}

function AssistantCard({ round }: { round: RoundView }) {
  const generating = round.live && round.status === "generating";
  const finishedOk = round.done != null;
  // A round that was started (has a prompt) but is neither generating nor
  // finished (no done/error) was stopped by the user. Covers both the live
  // round right after Stop and archived history rounds.
  const stopped =
    !generating && !finishedOk && !round.error && round.prompt !== "";

  // Reasoning models stream their chain-of-thought separately; show it first,
  // followed by any non-JSON preamble parsed from the answer tokens.
  const displayThinking = [round.reasoning, round.thinking]
    .filter(Boolean)
    .join("\n\n");

  return (
    <div className="flex justify-start">
      <div className="w-full max-w-[92%] space-y-2 rounded-xl rounded-tl-sm border border-slate-200 bg-white p-3 shadow-sm">
        <StageStepper
          currentStage={round.currentStage}
          model={round.model}
          finished={finishedOk}
        />

        <ThinkingProcess
          thinking={displayThinking}
          streaming={generating && !round.done}
          defaultOpen={round.live}
          startedAt={round.startedAt}
        />

        {generating && !displayThinking && (
          <WaitingIndicator startedAt={round.startedAt} />
        )}

        {round.done && <ValidationBanner done={round.done} />}
        {round.error && <ErrorCard error={round.error} />}

        {stopped && (
          <div className="flex items-center gap-1.5 rounded-lg border border-slate-200 bg-slate-50 px-2.5 py-1.5 text-[11px] text-slate-400">
            <StopIcon size={11} />
            Generation stopped by user
          </div>
        )}
      </div>
    </div>
  );
}

function RoundBlock({ round }: { round: RoundView }) {
  return (
    <div className="space-y-2">
      <div className="flex justify-end">
        <div className="max-w-[85%] whitespace-pre-wrap rounded-xl rounded-tr-sm bg-brand-500 px-3.5 py-2 text-sm leading-6 text-white shadow-sm">
          {round.prompt}
        </div>
      </div>
      <AssistantCard round={round} />
    </div>
  );
}

export interface LiveRound {
  status: GenerationStatus;
  prompt: string;
  model: string | null;
  currentStage: StageName | null;
  reasoning: string;
  thinking: string;
  startedAt: number | null;
  done: DoneEvent | null;
  error: ErrorEvent | null;
}

interface ConversationPanelProps {
  history: RoundSnapshot[];
  live: LiveRound;
}

export function ConversationPanel({ history, live }: ConversationPanelProps) {
  const scrollRef = useRef<HTMLDivElement>(null);
  const showLive = live.status !== "idle" || live.prompt !== "";

  useEffect(() => {
    const el = scrollRef.current;
    if (el) el.scrollTop = el.scrollHeight;
  }, [history.length, live.status, live.thinking, live.currentStage, live.done, live.error]);

  return (
    <div ref={scrollRef} className="flex-1 overflow-y-auto px-4 py-4">
      {!showLive && history.length === 0 ? (
        <div className="flex h-full flex-col items-center justify-center gap-3 text-center">
          <span className="flex h-12 w-12 items-center justify-center rounded-2xl bg-brand-50 text-brand-500">
            <SparkleIcon size={22} />
          </span>
          <div>
            <p className="text-sm font-medium text-slate-700">
              Describe a UI and generate an A2UI protocol
            </p>
            <p className="mt-1 text-xs leading-5 text-slate-400">
              e.g. "Generate a weather card showing city, current temperature and
              condition"
              <br />
              or pick a preset from the left sidebar.
            </p>
          </div>
        </div>
      ) : (
        <div className="mx-auto max-w-2xl space-y-5">
          {history.map((snap) => (
            <RoundBlock
              key={snap.id}
              round={{
                id: snap.id,
                prompt: snap.prompt,
                model: snap.model,
                currentStage: null,
                reasoning: snap.reasoning,
                thinking: snap.thinking,
                startedAt: null,
                done: snap.done,
                error: snap.error,
                live: false,
                status: snap.error ? "error" : snap.done ? "done" : "idle",
              }}
            />
          ))}

          {showLive && (
            <RoundBlock
              round={{
                id: "live",
                prompt: live.prompt,
                model: live.model,
                currentStage: live.currentStage,
                reasoning: live.reasoning,
                thinking: live.thinking,
                startedAt: live.startedAt,
                done: live.done,
                error: live.error,
                live: true,
                status: live.status,
              }}
            />
          )}
        </div>
      )}
    </div>
  );
}
