#!/bin/bash
set -euo pipefail

# HarmonyOS stability test monitor
# Monitors process health, detects crashes and freezes, auto-restarts if needed
# Uses hdc (HarmonyOS Device Connector) — the HarmonyOS equivalent of adb
#
# Key difference from Android: HarmonyOS may auto-restart crashed processes,
# so we track PID changes and faultlog entries in addition to pidof absence.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUNDLE_NAME="com.harmony.agenui"
ABILITY_NAME="StabilityTestAbility"
MODULE_NAME="entry"

# Device file paths (HarmonyOS sandbox)
# HarmonyOS Stage model: /data/app/el2/100/base/{bundleName}/haps/{moduleName}/files/
DEVICE_FILES_DIR="/data/app/el2/100/base/${BUNDLE_NAME}/haps/${MODULE_NAME}/files/stability"
DEVICE_STATE_FILE="${DEVICE_FILES_DIR}/crash_state.json"
DEVICE_LAST_CRASH_FILE="${DEVICE_FILES_DIR}/last_crash_scenario.txt"
DEVICE_LOG_FILE="${DEVICE_FILES_DIR}/stability_log.jsonl"
DEVICE_DONE_FILE="${DEVICE_FILES_DIR}/stability_done.txt"

# Freeze detection timeout
FREEZE_TIMEOUT=30

# hdc command timeout (seconds) — prevents script from hanging if device is busy
HDC_TIMEOUT=5
# Longer timeout for log dump commands (file recv may have large payload)
HDC_LOG_TIMEOUT=8

# Portable timeout function using SIGKILL (cannot be caught/ignored)
# Works reliably on macOS where 'timeout' is not available and hdc ignores SIGALRM
if command -v timeout &>/dev/null; then
    _timeout() { timeout --signal=KILL "$@"; }
elif command -v gtimeout &>/dev/null; then
    _timeout() { gtimeout --signal=KILL "$@"; }
else
    _timeout() {
        local secs=$1; shift
        "$@" &
        local cmd_pid=$!
        ( sleep "$secs" && kill -9 "$cmd_pid" 2>/dev/null ) > /dev/null 2>&1 &
        disown $!
        wait "$cmd_pid" 2>/dev/null
        return $?
    }
fi

# Defaults
DURATION_MIN=480
OUTPUT_DIR="."
CHECK_INTERVAL=5
SCENARIO="all_combined"
ROUNDS=0
INTERVAL_MS=100
CRASH_THRESHOLD=5

while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration)         DURATION_MIN="$2"; shift 2 ;;
        --output-dir)       OUTPUT_DIR="$2"; shift 2 ;;
        --scenario)         SCENARIO="$2"; shift 2 ;;
        --rounds)           ROUNDS="$2"; shift 2 ;;
        --interval)         INTERVAL_MS="$2"; shift 2 ;;
        --crash-threshold)  CRASH_THRESHOLD="$2"; shift 2 ;;
        *)                  shift ;;
    esac
done

DURATION_SEC=$((DURATION_MIN * 60))
CRASH_DIR="${OUTPUT_DIR}/crashes"
mkdir -p "$CRASH_DIR"

START_TIME=$(date +%s)
CRASH_COUNT=0
FREEZE_COUNT=0
CRASH_SCENARIOS=()
FREEZE_SCENARIOS=()
LAST_RESTART_TIME=$START_TIME

# --- Helper: run hdc with timeout to avoid hanging ---
hdc_cmd() {
    _timeout "$HDC_TIMEOUT" hdc "$@" 2>/dev/null || true
}

# --- PID tracking: detect crash-restart cycles ---
# Get the initial PID of the app (set by launch.sh starting it)
EXPECTED_PID=$(_timeout "$HDC_TIMEOUT" hdc shell pidof "$BUNDLE_NAME" 2>/dev/null | tr -d '\r\n' || echo "")
LAST_FAULTLOG_COUNT=$(_timeout "$HDC_TIMEOUT" hdc shell "ls /data/log/faultlog/faultlogger/ 2>/dev/null | wc -l" 2>/dev/null | tr -d '\r\n' || echo "0")

