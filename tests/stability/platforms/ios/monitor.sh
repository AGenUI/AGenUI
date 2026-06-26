#!/bin/bash
set -euo pipefail

# iOS stability test monitor
# Monitors process health, detects crashes and freezes, auto-restarts if needed
# Supports both simulator (simctl) and physical device (devicectl/ios-deploy)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUNDLE_ID="org.cocoapods.demo.Playground"

# The iOS app writes heartbeat data to this file in its Documents directory.
# For simulator, we can access directly; for physical device, we poll via log output.
FREEZE_TIMEOUT=30

# Defaults
DURATION_MIN=480
OUTPUT_DIR="."
CHECK_INTERVAL=10
SCENARIO="all_combined"
INTERVAL_MS=100
CRASH_THRESHOLD=5
USE_SIMULATOR=false
DEVICE_ID=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration)         DURATION_MIN="$2"; shift 2 ;;
        --output-dir)       OUTPUT_DIR="$2"; shift 2 ;;
        --scenario)         SCENARIO="$2"; shift 2 ;;
        --interval)         INTERVAL_MS="$2"; shift 2 ;;
        --crash-threshold)  CRASH_THRESHOLD="$2"; shift 2 ;;
        --simulator)        USE_SIMULATOR=true; shift ;;
        --device-id)        DEVICE_ID="$2"; shift 2 ;;
        *)                  shift ;;
    esac
done

# Read device_info.json if available
DEVICE_INFO="${OUTPUT_DIR}/device_info.json"
if [[ -f "$DEVICE_INFO" ]]; then
    DEVICE_ID=$(python3 -c "import json; d=json.load(open('$DEVICE_INFO')); print(d.get('device_id',''))" 2>/dev/null || echo "$DEVICE_ID")
    USE_SIMULATOR=$(python3 -c "import json; d=json.load(open('$DEVICE_INFO')); print('true' if d.get('use_simulator') else 'false')" 2>/dev/null || echo "$USE_SIMULATOR")
    BUNDLE_ID=$(python3 -c "import json; d=json.load(open('$DEVICE_INFO')); print(d.get('bundle_id','$BUNDLE_ID'))" 2>/dev/null || echo "$BUNDLE_ID")
fi

DURATION_SEC=$((DURATION_MIN * 60))
CRASH_DIR="${OUTPUT_DIR}/crashes"
mkdir -p "$CRASH_DIR"

START_TIME=$(date +%s)
CRASH_COUNT=0
FREEZE_COUNT=0
CRASH_SCENARIOS=()
FREEZE_SCENARIOS=()
LAST_RESTART_TIME=$START_TIME

# === Helper functions ===

is_app_running() {
    if [[ "$USE_SIMULATOR" == true ]]; then
        # Check if app process exists in simulator
        xcrun simctl spawn "$DEVICE_ID" launchctl list 2>/dev/null | grep -q "$BUNDLE_ID" && return 0
        # Fallback: check via process list
        local SIM_PID
        SIM_PID=$(xcrun simctl spawn "$DEVICE_ID" /bin/sh -c "ps aux 2>/dev/null | grep -i 'Playground' | grep -v grep" 2>/dev/null || echo "")
        [[ -n "$SIM_PID" ]] && return 0
        return 1
    else
        # Physical device: check via devicectl
        if command -v xcrun &>/dev/null; then
            xcrun devicectl device info processes --device "$DEVICE_ID" 2>/dev/null | grep -qi "$BUNDLE_ID" && return 0
        fi
        # Fallback: ios-deploy
        if command -v ios-deploy &>/dev/null; then
            ios-deploy --exists --bundle_id "$BUNDLE_ID" --id "$DEVICE_ID" 2>/dev/null && return 0
        fi
        return 1
    fi
}

