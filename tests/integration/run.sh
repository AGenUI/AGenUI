#!/bin/bash
set -euo pipefail

# ============================================================================
# tests/integration/run.sh
# AGenUI One-Click Multi-Platform Integration Test Entry Point
#
# Runs Android / iOS / Harmony integration tests sequentially (or selectively),
# collects reports from each platform and generates a unified HTML summary report.
#
# Usage:
#   ./tests/integration/run.sh [options]
#
# Options:
#   --android               Run Android tests only
#   --ios                   Run iOS tests only
#   --harmony               Run Harmony tests only
#   --skip-android          Skip Android tests
#   --skip-ios              Skip iOS tests
#   --skip-harmony          Skip Harmony tests
#   --output-dir <dir>      Report root directory (default: reports/)
#   --open-report           Open HTML report after completion
#   --no-report             Skip report generation
#   -h, --help              Show help
#
# Examples:
#   ./tests/integration/run.sh                        # Run all platforms
#   ./tests/integration/run.sh --android              # Android only
#   ./tests/integration/run.sh --skip-harmony         # Skip Harmony
#   ./tests/integration/run.sh --open-report          # Open report when done
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../../scripts/common/_common.sh
source "${SCRIPT_DIR}/../../scripts/common/_common.sh"

# -------------------- Default parameters --------------------
RUN_ANDROID=true
RUN_IOS=true
RUN_HARMONY=true
OUTPUT_DIR="${AGENUI_ROOT}/reports"
USER_OUTPUT_DIR=""
OPEN_REPORT=false
GEN_REPORT=true

# -------------------- Argument parsing --------------------
show_help() {
    sed -n '5,28p' "$0" | sed 's/^# \?//'
    exit 0
}

# Single platform mode flag
SINGLE_PLATFORM=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --android)
            if [[ "$SINGLE_PLATFORM" == false ]]; then
                RUN_ANDROID=true; RUN_IOS=false; RUN_HARMONY=false; SINGLE_PLATFORM=true
            else
                RUN_ANDROID=true
            fi
            shift ;;
        --ios)
            if [[ "$SINGLE_PLATFORM" == false ]]; then
                RUN_ANDROID=false; RUN_IOS=true; RUN_HARMONY=false; SINGLE_PLATFORM=true
            else
                RUN_IOS=true
            fi
            shift ;;
        --harmony)
            if [[ "$SINGLE_PLATFORM" == false ]]; then
                RUN_ANDROID=false; RUN_IOS=false; RUN_HARMONY=true; SINGLE_PLATFORM=true
            else
                RUN_HARMONY=true
            fi
            shift ;;
        --skip-android)  RUN_ANDROID=false; shift ;;
        --skip-ios)      RUN_IOS=false; shift ;;
        --skip-harmony)  RUN_HARMONY=false; shift ;;
        --output-dir)    USER_OUTPUT_DIR="$2"; OUTPUT_DIR="$2"; shift 2 ;;
        --open-report)   OPEN_REPORT=true; shift ;;
        --no-report)     GEN_REPORT=false; shift ;;
        -h|--help)       show_help ;;
        *)               error "Unknown argument: $1" ;;
    esac
done

# -------------------- Generate Run Key --------------------
RUN_KEY=""
METADATA_JSON=""
if [[ -z "$USER_OUTPUT_DIR" ]]; then
    RUN_KEY=$(generate_run_key)
    OUTPUT_DIR="${AGENUI_ROOT}/reports/runs/${RUN_KEY}"
    METADATA_JSON=$(collect_git_metadata "$RUN_KEY")
fi

# -------------------- Print plan --------------------
info "========================================"
info "AGenUI Multi-Platform Integration Tests"
info "========================================"
info "Android : $( [[ "$RUN_ANDROID" == true ]] && echo "✓ run" || echo "✗ skip" )"
info "iOS     : $( [[ "$RUN_IOS"     == true ]] && echo "✓ run" || echo "✗ skip" )"
info "Harmony : $( [[ "$RUN_HARMONY" == true ]] && echo "✓ run" || echo "✗ skip" )"
info "Run Key : ${RUN_KEY:-(custom output dir)}"
info "Output  : ${OUTPUT_DIR}"
info "========================================"

mkdir -p "${OUTPUT_DIR}/android" "${OUTPUT_DIR}/ios" "${OUTPUT_DIR}/harmony"

# Track exit codes per platform
EXIT_ANDROID=0
EXIT_IOS=0
EXIT_HARMONY=0

# -------------------- Android --------------------
if [[ "$RUN_ANDROID" == true ]]; then
    info ""
    info "--- Android Tests ---"
    bash "${SCRIPT_DIR}/platforms/android/android.sh" \
        --output-dir "${OUTPUT_DIR}/android" \
    && EXIT_ANDROID=0 || EXIT_ANDROID=$?
    if [[ "$EXIT_ANDROID" -ne 0 ]]; then
        warn "Android tests failed (exit code: ${EXIT_ANDROID})"
    fi
fi

