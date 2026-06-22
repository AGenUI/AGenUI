#!/bin/bash
set -euo pipefail

# ============================================================================
# tests/integration/generate_report.sh
# Parse multi-platform JUnit XML test reports, generate unified HTML summary report
#
# Two modes supported:
#   1. Shell built-in mode (no external deps): directly parse XML, generate simple HTML
#   2. Python enhanced mode (recommended, requires Python3): generates rich multi-platform summary tables
#
# Usage:
#   ./tests/integration/generate_report.sh [options]
#
# Options:
#   --reports-dir <dir>   Multi-platform reports root directory (default: reports/)
#   --output <file>       HTML output path (default: reports/summary.html)
#   --open                Open in browser after generation
#   -h, --help            Show help
#
# Directory structure convention:
#   reports/
#     android/    Android JUnit XML results
#     ios/        iOS xcresult / JUnit XML results
#     harmony/    HarmonyOS test raw output
#
# Examples:
#   ./tests/integration/generate_report.sh
#   ./tests/integration/generate_report.sh --reports-dir /tmp/reports --output /tmp/report.html
#   ./tests/integration/generate_report.sh --open
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../../scripts/common/_common.sh
source "${SCRIPT_DIR}/../../scripts/common/_common.sh"

# -------------------- Default parameters --------------------
REPORTS_DIR="${AGENUI_ROOT}/reports"
OUTPUT_HTML="${REPORTS_DIR}/summary.html"
AUTO_OPEN=false
METADATA_JSON=""

# -------------------- Argument parsing --------------------
show_help() {
    sed -n '5,24p' "$0" | sed 's/^# \?//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --reports-dir)    REPORTS_DIR="$2"; OUTPUT_HTML="${REPORTS_DIR}/summary.html"; shift 2 ;;
        --output)         OUTPUT_HTML="$2"; shift 2 ;;
        --open)           AUTO_OPEN=true; shift ;;
        --metadata-json)  METADATA_JSON="$2"; shift 2 ;;
        -h|--help)        show_help ;;
        *)                error "Unknown argument: $1" ;;
    esac
done

mkdir -p "$(dirname "$OUTPUT_HTML")"

# ============================================================================
# Python enhanced mode
# ============================================================================
PYTHON_SCRIPT="${SCRIPT_DIR}/parse_reports.py"

if command -v python3 >/dev/null 2>&1 && [[ -f "$PYTHON_SCRIPT" ]]; then
    info "Generating report using Python3..."
    PY_ARGS=(--reports-dir "$REPORTS_DIR" --output "$OUTPUT_HTML" --project-root "$AGENUI_ROOT")
    [[ -n "$METADATA_JSON" ]] && PY_ARGS+=(--metadata-json "$METADATA_JSON")
    python3 "$PYTHON_SCRIPT" "${PY_ARGS[@]}"