# Track the latest known faultlog filename for our bundle (used to identify NEW crash logs)
# Use ls -t (sort by modification time) NOT sort -r (lexicographic), because
# "jscrash-" > "cppcrash-" alphabetically, which breaks chronological ordering.
LAST_KNOWN_FAULTLOG=$(_timeout "$HDC_TIMEOUT" hdc shell "ls -t /data/log/faultlog/faultlogger/ 2>/dev/null | grep '${BUNDLE_NAME}' | head -1" 2>/dev/null | tr -d '\r\n' || echo "")

echo "[Monitor] Monitoring ${BUNDLE_NAME} for ${DURATION_MIN} minutes..."
echo "[Monitor] Scenario: ${SCENARIO}, Interval: ${INTERVAL_MS}ms, Crash threshold: ${CRASH_THRESHOLD}"
echo "[Monitor] Check interval: ${CHECK_INTERVAL}s, hdc timeout: ${HDC_TIMEOUT}s"
echo "[Monitor] Initial PID: ${EXPECTED_PID:-none}"
echo "[Monitor] Crash logs: ${CRASH_DIR}"

while true; do
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))

    # Check if duration exceeded
    if [[ $ELAPSED -ge $DURATION_SEC ]]; then
        echo "[Monitor] Duration limit reached (${DURATION_MIN} min). Stopping."
        break
    fi

    # Check graceful exit marker FIRST (regardless of process state)
    # In HarmonyOS, terminateSelf() kills the Ability but the process may survive
    DONE_CONTENT=$(_timeout "$HDC_TIMEOUT" hdc shell "cat '${DEVICE_DONE_FILE}' 2>/dev/null" 2>/dev/null | tr -d '\r' || echo "")
    if [[ -n "$DONE_CONTENT" ]]; then
        echo "[Monitor] App exited gracefully: ${DONE_CONTENT}"
        break
    fi

    # --- Crash detection: PID-based + faultlog-based ---
    CURRENT_PID=$(_timeout "$HDC_TIMEOUT" hdc shell pidof "$BUNDLE_NAME" 2>/dev/null | tr -d '\r\n' || echo "")
    CRASH_DETECTED=false
    CRASH_REASON=""

    if [[ -z "$CURRENT_PID" ]]; then
        # Case 1: Process gone entirely
        # Verify device is reachable (distinguish crash from device disconnect)
        if ! _timeout "$HDC_TIMEOUT" hdc shell echo ok > /dev/null 2>&1; then
            echo "[Monitor] Device unreachable, waiting for reconnection..."
            sleep $CHECK_INTERVAL
            continue
        fi
        CRASH_DETECTED=true
        CRASH_REASON="process_gone"
    elif [[ -n "$EXPECTED_PID" && "$CURRENT_PID" != "$EXPECTED_PID" ]]; then
        # Case 2: PID changed — process was restarted (crash + auto-restart by system)
        CRASH_DETECTED=true
        CRASH_REASON="pid_changed(${EXPECTED_PID}->${CURRENT_PID})"
    fi

    # Case 3: Check for new faultlog entries (detects crashes even if PID stays/returns quickly)
    if [[ "$CRASH_DETECTED" == false ]]; then
        CURRENT_FAULTLOG_COUNT=$(_timeout "$HDC_TIMEOUT" hdc shell "ls /data/log/faultlog/faultlogger/ 2>/dev/null | wc -l" 2>/dev/null | tr -d '\r\n' || echo "0")
        if [[ "$CURRENT_FAULTLOG_COUNT" -gt "$LAST_FAULTLOG_COUNT" ]]; then
            # Verify this faultlog belongs to our app
            LATEST_FAULTLOG=$(_timeout "$HDC_TIMEOUT" hdc shell "ls -t /data/log/faultlog/faultlogger/ 2>/dev/null | head -1" 2>/dev/null | tr -d '\r\n' || echo "")
            if [[ -n "$LATEST_FAULTLOG" ]]; then
                FAULTLOG_OWNER=$(_timeout "$HDC_TIMEOUT" hdc shell "head -20 /data/log/faultlog/faultlogger/${LATEST_FAULTLOG} 2>/dev/null | grep -i '${BUNDLE_NAME}'" 2>/dev/null | tr -d '\r' || echo "")
                if [[ -n "$FAULTLOG_OWNER" ]]; then
                    CRASH_DETECTED=true
                    CRASH_REASON="new_faultlog(${LATEST_FAULTLOG})"
                fi
            fi
            LAST_FAULTLOG_COUNT="$CURRENT_FAULTLOG_COUNT"
        fi
    fi

    if [[ "$CRASH_DETECTED" == true ]]; then
        CRASH_COUNT=$((CRASH_COUNT + 1))
        CRASH_TS=$(date +%Y%m%d_%H%M%S)
        echo "[Monitor] CRASH DETECTED at ${CRASH_TS} (crash #${CRASH_COUNT}, reason: ${CRASH_REASON})"

        # === Phase 1: Restart FIRST (minimize downtime) ===
        # Force-stop any auto-restarted instance
        _timeout "$HDC_TIMEOUT" hdc shell aa force-stop "$BUNDLE_NAME" 2>/dev/null || true
        sleep 1

        CRASH_SCENARIO="unknown"
        REMAINING=$((DURATION_SEC - ELAPSED))
        if [[ $REMAINING -gt 60 ]]; then
            echo "[Monitor] Restarting app (${REMAINING}s remaining)..."
            _timeout "$HDC_TIMEOUT" hdc shell aa start \
                -a "$ABILITY_NAME" \
                -b "$BUNDLE_NAME" \
                -m "$MODULE_NAME" \
                --ps scenario "$SCENARIO" \
                --pi duration_minutes "$((REMAINING / 60))" \
                --pi rounds "$ROUNDS" \
                --pi interval_ms "$INTERVAL_MS" \
                --pi crash_threshold "$CRASH_THRESHOLD" \
                --ps restart_reason "crash" 2>/dev/null || true
            sleep 3
            LAST_RESTART_TIME=$(date +%s)

            # Update expected PID to the new process
            EXPECTED_PID=$(_timeout "$HDC_TIMEOUT" hdc shell pidof "$BUNDLE_NAME" 2>/dev/null | tr -d '\r\n' || echo "")
            echo "[Monitor] New PID after restart: ${EXPECTED_PID:-none}"
        else
            echo "[Monitor] Less than 60s remaining, not restarting."
        fi

        # === Phase 2: Collect crash data (best-effort, short timeouts) ===
        echo "[Monitor] Collecting crash data..."
        CRASH_FILE="${CRASH_DIR}/crash_${CRASH_COUNT}_${CRASH_TS}.txt"
        {
            echo "--- Crash #${CRASH_COUNT} at ${CRASH_TS} ---"
            echo "Host time: $(date '+%Y-%m-%d %H:%M:%S')"
            echo "Elapsed: ${ELAPSED}s"
            echo "Detection reason: ${CRASH_REASON}"
            echo "Expected PID: ${EXPECTED_PID:-}, Current PID: ${CURRENT_PID:-}"
            echo "Crashed scenario: (pending attribution)"
            echo ""
        } > "$CRASH_FILE"

        # Wait for system to finish writing the faultlog file, then poll with retries.
        # HarmonyOS faultlog generation is asynchronous; may take several seconds after crash.
        CURRENT_FAULTLOG=""
        for RETRY in 1 2 3 4 5; do
            sleep 2
            # Use ls -t (modification time) NOT sort -r (lexicographic breaks on jscrash vs cppcrash prefix)
            CURRENT_FAULTLOG=$(_timeout "$HDC_TIMEOUT" hdc shell "ls -t /data/log/faultlog/faultlogger/ 2>/dev/null | grep '${BUNDLE_NAME}' | head -1" 2>/dev/null | tr -d '\r\n' || echo "")
            if [[ -n "$CURRENT_FAULTLOG" && "$CURRENT_FAULTLOG" != "$LAST_KNOWN_FAULTLOG" ]]; then
                echo "[Monitor] New faultlog found on retry #${RETRY}: ${CURRENT_FAULTLOG}"
                break
            fi
            CURRENT_FAULTLOG=""
            echo "[Monitor] Waiting for faultlog (attempt ${RETRY}/5, baseline: ${LAST_KNOWN_FAULTLOG:-none})..."
        done

        if [[ -n "$CURRENT_FAULTLOG" ]]; then
            # Pull only the specific new faultlog file
            echo "[Monitor] Pulling faultlog: ${CURRENT_FAULTLOG}"
            _timeout "$HDC_LOG_TIMEOUT" hdc file recv "/data/log/faultlog/faultlogger/${CURRENT_FAULTLOG}" "${CRASH_DIR}/${CURRENT_FAULTLOG}" 2>/dev/null || true

            if [[ -f "${CRASH_DIR}/${CURRENT_FAULTLOG}" && -s "${CRASH_DIR}/${CURRENT_FAULTLOG}" ]]; then
                echo "=== FAULTLOG: ${CURRENT_FAULTLOG} ===" >> "$CRASH_FILE"
                cat "${CRASH_DIR}/${CURRENT_FAULTLOG}" >> "$CRASH_FILE"
            else
                echo "(faultlog pull failed: ${CURRENT_FAULTLOG})" >> "$CRASH_FILE"
            fi

            # Update baseline to avoid re-pulling
            LAST_KNOWN_FAULTLOG="$CURRENT_FAULTLOG"
        else
            # Log diagnostic info for debugging
            ALL_FAULTLOGS=$(_timeout "$HDC_TIMEOUT" hdc shell "ls -t /data/log/faultlog/faultlogger/ 2>/dev/null | grep '${BUNDLE_NAME}'" 2>/dev/null | tr -d '\r' || echo "(empty)")
            echo "[Monitor] No new faultlog after 5 retries. Baseline: ${LAST_KNOWN_FAULTLOG:-none}, Device has: ${ALL_FAULTLOGS}"
            echo "(no new faultlog found for ${BUNDLE_NAME}, baseline=${LAST_KNOWN_FAULTLOG:-none})" >> "$CRASH_FILE"
        fi

        echo "[Monitor] Crash log saved: ${CRASH_FILE}"

        # === Phase 3: Read crash attribution ===
        if [[ $REMAINING -gt 60 ]]; then
            # Read crash attribution from durable file (app should have written it before crash)
            LAST_CRASH_JSON=$(_timeout "$HDC_TIMEOUT" hdc shell cat "$DEVICE_LAST_CRASH_FILE" 2>/dev/null | tr -d '\r' || echo "")
            if [[ -n "$LAST_CRASH_JSON" ]]; then
                SCENARIO_VAL=$(echo "$LAST_CRASH_JSON" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                if [[ -n "$SCENARIO_VAL" ]]; then
                    CRASH_SCENARIO="$SCENARIO_VAL"
                fi
            fi
        else
            STATE_JSON=$(_timeout "$HDC_TIMEOUT" hdc shell cat "$DEVICE_STATE_FILE" 2>/dev/null | tr -d '\r' || echo "")
            if [[ -n "$STATE_JSON" ]]; then
                SUB=$(echo "$STATE_JSON" | grep -o '"sub_scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                if [[ -n "$SUB" ]]; then
                    CRASH_SCENARIO="$SUB"
                else
                    MAIN=$(echo "$STATE_JSON" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                    [[ -n "$MAIN" ]] && CRASH_SCENARIO="$MAIN"
                fi
            fi
        fi

        # Fallback: read from JSONL last entry
        if [[ "$CRASH_SCENARIO" == "unknown" ]]; then
            LAST_LINE=$(_timeout "$HDC_TIMEOUT" hdc shell "tail -5 '$DEVICE_LOG_FILE' 2>/dev/null | grep '\"scenario\"' | tail -1" 2>/dev/null | tr -d '\r' || echo "")
            if [[ -n "$LAST_LINE" ]]; then
                JSONL_SCENARIO=$(echo "$LAST_LINE" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                [[ -n "$JSONL_SCENARIO" ]] && CRASH_SCENARIO="$JSONL_SCENARIO"
            fi
        fi

        # Backfill attribution in crash file
        sed "s/Crashed scenario: (pending attribution)/Crashed scenario: ${CRASH_SCENARIO}/" "$CRASH_FILE" > "${CRASH_FILE}.tmp" \
            && mv "${CRASH_FILE}.tmp" "$CRASH_FILE"

        CRASH_SCENARIOS+=("$CRASH_SCENARIO")
        echo "[Monitor] Crashed scenario: ${CRASH_SCENARIO}"

        # Update faultlog baseline
        LAST_FAULTLOG_COUNT=$(_timeout "$HDC_TIMEOUT" hdc shell "ls /data/log/faultlog/faultlogger/ 2>/dev/null | wc -l" 2>/dev/null | tr -d '\r\n' || echo "0")

        if [[ $REMAINING -le 60 ]]; then
            break
        fi
    else
        # Process is alive with same PID — check for freeze (heartbeat detection)
        SINCE_RESTART=$((CURRENT_TIME - LAST_RESTART_TIME))
        if [[ $ELAPSED -gt $FREEZE_TIMEOUT && $SINCE_RESTART -gt $FREEZE_TIMEOUT ]]; then
            # stat -c %Y works on HarmonyOS (Linux kernel based)
            FILE_MTIME=$(_timeout "$HDC_TIMEOUT" hdc shell "stat -c %Y '$DEVICE_LOG_FILE' 2>/dev/null || echo 0" 2>/dev/null | tr -d '\r' || echo "0")
            DEVICE_NOW=$(_timeout "$HDC_TIMEOUT" hdc shell "date +%s" 2>/dev/null | tr -d '\r' || echo "0")

            if [[ "$FILE_MTIME" != "0" && "$DEVICE_NOW" != "0" ]]; then
                HEARTBEAT_AGE=$((DEVICE_NOW - FILE_MTIME))
                if [[ $HEARTBEAT_AGE -gt $FREEZE_TIMEOUT ]]; then
                    FREEZE_COUNT=$((FREEZE_COUNT + 1))
                    FREEZE_TS=$(date +%Y%m%d_%H%M%S)
                    echo "[Monitor] FREEZE DETECTED at ${FREEZE_TS} (freeze #${FREEZE_COUNT}, no heartbeat for ${HEARTBEAT_AGE}s)"

                    # Read scenario
                    FREEZE_SCENARIO="unknown"
                    STATE_JSON=$(_timeout "$HDC_TIMEOUT" hdc shell cat "$DEVICE_STATE_FILE" 2>/dev/null | tr -d '\r' || echo "")
                    if [[ -n "$STATE_JSON" ]]; then
                        SUB=$(echo "$STATE_JSON" | grep -o '"sub_scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                        if [[ -n "$SUB" ]]; then
                            FREEZE_SCENARIO="$SUB"
                        else
                            MAIN=$(echo "$STATE_JSON" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                            [[ -n "$MAIN" ]] && FREEZE_SCENARIO="$MAIN"
                        fi
                    fi
                    # Fallback: JSONL last entry
                    if [[ "$FREEZE_SCENARIO" == "unknown" ]]; then
                        LAST_LINE=$(_timeout "$HDC_TIMEOUT" hdc shell "tail -5 '$DEVICE_LOG_FILE' 2>/dev/null | grep '\"scenario\"' | tail -1" 2>/dev/null | tr -d '\r' || echo "")
                        if [[ -n "$LAST_LINE" ]]; then
                            JSONL_SCENARIO=$(echo "$LAST_LINE" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                            [[ -n "$JSONL_SCENARIO" ]] && FREEZE_SCENARIO="$JSONL_SCENARIO"
                        fi
                    fi

                    FREEZE_SCENARIOS+=("$FREEZE_SCENARIO")
                    echo "[Monitor] Frozen scenario: ${FREEZE_SCENARIO}"

                    # Pull the latest appfreeze log for our bundle (specific file, not whole directory)
                    FREEZE_FILE="${CRASH_DIR}/freeze_${FREEZE_COUNT}_${FREEZE_TS}.txt"
                    {
                        echo "--- Freeze #${FREEZE_COUNT} at ${FREEZE_TS} ---"
                        echo "Host time: $(date '+%Y-%m-%d %H:%M:%S')"
                        echo "Elapsed: ${ELAPSED}s"
                        echo "Heartbeat age: ${HEARTBEAT_AGE}s (timeout: ${FREEZE_TIMEOUT}s)"
                        echo "Frozen scenario: ${FREEZE_SCENARIO}"
                        echo ""
                    } > "$FREEZE_FILE"

                    # Find the latest appfreeze file for our bundle on device
                    LATEST_APPFREEZE=$(_timeout "$HDC_TIMEOUT" hdc shell "ls /data/log/faultlog/faultlogger/ 2>/dev/null | grep 'appfreeze.*${BUNDLE_NAME}' | sort -r | head -1" 2>/dev/null | tr -d '\r\n' || echo "")
                    if [[ -n "$LATEST_APPFREEZE" ]]; then
                        _timeout "$HDC_LOG_TIMEOUT" hdc file recv "/data/log/faultlog/faultlogger/${LATEST_APPFREEZE}" "${CRASH_DIR}/${LATEST_APPFREEZE}" 2>/dev/null || true
                        if [[ -f "${CRASH_DIR}/${LATEST_APPFREEZE}" && -s "${CRASH_DIR}/${LATEST_APPFREEZE}" ]]; then
                            echo "=== FAULTLOG: ${LATEST_APPFREEZE} ===" >> "$FREEZE_FILE"
                            cat "${CRASH_DIR}/${LATEST_APPFREEZE}" >> "$FREEZE_FILE"
                        fi
                    fi

                    echo "[Monitor] Freeze log saved: ${FREEZE_FILE}"

                    # Force-stop and restart
                    REMAINING=$((DURATION_SEC - ELAPSED))
                    if [[ $REMAINING -gt 60 ]]; then
                        echo "[Monitor] Force-stopping frozen app and restarting..."
                        _timeout "$HDC_TIMEOUT" hdc shell aa force-stop "$BUNDLE_NAME" 2>/dev/null || true
                        sleep 2
                        _timeout "$HDC_TIMEOUT" hdc shell aa start \
                            -a "$ABILITY_NAME" \
                            -b "$BUNDLE_NAME" \
                            -m "$MODULE_NAME" \
                            --ps scenario "$SCENARIO" \
                            --pi duration_minutes "$((REMAINING / 60))" \
                            --pi rounds "$ROUNDS" \
                            --pi interval_ms "$INTERVAL_MS" \
                            --pi crash_threshold "$CRASH_THRESHOLD" \
                            --ps restart_reason "freeze" 2>/dev/null || true
                        sleep 5
                        LAST_RESTART_TIME=$(date +%s)
                        EXPECTED_PID=$(_timeout "$HDC_TIMEOUT" hdc shell pidof "$BUNDLE_NAME" 2>/dev/null | tr -d '\r\n' || echo "")
                    fi
                fi
            fi
        fi
    fi

    # Print status every check interval
    if [[ $((ELAPSED % 30)) -lt $CHECK_INTERVAL ]]; then
        ELAPSED_MIN=$((ELAPSED / 60))
        echo "[Monitor] Running... ${ELAPSED_MIN}/${DURATION_MIN} min, crashes: ${CRASH_COUNT}, freezes: ${FREEZE_COUNT}, pid: ${CURRENT_PID:-gone}"
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