is_device_reachable() {
    if [[ "$USE_SIMULATOR" == true ]]; then
        xcrun simctl list devices booted -j 2>/dev/null | grep -q "$DEVICE_ID" && return 0
        return 1
    else
        if command -v xcrun &>/dev/null; then
            xcrun devicectl list devices 2>/dev/null | grep -q "$DEVICE_ID" && return 0
        fi
        if command -v ios-deploy &>/dev/null; then
            ios-deploy --detect --timeout 3 2>/dev/null | grep -q "$DEVICE_ID" && return 0
        fi
        return 1
    fi
}

collect_crash_log() {
    local CRASH_FILE="$1"
    local DEVICE_TIME
    DEVICE_TIME=$(date '+%Y-%m-%d %H:%M:%S')

    {
        echo "--- Crash #${CRASH_COUNT} at $(date +%Y%m%d_%H%M%S) ---"
        echo "Device time: ${DEVICE_TIME}"
        echo "Elapsed: ${ELAPSED}s"
        echo "Crashed scenario: (pending attribution)"
        echo ""
        echo "=== SYSTEM LOG (last 2000 lines) ==="
    } > "$CRASH_FILE"

    if [[ "$USE_SIMULATOR" == true ]]; then
        # Simulator: read system log
        xcrun simctl spawn "$DEVICE_ID" log show --predicate "subsystem == 'com.amap.agenui'" \
            --last 5m --style compact 2>/dev/null | tail -2000 >> "$CRASH_FILE" 2>/dev/null || true
        # Also try general crash-related logs
        echo "" >> "$CRASH_FILE"
        echo "=== CRASH REPORT (if available) ===" >> "$CRASH_FILE"
        # Check for recent .ips crash reports
        local SIM_LOG_DIR
        SIM_LOG_DIR="$HOME/Library/Logs/DiagnosticReports"
        if [[ -d "$SIM_LOG_DIR" ]]; then
            # The .ips file is often written before we create crash_*.txt, so
            # filtering by CRASH_FILE mtime drops the very report we need.
            # Use a recent-time window only and include simulator-native .ips reports.
            find "$SIM_LOG_DIR" -type f \( -name "Playground*" -o -name "*.ips" \) -mmin -5 2>/dev/null | \
                while read -r f; do
                    echo "--- $(basename "$f") ---"
                    head -200 "$f"
                done >> "$CRASH_FILE" 2>/dev/null || true
        fi
    else
        # Physical device: collect syslog
        if command -v idevicesyslog &>/dev/null; then
            # idevicesyslog in one-shot mode (capture recent buffer)
            timeout 5 idevicesyslog -u "$DEVICE_ID" 2>/dev/null | tail -2000 >> "$CRASH_FILE" || true
        fi
        # Pull crash reports from device
        echo "" >> "$CRASH_FILE"
        echo "=== DEVICE CRASH REPORTS ===" >> "$CRASH_FILE"
        if command -v idevicecrashreport &>/dev/null; then
            local CRASH_PULL_DIR="${CRASH_DIR}/.crash_pull_tmp"
            mkdir -p "$CRASH_PULL_DIR"
            idevicecrashreport -u "$DEVICE_ID" -e "$CRASH_PULL_DIR" 2>/dev/null || true
            # Find recent Playground crash reports
            find "$CRASH_PULL_DIR" -name "Playground*" -mmin -5 2>/dev/null | \
                while read -r f; do
                    echo "--- $(basename "$f") ---"
                    head -200 "$f"
                done >> "$CRASH_FILE" 2>/dev/null || true
            rm -rf "$CRASH_PULL_DIR"
        else
            echo "(idevicecrashreport not available)" >> "$CRASH_FILE"
        fi
    fi

    echo "[Monitor] Crash log saved: ${CRASH_FILE}"
}

