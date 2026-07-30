/** Model picker: lists providers that have a configured api_key. */

import { ChevronDownIcon } from "@/components/icons";
import type { Provider } from "@/types";

interface ModelSelectorProps {
  providers: Provider[];
  active: string | null;
  value: string | null;
  disabled: boolean;
  onChange: (name: string) => void;
}

export function ModelSelector({
  providers,
  active,
  value,
  disabled,
  onChange,
}: ModelSelectorProps) {
  const selected = value ?? active ?? providers[0]?.name ?? "";

  return (
    <div className="relative inline-flex">
      <select
        value={selected}
        disabled={disabled || providers.length === 0}
        onChange={(e) => onChange(e.target.value)}
        title="Select model"
        className="max-w-[180px] cursor-pointer appearance-none truncate rounded-lg border border-slate-200 bg-white py-1.5 pl-2.5 pr-7 text-xs font-medium text-slate-700 outline-none transition hover:border-slate-300 focus:border-brand-500 disabled:cursor-not-allowed disabled:opacity-50"
      >
        {providers.length === 0 && <option value="">No model configured</option>}
        {providers.map((p) => (
          <option key={p.name} value={p.name}>
            {p.name} / {p.model}
          </option>
        ))}
      </select>
      <ChevronDownIcon
        size={13}
        className="pointer-events-none absolute right-2 top-1/2 -translate-y-1/2 text-slate-400"
      />
    </div>
  );
}
