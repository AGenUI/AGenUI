#!/usr/bin/env node
"use strict";

/**
 * agenui-studio launcher.
 *
 * One-command local install & start:
 *   npx agenui-studio              # first run: download + venv + deps + start
 *   npx agenui-studio              # later: start directly
 *   npx agenui-studio --update     # download latest release then start
 *   npx agenui-studio --port 9000  # custom port
 *
 * Layout under ~/.agenui:
 *   app/    extracted AGenUI Studio (from GitHub Release zip)
 *   venv/   Python virtual environment with the server dependencies
 *   config.json / protocols/   created by the server on first run
 */

const { spawn, spawnSync } = require("child_process");
const fs = require("fs");
const https = require("https");
const os = require("os");
const path = require("path");

const GITHUB_OWNER = "AGenUI";
const GITHUB_REPO = "AGenUI";
const ASSET_NAME = "agenui-studio.zip";
const API_URL = `https://api.github.com/repos/${GITHUB_OWNER}/${GITHUB_REPO}/releases/latest`;

const BASE_DIR = path.join(os.homedir(), ".agenui");
const APP_DIR = path.join(BASE_DIR, "app");
const VENV_DIR = path.join(BASE_DIR, "venv");
const VERSION_FILE = path.join(APP_DIR, ".version");
const REQUIREMENTS = path.join(APP_DIR, "playground", "studio", "server", "requirements.txt");

const IS_WIN = process.platform === "win32";

// --- argument parsing ------------------------------------------------------
const argv = process.argv.slice(2);
const WANT_UPDATE = argv.includes("--update");
let PORT = null;
const portIdx = argv.indexOf("--port");
if (portIdx !== -1 && argv[portIdx + 1]) {
  PORT = argv[portIdx + 1];
}

// --- helpers ---------------------------------------------------------------
function log(msg) {
  console.log(`[agenui-studio] ${msg}`);
}

function fail(msg) {
  console.error(`[agenui-studio] ERROR: ${msg}`);
  process.exit(1);
}

function runOrDie(cmd, args, opts) {
  const res = spawnSync(cmd, args, Object.assign({ stdio: "inherit" }, opts || {}));
  if (res.error) {
    fail(`Failed to run '${cmd}': ${res.error.message}`);
  }
  if (res.status !== 0) {
    fail(`Command exited with code ${res.status}: ${cmd} ${args.join(" ")}`);
  }
}

function venvBin(name) {
  const dir = IS_WIN ? "Scripts" : "bin";
  const exe = IS_WIN ? `${name}.exe` : name;
  return path.join(VENV_DIR, dir, exe);
}

// --- HTTP download with redirect support -----------------------------------
function download(url, dest) {
  return new Promise((resolve, reject) => {
    const file = fs.createWriteStream(dest);
    const request = (u) => {
      https
        .get(u, { headers: { "User-Agent": "agenui-studio-installer" } }, (res) => {
          if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
            request(res.headers.location);
            return;
          }
          if (res.statusCode !== 200) {
            reject(new Error(`Download failed: HTTP ${res.statusCode}`));
            return;
          }
          res.pipe(file);
          file.on("finish", () => {
            file.close();
            resolve();
          });
        })
        .on("error", (err) => {
          fs.unlinkSync(dest);
          reject(err);
        });
    };
    request(url);
  });
}

function fetchJson(url) {
  return new Promise((resolve, reject) => {
    https
      .get(url, { headers: { "User-Agent": "agenui-studio-installer" } }, (res) => {
        if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
          fetchJson(res.headers.location).then(resolve, reject);
          return;
        }
        if (res.statusCode !== 200) {
          reject(new Error(`API request failed: HTTP ${res.statusCode}`));
          return;
        }
        let data = "";
        res.on("data", (chunk) => (data += chunk));
        res.on("end", () => {
          try {
            resolve(JSON.parse(data));
          } catch (e) {
            reject(new Error(`Invalid JSON response: ${e.message}`));
          }
        });
      })
      .on("error", reject);
  });
}

// --- steps -----------------------------------------------------------------
function checkPython() {
  const res = spawnSync("python3", ["--version"], { encoding: "utf8" });
  if (res.status !== 0) {
    fail(
      "python3 not found. Install Python 3.10+ from https://www.python.org/downloads/ and ensure it is on PATH."
    );
  }
  const out = (res.stdout || "") + (res.stderr || "");
  const m = /Python (\d+)\.(\d+)/.exec(out);
  if (!m) {
    fail(`Could not determine Python version from: ${out.trim()}`);
  }
  const major = parseInt(m[1], 10);
  const minor = parseInt(m[2], 10);
  if (major < 3 || (major === 3 && minor < 10)) {
    fail(`Python 3.10+ is required, found ${major}.${minor}. Please upgrade.`);
  }
  log(`Python ${major}.${minor} detected`);
}

