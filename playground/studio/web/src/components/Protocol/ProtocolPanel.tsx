/** Right panel: tabbed protocol editors + QR code + save bar. */

import { useEffect, useState } from "react";
import { ProtocolEditor } from "./ProtocolEditor";
import { PreviewScanStrip } from "./PreviewScanStrip";
import { SaveBar, type SaveState } from "./SaveBar";
import { BoxIcon } from "@/components/icons";
import { cn } from "@/lib/utils";
import type { A2uiPayload } from "@/types";

type Tab = "components" | "datamodel";

interface ProtocolPanelProps {
  title: string;
  componentsText: string;
  datamodelText: string;
  onComponentsChange: (value: string) => void;
  onDatamodelChange: (value: string) => void;
  streaming: boolean;
  protocolId: string | null;
  /** Selected preset id (null for non-preset protocols). */
  presetId: string | null;
  /** rendering.png URL for the selected preset, or null. */
  renderingUrl: string | null;
  qrUrl: string | null;
  onSave: (components: A2uiPayload, datamodel: A2uiPayload | null) => Promise<void>;
}

export function ProtocolPanel({
  title,
  componentsText,
  datamodelText,
  onComponentsChange,
  onDatamodelChange,
  streaming,
  protocolId,
  presetId,
  renderingUrl,
  qrUrl,
  onSave,
}: ProtocolPanelProps) {
  const [tab, setTab] = useState<Tab>("components");
  const [saveState, setSaveState] = useState<SaveState>("idle");
  const [saveError, setSaveError] = useState<string | null>(null);

  // Reset save feedback whenever the loaded content changes.
  useEffect(() => {
    setSaveState("idle");
    setSaveError(null);
  }, [protocolId, title, componentsText, datamodelText]);

  const handleSave = async () => {
    let components: A2uiPayload;
    let datamodel: A2uiPayload | null = null;
    try {
      components = JSON.parse(componentsText || "{}");
      if (datamodelText.trim()) datamodel = JSON.parse(datamodelText);
    } catch (err) {
      setSaveState("error");
      setSaveError(err instanceof Error ? `Invalid JSON: ${err.message}` : "Invalid JSON");
      return;
    }
    setSaveState("saving");
    setSaveError(null);
    try {
      await onSave(components, datamodel);
      setSaveState("saved");
    } catch (err) {
      setSaveState("error");
      setSaveError(err instanceof Error ? err.message : "Save failed");
    }
  };

  const hasContent = componentsText.trim().length > 0 || datamodelText.trim().length > 0;

  return (
    <div className="flex h-full min-w-0 flex-col border-l border-slate-200 bg-slate-50/50">
      {/* Header */}
      <div className="flex shrink-0 items-center gap-2 border-b border-slate-200 bg-white px-3 py-2">
        <BoxIcon size={15} className="text-brand-500" />
        <span className="truncate text-xs font-semibold text-slate-700" title={title}>
          {title || "Protocol"}
        </span>
        {streaming && (
          <span className="ml-auto flex items-center gap-1 rounded-full bg-brand-50 px-2 py-0.5 text-[10px] font-medium text-brand-600">
            <span className="h-1.5 w-1.5 animate-pulse rounded-full bg-brand-500" />
            Streaming
          </span>
        )}
      </div>

      {/* Preview & Scan strip: rendering (presets) + QR code. */}
      <PreviewScanStrip presetId={presetId} renderingUrl={renderingUrl} qrUrl={qrUrl} />

      {hasContent ? (
        <>
          {/* Tabs */}
          <div className="flex shrink-0 items-center gap-1 border-b border-slate-200 bg-white px-2 pt-1.5">
            {(["components", "datamodel"] as Tab[]).map((t) => (
              <button
                key={t}
                type="button"
                onClick={() => setTab(t)}
                className={cn(
                  "rounded-t-md px-3 py-1.5 text-xs font-medium transition",
                  tab === t
                    ? "border border-b-0 border-slate-200 bg-slate-50 text-brand-600"
                    : "text-slate-400 hover:text-slate-600",
                )}
              >
                {t === "components" ? "updateComponents" : "updateDataModel"}
              </button>
            ))}
          </div>

          {/* Editor */}
          <div className="min-h-0 flex-1 overflow-hidden bg-white">
            <div className={cn("h-full", tab !== "components" && "hidden")}>
              <ProtocolEditor
                value={componentsText}
                onChange={onComponentsChange}
                readOnly={streaming}
              />
            </div>
            <div className={cn("h-full", tab !== "datamodel" && "hidden")}>
              <ProtocolEditor
                value={datamodelText}
                onChange={onDatamodelChange}
                readOnly={streaming}
              />
            </div>
          </div>

          {/* Footer: save */}
          <div className="shrink-0 border-t border-slate-200 bg-white p-2.5">
            <SaveBar
              canSave={protocolId != null}
              saveState={saveState}
              saveError={saveError}
              onSave={handleSave}
            />
          </div>
        </>
      ) : (
        <div className="flex flex-1 flex-col items-center justify-center gap-2 text-center">
          <BoxIcon size={28} className="text-slate-300" />
          <p className="text-xs text-slate-400">
            The generated or selected protocol will appear here
          </p>
        </div>
      )}
    </div>
  );
}
