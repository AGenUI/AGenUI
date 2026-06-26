#!/bin/bash
set -euo pipefail

# ============================================================================
# tests/stability/run.sh
# AGenUI SDK Stability Test Runner
#
# Launches stability test on specified platform, monitors for crashes,
# and collects results after completion.
#
# Usage:
#   ./tests/stability/run.sh [options]
#
# Options:
#   --android               Run on Android (default)
#   --ios                   Run on iOS (physical device or simulator)
#   --harmony               Run on HarmonyOS
#   --simulator             (iOS only) Use iOS Simulator instead of physical device
#   --device-id <id>        (iOS only) Specify device UDID
#   --scenario <name>       Scenario: session_storm|stream_marathon|multi_surface|
#                           action_flood|theme_switch|interrupt_recover|
#                           extreme_render|sdk_robustness|sdk_interface_stability|jni_bridge_race|
#                           all_combined (default: all_combined)
#   --duration <minutes>    Run duration in minutes (default: 480)
#   --rounds <n>            Max rounds, 0=unlimited (default: 0)
#   --interval <ms>         Delay between rounds in ms (default: 100)
#   --fixtures <list>       Comma-separated fixture paths to run (selective mode)
#   --fixtures-file <path>  Newline-separated fixture paths to run (selective mode)
#   --install               Build and install before running
#   -h, --help              Show help
#
# Examples:
#   ./tests/stability/run.sh --android --duration 60
#   ./tests/stability/run.sh --ios --simulator --duration 30
#   ./tests/stability/run.sh --harmony --install --duration 120
#   ./tests/stability/run.sh --scenario extreme_render --duration 1440
#   ./tests/stability/run.sh --android --scenario jni_bridge_race --duration 60
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Source common utilities if available
if [[ -f "${REPO_ROOT}/scripts/common/_common.sh" ]]; then
    source "${REPO_ROOT}/scripts/common/_common.sh"
else
    # Minimal fallback
    info() { echo "[INFO] $*"; }
    warn() { echo "[WARN] $*" >&2; }
    error() { echo "[ERROR] $*" >&2; exit 1; }
    success() { echo "[OK] $*"; }
fi

# Defaults
PLATFORM="android"
SCENARIO="all_combined"
DURATION_MIN=480
ROUNDS=0
INTERVAL_MS=100
CRASH_THRESHOLD=5
DO_INSTALL=false
USE_SIMULATOR=false
DEVICE_ID=""
FIXTURES=""
FIXTURES_FILE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --android)          PLATFORM="android"; shift ;;
        --ios)              PLATFORM="ios"; shift ;;
        --harmony)          PLATFORM="harmony"; shift ;;
        --simulator)        USE_SIMULATOR=true; shift ;;
        --device-id)        DEVICE_ID="$2"; shift 2 ;;
        --scenario)         SCENARIO="$2"; shift 2 ;;
        --duration)         DURATION_MIN="$2"; shift 2 ;;
        --rounds|--round)    ROUNDS="$2"; shift 2 ;;
        --interval)         INTERVAL_MS="$2"; shift 2 ;;
        --crash-threshold)  CRASH_THRESHOLD="$2"; shift 2 ;;
        --fixtures)         FIXTURES="$2"; shift 2 ;;
        --fixtures-file)    FIXTURES_FILE="$2"; shift 2 ;;
        --install)          DO_INSTALL=true; shift ;;
        -h|--help)          sed -n '5,37p' "$0" | sed 's/^# \?//'; exit 0 ;;
        *)                  error "Unknown argument: $1" ;;
    esac
done

if [[ -n "$FIXTURES_FILE" ]]; then
    [[ -f "$FIXTURES_FILE" ]] || error "Fixtures file not found: $FIXTURES_FILE"
    FIXTURES=$(python3 - "$FIXTURES_FILE" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
items = [line.strip() for line in path.read_text().splitlines() if line.strip()]
print(",".join(items))
PY
)
fi

# Output directory
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BRANCH=$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
COMMIT=$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo "unknown")
RUN_KEY="${BRANCH}_${COMMIT}_${TIMESTAMP}"
OUTPUT_DIR="${REPO_ROOT}/reports/stability/runs/${RUN_KEY}"
mkdir -p "${OUTPUT_DIR}/${PLATFORM}"