get_heartbeat_age() {
    # Returns the age of the heartbeat file in seconds, or -1 if unavailable
    if [[ "$USE_SIMULATOR" == true ]]; then
        # Direct filesystem access for simulator
        local CONTAINER
        CONTAINER=$(xcrun simctl get_app_container "$DEVICE_ID" "$BUNDLE_ID" data 2>/dev/null || echo "")
        if [[ -n "$CONTAINER" ]]; then
            local LOG_FILE="${CONTAINER}/Documents/stability/stability_log.jsonl"
            if [[ -f "$LOG_FILE" ]]; then
                local FILE_MTIME
                FILE_MTIME=$(stat -f %m "$LOG_FILE" 2>/dev/null || echo "0")
                local NOW
                NOW=$(date +%s)
                echo $((NOW - FILE_MTIME))
                return
            fi
        fi
    else
        # Physical device: use log marker approach
        # The app outputs a heartbeat marker in syslog that we monitor
        # Fallback: we track the last time we saw app activity in syslog
        echo "-1"
        return
    fi
    echo "-1"
}

restart_app() {
    local REMAINING=$1
    local RESTART_REASON=$2
    local REMAINING_MIN=$((REMAINING / 60))

    if [[ "$USE_SIMULATOR" == true ]]; then
        xcrun simctl terminate "$DEVICE_ID" "$BUNDLE_ID" 2>/dev/null || true
        sleep 1
        xcrun simctl launch "$DEVICE_ID" "$BUNDLE_ID" \
            --stability-test \
            --scenario "$SCENARIO" \
            --duration "$REMAINING_MIN" \
            --rounds "0" \
            --interval "$INTERVAL_MS" \
            --crash-threshold "$CRASH_THRESHOLD" \
            --restart-reason "$RESTART_REASON"
    else
        if command -v xcrun &>/dev/null; then
            xcrun devicectl device process terminate --device "$DEVICE_ID" --bundle-id "$BUNDLE_ID" 2>/dev/null || true
            sleep 1
            xcrun devicectl device process launch --device "$DEVICE_ID" "$BUNDLE_ID" \
                -- --stability-test \
                --scenario "$SCENARIO" \
                --duration "$REMAINING_MIN" \
                --rounds "0" \
                --interval "$INTERVAL_MS" \
                --crash-threshold "$CRASH_THRESHOLD" \
                --restart-reason "$RESTART_REASON" 2>/dev/null || \
            xcrun devicectl device process launch --device "$DEVICE_ID" "$BUNDLE_ID" 2>/dev/null || true
        elif command -v ios-deploy &>/dev/null; then
            ios-deploy --bundle_id "$BUNDLE_ID" --id "$DEVICE_ID" --justlaunch 2>/dev/null || true
        fi
    fi
    sleep 3
    LAST_RESTART_TIME=$(date +%s)
}

