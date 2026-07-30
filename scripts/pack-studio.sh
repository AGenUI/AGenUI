#!/usr/bin/env bash
# Pack AGenUI Studio into a distributable zip archive.
#
# Usage:
#   ./scripts/pack-studio.sh [output_dir]
#
# Output: <output_dir>/agenui-studio.zip (default: dist/)
#
# Contents of the zip:
#   agenui-studio/
#   ├── playground/studio/  Python server + static frontend + web source
#   ├── samples/            Preset A2UI protocol examples
#   └── skills/             a2ui-generation skill (prompt + validator)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUTPUT_DIR="${1:-${REPO_ROOT}/dist}"
mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"
STAGE_DIR="${OUTPUT_DIR}/agenui-studio"
ZIP_FILE="${OUTPUT_DIR}/agenui-studio.zip"

echo "[pack-studio] Repo root: ${REPO_ROOT}"
echo "[pack-studio] Output:    ${ZIP_FILE}"

# --- clean previous build ---
rm -rf "${STAGE_DIR}" "${ZIP_FILE}"
mkdir -p "${STAGE_DIR}"

# --- build frontend so the zip always ships a fresh, complete UI ---
# server/static is a build artifact (not in git); without building here the
# packed zip would contain no web UI.
echo "[pack-studio] Building frontend (npm run build) ..."
if ! command -v npm >/dev/null 2>&1; then
  echo "[pack-studio] ERROR: npm not found. Install Node.js 18+ to pack Studio." >&2
  exit 1
fi
(
  cd "${REPO_ROOT}/playground/studio/web"
  npm install --no-audit --no-fund
  npm run build
)

# --- copy studio server (exclude __pycache__, node_modules) ---
echo "[pack-studio] Copying playground/studio/ ..."
mkdir -p "${STAGE_DIR}/playground"
rsync -a \
  --exclude='__pycache__' \
  --exclude='node_modules' \
  --exclude='*.pyc' \
  "${REPO_ROOT}/playground/studio/" "${STAGE_DIR}/playground/studio/"

# --- copy playground/__init__.py (package marker) ---
cp "${REPO_ROOT}/playground/__init__.py" "${STAGE_DIR}/playground/"

# --- copy samples (protocols only) ---
echo "[pack-studio] Copying samples/protocols/ ..."
mkdir -p "${STAGE_DIR}/samples/protocols"
rsync -a "${REPO_ROOT}/samples/protocols/" "${STAGE_DIR}/samples/protocols/"

# --- copy required skill directory (prompt builder + validator) ---
echo "[pack-studio] Copying skills/a2ui-generation/ ..."
mkdir -p "${STAGE_DIR}/skills/a2ui-generation"
rsync -a \
  --exclude='__pycache__' \
  --exclude='*.pyc' \
  "${REPO_ROOT}/skills/a2ui-generation/" "${STAGE_DIR}/skills/a2ui-generation/"

# --- create zip ---
echo "[pack-studio] Creating zip ..."
cd "${OUTPUT_DIR}"
rm -f agenui-studio.zip
zip -qr agenui-studio.zip agenui-studio/

# --- report ---
SIZE=$(du -h "${ZIP_FILE}" | cut -f1)
echo "[pack-studio] Done: ${ZIP_FILE} (${SIZE})"