info "========================================"
info "AGenUI Stability Test"
info "========================================"
info "Platform   : ${PLATFORM}"
info "Scenario   : ${SCENARIO}"
info "Duration   : ${DURATION_MIN} minutes"
info "Max Rounds : ${ROUNDS} (0=unlimited)"
info "Interval   : ${INTERVAL_MS}ms"
info "CrashThres : ${CRASH_THRESHOLD}"
info "Run Key    : ${RUN_KEY}"
info "Output     : ${OUTPUT_DIR}"
info "========================================"

# Write metadata
cat > "${OUTPUT_DIR}/metadata.json" <<EOF
{
  "run_key": "${RUN_KEY}",
  "platform": "${PLATFORM}",
  "scenario": "${SCENARIO}",
  "duration_minutes": ${DURATION_MIN},
  "max_rounds": ${ROUNDS},
  "interval_ms": ${INTERVAL_MS},
  "git_branch": "${BRANCH}",
  "git_commit": "${COMMIT}",
  "start_time": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

# Dispatch to platform-specific script
case "$PLATFORM" in
    android)
        # ---------- Multi-device selection (10s timeout: prefer emulator, otherwise first device) ----------
        # Clear inherited ANDROID_SERIAL to avoid parent shell interference; export after selection
        unset ANDROID_SERIAL

        # Auto-detect ANDROID_HOME (consistent with launch.sh)
        if [[ -z "${ANDROID_HOME:-}" ]]; then
            if [[ -n "${ANDROID_SDK_ROOT:-}" ]]; then
                export ANDROID_HOME="${ANDROID_SDK_ROOT}"
            elif [[ -d "${HOME}/Library/Android/sdk" ]]; then
                export ANDROID_HOME="${HOME}/Library/Android/sdk"
            elif [[ -d "${HOME}/Android/Sdk" ]]; then
                export ANDROID_HOME="${HOME}/Android/Sdk"
            fi
        fi

        # List all online devices/emulators (exclude offline / unauthorized)
        CONNECTED_DEVICES=($(adb devices 2>/dev/null | awk '/\t(device|emulator)$/{print $1}'))
        if [[ ${#CONNECTED_DEVICES[@]} -eq 0 ]]; then
            error "No available Android device or emulator detected. Please connect a device or start an emulator."
        fi

        # User specifies --device-id > multi-device interactive (10s timeout) > single device direct
        if [[ -n "$DEVICE_ID" ]]; then
            TARGET_SERIAL="$DEVICE_ID"
        elif [[ ${#CONNECTED_DEVICES[@]} -gt 1 ]]; then
            echo ""
            info "Detected ${#CONNECTED_DEVICES[@]} Android devices, please select target device (auto-select in 10s if no input):"
            echo ""
            for i in "${!CONNECTED_DEVICES[@]}"; do
                MODEL=$(adb -s "${CONNECTED_DEVICES[$i]}" shell getprop ro.product.model 2>/dev/null | tr -d '\r' || echo "unknown")
                EMULATOR_TAG=""
                [[ "${CONNECTED_DEVICES[$i]}" == emulator-* ]] && EMULATOR_TAG=" [emulator]"
                printf "  [%d] %s  (%s)%s\n" "$((i+1))" "${CONNECTED_DEVICES[$i]}" "$MODEL" "$EMULATOR_TAG"
            done
            echo ""

            # Auto-select on timeout: prefer emulator, otherwise random device
            _auto_select_device() {
                local emus=()
                for d in "${CONNECTED_DEVICES[@]}"; do
                    [[ "$d" == emulator-* ]] && emus+=("$d")
                done
                if [[ ${#emus[@]} -gt 0 ]]; then
                    # Multiple emulators: pick random
                    echo "${emus[$((RANDOM % ${#emus[@]}))]}"
                else
                    # No emulator, pick random physical device
                    echo "${CONNECTED_DEVICES[$((RANDOM % ${#CONNECTED_DEVICES[@]}))]}"
                fi
            }

            TARGET_SERIAL=""
            TIMEOUT_SEC=10
            ATTEMPTS=0
            while [[ -z "$TARGET_SERIAL" ]]; do
                if [[ $ATTEMPTS -ge $TIMEOUT_SEC ]]; then
                    TARGET_SERIAL=$(_auto_select_device)
                    echo ""
                    info "Timeout, auto-selected: ${TARGET_SERIAL}"
                    break
                fi
                REMAIN=$((TIMEOUT_SEC - ATTEMPTS))
                printf "\rEnter number (1-%d) [auto-select in %ds]: " "${#CONNECTED_DEVICES[@]}" "$REMAIN"
                if read -t 1 -r CHOICE 2>/dev/null; then
                    if [[ -n "$CHOICE" ]] && [[ "$CHOICE" =~ ^[0-9]+$ ]] && \
                       [[ "$CHOICE" -ge 1 ]] && [[ "$CHOICE" -le ${#CONNECTED_DEVICES[@]} ]]; then
                        TARGET_SERIAL="${CONNECTED_DEVICES[$((CHOICE-1))]}"
                        echo ""
                    else
                        echo ""
                        echo "  Invalid input, please enter a number between 1 and ${#CONNECTED_DEVICES[@]}"
                    fi
                fi
                ATTEMPTS=$((ATTEMPTS + 1))
            done
        else
            TARGET_SERIAL="${CONNECTED_DEVICES[0]}"
        fi

        # Verify target device is actually online
        if ! printf '%s\n' "${CONNECTED_DEVICES[@]}" | grep -qx "$TARGET_SERIAL"; then
            error "Specified device ${TARGET_SERIAL} is not in the connected device list: ${CONNECTED_DEVICES[*]}"
        fi

        # Lock all subsequent adb/gradle calls to this device
        export ANDROID_SERIAL="$TARGET_SERIAL"
        info "Target device: ${ANDROID_SERIAL} (${#CONNECTED_DEVICES[@]} device(s) online)"

        bash "${SCRIPT_DIR}/platforms/android/launch.sh" \
            --scenario "$SCENARIO" \
            --duration "$DURATION_MIN" \
            --rounds "$ROUNDS" \
            --interval "$INTERVAL_MS" \
            --crash-threshold "$CRASH_THRESHOLD" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}" \
            $([ "$DO_INSTALL" = true ] && echo "--install" || true) \
            $([ -n "$FIXTURES" ] && echo "--fixtures $FIXTURES" || true)

        # monitor.sh returns exit 1 on crashes/freezes — don't let it abort the pipeline
        MONITOR_EXIT=0
        bash "${SCRIPT_DIR}/platforms/android/monitor.sh" \
            --duration "$DURATION_MIN" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}" \
            --scenario "$SCENARIO" \
            --interval "$INTERVAL_MS" \
            --crash-threshold "$CRASH_THRESHOLD" || MONITOR_EXIT=$?

        bash "${SCRIPT_DIR}/platforms/android/collect.sh" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}"

        # Symbolicate crash logs (resolve native addresses to function/file/line)
        if command -v python3 &> /dev/null; then
            python3 "${SCRIPT_DIR}/symbolicate.py" \
                --input-dir "${OUTPUT_DIR}/${PLATFORM}" \
                --platform android || warn "Symbolication failed (non-fatal)"
        fi

        # Generate HTML report
        if command -v python3 &> /dev/null; then
            python3 "${SCRIPT_DIR}/generate_report.py" \
                --input-dir "${OUTPUT_DIR}/${PLATFORM}"
            info "Report generated: ${OUTPUT_DIR}/${PLATFORM}/report.html"
        else
            warn "python3 not found, skipping HTML report generation"
        fi
        ;;
    ios)
        # Build iOS-specific args
        IOS_EXTRA_ARGS=""
        [[ "$USE_SIMULATOR" == true ]] && IOS_EXTRA_ARGS+=" --simulator"
        [[ -n "$DEVICE_ID" ]] && IOS_EXTRA_ARGS+=" --device-id $DEVICE_ID"

        bash "${SCRIPT_DIR}/platforms/ios/launch.sh" \
            --scenario "$SCENARIO" \
            --duration "$DURATION_MIN" \
            --rounds "$ROUNDS" \
            --interval "$INTERVAL_MS" \
            --crash-threshold "$CRASH_THRESHOLD" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}" \
            $([ "$DO_INSTALL" = true ] && echo "--install" || true) \
            $([ -n "$FIXTURES" ] && echo "--fixtures $FIXTURES" || true) \
            $IOS_EXTRA_ARGS

        MONITOR_EXIT=0
        bash "${SCRIPT_DIR}/platforms/ios/monitor.sh" \
            --duration "$DURATION_MIN" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}" \
            --scenario "$SCENARIO" \
            --interval "$INTERVAL_MS" \
            --crash-threshold "$CRASH_THRESHOLD" \
            $IOS_EXTRA_ARGS || MONITOR_EXIT=$?

        bash "${SCRIPT_DIR}/platforms/ios/collect.sh" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}" \
            $IOS_EXTRA_ARGS

        # Symbolicate crash logs
        if command -v python3 &> /dev/null; then
            python3 "${SCRIPT_DIR}/symbolicate.py" \
                --input-dir "${OUTPUT_DIR}/${PLATFORM}" \
                --platform ios || warn "Symbolication failed (non-fatal)"
        fi

        # Generate HTML report
        if command -v python3 &> /dev/null; then
            python3 "${SCRIPT_DIR}/generate_report.py" \
                --input-dir "${OUTPUT_DIR}/${PLATFORM}"
            info "Report generated: ${OUTPUT_DIR}/${PLATFORM}/report.html"
        else
            warn "python3 not found, skipping HTML report generation"
        fi
        ;;
    harmony)
        bash "${SCRIPT_DIR}/platforms/harmony/launch.sh" \
            --scenario "$SCENARIO" \
            --duration "$DURATION_MIN" \
            --rounds "$ROUNDS" \
            --interval "$INTERVAL_MS" \
            --crash-threshold "$CRASH_THRESHOLD" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}" \
            $([ "$DO_INSTALL" = true ] && echo "--install" || true) \
            $([ -n "$FIXTURES" ] && echo "--fixtures $FIXTURES" || true)

        MONITOR_EXIT=0
        bash "${SCRIPT_DIR}/platforms/harmony/monitor.sh" \
            --duration "$DURATION_MIN" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}" \
            --scenario "$SCENARIO" \
            --rounds "$ROUNDS" \
            --interval "$INTERVAL_MS" \
            --crash-threshold "$CRASH_THRESHOLD" || MONITOR_EXIT=$?

        bash "${SCRIPT_DIR}/platforms/harmony/collect.sh" \
            --output-dir "${OUTPUT_DIR}/${PLATFORM}"

        # Symbolicate crash logs
        if command -v python3 &> /dev/null; then
            python3 "${SCRIPT_DIR}/symbolicate.py" \
                --input-dir "${OUTPUT_DIR}/${PLATFORM}" \
                --platform harmony || warn "Symbolication failed (non-fatal)"
        fi

        # Generate HTML report
        if command -v python3 &> /dev/null; then
            python3 "${SCRIPT_DIR}/generate_report.py" \
                --input-dir "${OUTPUT_DIR}/${PLATFORM}"
            info "Report generated: ${OUTPUT_DIR}/${PLATFORM}/report.html"
        else
            warn "python3 not found, skipping HTML report generation"
        fi
        ;;
    *)
        error "Platform '${PLATFORM}' not supported. Use: --android, --ios, --harmony"
        ;;
esac

# Final summary
info ""
info "========================================"
info "Stability Test Complete"
info "========================================"
info "Results: ${OUTPUT_DIR}"
if [[ -f "${OUTPUT_DIR}/${PLATFORM}/stability_log.jsonl" ]]; then
    TOTAL_LINES=$(wc -l < "${OUTPUT_DIR}/${PLATFORM}/stability_log.jsonl" | tr -d ' ')
    info "Log entries: ${TOTAL_LINES}"
fi
info "========================================"
success "Done!"

# Propagate monitor exit code (non-zero if crashes/freezes detected)
exit ${MONITOR_EXIT:-0}
