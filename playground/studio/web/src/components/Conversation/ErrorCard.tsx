/** Error card for provider / stream / extraction failures. */

import { AlertIcon } from "@/components/icons";
import type { ErrorEvent } from "@/types";

const CODE_LABELS: Record<string, string> = {
  auth: "Authentication failed",
  rate_limit: "Rate limited",
  balance: "Insufficient balance",
  timeout: "Request timed out",
  network: "Network error",
  server_error: "Provider server error",
  extraction_failed: "Could not extract A2UI protocol",
  stream_error: "Stream interrupted",
  unknown: "Unknown error",
};

interface ErrorCardProps {
  error: ErrorEvent;
}

export function ErrorCard({ error }: ErrorCardProps) {
  const label = CODE_LABELS[error.code] ?? error.code;

  return (
    <div className="rounded-lg border border-red-200 bg-red-50 px-3 py-2.5 text-xs text-red-700">
      <div className="flex items-center gap-1.5 font-medium">
        <AlertIcon size={13} className="shrink-0" />
        {label}
        {error.status_code && (
          <span className="rounded bg-red-100 px-1.5 py-0.5 font-mono text-[10px] text-red-500">
            HTTP {error.status_code}
          </span>
        )}
      </div>

      <p className="mt-1.5 leading-5">{error.message}</p>

      {error.detail && error.detail !== error.message && (
        <p className="mt-1.5 rounded border border-red-200/70 bg-white/60 px-2 py-1.5 font-mono text-[11px] leading-4 text-red-600">
          {error.detail}
        </p>
      )}

      {error.raw_response && (
        <details className="mt-1.5">
          <summary className="cursor-pointer select-none text-[11px] text-red-500 hover:text-red-700">
            Raw model response
          </summary>
          <pre className="mt-1 max-h-40 overflow-auto whitespace-pre-wrap rounded bg-white/60 p-2 font-mono text-[10px] leading-4 text-red-600">
            {error.raw_response}
          </pre>
        </details>
      )}
    </div>
  );
}
