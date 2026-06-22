#!/bin/bash
set -euo pipefail

# Android stability test results collector
# Pulls log files from device and generates summary

PACKAGE="com.amap.agenuiplayground"
OUTPUT_DIR="."

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output-dir)  OUTPUT_DIR="$2"; shift 2 ;;
        *)             shift ;;
    esac
done

echo "[Collect] Collecting stability test results..."

# Stop the app gracefully
adb shell am force-stop "$PACKAGE" 2>/dev/null || true
sleep 2

# Pull log file from device
DEVICE_LOG_DIR="/sdcard/Android/data/${PACKAGE}/files/stability"
echo "[Collect] Pulling logs from ${DEVICE_LOG_DIR}..."

if adb shell "test -d ${DEVICE_LOG_DIR}" 2>/dev/null; then
    adb pull "${DEVICE_LOG_DIR}/stability_log.jsonl" "${OUTPUT_DIR}/stability_log.jsonl" 2>/dev/null || true
    echo "[Collect] Log file pulled successfully"
else
    echo "[Collect] Warning: Log directory not found on device"
    # Try alternative path
    ALT_DIR="/data/data/${PACKAGE}/files/stability"
    if adb shell "run-as ${PACKAGE} test -d files/stability" 2>/dev/null; then
        adb shell "run-as ${PACKAGE} cat files/stability/stability_log.jsonl" > "${OUTPUT_DIR}/stability_log.jsonl" 2>/dev/null || true
        echo "[Collect] Log pulled via run-as"
    fi
fi

# Analyze results
if [[ -f "${OUTPUT_DIR}/stability_log.jsonl" ]]; then
    TOTAL_ROUNDS=$(grep -c '"round"' "${OUTPUT_DIR}/stability_log.jsonl" || echo "0")
    ERROR_COUNT=$(grep -c '"status":"error"' "${OUTPUT_DIR}/stability_log.jsonl" || echo "0")
    OK_COUNT=$(grep -c '"status":"ok"' "${OUTPUT_DIR}/stability_log.jsonl" || echo "0")

    echo "[Collect] Results:"
    echo "[Collect]   Total rounds: ${TOTAL_ROUNDS}"
    echo "[Collect]   Successful:   ${OK_COUNT}"
    echo "[Collect]   Errors:       ${ERROR_COUNT}"

    # Extract memory trend (first and last entries)
    FIRST_MEM=$(head -5 "${OUTPUT_DIR}/stability_log.jsonl" | grep "memory_total_mb" | head -1 | grep -o '"memory_total_mb":[0-9.]*' | cut -d: -f2 || echo "N/A")
    LAST_MEM=$(tail -5 "${OUTPUT_DIR}/stability_log.jsonl" | grep "memory_total_mb" | tail -1 | grep -o '"memory_total_mb":[0-9.]*' | cut -d: -f2 || echo "N/A")
    echo "[Collect]   Memory start: ${FIRST_MEM} MB"
    echo "[Collect]   Memory end:   ${LAST_MEM} MB"

    # Write collection summary
    cat > "${OUTPUT_DIR}/collection_summary.json" <<EOF
{
  "total_rounds": ${TOTAL_ROUNDS},
  "ok_count": ${OK_COUNT},
  "error_count": ${ERROR_COUNT},
  "memory_start_mb": "${FIRST_MEM}",
  "memory_end_mb": "${LAST_MEM}",
  "collected_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
else
    echo "[Collect] Warning: No log file found"
fi

# Also pull final logcat
echo "[Collect] Pulling final logcat..."
adb logcat -d -t 1000 > "${OUTPUT_DIR}/final_logcat.txt" 2>/dev/null || true

echo "[Collect] Collection complete: ${OUTPUT_DIR}"
