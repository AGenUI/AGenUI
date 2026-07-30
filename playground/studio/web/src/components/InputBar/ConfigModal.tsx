/** Modal for viewing/editing all provider configurations in config.json. */

import { useCallback, useEffect, useState } from "react";
import { fetchAllConfig, saveConfig } from "@/api/client";
import { EyeIcon, EyeOffIcon, PlusIcon, TrashIcon, XIcon } from "@/components/icons";
import type { ConfigProvider } from "@/types";

interface ConfigModalProps {
  open: boolean;
  onClose: () => void;
  onSaved: () => void;
}

export function ConfigModal({ open, onClose, onSaved }: ConfigModalProps) {
  const [providers, setProviders] = useState<ConfigProvider[]>([]);
  const [originalNames, setOriginalNames] = useState<string[]>([]);
  const [newNames, setNewNames] = useState<Set<string>>(new Set());
  const [showKeys, setShowKeys] = useState<Set<string>>(new Set());
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  // Load config when modal opens.
  useEffect(() => {
    if (!open) return;
    setLoading(true);
    setError(null);
    setNewNames(new Set());
    setShowKeys(new Set());
    fetchAllConfig()
      .then((data) => {
        setProviders(data.providers);
        setOriginalNames(data.providers.map((p) => p.name));
      })
      .catch((e) => setError(e.message))
      .finally(() => setLoading(false));
  }, [open]);

  const updateField = useCallback(
    (name: string, field: keyof ConfigProvider, value: string | number) => {
      setProviders((prev) =>
        prev.map((p) => (p.name === name ? { ...p, [field]: value } : p)),
      );
    },
    [],
  );

  const toggleKey = (name: string) => {
    setShowKeys((prev) => {
      const next = new Set(prev);
      if (next.has(name)) next.delete(name);
      else next.add(name);
      return next;
    });
  };

  const addProvider = () => {
    const name = `custom_${Date.now().toString(36)}`;
    setProviders((prev) => [
      ...prev,
      { name, base_url: "", api_key: "", model: "", max_tokens: 8192 },
    ]);
    setNewNames((prev) => new Set(prev).add(name));
    setShowKeys((prev) => new Set(prev).add(name));
  };

  const removeProvider = (name: string) => {
    setProviders((prev) => prev.filter((p) => p.name !== name));
    setNewNames((prev) => {
      const next = new Set(prev);
      next.delete(name);
      return next;
    });
  };

  const handleSave = async () => {
    setSaving(true);
    setError(null);
    try {
      // Providers present in the loaded config but no longer in the list were
      // deleted by the user; tell the backend to remove them from config.json.
      const currentNames = new Set(providers.map((p) => p.name));
      const removed = originalNames.filter((n) => !currentNames.has(n));
      await saveConfig(providers, removed);
      onSaved();
      onClose();
    } catch (e) {
      setError(e instanceof Error ? e.message : "Save failed");
    } finally {
      setSaving(false);
    }
  };

  if (!open) return null;

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4 backdrop-blur-sm"
      onClick={onClose}
    >
      <div
        className="flex max-h-[85vh] w-full max-w-2xl flex-col rounded-2xl bg-white shadow-2xl"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex shrink-0 items-center justify-between border-b border-slate-200 px-5 py-3.5">
          <h2 className="text-sm font-semibold text-slate-800">Model Configuration</h2>
          <button
            type="button"
            onClick={onClose}
            className="flex h-7 w-7 items-center justify-center rounded-lg text-slate-400 transition hover:bg-slate-100 hover:text-slate-600"
          >
            <XIcon size={15} />
          </button>
        </div>

        {/* Body */}
        <div className="min-h-0 flex-1 space-y-3 overflow-y-auto px-5 py-4">
          {loading && (
            <p className="py-8 text-center text-xs text-slate-400">Loading...</p>
          )}

          {!loading &&
            providers.map((p) => (
              <div
                key={p.name}
                className="rounded-xl border border-slate-200 bg-slate-50/50 p-3.5"
              >
                {/* Provider name + delete */}
                <div className="mb-2.5 flex items-center justify-between">
                  {newNames.has(p.name) ? (
                    <input
                      value={p.name}
                      onChange={(e) => updateField(p.name, "name", e.target.value)}
                      placeholder="provider name"
                      className="w-40 rounded-md border border-slate-300 bg-white px-2 py-1 text-xs font-semibold text-slate-700 outline-none focus:border-brand-500"
                    />
                  ) : (
                    <span className="text-xs font-semibold uppercase tracking-wide text-slate-500">
                      {p.name}
                    </span>
                  )}
                  <button
                    type="button"
                    onClick={() => removeProvider(p.name)}
                    title="Remove provider"
                    className="flex h-6 w-6 items-center justify-center rounded-md text-slate-300 transition hover:bg-red-50 hover:text-red-500"
                  >
                    <TrashIcon size={13} />
                  </button>
                </div>

                {/* Fields */}
                <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
                  <label className="block">
                    <span className="mb-0.5 block text-[10px] font-medium uppercase tracking-wide text-slate-400">
                      Base URL
                    </span>
                    <input
                      value={p.base_url}
                      onChange={(e) => updateField(p.name, "base_url", e.target.value)}
                      placeholder="https://api.example.com/v1"
                      className="w-full rounded-md border border-slate-200 bg-white px-2.5 py-1.5 text-xs text-slate-700 outline-none transition focus:border-brand-500 focus:ring-1 focus:ring-brand-500/20"
                    />
                  </label>

                  <label className="block">
                    <span className="mb-0.5 block text-[10px] font-medium uppercase tracking-wide text-slate-400">
                      Model
                    </span>
                    <input
                      value={p.model}
                      onChange={(e) => updateField(p.name, "model", e.target.value)}
                      placeholder="model-name"
                      className="w-full rounded-md border border-slate-200 bg-white px-2.5 py-1.5 text-xs text-slate-700 outline-none transition focus:border-brand-500 focus:ring-1 focus:ring-brand-500/20"
                    />
                  </label>

                  <label className="block sm:col-span-2">
                    <span className="mb-0.5 block text-[10px] font-medium uppercase tracking-wide text-slate-400">
                      API Key
                    </span>
                    <div className="relative">
                      <input
                        type={showKeys.has(p.name) ? "text" : "password"}
                        value={p.api_key}
                        onChange={(e) => updateField(p.name, "api_key", e.target.value)}
                        placeholder="sk-..."
                        className="w-full rounded-md border border-slate-200 bg-white px-2.5 py-1.5 pr-9 text-xs text-slate-700 outline-none transition focus:border-brand-500 focus:ring-1 focus:ring-brand-500/20"
                      />
                      <button
                        type="button"
                        onClick={() => toggleKey(p.name)}
                        className="absolute right-2 top-1/2 -translate-y-1/2 text-slate-400 transition hover:text-slate-600"
                      >
                        {showKeys.has(p.name) ? <EyeOffIcon size={14} /> : <EyeIcon size={14} />}
                      </button>
                    </div>
                  </label>
                </div>
              </div>
            ))}

          {/* Add provider */}
          {!loading && (
            <button
              type="button"
              onClick={addProvider}
              className="flex w-full items-center justify-center gap-1.5 rounded-xl border border-dashed border-slate-300 py-2.5 text-xs font-medium text-slate-500 transition hover:border-brand-400 hover:text-brand-600"
            >
              <PlusIcon size={13} />
              Add Provider
            </button>
          )}

          {error && (
            <p className="rounded-lg bg-red-50 px-3 py-2 text-xs text-red-600">{error}</p>
          )}
        </div>

        {/* Footer */}
        <div className="flex shrink-0 items-center justify-end gap-2.5 border-t border-slate-200 px-5 py-3.5">
          <button
            type="button"
            onClick={onClose}
            className="rounded-lg border border-slate-200 px-4 py-1.5 text-xs font-medium text-slate-600 transition hover:bg-slate-50"
          >
            Cancel
          </button>
          <button
            type="button"
            onClick={handleSave}
            disabled={saving || loading}
            className="rounded-lg bg-brand-500 px-4 py-1.5 text-xs font-medium text-white shadow-sm transition hover:bg-brand-600 disabled:opacity-50"
          >
            {saving ? "Saving..." : "Save"}
          </button>
        </div>
      </div>
    </div>
  );
}
