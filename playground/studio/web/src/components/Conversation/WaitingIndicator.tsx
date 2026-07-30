/** Waiting indicator shown after a prompt is sent but before any model output
 * (reasoning or content) arrives.
 *
 * Non-reasoning models expose no chain-of-thought, so their time-to-first-token
 * is a silent wait. To reassure the user the request is alive (not frozen),
 * this component animates bouncing dots, ticks an elapsed-seconds timer, and
 * escalates the message the longer the wait lasts. It re-renders once a second
 * and is isolated so that tick does not re-render the whole conversation tree.
 */

import { useEffect, useState } from "react";

interface WaitingIndicatorProps {
  startedAt: number | null;
}

function messageFor(secs: number): string {
  if (secs < 5) return "Waiting for the model to respond...";
  if (secs < 20) return "The model is processing your request...";
  return "Still working — large prompts can take a while...";
}

export function WaitingIndicator({ startedAt }: WaitingIndicatorProps) {
  const [now, setNow] = useState(() => Date.now());

  useEffect(() => {
    if (startedAt == null) return;
    setNow(Date.now());
    const id = window.setInterval(() => setNow(Date.now()), 1000);
    return () => window.clearInterval(id);
  }, [startedAt]);

  const secs =
    startedAt != null ? Math.max(0, Math.floor((now - startedAt) / 1000)) : 0;

  return (
    <div className="flex items-center gap-2 text-xs text-slate-400">
      <span className="flex items-center gap-1">
        {[0, 1, 2].map((i) => (
          <span
            key={i}
            className="thinking-dot h-1.5 w-1.5 rounded-full bg-brand-500"
            style={{ animationDelay: `${i * 0.15}s` }}
          />
        ))}
      </span>
      <span>{messageFor(secs)}</span>
      <span className="tabular-nums text-slate-400">{secs}s</span>
    </div>
  );
}
