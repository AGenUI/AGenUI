/** Horizontal stage indicator for the generation pipeline. */

import { CheckIcon } from "@/components/icons";
import { cn } from "@/lib/utils";
import type { StageName } from "@/types";

const STAGES: Array<{ key: StageName; label: string }> = [
  { key: "building_prompt", label: "Preparing" },
  { key: "calling_model", label: "Thinking" },
  { key: "extracting", label: "Extracting" },
  { key: "validating", label: "Validating" },
  { key: "saving", label: "Saving" },
];

interface StageStepperProps {
  currentStage: StageName | null;
  model?: string | null;
  finished: boolean;
}

export function StageStepper({ currentStage, model, finished }: StageStepperProps) {
  const currentIdx = currentStage
    ? STAGES.findIndex((s) => s.key === currentStage)
    : -1;

  return (
    <div className="flex flex-wrap items-center gap-y-1.5">
      {STAGES.map((stage, idx) => {
        const isDone = finished || (currentIdx > -1 && idx < currentIdx);
        const isCurrent = !finished && idx === currentIdx;
        return (
          <div key={stage.key} className="flex items-center">
            {idx > 0 && (
              <div
                className={cn(
                  "mx-1.5 h-px w-4",
                  isDone || isCurrent ? "bg-brand-500/50" : "bg-slate-200",
                )}
              />
            )}
            <div
              className={cn(
                "flex items-center gap-1.5 rounded-full px-2 py-0.5 text-[11px] font-medium",
                isCurrent && "bg-brand-50 text-brand-600",
                isDone && "text-emerald-600",
                !isCurrent && !isDone && "text-slate-400",
              )}
            >
              {isDone ? (
                <CheckIcon size={11} />
              ) : (
                <span
                  className={cn(
                    "h-1.5 w-1.5 rounded-full",
                    isCurrent ? "animate-pulse bg-brand-500" : "bg-slate-300",
                  )}
                />
              )}
              {stage.label}
              {isCurrent && stage.key === "calling_model" && model && (
                <span className="font-normal text-brand-500/80">({model})</span>
              )}
            </div>
          </div>
        );
      })}
    </div>
  );
}
