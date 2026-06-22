#!/bin/bash
set -euo pipefail

# Android stability test launcher
# Builds (optional), installs, and starts StabilityTestActivity

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# launch.sh lives at tests/stability/platforms/android/, so REPO_ROOT (AGenUI) is 4 levels up.
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

# Defaults
SCENARIO="all_combined"
DURATION_MIN=480
ROUNDS=0
INTERVAL_MS=100
CRASH_THRESHOLD=5
OUTPUT_DIR=""
DO_INSTALL=false
PACKAGE="com.amap.agenuiplayground"
ACTIVITY=".stability.StabilityTestActivity"

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

echo "[Android] Launching stability test..."

# Check adb
if ! command -v adb &>/dev/null; then
    echo "[ERROR] adb not found in PATH" >&2
    exit 1
fi

# Check device connected
DEVICE_COUNT=$(adb devices | grep -c "device$" || true)
if [[ "$DEVICE_COUNT" -eq 0 ]]; then
    echo "[ERROR] No Android device/emulator connected" >&2
    exit 1
fi
echo "[Android] Device connected (${DEVICE_COUNT} device(s))"

# Try to enable adb root so monitor.sh can read /data/tombstones/ on userdebug builds.
# On user-build devices this is a no-op; we silently ignore failure.
if adb root 2>/dev/null | grep -q "running as root\|already running as root"; then
    adb wait-for-device 2>/dev/null || true
    echo "[Android] adb root enabled (tombstones will be readable)"
else
    echo "[Android] adb root not available (user build) — tombstones may be inaccessible"
fi

# Enlarge logcat ringbuffer so the crash backtrace is not flushed away by chatty
# system logs (WifiThroughputPredictor / skia AGTM / etc.). Default is 256KB which
# only retains ~22s of logs on busy emulators.
adb logcat -G 16M 2>/dev/null || true
# Drop noisy tags to silent — we don't need them for crash analysis and they
# crowd out AndroidRuntime / DEBUG / libc messages.
adb shell setprop log.tag.WifiThroughputPredictor SILENT 2>/dev/null || true
adb shell setprop log.tag.WifiClientModeImpl SILENT 2>/dev/null || true
adb shell setprop log.tag.WifiScoreCard SILENT 2>/dev/null || true
adb shell setprop log.tag.HalDevMgr SILENT 2>/dev/null || true
# Clear logcat buffer so the next dump starts from a clean state.
adb logcat -c 2>/dev/null || true
echo "[Android] logcat ringbuffer expanded to 16M and noisy tags muted"

# Pre-flight: detect whether the installed APK contains StabilityTestActivity.
# If not installed, or installed but missing the activity (older APK), auto-install
# so users don't hit "Activity class does not exist" when forgetting --install.
FULL_ACTIVITY_CLASS="${PACKAGE}${ACTIVITY}"  # com.amap.agenuiplayground.stability.StabilityTestActivity
if [[ "$DO_INSTALL" != true ]]; then
    if ! adb shell pm list packages 2>/dev/null | tr -d '\r' | grep -qx "package:${PACKAGE}"; then
        echo "[Android] Package ${PACKAGE} not installed on device — auto-installing..."
        DO_INSTALL=true
    elif ! adb shell pm dump "$PACKAGE" 2>/dev/null | grep -qF "$FULL_ACTIVITY_CLASS"; then
        echo "[Android] Installed APK does not contain ${FULL_ACTIVITY_CLASS} — auto-installing..."
        DO_INSTALL=true
    fi
fi

# Build and install if requested or auto-detected as needed
if [[ "$DO_INSTALL" == true ]]; then
    echo "[Android] Building and installing APK..."
    cd "${REPO_ROOT}/playground/android"
    ./gradlew :app:installDebug -q
    cd "$REPO_ROOT"
    echo "[Android] APK installed successfully"
fi

# Force stop any existing instance (including the instrumentation test apk,
# whose InstrumentationActivityInvoker$EmptyActivity may otherwise occupy the
# foreground and starve StabilityTestActivity of frame callbacks).
adb shell am force-stop "$PACKAGE" 2>/dev/null || true
adb shell am force-stop "${PACKAGE}.test" 2>/dev/null || true
sleep 1

# Clear stale state from previous runs (blacklist, done marker, crash state, old logs)
DEVICE_STABILITY_DIR="/sdcard/Android/data/${PACKAGE}/files/stability"
adb shell "rm -f '${DEVICE_STABILITY_DIR}/crash_registry.json' \
                  '${DEVICE_STABILITY_DIR}/crash_registry.json.tmp' \
                  '${DEVICE_STABILITY_DIR}/crash_state.json' \
                  '${DEVICE_STABILITY_DIR}/crash_state.json.tmp' \
                  '${DEVICE_STABILITY_DIR}/stability_done.txt' \
                  '${DEVICE_STABILITY_DIR}/stability_log.jsonl'" 2>/dev/null || true
echo "[Android] Cleared previous crash registry, markers, and logs"

# Launch activity with parameters
echo "[Android] Starting StabilityTestActivity..."
echo "[Android]   scenario=${SCENARIO}, duration=${DURATION_MIN}min, rounds=${ROUNDS}, interval=${INTERVAL_MS}ms, crash_threshold=${CRASH_THRESHOLD}"

adb shell am start -n "${PACKAGE}/${ACTIVITY}" \
    --es scenario "$SCENARIO" \
    --ei duration_minutes "$DURATION_MIN" \
    --ei rounds "$ROUNDS" \
    --ei interval_ms "$INTERVAL_MS" \
    --ei crash_threshold "$CRASH_THRESHOLD"

# Brief check that the activity was accepted by the system
sleep 1

# Verify it started (check quickly — the app may crash shortly after due to native issues,
# but that's handled by monitor.sh, not here)
if adb shell pidof "$PACKAGE" > /dev/null 2>&1; then
    PID=$(adb shell pidof "$PACKAGE")
    echo "[Android] App started successfully (PID: ${PID})"
else
    # App might have already crashed — this is OK for stability testing.
    # monitor.sh will handle restart. Only fail if am start itself reported an error.
    echo "[Android] Warning: App process not found after launch (may have crashed immediately)"
    echo "[Android] Continuing — monitor.sh will handle crash detection and restart"
fi
