/** Small ticking display of the seconds elapsed since a round started.
 *
 * Isolated in its own component so the 1s re-render tick does not re-render
 * the whole conversation tree.
 */

import { useEffect, useState } from "react";

interface ElapsedTimerProps {
  startedAt: number | null;
  className?: string;
}

export function ElapsedTimer({ startedAt, className }: ElapsedTimerProps) {
  const [now, setNow] = useState(() => Date.now());

  useEffect(() => {
    if (startedAt == null) return;
    setNow(Date.now());
    const id = window.setInterval(() => setNow(Date.now()), 1000);
    return () => window.clearInterval(id);
  }, [startedAt]);

  if (startedAt == null) return null;
  const secs = Math.max(0, Math.floor((now - startedAt) / 1000));
  return <span className={className}>{secs}s</span>;
}