read_crash_scenario() {
    # Read current scenario from crash_state.json on device
    local STATE_JSON=""
    if [[ "$USE_SIMULATOR" == true ]]; then
        local CONTAINER
        CONTAINER=$(xcrun simctl get_app_container "$DEVICE_ID" "$BUNDLE_ID" data 2>/dev/null || echo "")
        if [[ -n "$CONTAINER" ]]; then
            local STATE_FILE="${CONTAINER}/Documents/stability/crash_state.json"
            if [[ -f "$STATE_FILE" ]]; then
                STATE_JSON=$(cat "$STATE_FILE" 2>/dev/null || echo "")
            fi
            # Also try last_crash_scenario.txt (durable file written by app on restart)
            if [[ -z "$STATE_JSON" ]]; then
                local LAST_CRASH_FILE="${CONTAINER}/Documents/stability/last_crash_scenario.txt"
                if [[ -f "$LAST_CRASH_FILE" ]]; then
                    STATE_JSON=$(cat "$LAST_CRASH_FILE" 2>/dev/null || echo "")
                fi
            fi
            # Fallback: read last JSONL entry
            if [[ -z "$STATE_JSON" ]]; then
                local LOG_FILE="${CONTAINER}/Documents/stability/stability_log.jsonl"
                if [[ -f "$LOG_FILE" ]]; then
                    STATE_JSON=$(tail -5 "$LOG_FILE" 2>/dev/null | grep '"scenario"' | tail -1 || echo "")
                fi
            fi
        fi
    fi
    # Extract scenario from JSON
    if [[ -n "$STATE_JSON" ]]; then
        local SCENARIO_VAL
        SCENARIO_VAL=$(echo "$STATE_JSON" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
        if [[ -n "$SCENARIO_VAL" ]]; then
            echo "$SCENARIO_VAL"
            return
        fi
    fi
    echo "unknown"
}

read_done_marker() {
    if [[ "$USE_SIMULATOR" == true ]]; then
        local CONTAINER_PATH
        CONTAINER_PATH=$(xcrun simctl get_app_container "$DEVICE_ID" "$BUNDLE_ID" data 2>/dev/null || echo "")
        if [[ -n "$CONTAINER_PATH" ]]; then
            local DONE_FILE="${CONTAINER_PATH}/Documents/stability/stability_done.txt"
            if [[ -f "$DONE_FILE" ]]; then
                cat "$DONE_FILE" 2>/dev/null | tr -d '\r'
                return
            fi
        fi
    fi
    echo ""
}

# === Main monitoring loop ===

echo "[Monitor] Monitoring ${BUNDLE_ID} for ${DURATION_MIN} minutes..."
echo "[Monitor] Scenario: ${SCENARIO}, Interval: ${INTERVAL_MS}ms, Crash threshold: ${CRASH_THRESHOLD}"
echo "[Monitor] Check interval: ${CHECK_INTERVAL}s, Simulator: ${USE_SIMULATOR}"
echo "[Monitor] Device ID: ${DEVICE_ID}"
echo "[Monitor] Crash logs: ${CRASH_DIR}"

while true; do
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))

    # Check if duration exceeded
    if [[ $ELAPSED -ge $DURATION_SEC ]]; then
        echo "[Monitor] Duration limit reached (${DURATION_MIN} min). Stopping."
        break
    fi

    # Check process alive
    if ! is_app_running; then
        # Retry once after short delay to avoid false positives
        sleep 2
        if ! is_app_running; then
            DONE_CONTENT=""
            DONE_CONTENT=$(read_done_marker)
            if [[ -n "$DONE_CONTENT" ]]; then
                echo "[Monitor] App completed gracefully before process exit: ${DONE_CONTENT}"
                echo "[Monitor] Test completed by app (all scenarios exhausted/blacklisted)."
                break
            fi

            # Also check heartbeat file - if updated within last 15s, app is still alive
            HEARTBEAT_FRESH=false
            if [[ "$USE_SIMULATOR" == true ]]; then
                CONTAINER_PATH=$(xcrun simctl get_app_container "$DEVICE_ID" "$BUNDLE_ID" data 2>/dev/null || echo "")
                if [[ -n "$CONTAINER_PATH" ]]; then
                    HB_LOG_FILE="${CONTAINER_PATH}/Documents/stability/stability_log.jsonl"
                    if [[ -f "$HB_LOG_FILE" ]]; then
                        LOG_MTIME=$(stat -f %m "$HB_LOG_FILE" 2>/dev/null || echo "0")
                        FILE_AGE=$(( $(date +%s) - LOG_MTIME ))
                        if [[ $FILE_AGE -lt 15 ]]; then
                            HEARTBEAT_FRESH=true
                        fi
                    fi
                fi
            fi

            if [[ "$HEARTBEAT_FRESH" == true ]]; then
                echo "[Monitor] Process check failed but heartbeat is fresh, skipping false positive"
                sleep $CHECK_INTERVAL
                continue
            fi

            # Verify device is reachable
            if ! is_device_reachable; then
                echo "[Monitor] Device unreachable, waiting for reconnection..."
                sleep $CHECK_INTERVAL
                continue
            fi

            CRASH_COUNT=$((CRASH_COUNT + 1))
            CRASH_TS=$(date +%Y%m%d_%H%M%S)
            echo "[Monitor] CRASH DETECTED at ${CRASH_TS} (crash #${CRASH_COUNT})"

            # === Phase 1: Collect crash data BEFORE restarting ===
            CRASH_FILE="${CRASH_DIR}/crash_${CRASH_COUNT}_${CRASH_TS}.txt"
            collect_crash_log "$CRASH_FILE"

            # === Phase 2: Restart app and read crash attribution ===
            CRASH_SCENARIO="unknown"
            REMAINING=$((DURATION_SEC - ELAPSED))
            if [[ $REMAINING -gt 60 ]]; then
                echo "[Monitor] Restarting app (${REMAINING}s remaining)..."
                restart_app "$REMAINING" "crash"
                # After restart, try to read attribution
                sleep 3
                CRASH_SCENARIO=$(read_crash_scenario)
            else
                # Not enough time — try reading scenario from state file
                CRASH_SCENARIO=$(read_crash_scenario)
                echo "[Monitor] Less than 60s remaining, not restarting."
            fi

            # === Phase 3: Backfill attribution ===
            sed "s/Crashed scenario: (pending attribution)/Crashed scenario: ${CRASH_SCENARIO}/" "$CRASH_FILE" > "${CRASH_FILE}.tmp" \
                && mv "${CRASH_FILE}.tmp" "$CRASH_FILE"

            CRASH_SCENARIOS+=("$CRASH_SCENARIO")
            echo "[Monitor] Crashed scenario: ${CRASH_SCENARIO}"

            if [[ $REMAINING -le 60 ]]; then
                break
            fi
        fi
    else
        # Process is alive — check for graceful completion before freeze detection
        DONE_CONTENT=""
        DONE_CONTENT=$(read_done_marker)
        if [[ -n "$DONE_CONTENT" ]]; then
            echo "[Monitor] App completed gracefully while process is still alive: ${DONE_CONTENT}"
            echo "[Monitor] Test completed by app (all scenarios exhausted/blacklisted)."
            break
        fi

        # Process is alive — check for freeze (heartbeat detection)
        SINCE_RESTART=$((CURRENT_TIME - LAST_RESTART_TIME))
        if [[ $ELAPSED -gt $FREEZE_TIMEOUT && $SINCE_RESTART -gt $FREEZE_TIMEOUT ]]; then
            HEARTBEAT_AGE=$(get_heartbeat_age)
            if [[ "$HEARTBEAT_AGE" != "-1" && $HEARTBEAT_AGE -gt $FREEZE_TIMEOUT ]]; then
                FREEZE_COUNT=$((FREEZE_COUNT + 1))
                FREEZE_TS=$(date +%Y%m%d_%H%M%S)
                echo "[Monitor] FREEZE DETECTED at ${FREEZE_TS} (freeze #${FREEZE_COUNT}, no heartbeat for ${HEARTBEAT_AGE}s)"

                # Read scenario
                FREEZE_SCENARIO=$(read_crash_scenario)
                FREEZE_SCENARIOS+=("$FREEZE_SCENARIO")
                echo "[Monitor] Frozen scenario: ${FREEZE_SCENARIO}"

                # Collect freeze diagnostics
                FREEZE_FILE="${CRASH_DIR}/freeze_${FREEZE_COUNT}_${FREEZE_TS}.txt"
                {
                    echo "--- Freeze #${FREEZE_COUNT} at ${FREEZE_TS} ---"
                    echo "Device time: $(date '+%Y-%m-%d %H:%M:%S')"
                    echo "Elapsed: ${ELAPSED}s"
                    echo "Heartbeat age: ${HEARTBEAT_AGE}s (timeout: ${FREEZE_TIMEOUT}s)"
                    echo "Frozen scenario: ${FREEZE_SCENARIO}"
                    echo ""
                    echo "=== SYSTEM LOG (last 1000 lines) ==="
                } > "$FREEZE_FILE"

                if [[ "$USE_SIMULATOR" == true ]]; then
                    xcrun simctl spawn "$DEVICE_ID" log show \
                        --predicate "subsystem == 'com.amap.agenui'" \
                        --last 3m --style compact 2>/dev/null | tail -1000 >> "$FREEZE_FILE" 2>/dev/null || true

                    # Thread dump via spindump
                    echo "" >> "$FREEZE_FILE"
                    echo "=== SPINDUMP ===" >> "$FREEZE_FILE"
                    APP_PID=$(xcrun simctl spawn "$DEVICE_ID" /bin/sh -c "pgrep -f Playground" 2>/dev/null | head -1 || echo "")
                    if [[ -n "$APP_PID" ]]; then
                        xcrun simctl spawn "$DEVICE_ID" sample "$APP_PID" 1 2>/dev/null | tail -100 >> "$FREEZE_FILE" 2>/dev/null || \
                            echo "(sample not available)" >> "$FREEZE_FILE"
                    fi
                else
                    if command -v idevicesyslog &>/dev/null; then
                        timeout 5 idevicesyslog -u "$DEVICE_ID" 2>/dev/null | tail -1000 >> "$FREEZE_FILE" || true
                    fi
                fi

                echo "[Monitor] Freeze log saved: ${FREEZE_FILE}"

                # Force-terminate and restart
                REMAINING=$((DURATION_SEC - ELAPSED))
                if [[ $REMAINING -gt 60 ]]; then
                    echo "[Monitor] Force-stopping frozen app and restarting..."
                    restart_app "$REMAINING" "freeze"
                fi
            fi
        fi
    fi

    # Print status every 60 seconds
    if [[ $((ELAPSED % 60)) -lt $CHECK_INTERVAL ]]; then
        ELAPSED_MIN=$((ELAPSED / 60))
        echo "[Monitor] Running... ${ELAPSED_MIN}/${DURATION_MIN} min, crashes: ${CRASH_COUNT}, freezes: ${FREEZE_COUNT}"
    fi

    sleep $CHECK_INTERVAL
