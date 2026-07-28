/** Bottom input bar: model selector (left), auto-growing textarea, send/stop (right). */

import { useEffect, useRef, useState } from "react";
import { ConfigModal } from "./ConfigModal";
import { ModelSelector } from "./ModelSelector";
import { SendButton } from "./SendButton";
import { SettingsIcon, XIcon } from "@/components/icons";
import { cn } from "@/lib/utils";
import type { Provider } from "@/types";

interface InputBarProps {
  providers: Provider[];
  active: string | null;
  /** Whether the provider list has finished its initial load (avoids flashing
   * the setup tip before we know if any api_key is configured). */
  providersLoaded: boolean;
  isGenerating: boolean;
  onSend: (prompt: string, provider: string | null, reasoning: boolean) => void;
  onStop: () => void;
  onConfigSaved: () => void;
}

export function InputBar({
  providers,
  active,
  providersLoaded,
  isGenerating,
  onSend,
  onStop,
  onConfigSaved,
}: InputBarProps) {
  const [text, setText] = useState("");
  const [provider, setProvider] = useState<string | null>(null);
  const [reasoning, setReasoning] = useState(false);
  const [configOpen, setConfigOpen] = useState(false);
  const [setupTipDismissed, setSetupTipDismissed] = useState(false);
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  // Nudge the user to configure a model when no provider has an api_key yet.
  const showSetupTip = providersLoaded && providers.length === 0 && !setupTipDismissed;

  // Auto-dismiss the setup tip after 10 seconds.
  useEffect(() => {
    if (!showSetupTip) return;
    const timer = window.setTimeout(() => setSetupTipDismissed(true), 10_000);
    return () => window.clearTimeout(timer);
  }, [showSetupTip]);

  const canSend = text.trim().length > 0 && !isGenerating;

  const autoResize = () => {
    const el = textareaRef.current;
    if (!el) return;
    el.style.height = "auto";
    el.style.height = `${Math.min(el.scrollHeight, 160)}px`;
  };

  const handleSend = () => {
    const prompt = text.trim();
    if (!prompt || isGenerating) return;
    onSend(prompt, provider, reasoning);
    setText("");
    requestAnimationFrame(autoResize);
  };

  return (
    <div className="shrink-0 border-t border-slate-200 bg-white px-4 py-3">
      <div className="mx-auto max-w-3xl space-y-2">
        {/* Input row: full-width auto-growing textarea. */}
        <div className="rounded-xl border border-slate-200 bg-slate-50 px-3 py-2 transition focus-within:border-brand-500 focus-within:bg-white focus-within:ring-2 focus-within:ring-brand-500/15">
          <textarea
            ref={textareaRef}
            rows={1}
            value={text}
            disabled={isGenerating}
            placeholder={
              isGenerating
                ? "Generating, please wait..."
                : "Describe the UI you want, e.g. generate a weather card showing city, temperature and condition"
            }
            onChange={(e) => {
              setText(e.target.value);
              autoResize();
            }}
            onKeyDown={(e) => {
              if (e.key === "Enter" && !e.shiftKey && !e.nativeEvent.isComposing) {
                e.preventDefault();
                handleSend();
              }
            }}
            className="block w-full resize-none bg-transparent text-sm leading-5 text-slate-800 outline-none placeholder:text-slate-400 disabled:opacity-60"
          />
        </div>

        {/* Bottom row: model selector + settings + reasoning (left), hint + send/stop (right). */}
        <div className="flex items-start justify-between gap-3">
          <div className="flex items-start gap-2">
            <ModelSelector
              providers={providers}
              active={active}
              value={provider}
              disabled={isGenerating}
              onChange={setProvider}
            />
            {/* Settings + reasoning form a tight control cluster. */}
            <div className="flex items-start gap-1.5">
              <div className="relative">
                <button
                  type="button"
                  onClick={() => {
                    setConfigOpen(true);
                    setSetupTipDismissed(true);
                  }}
                  title="Configure model API keys"
                  className="flex h-[30px] w-[30px] items-center justify-center rounded-lg border border-slate-200 text-slate-400 transition hover:border-slate-300 hover:text-slate-600"
                >
                  <SettingsIcon size={14} />
                </button>

                {showSetupTip && (
                  <div className="absolute bottom-full left-0 z-20 mb-2.5 w-60 rounded-lg bg-slate-900 px-3 py-2.5 shadow-xl">
                    {/* Arrow pointing down at the settings button. */}
                    <span className="absolute -bottom-1 left-3 h-2 w-2 rotate-45 bg-slate-900" />
                    <div className="flex items-start gap-2">
                      <p className="flex-1 text-xs leading-relaxed text-slate-100">
                        No model configured yet — click here to add your API key.
                      </p>
                      <button
                        type="button"
                        onClick={() => setSetupTipDismissed(true)}
                        aria-label="Dismiss"
                        className="-mr-1 -mt-0.5 rounded p-0.5 text-slate-400 transition hover:bg-white/10 hover:text-white"
                      >
                        <XIcon size={12} />
                      </button>
                    </div>
                  </div>
                )}
              </div>

              {/* Reasoning switch with hover tooltip. */}
              <div className="group relative flex h-[30px] items-center gap-1.5">
                <button
                  type="button"
                  role="switch"
                  aria-checked={reasoning}
                  onClick={() => setReasoning((r) => !r)}
                  disabled={isGenerating}
                  className={cn(
                    "relative inline-flex h-[18px] w-[32px] shrink-0 items-center rounded-full transition-colors duration-200 disabled:cursor-not-allowed disabled:opacity-60",
                    reasoning ? "bg-brand-500" : "bg-slate-300",
                  )}
                >
                  <span
                    className={cn(
                      "inline-block h-[14px] w-[14px] transform rounded-full bg-white shadow transition-transform duration-200",
                      reasoning ? "translate-x-[16px]" : "translate-x-[2px]",
                    )}
                  />
                </button>
                <span
                  className={cn(
                    "text-xs font-medium transition-colors",
                    reasoning ? "text-brand-600" : "text-slate-400",
                  )}
                >
                  Reasoning
                </span>

                {/* Hover tooltip: warn that reasoning slows generation down. */}
                <div className="pointer-events-none absolute bottom-full left-1/2 z-20 mb-2 w-max max-w-[230px] -translate-x-1/2 rounded-lg bg-slate-900 px-2.5 py-1.5 text-center text-[11px] leading-snug text-slate-100 opacity-0 shadow-xl transition-opacity duration-150 group-hover:opacity-100">
                  <span className="absolute -bottom-1 left-1/2 h-2 w-2 -translate-x-1/2 rotate-45 bg-slate-900" />
                  Enabling model reasoning may increase generation time
                </div>
              </div>
            </div>
          </div>
          <div className="flex items-center gap-2 pt-1">
            <span className="hidden text-[11px] text-slate-400 md:block">
              Enter to send, Shift+Enter for a new line
            </span>
            <SendButton
              isGenerating={isGenerating}
              canSend={canSend}
              onSend={handleSend}
              onStop={onStop}
            />
          </div>
        </div>
      </div>

      {/* Config modal */}
      <ConfigModal
        open={configOpen}
        onClose={() => setConfigOpen(false)}
        onSaved={onConfigSaved}
      />
    </div>
  );
}
