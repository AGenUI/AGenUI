#!/bin/bash
set -euo pipefail

# iOS stability test results collector
# Pulls log files from device/simulator and generates summary

BUNDLE_ID="org.cocoapods.demo.Playground"
OUTPUT_DIR="."
USE_SIMULATOR=false
DEVICE_ID=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output-dir)  OUTPUT_DIR="$2"; shift 2 ;;
        --simulator)   USE_SIMULATOR=true; shift ;;
        --device-id)   DEVICE_ID="$2"; shift 2 ;;
        *)             shift ;;
    esac
done

# Read device_info.json if available
DEVICE_INFO="${OUTPUT_DIR}/device_info.json"
if [[ -f "$DEVICE_INFO" ]]; then
    DEVICE_ID=$(python3 -c "import json; d=json.load(open('$DEVICE_INFO')); print(d.get('device_id',''))" 2>/dev/null || echo "$DEVICE_ID")
    USE_SIMULATOR=$(python3 -c "import json; d=json.load(open('$DEVICE_INFO')); print('true' if d.get('use_simulator') else 'false')" 2>/dev/null || echo "$USE_SIMULATOR")
    BUNDLE_ID=$(python3 -c "import json; d=json.load(open('$DEVICE_INFO')); print(d.get('bundle_id','$BUNDLE_ID'))" 2>/dev/null || echo "$BUNDLE_ID")
fi

echo "[Collect] Collecting iOS stability test results..."

# Terminate app gracefully
if [[ "$USE_SIMULATOR" == true ]]; then
    xcrun simctl terminate "$DEVICE_ID" "$BUNDLE_ID" 2>/dev/null || true
else
    if command -v xcrun &>/dev/null; then
        xcrun devicectl device process terminate --device "$DEVICE_ID" --bundle-id "$BUNDLE_ID" 2>/dev/null || true
    fi
fi
sleep 2

# Pull log file
echo "[Collect] Pulling stability logs..."

if [[ "$USE_SIMULATOR" == true ]]; then
    # Simulator: direct filesystem access
    CONTAINER=$(xcrun simctl get_app_container "$DEVICE_ID" "$BUNDLE_ID" data 2>/dev/null || echo "")
    if [[ -n "$CONTAINER" ]]; then
        LOG_DIR="${CONTAINER}/Documents/stability"
        if [[ -d "$LOG_DIR" ]]; then
            cp "${LOG_DIR}/stability_log.jsonl" "${OUTPUT_DIR}/stability_log.jsonl" 2>/dev/null || true
            cp "${LOG_DIR}/crash_registry.json" "${OUTPUT_DIR}/crash_registry.json" 2>/dev/null || true
            echo "[Collect] Log file copied from simulator container"
        else
            echo "[Collect] Warning: Stability log directory not found in simulator"
        fi
    else
        echo "[Collect] Warning: Could not locate app container"
    fi
else
    # Physical device: use ios-deploy to download or ifuse
    if command -v ios-deploy &>/dev/null; then
        ios-deploy --download="/Documents/stability/stability_log.jsonl" \
            --bundle_id "$BUNDLE_ID" --id "$DEVICE_ID" \
            --to "${OUTPUT_DIR}/" 2>/dev/null || true
        ios-deploy --download="/Documents/stability/crash_registry.json" \
            --bundle_id "$BUNDLE_ID" --id "$DEVICE_ID" \
            --to "${OUTPUT_DIR}/" 2>/dev/null || true
        # ios-deploy creates nested path, move file
        if [[ -f "${OUTPUT_DIR}/Documents/stability/stability_log.jsonl" ]]; then
            mv "${OUTPUT_DIR}/Documents/stability/stability_log.jsonl" "${OUTPUT_DIR}/stability_log.jsonl"
        fi
        if [[ -f "${OUTPUT_DIR}/Documents/stability/crash_registry.json" ]]; then
            mv "${OUTPUT_DIR}/Documents/stability/crash_registry.json" "${OUTPUT_DIR}/crash_registry.json"
        fi
        rm -rf "${OUTPUT_DIR}/Documents" 2>/dev/null || true
        echo "[Collect] Log pulled via ios-deploy"
    elif command -v ideviceinstaller &>/dev/null; then
        # Alternative: ideviceinstaller for older libimobiledevice
        echo "[Collect] Warning: Attempting alternative log retrieval..."
        echo "[Collect] Consider installing ios-deploy for better file access"
    else
        echo "[Collect] Warning: Cannot pull files from physical device (install ios-deploy)"
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

    # Memory trend (iOS uses "memory_mb", Android uses "memory_total_mb")
    FIRST_MEM=$(head -5 "${OUTPUT_DIR}/stability_log.jsonl" | grep -o '"memory_mb":[0-9.]*\|"memory_total_mb":[0-9.]*' | head -1 | cut -d: -f2 || echo "N/A")
    LAST_MEM=$(tail -5 "${OUTPUT_DIR}/stability_log.jsonl" | grep -o '"memory_mb":[0-9.]*\|"memory_total_mb":[0-9.]*' | tail -1 | cut -d: -f2 || echo "N/A")
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

# Collect crash reports from system
echo "[Collect] Collecting crash reports..."
CRASH_REPORT_DIR="${OUTPUT_DIR}/crashes"
mkdir -p "$CRASH_REPORT_DIR"

if [[ "$USE_SIMULATOR" == true ]]; then
    # Simulator crash reports are in ~/Library/Logs/DiagnosticReports/
    DIAG_DIR="$HOME/Library/Logs/DiagnosticReports"
    if [[ -d "$DIAG_DIR" ]]; then
        find "$DIAG_DIR" -name "Playground*" -newer "${OUTPUT_DIR}" -type f 2>/dev/null | \
            while read -r f; do
                cp "$f" "$CRASH_REPORT_DIR/" 2>/dev/null || true
            done
    fi
else
    # Physical device: pull crash reports
    if command -v idevicecrashreport &>/dev/null; then
        idevicecrashreport -u "$DEVICE_ID" -e "$CRASH_REPORT_DIR" 2>/dev/null || true
        # Remove non-Playground reports
        find "$CRASH_REPORT_DIR" -type f ! -name "Playground*" -delete 2>/dev/null || true
    fi
fi

# Pull final system log
echo "[Collect] Pulling final system log..."
if [[ "$USE_SIMULATOR" == true ]]; then
    xcrun simctl spawn "$DEVICE_ID" log show \
        --predicate "subsystem == 'com.amap.agenui'" \
        --last 10m --style compact 2>/dev/null | tail -1000 > "${OUTPUT_DIR}/final_syslog.txt" 2>/dev/null || true
else
    if command -v idevicesyslog &>/dev/null; then
        timeout 5 idevicesyslog -u "$DEVICE_ID" 2>/dev/null | tail -1000 > "${OUTPUT_DIR}/final_syslog.txt" || true
    fi
fi

echo "[Collect] Collection complete: ${OUTPUT_DIR}"
