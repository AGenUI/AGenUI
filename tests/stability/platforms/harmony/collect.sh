#!/bin/bash
set -euo pipefail

# HarmonyOS stability test results collector
# Pulls log files from device and generates summary
# Uses hdc (HarmonyOS Device Connector) for device communication

BUNDLE_NAME="com.harmony.agenui"
MODULE_NAME="entry"
OUTPUT_DIR="."
DEVICE_FILES_DIR="/data/app/el2/100/base/${BUNDLE_NAME}/haps/${MODULE_NAME}/files/stability"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output-dir)  OUTPUT_DIR="$2"; shift 2 ;;
        *)             shift ;;
    esac
done

echo "[Collect] Collecting HarmonyOS stability test results..."

# Stop the app gracefully
hdc shell aa force-stop "$BUNDLE_NAME" 2>/dev/null || true
sleep 2

# Pull log file from device
echo "[Collect] Pulling logs from ${DEVICE_FILES_DIR}..."

if hdc shell "test -d ${DEVICE_FILES_DIR}" 2>/dev/null; then
    # hdc file recv is the HarmonyOS equivalent of adb pull
    hdc file recv "${DEVICE_FILES_DIR}/stability_log.jsonl" "${OUTPUT_DIR}/stability_log.jsonl" 2>/dev/null || true
    echo "[Collect] Log file pulled successfully"
else
    echo "[Collect] Warning: Log directory not found on device"
    # Try alternative sandbox path
    ALT_DIR="/data/storage/el2/base/files/stability"
    if hdc shell "test -d ${ALT_DIR}" 2>/dev/null; then
        hdc file recv "${ALT_DIR}/stability_log.jsonl" "${OUTPUT_DIR}/stability_log.jsonl" 2>/dev/null || true
        echo "[Collect] Log pulled from alternative path"
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

    # Memory trend
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

# Collect faultlogs (HarmonyOS crash reports)
echo "[Collect] Pulling faultlogs..."
FAULTLOG_DIR="${OUTPUT_DIR}/crashes"
mkdir -p "$FAULTLOG_DIR"
# Use hdc file recv for the faultlog directory (official approach)
# Official docs: hdc file recv /data/log/faultlog/faultlogger /${workpath}/
# Note: hdc creates a 'faultlogger/' subdirectory inside local target
hdc file recv /data/log/faultlog/faultlogger "${FAULTLOG_DIR}/" 2>/dev/null || true
# Keep only files related to our app, but preserve crash_*.txt and freeze_*.txt
# created by monitor.sh (they contain monitor context + faultlog content)
find "$FAULTLOG_DIR" -type f \
    ! -name "*${BUNDLE_NAME}*" \
    ! -name "crash_*.txt" \
    ! -name "freeze_*.txt" \
    -delete 2>/dev/null || true
# Remove empty directories left after deletion
find "$FAULTLOG_DIR" -type d -empty -delete 2>/dev/null || true

echo "[Collect] Collection complete: ${OUTPUT_DIR}"
