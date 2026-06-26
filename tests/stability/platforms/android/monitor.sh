#!/bin/bash
set -euo pipefail

# Android stability test monitor
# Monitors process health, detects crashes, auto-restarts if needed
# Supports crash attribution via crash_state.json on device

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PACKAGE="com.amap.agenuiplayground"
DEVICE_STATE_FILE="/sdcard/Android/data/${PACKAGE}/files/stability/crash_state.json"
# last_crash_scenario.txt is written by the app's onProcessStart() after it detects a crash.
# Unlike crash_state.json (which gets deleted by onProcessStart), this file persists and is
# only overwritten — never deleted. This avoids the race condition where the app auto-restarts
# and deletes crash_state.json before monitor.sh's check interval detects the crash.
DEVICE_LAST_CRASH_FILE="/sdcard/Android/data/${PACKAGE}/files/stability/last_crash_scenario.txt"
DEVICE_LOG_FILE="/sdcard/Android/data/${PACKAGE}/files/stability/stability_log.jsonl"
DEVICE_DONE_FILE="/sdcard/Android/data/${PACKAGE}/files/stability/stability_done.txt"

# Freeze detection: if the JSONL log file hasn't been updated in FREEZE_TIMEOUT seconds,
# the app is considered frozen (ANR/deadlock). Uses file modification time comparison.
FREEZE_TIMEOUT=30

# Defaults
DURATION_MIN=480
OUTPUT_DIR="."
# Polling interval is intentionally short: at the previous 10s value, the
# logcat ringbuffer on chatty emulators (Wi-Fi/Skia spam) could roll over
# the crash signature before we got a chance to dump it.
CHECK_INTERVAL=2  # seconds
SCENARIO="all_combined"
INTERVAL_MS=100
CRASH_THRESHOLD=5

while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration)         DURATION_MIN="$2"; shift 2 ;;
        --output-dir)       OUTPUT_DIR="$2"; shift 2 ;;
        --scenario)         SCENARIO="$2"; shift 2 ;;
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
# Track last restart time to suppress false freeze detections after restart
LAST_RESTART_TIME=$START_TIME

