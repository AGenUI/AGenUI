/** Validation result banner shown after generation completes. */

import { useState } from "react";
import { AlertIcon, CheckIcon, ChevronDownIcon, QrIcon, XIcon } from "@/components/icons";
import { cn } from "@/lib/utils";
import type { DoneEvent } from "@/types";

interface ValidationBannerProps {
  done: DoneEvent;
}

export function ValidationBanner({ done }: ValidationBannerProps) {
  const errors = done.validation_errors ?? [];
  const warnings = done.validation_warnings ?? [];
  const passed = done.validation_passed;
  const hasDetails = errors.length > 0 || warnings.length > 0;
  // Validation details are collapsed by default so the result card stays
  // compact; the user can expand them to inspect individual errors/warnings.
  const [detailsOpen, setDetailsOpen] = useState(false);

  const tone = !passed
    ? "border-red-200 bg-red-50 text-red-700"
    : warnings.length > 0
      ? "border-amber-200 bg-amber-50 text-amber-700"
      : "border-emerald-200 bg-emerald-50 text-emerald-700";

  const detailSummary = [
    errors.length > 0 ? `${errors.length} error(s)` : null,
    warnings.length > 0 ? `${warnings.length} warning(s)` : null,
  ]
    .filter(Boolean)
    .join(", ");

  return (
    <div className={cn("rounded-lg border px-3 py-2 text-xs", tone)}>
      <div className="flex items-center gap-1.5 font-medium">
        {!passed ? (
          <XIcon size={13} />
        ) : warnings.length > 0 ? (
          <AlertIcon size={13} />
        ) : (
          <CheckIcon size={13} />
        )}
        {!passed
          ? "A2UI validation failed — protocol saved anyway"
          : warnings.length > 0
            ? `Validation passed with ${warnings.length} warning(s)`
            : "A2UI validation passed"}
      </div>

      {/* Scan-to-verify hint, shown for both success and failure. */}
      <div className="mt-1.5 flex items-center gap-1.5 text-[11px] opacity-80">
        <QrIcon size={12} className="shrink-0" />
        <span>Scan the QR code to verify the rendering effect on your device</span>
      </div>

      {hasDetails && (
        <div className="mt-1.5">
          <button
            type="button"
            onClick={() => setDetailsOpen((o) => !o)}
            className="flex items-center gap-1 font-medium opacity-80 transition hover:opacity-100"
          >
            <ChevronDownIcon
              size={12}
              className={cn("transition-transform", !detailsOpen && "-rotate-90")}
            />
            {detailsOpen ? "Hide details" : `Show details (${detailSummary})`}
          </button>

          {detailsOpen && (
            <>
              {errors.length > 0 && (
                <ul className="mt-1.5 space-y-1 border-t border-red-200/60 pt-1.5">
                  {errors.map((err, i) => (
                    <li key={i} className="flex gap-1.5 font-mono text-[11px] leading-4">
                      <span className="shrink-0 text-red-400">✗</span>
                      <span className="break-all">{err}</span>
                    </li>
                  ))}
                </ul>
              )}

              {warnings.length > 0 && (
                <ul className="mt-1.5 space-y-1 border-t border-amber-200/60 pt-1.5">
                  {warnings.map((warn, i) => (
                    <li key={i} className="flex gap-1.5 font-mono text-[11px] leading-4">
                      <span className="shrink-0 text-amber-400">⚠</span>
                      <span className="break-all">{warn}</span>
                    </li>
                  ))}
                </ul>
              )}
            </>
          )}
        </div>
      )}
    </div>
  );
}