async function ensureApp() {
  fs.mkdirSync(BASE_DIR, { recursive: true });

  // Check if already installed and no update requested
  if (fs.existsSync(VERSION_FILE) && !WANT_UPDATE) {
    const ver = fs.readFileSync(VERSION_FILE, "utf8").trim();
    log(`Using installed version ${ver} (run with --update to refresh)`);
    return;
  }

  // Fetch latest release info
  log("Fetching latest release info...");
  let release;
  try {
    release = await fetchJson(API_URL);
  } catch (e) {
    if (fs.existsSync(VERSION_FILE)) {
      log(`Warning: could not check for updates (${e.message}), using cached version`);
      return;
    }
    fail(`Cannot fetch release info: ${e.message}`);
  }

  const tag = release.tag_name;
  const asset = (release.assets || []).find((a) => a.name === ASSET_NAME);
  if (!asset) {
    fail(`Release ${tag} does not contain ${ASSET_NAME}. Available: ${(release.assets || []).map((a) => a.name).join(", ") || "none"}`);
  }

  // Skip if already at this version
  if (fs.existsSync(VERSION_FILE) && fs.readFileSync(VERSION_FILE, "utf8").trim() === tag) {
    log(`Already at latest version ${tag}`);
    return;
  }

  // Download
  const zipPath = path.join(BASE_DIR, ASSET_NAME);
  log(`Downloading ${ASSET_NAME} (${tag}, ${(asset.size / 1024 / 1024).toFixed(1)} MB)...`);
  await download(asset.browser_download_url, zipPath);

  // Extract
  log("Extracting...");
  if (fs.existsSync(APP_DIR)) {
    fs.rmSync(APP_DIR, { recursive: true, force: true });
  }
  fs.mkdirSync(APP_DIR, { recursive: true });

  if (IS_WIN) {
    runOrDie("powershell", [
      "-NoProfile",
      "-Command",
      `Expand-Archive -Path '${zipPath}' -DestinationPath '${APP_DIR}' -Force`,
    ]);
  } else {
    runOrDie("unzip", ["-qo", zipPath, "-d", APP_DIR]);
  }

  // The zip contains agenui-studio/ subfolder — flatten it
  const inner = path.join(APP_DIR, "agenui-studio");
  if (fs.existsSync(inner)) {
    for (const entry of fs.readdirSync(inner)) {
      fs.renameSync(path.join(inner, entry), path.join(APP_DIR, entry));
    }
    fs.rmdirSync(inner);
  }

  // Write version marker
  fs.writeFileSync(VERSION_FILE, tag + "\n");
  fs.unlinkSync(zipPath);

  // Remove cached presets so they get re-seeded from the fresh app bundle
  const presetsDir = path.join(BASE_DIR, "protocols", "presets");
  if (fs.existsSync(presetsDir)) {
    fs.rmSync(presetsDir, { recursive: true, force: true });
    log("Cleared cached presets (will re-seed on next start)");
  }

  log(`Installed version ${tag}`);
}

function ensureVenv() {
  const pyvenvCfg = path.join(VENV_DIR, "pyvenv.cfg");
  if (!fs.existsSync(pyvenvCfg)) {
    log("Creating virtual environment...");
    runOrDie("python3", ["-m", "venv", VENV_DIR]);
  }
  const pip = venvBin("pip");
  log("Installing/refreshing dependencies...");
  runOrDie(pip, ["install", "-q", "--upgrade", "pip"]);
  runOrDie(pip, ["install", "-q", "-r", REQUIREMENTS]);
}

function startServer() {
  const python = venvBin("python");
  const serverArgs = ["-m", "playground.studio.server"];
  if (PORT) {
    serverArgs.push("--port", PORT);
  }
  log("Starting AGenUI Studio (Ctrl+C to stop)...");
  const child = spawn(python, serverArgs, { cwd: APP_DIR, stdio: "inherit" });

  const forward = (sig) => {
    try {
      child.kill(sig);
    } catch (_) {
      /* ignore */
    }
  };
  process.on("SIGINT", () => forward("SIGINT"));
  process.on("SIGTERM", () => forward("SIGTERM"));

  child.on("error", (err) => {
    fail(`Failed to start server: ${err.message}`);
  });
  child.on("exit", (code, signal) => {
    if (signal) {
      process.exit(0);
    }
    process.exit(code == null ? 0 : code);
  });
}

// --- main ------------------------------------------------------------------
async function main() {
  checkPython();
  await ensureApp();
  ensureVenv();
  startServer();
}

main().catch((err) => fail(err.message));
