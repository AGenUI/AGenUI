#!/bin/bash
set -euo pipefail

export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

# ============================================================================
# tests/integration/platforms/ios.sh
# Run iOS Playground integration tests (XCTest)
#
# Prerequisites:
#   - macOS + Xcode installed
#   - CocoaPods installed (pod command available)
#
# Usage:
#   ./tests/integration/platforms/ios.sh [options]
#
# Options:
#   --simulator <name>      Simulator name (default "iPhone 16", only effective when no --udid and no booted simulator)
#   --os <version>          Simulator OS version (default "latest")
#   --udid <udid>           Specify simulator UDID directly (highest priority)
#   --scheme <scheme>       Xcode scheme (default "AGenUITests")
#   --output-dir <dir>      JUnit XML report output directory (default: reports/ios)
#   --no-pod-install        Skip pod install
#   --test-class <class>    Run only specified test class (e.g. InitializationTest)
#   -h, --help              Show help
#
# Simulator selection priority (automatic):
#   1) --udid explicitly specified
#   2) Currently booted iOS simulator (avoids starting a new one each time)
#   3) --simulator + --os match by name + version
#
# Examples:
#   ./tests/integration/platforms/ios.sh
#   ./tests/integration/platforms/ios.sh --simulator "iPhone 14" --os "17.0"
#   ./tests/integration/platforms/ios.sh --output-dir /tmp/reports/ios
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../../../../scripts/common/_common.sh
source "${SCRIPT_DIR}/../../../../scripts/common/_common.sh"

# -------------------- Default parameters --------------------
IOS_PLAYGROUND="${AGENUI_ROOT}/playground/ios/Playground"
OUTPUT_DIR="${AGENUI_ROOT}/reports/ios"
SIMULATOR_NAME="iPhone 16"
SIMULATOR_OS="latest"
SIMULATOR_UDID=""
SCHEME="Playground"
SKIP_POD_INSTALL=false
TEST_CLASS=""

# -------------------- Argument parsing --------------------
show_help() {
    sed -n '5,32p' "$0" | sed 's/^# \?//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --simulator)       SIMULATOR_NAME="$2"; shift 2 ;;
        --os)              SIMULATOR_OS="$2"; shift 2 ;;
        --udid)            SIMULATOR_UDID="$2"; shift 2 ;;
        --scheme)          SCHEME="$2"; shift 2 ;;
        --output-dir)      OUTPUT_DIR="$2"; shift 2 ;;
        --no-pod-install)  SKIP_POD_INSTALL=true; shift ;;
        --test-class)      TEST_CLASS="$2"; shift 2 ;;
        -h|--help)         show_help ;;
        *)                 error "Unknown argument: $1" ;;
    esac
done

# -------------------- Prerequisites --------------------
command -v xcodebuild >/dev/null 2>&1 || error "xcodebuild not found, please ensure Xcode is installed"
[[ -d "$IOS_PLAYGROUND" ]] || error "iOS Playground directory not found: ${IOS_PLAYGROUND}"

mkdir -p "$OUTPUT_DIR"
info "iOS test report output directory: ${OUTPUT_DIR}"

# -------------------- Sync test_fixtures to Bundle --------------------
TEST_FIXTURES_SRC="${AGENUI_ROOT}/tests/integration/fixtures/test_fixtures"
TEST_FIXTURES_DEST="${IOS_PLAYGROUND}/AGenUITests/test_fixtures"
if [[ -d "$TEST_FIXTURES_SRC" ]]; then
    info "Syncing test_fixtures to test Bundle..."
    rm -rf "$TEST_FIXTURES_DEST"
    cp -r "$TEST_FIXTURES_SRC" "$TEST_FIXTURES_DEST"
else
    warn "test_fixtures directory not found: ${TEST_FIXTURES_SRC}"
fi

# -------------------- Ensure AGenUITests target exists --------------------
ADD_TARGET_SCRIPT="${SCRIPT_DIR}/add_test_target.rb"
if [[ -f "$ADD_TARGET_SCRIPT" ]]; then
    if command -v ruby >/dev/null 2>&1; then
        info "Checking AGenUITests target..."
        ruby "$ADD_TARGET_SCRIPT"
    else
        warn "ruby not found, skipping AGenUITests target check"
    fi
