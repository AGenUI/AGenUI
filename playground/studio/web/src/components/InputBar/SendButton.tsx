/** Send button: idle -> "Send" (primary), generating -> "Stop" (danger). */

import { SendIcon, StopIcon } from "@/components/icons";
import { cn } from "@/lib/utils";

interface SendButtonProps {
  isGenerating: boolean;
  canSend: boolean;
  onSend: () => void;
  onStop: () => void;
}

export function SendButton({ isGenerating, canSend, onSend, onStop }: SendButtonProps) {
  if (isGenerating) {
    return (
      <button
        type="button"
        onClick={onStop}
        title="Stop generation"
        className="flex h-9 items-center gap-1.5 rounded-lg bg-red-500 px-3.5 text-sm font-medium text-white shadow-sm transition hover:bg-red-600 active:scale-95"
      >
        <StopIcon size={14} />
        Stop
      </button>
    );
  }

  return (
    <button
      type="button"
      onClick={onSend}
      disabled={!canSend}
      title={canSend ? "Send (Enter)" : "Type a prompt first"}
      className={cn(
        "flex h-9 items-center gap-1.5 rounded-lg px-3.5 text-sm font-medium shadow-sm transition active:scale-95",
        canSend
          ? "bg-brand-500 text-white hover:bg-brand-600"
          : "cursor-not-allowed bg-slate-200 text-slate-400",
      )}
    >
      <SendIcon size={14} />
      Send
    </button>
  );
}
