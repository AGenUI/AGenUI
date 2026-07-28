/** Collapsible area streaming the model's reasoning text.
 *
 * `thinking` is the combined display text: a reasoning model's
 * chain-of-thought (streamed via `reasoning_content`) plus any non-JSON
 * preamble the model emits in its final answer.
 */

import { useEffect, useRef, useState } from "react";
import { ElapsedTimer } from "./ElapsedTimer";
import { ChevronDownIcon, SparkleIcon } from "@/components/icons";
import { cn } from "@/lib/utils";

interface ThinkingProcessProps {
  thinking: string;
  streaming: boolean;
  defaultOpen?: boolean;
  startedAt?: number | null;
}

export function ThinkingProcess({
  thinking,
  streaming,
  defaultOpen = true,
  startedAt = null,
}: ThinkingProcessProps) {
  const [open, setOpen] = useState(defaultOpen);
  const bodyRef = useRef<HTMLDivElement>(null);

  // Keep the visible tail in view while streaming.
  useEffect(() => {
    if (open && streaming && bodyRef.current) {
      bodyRef.current.scrollTop = bodyRef.current.scrollHeight;
    }
  }, [thinking, open, streaming]);

  if (!thinking) return null;

  return (
    <div className="rounded-lg border border-slate-200 bg-slate-50/70">
      <button
        type="button"
        onClick={() => setOpen((o) => !o)}
        className="flex w-full items-center gap-1.5 px-2.5 py-1.5 text-left text-[11px] font-medium text-slate-500 hover:text-slate-700"
      >
        <SparkleIcon size={12} className={cn(streaming && "animate-pulse text-brand-500")} />
        {streaming ? "Thinking..." : "Thinking process"}
        <span className="ml-auto flex items-center gap-1 font-normal text-slate-400">
          {streaming && (
            <ElapsedTimer startedAt={startedAt} className="tabular-nums text-brand-500/80" />
          )}
          {thinking.length} chars
          <ChevronDownIcon
            size={12}
            className={cn("transition-transform", open && "rotate-180")}
          />
        </span>
      </button>
      {open && (
        <div
          ref={bodyRef}
          className="max-h-40 overflow-y-auto whitespace-pre-wrap border-t border-slate-200/70 px-3 py-2 text-xs leading-5 text-slate-500"
        >
          {thinking}
          {streaming && <span className="ml-0.5 animate-pulse">▍</span>}
        </div>
      )}
    </div>
  );
}
