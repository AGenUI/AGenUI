/** Reasoning-mode switch: a compact toggle + an English caption beneath it.
 *
 * Off (default) makes reasoning models (GLM/DeepSeek/Qwen) skip the slow
 * chain-of-thought for a faster first token; on brings back the live
 * "thinking" stream. The backend maps this intent to the correct
 * vendor-specific parameter (see providers._reasoning_family).
 */

interface ReasoningToggleProps {
  enabled: boolean;
  disabled: boolean;
  onChange: (enabled: boolean) => void;
}

export function ReasoningToggle({ enabled, disabled, onChange }: ReasoningToggleProps) {
  return (
    <div className="flex flex-col gap-1">
      <button
        type="button"
        role="switch"
        aria-checked={enabled}
        aria-label="Reasoning mode"
        disabled={disabled}
        onClick={() => onChange(!enabled)}
        className="group inline-flex cursor-pointer items-center gap-1.5 disabled:cursor-not-allowed disabled:opacity-50"
      >
        {/* Switch track + sliding knob. */}
        <span
          className={
            "relative inline-flex h-[18px] w-[32px] shrink-0 items-center rounded-full transition-colors " +
            (enabled ? "bg-brand-500" : "bg-slate-300 group-hover:bg-slate-400")
          }
        >
          <span
            className={
              "absolute left-[2px] h-[14px] w-[14px] rounded-full bg-white shadow-sm transition-transform " +
              (enabled ? "translate-x-[14px]" : "translate-x-0")
            }
          />
        </span>
        <span
          className={
            "text-xs font-medium transition-colors " +
            (enabled ? "text-brand-600" : "text-slate-600")
          }
        >
          Reasoning
        </span>
      </button>
      {/* English description beneath the switch. */}
      <span className="max-w-[220px] text-[10px] leading-tight text-slate-400">
        {enabled
          ? "On · model thinks step-by-step before answering (slower)"
          : "Off · skip thinking for a faster response"}
      </span>
    </div>
  );
}
