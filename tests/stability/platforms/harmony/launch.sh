#!/bin/bash
set -euo pipefail

# HarmonyOS stability test launcher
# Builds (optional), installs, and starts StabilityTestAbility
# Uses hdc (HarmonyOS Device Connector) for device communication

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Defaults
SCENARIO="all_combined"
DURATION_MIN=480
ROUNDS=0
INTERVAL_MS=100
CRASH_THRESHOLD=5
OUTPUT_DIR=""
DO_INSTALL=false
BUNDLE_NAME="com.harmony.agenui"
ABILITY_NAME="StabilityTestAbility"
MODULE_NAME="entry"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenario)         SCENARIO="$2"; shift 2 ;;
        --duration)         DURATION_MIN="$2"; shift 2 ;;
        --rounds)           ROUNDS="$2"; shift 2 ;;
        --interval)         INTERVAL_MS="$2"; shift 2 ;;
        --crash-threshold)  CRASH_THRESHOLD="$2"; shift 2 ;;
        --output-dir)       OUTPUT_DIR="$2"; shift 2 ;;
        --install)          DO_INSTALL=true; shift ;;
        *)                  shift ;;
    esac
done

echo "[Harmony] Launching stability test..."

# Check hdc
if ! command -v hdc &>/dev/null; then
    echo "[ERROR] hdc not found in PATH (HarmonyOS Device Connector required)" >&2
    exit 1
fi

# Check device connected
DEVICE_COUNT=$(hdc list targets 2>/dev/null | grep -cv "^\[Empty\]$" || echo "0")
if [[ "$DEVICE_COUNT" -eq 0 ]] || hdc list targets 2>/dev/null | grep -q "^\[Empty\]$"; then
    echo "[ERROR] No HarmonyOS device connected (hdc list targets shows empty)" >&2
    exit 1
fi
echo "[Harmony] Device connected (${DEVICE_COUNT} device(s))"

# Build and install if requested
if [[ "$DO_INSTALL" == true ]]; then
    echo "[Harmony] Building and installing HAP..."
    cd "${REPO_ROOT}/playground/harmony"

    # Ensure DevEco SDK environment is configured (hvigor requires DEVECO_SDK_HOME)
    DEVECO_HOME="${DEVECO_HOME:-/Applications/DevEco-Studio.app/Contents}"
    if [[ -d "$DEVECO_HOME" ]]; then
        export DEVECO_SDK_HOME="${DEVECO_SDK_HOME:-${DEVECO_HOME}/sdk}"
        export PATH="${DEVECO_HOME}/tools/hvigor/bin:${DEVECO_HOME}/tools/ohpm/bin:${DEVECO_HOME}/tools/node/bin:${DEVECO_HOME}/sdk/default/openharmony/toolchains:${PATH}"
    fi

    # Build using hvigor
    HVIGORW=""
    if command -v hvigorw &>/dev/null; then
        HVIGORW="hvigorw"
    elif [[ -f "./hvigorw" ]]; then
        HVIGORW="./hvigorw"
    elif [[ -f "/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw" ]]; then
        HVIGORW="/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw"
    elif [[ -n "${DEVECO_SDK_HOME:-}" && -f "${DEVECO_SDK_HOME}/tools/hvigor/bin/hvigorw" ]]; then
        HVIGORW="${DEVECO_SDK_HOME}/tools/hvigor/bin/hvigorw"
    else
        echo "[ERROR] hvigorw not found. Please ensure DevEco-Studio is installed or add hvigorw to PATH." >&2
        exit 1
    fi
    echo "[Harmony] Using hvigorw: ${HVIGORW}"
    BUILD_LOG="${OUTPUT_DIR:-.}/harmony_build.log"
    mkdir -p "$(dirname "$BUILD_LOG")"
    if ! "$HVIGORW" assembleHap --mode module -p module="${MODULE_NAME}" -p product=default --no-daemon 2>&1 | tee "$BUILD_LOG" | tail -10; then
        echo "[ERROR] hvigor assembleHap failed; full log: ${BUILD_LOG}" >&2
        exit 1
    fi
    # Find and install HAP (prefer signed, fallback to unsigned)
    HAP_PATH=$(find . -name "${MODULE_NAME}-default-signed.hap" 2>/dev/null | head -1)
    if [[ -z "$HAP_PATH" ]]; then
        HAP_PATH=$(find . -name "${MODULE_NAME}-default-unsigned.hap" -not -path "*/ohosTest/*" 2>/dev/null | head -1)
    fi
    if [[ -z "$HAP_PATH" ]]; then
        HAP_PATH=$(find . -name "${MODULE_NAME}-default.hap" -not -path "*/ohosTest/*" 2>/dev/null | head -1)
    fi
    if [[ -n "$HAP_PATH" ]]; then
        echo "[Harmony] Installing: ${HAP_PATH}"
        hdc install "$HAP_PATH"
        echo "[Harmony] HAP installed successfully"
    else
        echo "[ERROR] HAP file not found after build" >&2
        exit 1
    fi
    cd "$REPO_ROOT"
fi

# Force stop any existing instance
hdc shell aa force-stop "$BUNDLE_NAME" 2>/dev/null || true
sleep 1

# Clear stale state from previous runs (logs, crash state, done marker)
DEVICE_STABILITY_DIR="/data/app/el2/100/base/${BUNDLE_NAME}/haps/${MODULE_NAME}/files/stability"
hdc shell "rm -f '${DEVICE_STABILITY_DIR}/crash_registry.json' \
                  '${DEVICE_STABILITY_DIR}/crash_state.json' \
                  '${DEVICE_STABILITY_DIR}/last_crash_scenario.txt' \
                  '${DEVICE_STABILITY_DIR}/stability_done.txt' \
                  '${DEVICE_STABILITY_DIR}/stability_log.jsonl'" 2>/dev/null || true
echo "[Harmony] Cleared previous crash registry, markers, and logs"

# Launch ability with stability test parameters via want extras
echo "[Harmony] Starting StabilityTestAbility..."
echo "[Harmony]   scenario=${SCENARIO}, duration=${DURATION_MIN}min, rounds=${ROUNDS}, interval=${INTERVAL_MS}ms, crash_threshold=${CRASH_THRESHOLD}"

hdc shell aa start \
    -a "$ABILITY_NAME" \
    -b "$BUNDLE_NAME" \
    -m "$MODULE_NAME" \
    --ps scenario "$SCENARIO" \
    --pi duration_minutes "$DURATION_MIN" \
    --pi rounds "$ROUNDS" \
    --pi interval_ms "$INTERVAL_MS" \
    --pi crash_threshold "$CRASH_THRESHOLD"

sleep 2

# Verify it started
if hdc shell pidof "$BUNDLE_NAME" > /dev/null 2>&1; then
    PID=$(hdc shell pidof "$BUNDLE_NAME" | tr -d '\r\n')
    echo "[Harmony] App started successfully (PID: ${PID})"
else
    echo "[Harmony] Warning: App process not found after launch (may have crashed immediately)"
    echo "[Harmony] Continuing — monitor.sh will handle crash detection and restart"
fi