fi

# -------------------- pod install --------------------
PODFILE_DIR="$IOS_PLAYGROUND"
if [[ "$SKIP_POD_INSTALL" == false ]]; then
    command -v pod >/dev/null 2>&1 || error "pod command not found, please install CocoaPods first"
    NEED_POD_INSTALL=false
    if [[ ! -d "${PODFILE_DIR}/Pods" ]] || \
       ! diff -q "${PODFILE_DIR}/Podfile.lock" "${PODFILE_DIR}/Pods/Manifest.lock" >/dev/null 2>&1; then
        NEED_POD_INSTALL=true
    else
        # Also need pod install when podspec is newer than Manifest.lock (source file list may have changed)
        MANIFEST_LOCK="${PODFILE_DIR}/Pods/Manifest.lock"
        if [[ -f "$MANIFEST_LOCK" ]]; then
            MANIFEST_MTIME=$(stat -f "%m" "$MANIFEST_LOCK" 2>/dev/null || stat -c "%Y" "$MANIFEST_LOCK" 2>/dev/null || echo 0)
            while IFS= read -r SPEC; do
                [[ -f "$SPEC" ]] || continue
                SPEC_MTIME=$(stat -f "%m" "$SPEC" 2>/dev/null || stat -c "%Y" "$SPEC" 2>/dev/null || echo 0)
                if [[ "$SPEC_MTIME" -gt "$MANIFEST_MTIME" ]]; then
                    info "Detected podspec newer than Manifest.lock: ${SPEC}"
                    NEED_POD_INSTALL=true
                    break
                fi
            done < <(find "${AGENUI_ROOT}" -maxdepth 4 -name "*.podspec" -not -path "*/Pods/*" -not -path "*/vendor/*" 2>/dev/null)
        fi
    fi
    if [[ "$NEED_POD_INSTALL" == true ]]; then
        info "Running pod install..."
        (cd "$PODFILE_DIR" && pod install 2>&1 | cat)
    else
        info "Pods up to date, skipping pod install"
    fi
fi

# -------------------- Determine workspace / project --------------------
WORKSPACE="${IOS_PLAYGROUND}/Playground.xcworkspace"
PROJECT="${IOS_PLAYGROUND}/Playground.xcodeproj"

if [[ -d "$WORKSPACE" ]]; then
    XCODE_ARG=(-workspace "$WORKSPACE")
    info "Using workspace: ${WORKSPACE}"
elif [[ -d "$PROJECT" ]]; then
    XCODE_ARG=(-project "$PROJECT")
    info "Using project: ${PROJECT}"
else
    error "Playground.xcworkspace or Playground.xcodeproj not found: ${IOS_PLAYGROUND}"
fi

