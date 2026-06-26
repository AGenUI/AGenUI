#!/bin/bash
set -euo pipefail

# iOS stability test launcher
# Builds (optional), installs, and starts StabilityTestViewController
# Supports both physical devices (via ios-deploy/devicectl) and simulators (via simctl)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

# Defaults
SCENARIO="all_combined"
DURATION_MIN=480
ROUNDS=0
INTERVAL_MS=100
CRASH_THRESHOLD=5
OUTPUT_DIR=""
DO_INSTALL=false
FIXTURES=""
BUNDLE_ID="org.cocoapods.demo.Playground"
USE_SIMULATOR=false
DEVICE_ID=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenario)         SCENARIO="$2"; shift 2 ;;
        --duration)         DURATION_MIN="$2"; shift 2 ;;
        --rounds)           ROUNDS="$2"; shift 2 ;;
        --interval)         INTERVAL_MS="$2"; shift 2 ;;
        --crash-threshold)  CRASH_THRESHOLD="$2"; shift 2 ;;
        --output-dir)       OUTPUT_DIR="$2"; shift 2 ;;
        --install)          DO_INSTALL=true; shift ;;
        --fixtures)         FIXTURES="$2"; shift 2 ;;
        --simulator)        USE_SIMULATOR=true; shift ;;
        --device-id)        DEVICE_ID="$2"; shift 2 ;;
        *)                  shift ;;
    esac
done

echo "[iOS] Launching stability test..."

