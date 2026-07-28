/** Loads the preset samples gallery and the user's generated protocols. */

import { useCallback, useEffect, useState } from "react";
import { fetchPresets, fetchProtocols } from "@/api/client";
import type { PresetSummary, ProtocolSummary } from "@/types";

export function useLibrary() {
  const [presets, setPresets] = useState<PresetSummary[]>([]);
  const [protocols, setProtocols] = useState<ProtocolSummary[]>([]);
  const [loading, setLoading] = useState(true);

  const refresh = useCallback(async () => {
    setLoading(true);
    try {
      const [presetList, protocolList] = await Promise.all([
        fetchPresets(),
        fetchProtocols(),
      ]);
      setPresets(presetList);
      setProtocols(protocolList);
    } catch {
      // Server unreachable; keep previous state.
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  return { presets, protocols, loading, refresh };
}