# -------------------- Determine simulator destination --------------------
# Priority: --udid explicit > currently booted simulator > match by name+OS
if [[ -z "$SIMULATOR_UDID" ]]; then
    BOOTED_UDID=$(xcrun simctl list devices booted -j 2>/dev/null | \
        python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    for runtime, devices in data.get('devices', {}).items():
        for d in devices:
            if d.get('state') == 'Booted' and 'iOS' in runtime:
                print(d.get('udid', ''))
                sys.exit(0)
except Exception:
    pass
" 2>/dev/null || echo "")
    if [[ -n "$BOOTED_UDID" ]]; then
        SIMULATOR_UDID="$BOOTED_UDID"
        BOOTED_NAME=$(xcrun simctl list devices booted 2>/dev/null | grep "$BOOTED_UDID" | sed -E 's/^ *([^(]+) *\(.*/\1/' | head -1 | xargs || echo "")
        info "Reusing currently booted iOS simulator: ${BOOTED_NAME:-unknown} (${SIMULATOR_UDID})"
    fi
fi

if [[ -n "$SIMULATOR_UDID" ]]; then
    DESTINATION="platform=iOS Simulator,id=${SIMULATOR_UDID}"
elif [[ "$SIMULATOR_OS" == "latest" ]]; then
    # Auto-detect the latest OS version for the specified simulator name
    DETECTED_OS=$(xcrun simctl list devices available -j 2>/dev/null | \
        python3 -c "
import sys, json
data = json.load(sys.stdin)
name = '$SIMULATOR_NAME'
best_os = ''
for runtime, devices in data.get('devices', {}).items():
    for d in devices:
        if d.get('name') == name and d.get('isAvailable', False):
            # runtime format: com.apple.CoreSimulator.SimRuntime.iOS-18-6
            parts = runtime.split('.')[-1].replace('iOS-', '').replace('-', '.')
            if parts > best_os:
                best_os = parts
print(best_os)
" 2>/dev/null || echo "")
    if [[ -n "$DETECTED_OS" ]]; then
        info "Auto-detected available OS version for ${SIMULATOR_NAME}: ${DETECTED_OS}"
        DESTINATION="platform=iOS Simulator,name=${SIMULATOR_NAME},OS=${DETECTED_OS}"
    else
        DESTINATION="platform=iOS Simulator,name=${SIMULATOR_NAME}"
    fi
else
    DESTINATION="platform=iOS Simulator,name=${SIMULATOR_NAME},OS=${SIMULATOR_OS}"
fi

# -------------------- Build test args --------------------
RESULT_PATH="${OUTPUT_DIR}/result.xcresult"

# Clean old xcresult (xcodebuild won't overwrite an existing result bundle)
if [[ -d "$RESULT_PATH" ]]; then
    info "Cleaning old xcresult: ${RESULT_PATH}"
    rm -rf "$RESULT_PATH"
fi

TEST_ARGS=()
if [[ -n "$TEST_CLASS" ]]; then
    TEST_ARGS+=(-only-testing:"AGenUITests/${TEST_CLASS}")
fi

# -------------------- Run XCTest --------------------
info "Running iOS integration tests (scheme=${SCHEME}, destination=${DESTINATION})..."
set +e
xcodebuild test \
    "${XCODE_ARG[@]}" \
    -scheme "$SCHEME" \
    -destination "$DESTINATION" \
    -resultBundlePath "$RESULT_PATH" \
    -disable-concurrent-destination-testing \
    -parallel-testing-enabled NO \
    ${TEST_ARGS[@]+"${TEST_ARGS[@]}"} \
    2>&1 | tee "${OUTPUT_DIR}/xcodebuild_output.txt"
XCODEBUILD_EXIT=${PIPESTATUS[0]}
set -e

# Copy output to raw_output.txt for parse_reports.py parsing
cp "${OUTPUT_DIR}/xcodebuild_output.txt" "${OUTPUT_DIR}/raw_output.txt"

# -------------------- Convert to JUnit XML --------------------
JUNIT_XML="${OUTPUT_DIR}/junit.xml"
if command -v xcresulttool >/dev/null 2>&1; then
    info "Converting test results to JUnit XML..."
    xcrun xcresulttool export \
        --path "$RESULT_PATH" \
        --output-path "${OUTPUT_DIR}/junit_result" \
        --type directory 2>/dev/null || true
fi

# Use xchtmlreport or xcparse (if installed) to generate JUnit XML
if command -v xcparse >/dev/null 2>&1; then
    xcparse logs "$RESULT_PATH" "${OUTPUT_DIR}/" 2>/dev/null || true
fi

# -------------------- Output result summary --------------------
TESTS_RUN=$(grep -c "Test Case.*passed\|Test Case.*failed" "${OUTPUT_DIR}/raw_output.txt" 2>/dev/null || echo "0")
TESTS_PASSED=$(grep -c "Test Case.*passed" "${OUTPUT_DIR}/raw_output.txt" 2>/dev/null || echo "0")
TESTS_FAILED=$(grep -c "Test Case.*failed" "${OUTPUT_DIR}/raw_output.txt" 2>/dev/null || echo "0")

info "iOS test results: total=${TESTS_RUN} passed=${TESTS_PASSED} failed=${TESTS_FAILED}"

if [[ "$XCODEBUILD_EXIT" -eq 0 ]]; then
    success "iOS integration tests completed, results: ${RESULT_PATH}"
else
    warn "iOS integration tests have failures (exit code: ${XCODEBUILD_EXIT}), results: ${RESULT_PATH}"
    exit "$XCODEBUILD_EXIT"
fi
