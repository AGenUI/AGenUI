/** Save bar: persists manual edits to an existing custom protocol (PUT). */

import { CheckIcon, SaveIcon } from "@/components/icons";
import { cn } from "@/lib/utils";

export type SaveState = "idle" | "saving" | "saved" | "error";

interface SaveBarProps {
  /** Null when the current content is a preset (not savable via PUT). */
  canSave: boolean;
  saveState: SaveState;
  saveError: string | null;
  onSave: () => void;
}

export function SaveBar({ canSave, saveState, saveError, onSave }: SaveBarProps) {
  return (
    <div className="flex items-center gap-2">
      <button
        type="button"
        onClick={onSave}
        disabled={!canSave || saveState === "saving"}
        title={
          canSave
            ? "Save edits to this protocol"
            : "Only generated (custom) protocols can be saved here"
        }
        className={cn(
          "inline-flex h-8 items-center gap-1.5 rounded-lg px-3 text-xs font-medium transition active:scale-95",
          canSave && saveState !== "saving"
            ? "bg-brand-500 text-white hover:bg-brand-600"
            : "cursor-not-allowed bg-slate-200 text-slate-400",
        )}
      >
        {saveState === "saving" ? (
          <>
            <span className="h-3 w-3 animate-spin rounded-full border-2 border-white/40 border-t-white" />
            Saving...
          </>
        ) : saveState === "saved" ? (
          <>
            <CheckIcon size={13} />
            Saved
          </>
        ) : (
          <>
            <SaveIcon size={13} />
            Save
          </>
        )}
      </button>

      {saveState === "error" && saveError && (
        <span className="text-[11px] text-red-500">{saveError}</span>
      )}
    </div>
  );
}