done

# Final status
echo ""
echo "[Monitor] ========================================"
echo "[Monitor] Monitoring Complete"
echo "[Monitor] Duration: ${DURATION_MIN} minutes"
echo "[Monitor] Crashes detected: ${CRASH_COUNT}"
echo "[Monitor] Freezes detected: ${FREEZE_COUNT}"
if [[ ${#CRASH_SCENARIOS[@]} -gt 0 ]]; then
    echo "[Monitor] Crash scenarios: ${CRASH_SCENARIOS[*]}"
fi
if [[ ${#FREEZE_SCENARIOS[@]} -gt 0 ]]; then
    echo "[Monitor] Freeze scenarios: ${FREEZE_SCENARIOS[*]}"
fi
echo "[Monitor] ========================================"

# Build JSON arrays
CRASH_SCENARIOS_JSON="[]"
if [[ ${#CRASH_SCENARIOS[@]} -gt 0 ]]; then
    CRASH_SCENARIOS_JSON="["
    for i in "${!CRASH_SCENARIOS[@]}"; do
        [[ $i -gt 0 ]] && CRASH_SCENARIOS_JSON+=","
        CRASH_SCENARIOS_JSON+="\"${CRASH_SCENARIOS[$i]}\""
    done
    CRASH_SCENARIOS_JSON+="]"
fi

FREEZE_SCENARIOS_JSON="[]"
if [[ ${#FREEZE_SCENARIOS[@]} -gt 0 ]]; then
    FREEZE_SCENARIOS_JSON="["
    for i in "${!FREEZE_SCENARIOS[@]}"; do
        [[ $i -gt 0 ]] && FREEZE_SCENARIOS_JSON+=","
        FREEZE_SCENARIOS_JSON+="\"${FREEZE_SCENARIOS[$i]}\""
    done
    FREEZE_SCENARIOS_JSON+="]"
fi

# Write monitor summary
cat > "${OUTPUT_DIR}/monitor_summary.json" <<EOF
{
  "duration_minutes": ${DURATION_MIN},
  "crash_count": ${CRASH_COUNT},
  "freeze_count": ${FREEZE_COUNT},
  "crash_threshold": ${CRASH_THRESHOLD},
  "freeze_timeout_sec": ${FREEZE_TIMEOUT},
  "crash_scenarios": ${CRASH_SCENARIOS_JSON},
  "freeze_scenarios": ${FREEZE_SCENARIOS_JSON},
  "end_time": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

if [[ $CRASH_COUNT -gt 0 || $FREEZE_COUNT -gt 0 ]]; then
    exit 1
fi