# -------------------- iOS --------------------
if [[ "$RUN_IOS" == true ]]; then
    info ""
    info "--- iOS Tests ---"
    bash "${SCRIPT_DIR}/platforms/ios/ios.sh" \
        --output-dir "${OUTPUT_DIR}/ios" \
    && EXIT_IOS=0 || EXIT_IOS=$?
    if [[ "$EXIT_IOS" -ne 0 ]]; then
        warn "iOS tests failed (exit code: ${EXIT_IOS})"
    fi
fi

# -------------------- Harmony --------------------
if [[ "$RUN_HARMONY" == true ]]; then
    info ""
    info "--- Harmony Tests ---"
    bash "${SCRIPT_DIR}/platforms/harmony/harmony.sh" \
        --output-dir "${OUTPUT_DIR}/harmony" \
    && EXIT_HARMONY=0 || EXIT_HARMONY=$?
    if [[ "$EXIT_HARMONY" -ne 0 ]]; then
        warn "Harmony tests failed (exit code: ${EXIT_HARMONY})"
    fi
fi

# -------------------- Write metadata.json --------------------
if [[ -n "$RUN_KEY" ]]; then
    # Build platforms_run list
    PLATFORMS_RUN=""
    [[ "$RUN_ANDROID" == true ]] && PLATFORMS_RUN="${PLATFORMS_RUN}\"android\","
    [[ "$RUN_IOS"     == true ]] && PLATFORMS_RUN="${PLATFORMS_RUN}\"ios\","
    [[ "$RUN_HARMONY" == true ]] && PLATFORMS_RUN="${PLATFORMS_RUN}\"harmony\","
    PLATFORMS_RUN="[${PLATFORMS_RUN%,}]"

    # Merge git metadata + platform info into metadata.json
    # METADATA_JSON format: {"run_key":"...","git":{...},"timestamp":"..."}
    # Append platforms_run and exit_codes
    METADATA_FULL=$(printf '%s' "$METADATA_JSON" | sed "s/}$/,\"platforms_run\":${PLATFORMS_RUN},\"exit_codes\":{\"android\":${EXIT_ANDROID},\"ios\":${EXIT_IOS},\"harmony\":${EXIT_HARMONY}}}/")
    printf '%s\n' "$METADATA_FULL" > "${OUTPUT_DIR}/metadata.json"
    info "Metadata written: ${OUTPUT_DIR}/metadata.json"
fi

# -------------------- Generate unified report --------------------
if [[ "$GEN_REPORT" == true ]]; then
    info ""
    info "--- Generating unified test report ---"
    REPORT_ARGS=(--reports-dir "$OUTPUT_DIR" --output "${OUTPUT_DIR}/summary.html")
    [[ "$OPEN_REPORT" == true ]] && REPORT_ARGS+=(--open)
    [[ -n "$METADATA_JSON" ]] && REPORT_ARGS+=(--metadata-json "$METADATA_JSON")
    bash "${SCRIPT_DIR}/generate_report.sh" "${REPORT_ARGS[@]}" || true
fi

# -------------------- Update latest symlink + generate index page --------------------
if [[ -n "$RUN_KEY" ]]; then
    ln -sfn "runs/${RUN_KEY}" "${AGENUI_ROOT}/reports/latest"
    info "Latest symlink updated: reports/latest -> runs/${RUN_KEY}"

    # Generate history run index page
    PYTHON_SCRIPT="${SCRIPT_DIR}/parse_reports.py"
    if command -v python3 >/dev/null 2>&1 && [[ -f "$PYTHON_SCRIPT" ]]; then
        python3 "$PYTHON_SCRIPT" \
            --generate-index \
            --runs-dir "${AGENUI_ROOT}/reports/runs" \
            --output "${AGENUI_ROOT}/reports/index.html" || true
        info "Index page generated: reports/index.html"
    fi
fi

# -------------------- Summary --------------------
info ""
info "========================================"
info "Multi-Platform Test Summary"
info "========================================"
[[ "$RUN_ANDROID" == true ]] && { [[ "$EXIT_ANDROID" -eq 0 ]] && info "Android : PASS" || warn "Android : FAIL (exit=${EXIT_ANDROID})"; }
[[ "$RUN_IOS"     == true ]] && { [[ "$EXIT_IOS"     -eq 0 ]] && info "iOS     : PASS" || warn "iOS     : FAIL (exit=${EXIT_IOS})"; }
[[ "$RUN_HARMONY" == true ]] && { [[ "$EXIT_HARMONY" -eq 0 ]] && info "Harmony : PASS" || warn "Harmony : FAIL (exit=${EXIT_HARMONY})"; }
[[ "$GEN_REPORT"  == true ]] && info "HTML Report: ${OUTPUT_DIR}/summary.html"
[[ -n "$RUN_KEY" ]] && info "Quick link: ${AGENUI_ROOT}/reports/latest/summary.html"
info "========================================"

OVERALL=$((EXIT_ANDROID + EXIT_IOS + EXIT_HARMONY))
if [[ "$OVERALL" -eq 0 ]]; then
    success "All tests passed!"
    exit 0
else
    warn "Some platform tests failed, check the report for details"
    exit 1
fi
