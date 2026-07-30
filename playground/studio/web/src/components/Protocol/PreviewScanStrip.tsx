/** Top strip combining reference rendering (presets) and QR code.
 *
 * Layout variants:
 * - Preset selected: rendering preview (flexible) left + QR card (fixed) right.
 * - Custom protocol: QR card centered, no rendering area.
 *
 * Clicking the preview opens a lightbox overlay (click outside or X to close).
 */

import { useEffect, useState } from "react";
import { ImageOffIcon, XIcon } from "@/components/icons";
import { QrCodeCard } from "./QrCodeCard";

interface PreviewScanStripProps {
  /** Present only when a preset is selected; null = custom protocol. */
  presetId: string | null;
  /** URL of rendering.png; null means no image available. */
  renderingUrl: string | null;
  /** Protocol raw URL encoded in the QR code. */
  qrUrl: string | null;
}

export function PreviewScanStrip({ presetId, renderingUrl, qrUrl }: PreviewScanStripProps) {
  const [lightbox, setLightbox] = useState(false);

  // Close lightbox on Escape.
  useEffect(() => {
    if (!lightbox) return;
    const onKey = (e: KeyboardEvent) => e.key === "Escape" && setLightbox(false);
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [lightbox]);

  if (!qrUrl && !presetId) return null;

  return (
    <div className="shrink-0 border-b border-slate-200 bg-white p-2.5">
      {presetId ? (
        /* Preset: rendering left + QR right */
        <div className="flex items-stretch gap-2.5">
          <div className="relative min-w-0 flex-1 overflow-hidden rounded-lg border border-slate-200 bg-slate-50/70">
            {renderingUrl ? (
              <button
                type="button"
                onClick={() => setLightbox(true)}
                title="Click to enlarge"
                className="absolute inset-0 cursor-zoom-in"
              >
                <img
                  src={renderingUrl}
                  alt="Reference rendering"
                  className="h-full w-full object-contain"
                />
              </button>
            ) : (
              <div className="flex h-full min-h-[80px] flex-col items-center justify-center gap-1.5 text-slate-300">
                <ImageOffIcon size={16} />
                <span className="text-[10px]">No preview</span>
              </div>
            )}
          </div>
          {qrUrl && <QrCodeCard url={qrUrl} />}
        </div>
      ) : (
        /* Custom protocol: centered QR */
        <div className="flex justify-center">
          {qrUrl && <QrCodeCard url={qrUrl} />}
        </div>
      )}

      {/* Lightbox overlay */}
      {lightbox && renderingUrl && (
        <div
          className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-8 backdrop-blur-sm"
          onClick={() => setLightbox(false)}
        >
          <div
            className="relative max-h-full max-w-full"
            onClick={(e) => e.stopPropagation()}
          >
            <button
              type="button"
              onClick={() => setLightbox(false)}
              className="absolute -right-3 -top-3 flex h-7 w-7 items-center justify-center rounded-full bg-white text-slate-500 shadow-lg transition hover:bg-slate-100 hover:text-slate-700"
            >
              <XIcon size={14} />
            </button>
            <img
              src={renderingUrl}
              alt="Reference rendering (full size)"
              className="max-h-[80vh] max-w-full rounded-xl shadow-2xl"
            />
          </div>
        </div>
      )}
    </div>
  );
}
