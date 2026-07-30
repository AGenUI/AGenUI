/** Thin REST client for the AGenUI Studio backend (same-origin /api). */

import type {
  A2uiPayload,
  ConfigProvider,
  PresetRecord,
  PresetSummary,
  ProtocolRecord,
  ProtocolSummary,
  ProvidersResponse,
  ServerInfo,
} from "@/types";

async function getJson<T>(url: string): Promise<T> {
  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(`GET ${url} failed: ${res.status}`);
  }
  return res.json() as Promise<T>;
}

export async function fetchServerInfo(): Promise<ServerInfo> {
  return getJson<ServerInfo>("/api/server-info");
}

export async function fetchProviders(): Promise<ProvidersResponse> {
  return getJson<ProvidersResponse>("/api/providers");
}

export async function fetchAllConfig(): Promise<{ active: string | null; providers: ConfigProvider[] }> {
  return getJson("/api/config/all");
}

export async function saveConfig(
  providers: ConfigProvider[],
  removeProviders: string[] = [],
): Promise<void> {
  const set_providers: Record<string, { base_url: string; api_key: string; model: string; max_tokens: number }> = {};
  for (const p of providers) {
    set_providers[p.name] = {
      base_url: p.base_url,
      api_key: p.api_key,
      model: p.model,
      max_tokens: p.max_tokens,
    };
  }
  const res = await fetch("/api/config", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      set_providers,
      ...(removeProviders.length > 0 ? { remove_providers: removeProviders } : {}),
    }),
  });
  if (!res.ok) {
    throw new Error(`POST /api/config failed: ${res.status}`);
  }
}

export async function fetchPresets(): Promise<PresetSummary[]> {
  const data = await getJson<{ presets: PresetSummary[] }>("/api/presets");
  return data.presets;
}

export async function fetchPreset(id: string): Promise<PresetRecord> {
  return getJson<PresetRecord>(`/api/presets/${encodeURIComponent(id)}`);
}

export async function fetchProtocols(): Promise<ProtocolSummary[]> {
  const data = await getJson<{ protocols: ProtocolSummary[] }>("/api/protocols");
  return data.protocols;
}

export async function fetchProtocol(id: string): Promise<ProtocolRecord> {
  return getJson<ProtocolRecord>(`/api/protocols/${encodeURIComponent(id)}`);
}

/** Update an existing protocol's payloads in place (Save button). */
export async function updateProtocol(
  id: string,
  components: A2uiPayload,
  datamodel: A2uiPayload | null,
): Promise<{ ok: boolean; id: string }> {
  const res = await fetch(`/api/protocols/${encodeURIComponent(id)}`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ components, datamodel }),
  });
  if (!res.ok) {
    throw new Error(`PUT protocol ${id} failed: ${res.status}`);
  }
  return res.json();
}
