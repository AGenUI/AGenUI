/** Loads available providers (models with a configured api_key) and server info. */

import { useCallback, useEffect, useState } from "react";
import { fetchProviders, fetchServerInfo } from "@/api/client";
import type { Provider, ServerInfo } from "@/types";

export function useProviders() {
  const [providers, setProviders] = useState<Provider[]>([]);
  const [active, setActive] = useState<string | null>(null);
  const [serverInfo, setServerInfo] = useState<ServerInfo | null>(null);
  const [loaded, setLoaded] = useState(false);

  const refresh = useCallback(async () => {
    try {
      const [prov, info] = await Promise.all([fetchProviders(), fetchServerInfo()]);
      setProviders(prov.providers);
      setActive(prov.active);
      setServerInfo(info);
    } catch {
      // Server unreachable; keep previous state.
    } finally {
      setLoaded(true);
    }
  }, []);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  return { providers, active, serverInfo, loaded, refresh };
}
