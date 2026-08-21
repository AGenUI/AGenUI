# AGenUI Studio

> A local, **bring-your-own-key** workbench that turns natural language into renderable [A2UI](https://github.com/AGenUI/AGenUI) protocol — with streaming generation, live preview, validation, and one-tap push to your device.
> No cloud, no lock-in.

<!-- Screenshot placeholder: drop a product image here, e.g.
     ![AGenUI Studio](../../docs/images/agenui-studio.png) -->

## What is AGenUI Studio?

AGenUI Studio is a standalone local development tool in the AGenUI ecosystem. It drives an LLM to turn a natural-language description into A2UI protocol messages (`updateComponents` / `updateDataModel`) that AGenUI renders natively, and wraps the whole flow in a browser UI: streaming generation, live protocol viewing, validation, and QR-code push to a real device running AGenUI Playground.

### Built on the open-source A2UI Generation Skill

Studio does not reinvent the generation logic. It directly reuses this repo's open-source [`skills/a2ui-generation`](../../skills/a2ui-generation) Skill, shared read-only:

- **Design rules** — the Skill's `SKILL.md` constraints (component allowlists, layout & styling rules) are loaded into the LLM prompt, so the model emits A2UI v0.9–conformant protocol.
- **Executable validator** — the Skill's `scripts/validate_a2ui.py` validates every generated protocol and surfaces detailed diagnostics.

So you get the same high-quality output as mounting the Skill into an AI coding agent — but in a self-contained app with a UI, no agent runtime required.

## Features

- **BYOK multi-model** — DeepSeek, Qwen (DashScope), GLM (Zhipu), Moonshot, MiniMax, OpenAI, Gemini, Anthropic, OpenRouter, plus any OpenAI-compatible endpoint.
- **Real-time streaming** — generation streams over SSE with a live protocol preview.
- **Reasoning mode** — a toggle enables reasoning for capable models (may increase generation time).
- **Multi-turn refinement** — iterate on the generated protocol with follow-up prompts; the previous protocol is used as the baseline.
- **Protocol validation** — every result is checked against the A2UI validator with detailed errors and warnings.
- **On-device preview** — a QR code plus the LAN URL let you preview the result on a real device via [AGenUI Playground](../../README.md#debugging-with-the-playground).
- **Presets & history** — built-in example protocols and a local "My Generations" history.
- **100% local** — your API keys, config, and generated protocols never leave your machine; they live under `~/.agenui`.

## Quick Start

AGenUI Studio can be used in two ways, depending on who you are.

### For end users — no clone required

Run it directly through npm. The launcher downloads the latest release, sets up a Python environment, and opens the app in your browser:

```bash
npx agenui-studio                # first run: download + venv + deps + start
npx agenui-studio --port 9000    # use a custom port
npx agenui-studio --update       # pull the latest release, then start
```

On first run the launcher downloads `agenui-studio.zip` from GitHub Releases, extracts it to `~/.agenui/app/`, creates a virtualenv at `~/.agenui/venv/`, installs the server dependencies, starts the server, and opens your browser.

### For developers — run from source

If you have cloned this repository, run Studio straight from the source tree.

**Prerequisites:** Python 3.9+ and Node.js 18+.

1. Install the server dependencies:

   ```bash
   pip3 install -r playground/studio/server/requirements.txt
   ```

2. Build the frontend — **required on a fresh clone**:

   The web UI is a build artifact and is not checked into git, so a freshly cloned repository has no prebuilt UI yet. Build it once before starting the server:

   ```bash
   cd playground/studio/web
   npm install
   npm run build      # outputs to ../server/static, served by FastAPI
   cd ../../..        # back to the repository root
   ```

3. Start the server (from the repository root):

   ```bash
   python3 -m playground.studio.server
   ```

   The server prints both the local and LAN URLs and opens your browser:

   ```
   AGenUI Studio starting...
     Local:   http://127.0.0.1:8765
     Network: http://<your-lan-ip>:8765
   ```

   Optional flags: `--host HOST`, `--port PORT`, `--no-browser`.

4. (Optional) Work on the frontend. The server serves the built UI from `server/static`; after changing frontend source, rebuild to see the changes, or use the Vite dev server for hot reload:

   ```bash
   cd playground/studio/web
   npm run build      # rebuild into ../server/static
   npm run dev        # Vite dev server with hot reload
   ```

### Preview on a real device (requires AGenUI Playground)

Studio previews generated protocol on a real device by pushing it to the AGenUI Playground app via QR code, so you need the Playground installed and running first. A standalone Playground project is provided for each platform (Android / iOS / HarmonyOS) under the repo's `playground/` directory — build and run it on your device or a simulator, then scan the QR code shown in Studio.

👉 See [Debugging with the Playground](../../README.md#debugging-with-the-playground) for per-platform setup and installation instructions.

## Configuration

Studio reads its configuration from `~/.agenui/config.json`, which is created on first run. A reference template is available at [`config.example.json`](config.example.json). Generated protocols are saved under `~/.agenui/protocols/`.

### Providers

Each entry under `providers` is an OpenAI-compatible endpoint:

| Field        | Description                                        |
| ------------ | -------------------------------------------------- |
| `base_url`   | OpenAI-compatible API base URL                     |
| `api_key`    | Your API key (kept local, never uploaded)          |
| `model`      | Model name to call                                 |
| `max_tokens` | Maximum tokens for the generation response         |

The template ships with ready-to-fill entries for DeepSeek, Qwen, Moonshot, Zhipu (GLM), MiniMax, OpenAI, Gemini, Anthropic, and OpenRouter.

### Server

| Field         | Description | Default   |
| ------------- | ----------- | --------- |
| `server.host` | Bind host   | `0.0.0.0` |
| `server.port` | Bind port   | `8765`    |

### Managing providers from the UI

You don't have to edit JSON by hand. Open the **settings** dialog in the app to add, edit, or remove providers and to toggle API-key visibility. Changes are written back to `~/.agenui/config.json`.

## Project Structure

```
playground/studio/
├── config.example.json      # Reference configuration template
├── server/                  # FastAPI backend
│   ├── __main__.py          # Entry point: python3 -m playground.studio.server
│   ├── server.py            # HTTP API (config / providers / generate)
│   ├── config.py            # ~/.agenui/config.json management
│   ├── generator.py         # Streaming generation orchestration
│   ├── providers.py         # Multi-vendor LLM adapters
│   ├── storage.py           # Generated protocol persistence
│   ├── presets/             # Built-in example protocols
│   ├── benchmark/           # Reused generation / validation modules
│   └── requirements.txt     # Python dependencies
└── web/                     # React frontend (builds to server/static)
    └── src/
        ├── api/             # HTTP / SSE client
        ├── components/      # UI (Conversation / InputBar / Protocol / Sidebar)
        ├── hooks/           # useGeneration, useProviders, ...
        └── lib/             # Utilities
```

## FAQ

**The port is already in use.**
Start with a different port: `python3 -m playground.studio.server --port 9000` (or `npx agenui-studio --port 9000`).

**"Could not extract A2UI protocol".**
The model output could not be parsed into valid A2UI. Refine your prompt or try a different model; stronger models tend to produce more reliable protocol.

**Generation is slow after enabling Reasoning.**
That is expected — reasoning models spend extra tokens thinking before answering. Turn the toggle off for faster, simpler generations.

**My phone cannot reach the server.**
Make sure the device is on the same network and use the `Network:` (LAN IP) URL. Check your OS firewall if the connection is refused.

## Related

- [A2UI Generation Skill](../../skills/a2ui-generation) — the open-source Skill Studio is built on.
- [Debugging with the Playground](../../README.md#debugging-with-the-playground) — preview generated protocol on a real device.
- [AGenUI documentation](../../docs) — API reference and quick start.

## License

[Apache-2.0](../../LICENSE)