# Auto-fallback: if --simulator not specified but a simulator is already booted
# and no physical device tooling is available, prefer the booted simulator.
if [[ "$USE_SIMULATOR" != true ]]; then
    BOOTED_UDID=""
    if command -v xcrun &>/dev/null; then
        BOOTED_UDID=$(xcrun simctl list devices booted -j 2>/dev/null | python3 -c "
import json, sys
try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(1)
for runtime, devices in data.get('devices', {}).items():
    for d in devices:
        if d.get('state') == 'Booted':
            print(d['udid'])
            sys.exit(0)
sys.exit(1)
" 2>/dev/null || echo "")
    fi
    HAS_PHYS_TOOL=false
    if command -v xcrun &>/dev/null && xcrun devicectl list devices 2>/dev/null | grep -q "Connected"; then
        HAS_PHYS_TOOL=true
    elif command -v ios-deploy &>/dev/null; then
        HAS_PHYS_TOOL=true
    fi
    if [[ -n "$BOOTED_UDID" && "$HAS_PHYS_TOOL" != true ]]; then
        echo "[iOS] No physical device tool detected; falling back to booted simulator: ${BOOTED_UDID}"
        USE_SIMULATOR=true
        [[ -z "$DEVICE_ID" ]] && DEVICE_ID="$BOOTED_UDID"
    fi
fi

# Determine device type and verify connectivity
if [[ "$USE_SIMULATOR" == true ]]; then
    if ! command -v xcrun &>/dev/null; then
        echo "[ERROR] xcrun not found (Xcode required)" >&2
        exit 1
    fi
    # Get booted simulator or first available
    if [[ -z "$DEVICE_ID" ]]; then
        DEVICE_ID=$(xcrun simctl list devices booted -j 2>/dev/null | python3 -c "
import json, sys
data = json.load(sys.stdin)
for runtime, devices in data.get('devices', {}).items():
    for d in devices:
        if d.get('state') == 'Booted':
            print(d['udid'])
            sys.exit(0)
sys.exit(1)
" 2>/dev/null || echo "")
        if [[ -z "$DEVICE_ID" ]]; then
            echo "[ERROR] No booted iOS simulator found. Boot one with: xcrun simctl boot <device>" >&2
            exit 1
        fi
    fi
    echo "[iOS] Using simulator: ${DEVICE_ID}"
else
    # Physical device via devicectl or ios-deploy
    if command -v xcrun &>/dev/null && xcrun devicectl list devices 2>/dev/null | grep -q "Connected"; then
        if [[ -z "$DEVICE_ID" ]]; then
            DEVICE_ID=$(xcrun devicectl list devices 2>/dev/null | grep "Connected" | head -1 | awk '{print $NF}' || echo "")
        fi
        echo "[iOS] Using physical device (devicectl): ${DEVICE_ID}"
    elif command -v ios-deploy &>/dev/null; then
        if [[ -z "$DEVICE_ID" ]]; then
            DEVICE_ID=$(ios-deploy --detect --timeout 5 2>/dev/null | grep "Found" | head -1 | grep -o '\b[0-9a-f]\{40\}\b' || echo "")
        fi
        if [[ -z "$DEVICE_ID" ]]; then
            echo "[ERROR] No iOS device connected" >&2
            exit 1
        fi
        echo "[iOS] Using physical device (ios-deploy): ${DEVICE_ID}"
    else
        echo "[ERROR] No iOS device tool found. Install ios-deploy or use Xcode 15+ (devicectl)" >&2
        exit 1
    fi
fi

# Build and install if requested
if [[ "$DO_INSTALL" == true ]]; then
    echo "[iOS] Building and installing..."
    WORKSPACE="${REPO_ROOT}/playground/ios/Playground/Playground.xcworkspace"
    if [[ "$USE_SIMULATOR" == true ]]; then
        xcodebuild -workspace "$WORKSPACE" \
            -scheme Playground \
            -destination "id=${DEVICE_ID}" \
            -configuration Debug \
            build 2>&1 | tail -5
        # Install to simulator
        APP_PATH=$(xcodebuild -workspace "$WORKSPACE" \
            -scheme Playground \
            -destination "id=${DEVICE_ID}" \
            -configuration Debug \
            -showBuildSettings 2>/dev/null | grep "BUILT_PRODUCTS_DIR" | head -1 | awk '{print $3}')
        xcrun simctl install "$DEVICE_ID" "${APP_PATH}/Playground.app"
    else
        xcodebuild -workspace "$WORKSPACE" \
            -scheme Playground \
            -destination "id=${DEVICE_ID}" \
            -configuration Debug \
            build 2>&1 | tail -5
        # Install via ios-deploy if available
        if command -v ios-deploy &>/dev/null; then
            APP_PATH=$(xcodebuild -workspace "$WORKSPACE" \
                -scheme Playground \
                -destination "id=${DEVICE_ID}" \
                -configuration Debug \
                -showBuildSettings 2>/dev/null | grep "BUILT_PRODUCTS_DIR" | head -1 | awk '{print $3}')
            ios-deploy --bundle "${APP_PATH}/Playground.app" --id "$DEVICE_ID" --no-wifi 2>/dev/null || true
        fi
    fi
    echo "[iOS] App installed successfully"
fi

# Force-terminate any existing instance
if [[ "$USE_SIMULATOR" == true ]]; then
    xcrun simctl terminate "$DEVICE_ID" "$BUNDLE_ID" 2>/dev/null || true
else
    if command -v xcrun &>/dev/null; then
        xcrun devicectl device process terminate --device "$DEVICE_ID" --bundle-id "$BUNDLE_ID" 2>/dev/null || true
    fi
fi
sleep 1

# Clear stale state from previous runs (logs, crash state, done marker)
if [[ "$USE_SIMULATOR" == true ]]; then
    CONTAINER_PATH=$(xcrun simctl get_app_container "$DEVICE_ID" "$BUNDLE_ID" data 2>/dev/null || echo "")
    if [[ -n "$CONTAINER_PATH" && -d "$CONTAINER_PATH/Documents/stability" ]]; then
        rm -f "${CONTAINER_PATH}/Documents/stability/stability_log.jsonl" \
              "${CONTAINER_PATH}/Documents/stability/crash_registry.json" \
              "${CONTAINER_PATH}/Documents/stability/crash_state.json" \
              "${CONTAINER_PATH}/Documents/stability/last_crash_scenario.txt" \
              "${CONTAINER_PATH}/Documents/stability/stability_done.txt" 2>/dev/null || true
        echo "[iOS] Cleared previous stability logs and state (simulator)"
    fi
fi

# Launch app via xcrun simctl launch (simulator) or devicectl/ios-deploy (physical)
# Pass stability test parameters as launch arguments
echo "[iOS] Starting stability test..."
echo "[iOS]   scenario=${SCENARIO}, duration=${DURATION_MIN}min, rounds=${ROUNDS}, interval=${INTERVAL_MS}ms, crash_threshold=${CRASH_THRESHOLD}"

FIXTURES_ARGS=""
if [[ -n "$FIXTURES" ]]; then
    FIXTURES_ARGS="--fixtures $FIXTURES"
fi

if [[ "$USE_SIMULATOR" == true ]]; then
    xcrun simctl launch "$DEVICE_ID" "$BUNDLE_ID" \
        --stability-test \
        --scenario "$SCENARIO" \
        --duration "$DURATION_MIN" \
        --rounds "$ROUNDS" \
        --interval "$INTERVAL_MS" \
        --crash-threshold "$CRASH_THRESHOLD" \
        $FIXTURES_ARGS
else
    if command -v xcrun &>/dev/null; then
        xcrun devicectl device process launch --device "$DEVICE_ID" "$BUNDLE_ID" \
            -- --stability-test \
            --scenario "$SCENARIO" \
            --duration "$DURATION_MIN" \
            --rounds "$ROUNDS" \
            --interval "$INTERVAL_MS" \
            --crash-threshold "$CRASH_THRESHOLD" \
            $FIXTURES_ARGS 2>/dev/null || \
        xcrun devicectl device process launch --device "$DEVICE_ID" "$BUNDLE_ID" 2>/dev/null || true
    elif command -v ios-deploy &>/dev/null; then
        ios-deploy --bundle_id "$BUNDLE_ID" --id "$DEVICE_ID" --justlaunch 2>/dev/null || true
    fi
fi

sleep 2

# Verify app started
if [[ "$USE_SIMULATOR" == true ]]; then
    APP_PID=$(xcrun simctl spawn "$DEVICE_ID" launchctl list 2>/dev/null | grep "$BUNDLE_ID" | awk '{print $1}' || echo "")
    if [[ -n "$APP_PID" && "$APP_PID" != "-" ]]; then
        echo "[iOS] App started successfully (simulator, PID: ${APP_PID})"
    else
        echo "[iOS] Warning: Could not confirm app launch on simulator"
        echo "[iOS] Continuing — monitor.sh will handle crash detection and restart"
    fi
else
    echo "[iOS] App launch requested on physical device"
    echo "[iOS] Continuing — monitor.sh will handle crash detection and restart"
fi

# Write device info for monitor.sh
if [[ -n "$OUTPUT_DIR" ]]; then
    cat > "${OUTPUT_DIR}/device_info.json" <<EOF
{
  "device_id": "${DEVICE_ID}",
  "use_simulator": ${USE_SIMULATOR},
  "bundle_id": "${BUNDLE_ID}"
}
EOF
fi