else
    # ============================================================================
    # Shell built-in mode: parse JUnit XML and generate HTML
    # ============================================================================
    info "Generating report using Shell built-in mode..."

    parse_junit_xml() {
        local platform="$1"
        local xml_dir="$2"
        local total=0 passed=0 failed=0 errors=0

        if [[ ! -d "$xml_dir" ]]; then
            echo "${platform}|0|0|0|0|Directory not found"
            return
        fi

        local xmlfiles
        xmlfiles=$(find "$xml_dir" -name "*.xml" -type f 2>/dev/null)
        if [[ -z "$xmlfiles" ]]; then
            echo "${platform}|0|0|0|0|No XML report files"
            return
        fi

        while IFS= read -r xml; do
            local t f e
            t=$(grep -o 'tests="[0-9]*"' "$xml" 2>/dev/null | grep -o '[0-9]*' | awk '{s+=$1}END{print s}' || echo 0)
            f=$(grep -o 'failures="[0-9]*"' "$xml" 2>/dev/null | grep -o '[0-9]*' | awk '{s+=$1}END{print s}' || echo 0)
            e=$(grep -o 'errors="[0-9]*"' "$xml" 2>/dev/null | grep -o '[0-9]*' | awk '{s+=$1}END{print s}' || echo 0)
            total=$((total + ${t:-0}))
            failed=$((failed + ${f:-0}))
            errors=$((errors + ${e:-0}))
        done <<< "$xmlfiles"

        passed=$((total - failed - errors))
        echo "${platform}|${total}|${passed}|${failed}|${errors}|OK"
    }

    # Parse reports per platform
    ANDROID_STATS=$(parse_junit_xml "Android" "${REPORTS_DIR}/android")
    IOS_STATS=$(parse_junit_xml "iOS" "${REPORTS_DIR}/ios")
    HARMONY_STATS=$(parse_junit_xml "Harmony" "${REPORTS_DIR}/harmony")

    # Aggregate totals
    TOTAL_ALL=0
    PASS_ALL=0
    FAIL_ALL=0
    for stats in "$ANDROID_STATS" "$IOS_STATS" "$HARMONY_STATS"; do
        IFS='|' read -r _ t p f e _ <<< "$stats"
        TOTAL_ALL=$((TOTAL_ALL + ${t:-0}))
        PASS_ALL=$((PASS_ALL + ${p:-0}))
        FAIL_ALL=$((FAIL_ALL + ${f:-0} + ${e:-0}))
    done

    PASS_RATE=0
    if [[ "$TOTAL_ALL" -gt 0 ]]; then
        PASS_RATE=$(( PASS_ALL * 100 / TOTAL_ALL ))
    fi

    TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")

    # Extract git info from metadata JSON (if available)
    GIT_INFO_HTML=""
    if [[ -n "$METADATA_JSON" ]]; then
        _branch=$(printf '%s' "$METADATA_JSON" | sed -n 's/.*"branch":"\([^"]*\)".*/\1/p')
        _commit=$(printf '%s' "$METADATA_JSON" | sed -n 's/.*"commit_short":"\([^"]*\)".*/\1/p')
        _run_key=$(printf '%s' "$METADATA_JSON" | sed -n 's/.*"run_key":"\([^"]*\)".*/\1/p')
        if [[ -n "$_branch" || -n "$_commit" ]]; then
            GIT_INFO_HTML="<div style=\"margin-bottom:16px;display:flex;gap:8px;flex-wrap:wrap;align-items:center\">"
            [[ -n "$_branch" ]] && GIT_INFO_HTML="${GIT_INFO_HTML}<span style=\"background:#e8f0fe;color:#1a73e8;padding:3px 12px;border-radius:12px;font-size:12px;font-weight:600\">${_branch}</span>"
            [[ -n "$_commit" ]] && GIT_INFO_HTML="${GIT_INFO_HTML}<span style=\"background:#f3e8fd;color:#7c3aed;padding:3px 12px;border-radius:12px;font-size:12px;font-weight:600;font-family:monospace\">${_commit}</span>"
            [[ -n "$_run_key" ]] && GIT_INFO_HTML="${GIT_INFO_HTML}<span style=\"color:#aaa;font-size:11px;margin-left:8px\">${_run_key}</span>"
            GIT_INFO_HTML="${GIT_INFO_HTML}</div>"
        fi
    fi

    # Generate HTML
    cat > "$OUTPUT_HTML" << HTMLEOF
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AGenUI Integration Test Report</title>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 0; padding: 20px; background: #f5f7fa; color: #333; }
  .container { max-width: 900px; margin: 0 auto; }
  h1 { color: #1a1a2e; margin-bottom: 4px; }
  .timestamp { color: #888; font-size: 13px; margin-bottom: 24px; }
  .summary-cards { display: flex; gap: 16px; margin-bottom: 28px; flex-wrap: wrap; }
  .card { background: #fff; border-radius: 12px; padding: 20px 28px; box-shadow: 0 2px 8px rgba(0,0,0,.08); flex: 1; min-width: 160px; }
  .card .label { font-size: 13px; color: #888; margin-bottom: 6px; }
  .card .value { font-size: 32px; font-weight: 700; }
  .pass-rate { color: $([ "$FAIL_ALL" -eq 0 ] && echo '#27ae60' || echo '#e74c3c'); }
  .total-color { color: #2c3e50; }
  .pass-color { color: #27ae60; }
  .fail-color { color: $([ "$FAIL_ALL" -eq 0 ] && echo '#95a5a6' || echo '#e74c3c'); }
  table { width: 100%; border-collapse: collapse; background: #fff; border-radius: 12px; overflow: hidden; box-shadow: 0 2px 8px rgba(0,0,0,.08); }
  thead { background: #1a1a2e; color: #fff; }
  th { padding: 14px 20px; text-align: left; font-weight: 600; font-size: 14px; }
  td { padding: 14px 20px; border-bottom: 1px solid #f0f0f0; font-size: 14px; }
  tr:last-child td { border-bottom: none; }
  tr:hover td { background: #f9f9f9; }
  .badge { display: inline-block; padding: 3px 10px; border-radius: 12px; font-size: 12px; font-weight: 600; }
  .badge-pass { background: #e8f8f0; color: #27ae60; }
  .badge-fail { background: #fde8e8; color: #e74c3c; }
  .badge-na   { background: #f0f0f0; color: #999; }
  footer { margin-top: 24px; text-align: center; font-size: 12px; color: #bbb; }
</style>
</head>
<body>
<div class="container">
  <h1>AGenUI Integration Test Report</h1>
  ${GIT_INFO_HTML}
  <div class="timestamp">Generated: ${TIMESTAMP}</div>

  <div class="summary-cards">
    <div class="card">
      <div class="label">Total Cases</div>
      <div class="value total-color">${TOTAL_ALL}</div>
    </div>
    <div class="card">
      <div class="label">Passed</div>
      <div class="value pass-color">${PASS_ALL}</div>
    </div>
    <div class="card">
      <div class="label">Failed/Errors</div>
      <div class="value fail-color">${FAIL_ALL}</div>
    </div>
    <div class="card">
      <div class="label">Pass Rate</div>
      <div class="value pass-rate">${PASS_RATE}%</div>
    </div>
  </div>

  <table>
    <thead>
      <tr>
        <th>Platform</th>
        <th>Total</th>
        <th>Passed</th>
        <th>Failed</th>
        <th>Errors</th>
        <th>Status</th>
      </tr>
    </thead>
    <tbody>
HTMLEOF

    for stats in "$ANDROID_STATS" "$IOS_STATS" "$HARMONY_STATS"; do
        IFS='|' read -r platform total passed failed errors note <<< "$stats"
        if [[ "$total" -eq 0 ]]; then
            badge='<span class="badge badge-na">N/A</span>'
        elif [[ "$((failed + errors))" -gt 0 ]]; then
            badge='<span class="badge badge-fail">FAIL</span>'
        else
            badge='<span class="badge badge-pass">PASS</span>'
        fi
        echo "      <tr><td>${platform}</td><td>${total}</td><td>${passed}</td><td>${failed}</td><td>${errors}</td><td>${badge}</td></tr>" >> "$OUTPUT_HTML"
    done

    cat >> "$OUTPUT_HTML" << HTMLEOF
    </tbody>
  </table>
  <footer>AGenUI Test Automation Framework</footer>
</div>
</body>
</html>
HTMLEOF
fi  # end shell built-in mode

success "Test report generated: ${OUTPUT_HTML}"

if [[ "$AUTO_OPEN" == true ]]; then
    if command -v open >/dev/null 2>&1; then
        open "$OUTPUT_HTML"
    elif command -v xdg-open >/dev/null 2>&1; then
        xdg-open "$OUTPUT_HTML"
    fi
fi