echo "[Monitor] Monitoring ${PACKAGE} for ${DURATION_MIN} minutes..."
echo "[Monitor] Scenario: ${SCENARIO}, Interval: ${INTERVAL_MS}ms, Crash threshold: ${CRASH_THRESHOLD}"
echo "[Monitor] Check interval: ${CHECK_INTERVAL}s"
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
    if ! adb shell pidof "$PACKAGE" > /dev/null 2>&1; then
        # Verify device is actually reachable (distinguish real crash from device disconnect)
        if ! adb shell echo ok > /dev/null 2>&1; then
            echo "[Monitor] Device unreachable, waiting for reconnection..."
            sleep $CHECK_INTERVAL
            continue
        fi

        # Check if the app exited gracefully (wrote stability_done.txt)
        DONE_CONTENT=$(adb shell cat "$DEVICE_DONE_FILE" 2>/dev/null | tr -d '\r' || echo "")
        if [[ -n "$DONE_CONTENT" ]]; then
            echo "[Monitor] App exited gracefully: ${DONE_CONTENT}"
            echo "[Monitor] Test completed by app (all scenarios exhausted/blacklisted)."
            break
        fi

        CRASH_COUNT=$((CRASH_COUNT + 1))
        CRASH_TS=$(date +%Y%m%d_%H%M%S)
        echo "[Monitor] CRASH DETECTED at ${CRASH_TS} (crash #${CRASH_COUNT})"

        # === Phase 1: Immediately collect crash data (BEFORE restarting app) ===
        # The logcat buffer still contains the crash backtrace at this point.
        DEVICE_TIME=$(adb shell "date '+%Y-%m-%d %H:%M:%S.%3N'" 2>/dev/null | tr -d '\r' || echo "unknown")

        CRASH_FILE="${CRASH_DIR}/crash_${CRASH_COUNT}_${CRASH_TS}.txt"
        {
            echo "--- Crash #${CRASH_COUNT} at ${CRASH_TS} ---"
            echo "Device time: ${DEVICE_TIME}"
            echo "Elapsed: ${ELAPSED}s"
            echo "Crashed scenario: (pending attribution)"
            echo ""
            echo "=== LOGCAT (crash buffer, full) ==="
        } > "$CRASH_FILE"
        # The dedicated `crash` buffer retains DEBUG/libc/AndroidRuntime FATAL
        # entries far longer than the `main` buffer, so it is the most reliable
        # source for both native and Java crash backtraces.
        adb logcat -d -b crash -v threadtime >> "$CRASH_FILE" 2>/dev/null || true

        echo "" >> "$CRASH_FILE"
        echo "=== LOGCAT (main+system, filtered for crash signatures) ===" >> "$CRASH_FILE"
        # Only keep crash-relevant tags so the noisy Wi-Fi/Skia logs don't drown
        # the real signal. Note: do NOT pass -t here — we want all retained lines.
        adb logcat -d -b main -b system -v threadtime \
            AndroidRuntime:E DEBUG:V libc:F System.err:W \
            StabilityTest:V AGenUI:V agenui:V \
            ActivityManager:E ActivityTaskManager:E *:S \
            >> "$CRASH_FILE" 2>/dev/null || true

        echo "" >> "$CRASH_FILE"
        echo "=== LOGCAT (main, last 5000 lines, unfiltered tail) ===" >> "$CRASH_FILE"
        adb logcat -d -b main -t 5000 -v threadtime >> "$CRASH_FILE" 2>/dev/null || true

        # Collect tombstone (authoritative native crash backtrace).
        # Requires root or userdebug build. launch.sh attempts `adb root` first;
        # on user-build devices this still falls through to the fallback.
        echo "" >> "$CRASH_FILE"
        echo "=== TOMBSTONE (latest) ===" >> "$CRASH_FILE"
        TOMB_NAME=$(adb shell "ls -t /data/tombstones/ 2>/dev/null | head -1" 2>/dev/null | tr -d '\r' || echo "")
        if [[ -n "$TOMB_NAME" ]]; then
            echo "tombstone: ${TOMB_NAME}" >> "$CRASH_FILE"
            adb shell "cat /data/tombstones/${TOMB_NAME}" >> "$CRASH_FILE" 2>/dev/null \
                || echo "(tombstone present but unreadable — adb root failed)" >> "$CRASH_FILE"
        else
            echo "(no tombstone access — most likely non-root user build)" >> "$CRASH_FILE"
        fi

        # Collect dropbox records. Android 14+ removed the `--tag` flag, so we
        # pass the tag positionally. We also collect Java crash and ANR tags,
        # which the previous version of this script silently dropped.
        for DB_TAG in data_app_native_crash data_app_crash system_app_crash data_app_anr system_app_anr; do
            echo "" >> "$CRASH_FILE"
            echo "=== DROPBOX (${DB_TAG}) ===" >> "$CRASH_FILE"
            adb shell "dumpsys dropbox --print ${DB_TAG} 2>/dev/null | tail -300" \
                >> "$CRASH_FILE" 2>/dev/null || echo "(no dropbox data for ${DB_TAG})" >> "$CRASH_FILE"
        done

        # ApplicationExitInfo (API 30+) — does NOT require root and surfaces
        # OOM kills, ANRs, native crashes, and Java exceptions for our package
        # even when tombstones / dropbox are inaccessible.
        echo "" >> "$CRASH_FILE"
        echo "=== ApplicationExitInfo (dumpsys activity exit-info ${PACKAGE}) ===" >> "$CRASH_FILE"
        adb shell "dumpsys activity exit-info ${PACKAGE} 2>/dev/null | tail -400" \
            >> "$CRASH_FILE" 2>/dev/null || echo "(exit-info unavailable on this Android version)" >> "$CRASH_FILE"

        echo "[Monitor] Crash log saved: ${CRASH_FILE}"

        # Clear logcat buffer AFTER the dump completed, so the next crash
        # detection starts from a clean slate.
        adb logcat -c -b crash -b main -b system 2>/dev/null || true

        # === Phase 2: Restart app and read crash attribution ===
        CRASH_SCENARIO="unknown"
        REMAINING=$((DURATION_SEC - ELAPSED))
        if [[ $REMAINING -gt 60 ]]; then
            echo "[Monitor] Restarting app (${REMAINING}s remaining)..."
            adb shell am start -n "${PACKAGE}/.stability.StabilityTestActivity" \
                --es scenario "$SCENARIO" \
                --ei duration_minutes "$((REMAINING / 60))" \
                --ei interval_ms "$INTERVAL_MS" \
                --ei crash_threshold "$CRASH_THRESHOLD" \
                --es restart_reason "crash" 2>/dev/null || true
            # Wait for app to start and onProcessStart() to write last_crash_scenario.txt
            sleep 5
            # Reset heartbeat baseline to avoid false freeze detection after restart
            LAST_RESTART_TIME=$(date +%s)

            # Read crash attribution from the durable file
            LAST_CRASH_JSON=$(adb shell cat "$DEVICE_LAST_CRASH_FILE" 2>/dev/null || echo "")
            if [[ -n "$LAST_CRASH_JSON" ]]; then
                SCENARIO_VAL=$(echo "$LAST_CRASH_JSON" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                if [[ -n "$SCENARIO_VAL" ]]; then
                    CRASH_SCENARIO="$SCENARIO_VAL"
                fi
            fi
        else
            # Not enough time to restart — try reading crash_state.json directly
            STATE_JSON=$(adb shell cat "$DEVICE_STATE_FILE" 2>/dev/null || echo "")
            if [[ -n "$STATE_JSON" ]]; then
                SUB=$(echo "$STATE_JSON" | grep -o '"sub_scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                if [[ -n "$SUB" ]]; then
                    CRASH_SCENARIO="$SUB"
                else
                    MAIN=$(echo "$STATE_JSON" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                    if [[ -n "$MAIN" ]]; then
                        CRASH_SCENARIO="$MAIN"
                    fi
                fi
            fi
            echo "[Monitor] Less than 60s remaining, not restarting."
        fi

        # === Phase 3: Backfill attribution into the crash log file ===
        # Try to parse ApplicationExitInfo to derive the real exit reason. This
        # lets us distinguish a genuine crash (REASON_CRASH / REASON_CRASH_NATIVE
        # / REASON_ANR) from a graceful self-restart (REASON_USER_REQUESTED) or
        # an LMK kill (REASON_LOW_MEMORY) — addressing the case where pidof
        # alone falsely reports "crash" for legitimate process exits.
        EXIT_REASON=$(adb shell "dumpsys activity exit-info ${PACKAGE} 2>/dev/null" \
            | grep -m1 -E "reason=[A-Z_]+|description=" | head -1 | tr -d '\r' || echo "")
        if [[ -z "$EXIT_REASON" ]]; then
            EXIT_REASON="exit_info_unavailable"
        fi
        sed -e "s|Crashed scenario: (pending attribution)|Crashed scenario: ${CRASH_SCENARIO}\nExit info: ${EXIT_REASON}|" \
            "$CRASH_FILE" > "${CRASH_FILE}.tmp" \
            && mv "${CRASH_FILE}.tmp" "$CRASH_FILE"

        CRASH_SCENARIOS+=("$CRASH_SCENARIO")
        echo "[Monitor] Crashed scenario: ${CRASH_SCENARIO}"

        # If we already broke out (less than 60s remaining), exit the loop
        if [[ $REMAINING -le 60 ]]; then
            break
        fi
    else
        # Process is alive — check for graceful completion before freeze detection
        DONE_CONTENT=$(adb shell cat "$DEVICE_DONE_FILE" 2>/dev/null | tr -d '\r' || echo "")
        if [[ -n "$DONE_CONTENT" ]]; then
            echo "[Monitor] App completed gracefully while process is still alive: ${DONE_CONTENT}"
            echo "[Monitor] Test completed by app (all scenarios exhausted/blacklisted)."
            break
        fi

        # Process is alive — check for freeze (heartbeat detection)
        # Skip freeze check during startup grace period OR after a recent restart
        SINCE_RESTART=$((CURRENT_TIME - LAST_RESTART_TIME))
        if [[ $ELAPSED -gt $FREEZE_TIMEOUT && $SINCE_RESTART -gt $FREEZE_TIMEOUT ]]; then
            # Get file modification time (epoch) and current device time (epoch)
            FILE_MTIME=$(adb shell "stat -c %Y '$DEVICE_LOG_FILE' 2>/dev/null || echo 0" 2>/dev/null | tr -d '\r' || echo "0")
            DEVICE_NOW=$(adb shell "date +%s" 2>/dev/null | tr -d '\r' || echo "0")

            if [[ "$FILE_MTIME" != "0" && "$DEVICE_NOW" != "0" ]]; then
                HEARTBEAT_AGE=$((DEVICE_NOW - FILE_MTIME))
                if [[ $HEARTBEAT_AGE -gt $FREEZE_TIMEOUT ]]; then
                    FREEZE_COUNT=$((FREEZE_COUNT + 1))
                    FREEZE_TS=$(date +%Y%m%d_%H%M%S)
                    echo "[Monitor] FREEZE DETECTED at ${FREEZE_TS} (freeze #${FREEZE_COUNT}, no heartbeat for ${HEARTBEAT_AGE}s)"

                    # Read scenario from crash_state.json or JSONL last entry
                    FREEZE_SCENARIO="unknown"
                    STATE_JSON=$(adb shell cat "$DEVICE_STATE_FILE" 2>/dev/null || echo "")
                    if [[ -n "$STATE_JSON" ]]; then
                        SUB=$(echo "$STATE_JSON" | grep -o '"sub_scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                        if [[ -n "$SUB" ]]; then
                            FREEZE_SCENARIO="$SUB"
                        else
                            MAIN=$(echo "$STATE_JSON" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                            [[ -n "$MAIN" ]] && FREEZE_SCENARIO="$MAIN"
                        fi
                    fi
                    # Fallback: read last round entry from JSONL on device
                    if [[ "$FREEZE_SCENARIO" == "unknown" ]]; then
                        LAST_LINE=$(adb shell "tail -5 '$DEVICE_LOG_FILE' 2>/dev/null | grep '\"scenario\"' | tail -1" 2>/dev/null | tr -d '\r' || echo "")
                        if [[ -n "$LAST_LINE" ]]; then
                            JSONL_SCENARIO=$(echo "$LAST_LINE" | grep -o '"scenario":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "")
                            [[ -n "$JSONL_SCENARIO" ]] && FREEZE_SCENARIO="$JSONL_SCENARIO"
                        fi
                    fi

                    FREEZE_SCENARIOS+=("$FREEZE_SCENARIO")
                    echo "[Monitor] Frozen scenario: ${FREEZE_SCENARIO}"

                    # Collect freeze diagnostics
                    DEVICE_TIME=$(adb shell "date '+%Y-%m-%d %H:%M:%S.%3N'" 2>/dev/null | tr -d '\r' || echo "unknown")
                    FREEZE_FILE="${CRASH_DIR}/freeze_${FREEZE_COUNT}_${FREEZE_TS}.txt"
                    {
                        echo "--- Freeze #${FREEZE_COUNT} at ${FREEZE_TS} ---"
                        echo "Device time: ${DEVICE_TIME}"
                        echo "Elapsed: ${ELAPSED}s"
                        echo "Heartbeat age: ${HEARTBEAT_AGE}s (timeout: ${FREEZE_TIMEOUT}s)"
                        echo "Frozen scenario: ${FREEZE_SCENARIO}"
                        echo ""
                        echo "=== ANR TRACES ==="
                    } > "$FREEZE_FILE"
                    # Collect ANR traces (main thread stack)
                    adb shell "cat /data/anr/traces.txt 2>/dev/null | tail -200" \
                        >> "$FREEZE_FILE" 2>/dev/null || echo "(no ANR traces access)" >> "$FREEZE_FILE"

                    echo "" >> "$FREEZE_FILE"
                    echo "=== THREAD DUMP (app) ===" >> "$FREEZE_FILE"
                    APP_PID=$(adb shell pidof "$PACKAGE" 2>/dev/null | tr -d '\r' || echo "")
                    if [[ -n "$APP_PID" ]]; then
                        # Send SIGQUIT to trigger thread dump, then collect
                        adb shell "kill -3 $APP_PID" 2>/dev/null || true
                        sleep 2
                        adb shell "cat /data/anr/traces.txt 2>/dev/null | tail -300" \
                            >> "$FREEZE_FILE" 2>/dev/null || echo "(no thread dump)" >> "$FREEZE_FILE"
                    fi

                    echo "" >> "$FREEZE_FILE"
                    echo "=== LOGCAT (main+system, filtered) ===" >> "$FREEZE_FILE"
                    adb logcat -d -b main -b system -v threadtime \
                        AndroidRuntime:E DEBUG:V libc:F System.err:W \
                        StabilityTest:V AGenUI:V agenui:V \
                        ActivityManager:E ActivityTaskManager:E *:S \
                        >> "$FREEZE_FILE" 2>/dev/null || true

                    echo "" >> "$FREEZE_FILE"
                    echo "=== LOGCAT (main, last 3000 lines) ===" >> "$FREEZE_FILE"
                    adb logcat -d -b main -t 3000 -v threadtime >> "$FREEZE_FILE" 2>/dev/null || true

                    echo "" >> "$FREEZE_FILE"
                    echo "=== DROPBOX (ANR / crash tags) ===" >> "$FREEZE_FILE"
                    for DB_TAG in data_app_anr system_app_anr data_app_crash data_app_native_crash; do
                        echo "--- ${DB_TAG} ---" >> "$FREEZE_FILE"
                        adb shell "dumpsys dropbox --print ${DB_TAG} 2>/dev/null | tail -200" \
                            >> "$FREEZE_FILE" 2>/dev/null || echo "(no dropbox data for ${DB_TAG})" >> "$FREEZE_FILE"
                    done

                    echo "" >> "$FREEZE_FILE"
                    echo "=== ApplicationExitInfo ===" >> "$FREEZE_FILE"
                    adb shell "dumpsys activity exit-info ${PACKAGE} 2>/dev/null | tail -400" \
                        >> "$FREEZE_FILE" 2>/dev/null || echo "(exit-info unavailable)" >> "$FREEZE_FILE"

                    echo "[Monitor] Freeze log saved: ${FREEZE_FILE}"

                    # Force-stop and restart the app
                    REMAINING=$((DURATION_SEC - ELAPSED))
                    if [[ $REMAINING -gt 60 ]]; then
                        echo "[Monitor] Force-stopping frozen app and restarting..."
                        adb shell am force-stop "$PACKAGE" 2>/dev/null || true
                        # Also stop any lingering instrumentation test apk that may
                        # have pushed StabilityTestActivity into the background.
                        adb shell am force-stop "${PACKAGE}.test" 2>/dev/null || true
                        sleep 2
                        adb shell am start -n "${PACKAGE}/.stability.StabilityTestActivity" \
                            --es scenario "$SCENARIO" \
                            --ei duration_minutes "$((REMAINING / 60))" \
                            --ei interval_ms "$INTERVAL_MS" \
                            --ei crash_threshold "$CRASH_THRESHOLD" \
                            --es restart_reason "freeze" 2>/dev/null || true
                        sleep 5
                        LAST_RESTART_TIME=$(date +%s)
                    fi
                fi
            fi
        fi
    fi

    # Print status every 60 seconds
    if [[ $((ELAPSED % 60)) -lt $CHECK_INTERVAL ]]; then
        ELAPSED_MIN=$((ELAPSED / 60))
        echo "[Monitor] Running... ${ELAPSED_MIN}/${DURATION_MIN} min, crashes: ${CRASH_COUNT}"
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

# Build crash_scenarios JSON array
CRASH_SCENARIOS_JSON="[]"
if [[ ${#CRASH_SCENARIOS[@]} -gt 0 ]]; then
    CRASH_SCENARIOS_JSON="["
    for i in "${!CRASH_SCENARIOS[@]}"; do
        if [[ $i -gt 0 ]]; then
            CRASH_SCENARIOS_JSON+=","
        fi
        CRASH_SCENARIOS_JSON+="\"${CRASH_SCENARIOS[$i]}\""
    done
    CRASH_SCENARIOS_JSON+="]"
fi

# Build freeze_scenarios JSON array
FREEZE_SCENARIOS_JSON="[]"
if [[ ${#FREEZE_SCENARIOS[@]} -gt 0 ]]; then
    FREEZE_SCENARIOS_JSON="["
    for i in "${!FREEZE_SCENARIOS[@]}"; do
        if [[ $i -gt 0 ]]; then
            FREEZE_SCENARIOS_JSON+=","
        fi
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
