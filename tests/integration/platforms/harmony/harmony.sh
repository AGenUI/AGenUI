#!/bin/bash
set -euo pipefail

# ============================================================================
# tests/integration/platforms/harmony/harmony.sh
# Run Harmony Playground integration tests (ohosTest / hypium)
#
# Prerequisites:
#   - DevEco Studio installed (default: /Applications/DevEco-Studio.app)
#     Override via DEVECO_HOME environment variable
#   - HarmonyOS device connected or emulator running
#   - hdc command available
#
# Usage:
#   ./tests/integration/platforms/harmony/harmony.sh [options]
#
# Options:
#   --device <serialNo>   Specify device serial (default: first from hdc list targets)
#   --output-dir <dir>    JUnit XML report output directory (default: reports/harmony)
#   --test-class <class>  Run only specified test class (e.g. InitializationTest)
#   -h, --help            Show help
#
# Examples:
#   ./tests/integration/platforms/harmony/harmony.sh
#   ./tests/integration/platforms/harmony/harmony.sh --output-dir /tmp/reports/harmony
#   ./tests/integration/platforms/harmony/harmony.sh --test-class InitializationTest
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../../../../scripts/common/_common.sh
source "${SCRIPT_DIR}/../../../../scripts/common/_common.sh"

# -------------------- Default Parameters --------------------
HARMONY_PLAYGROUND="${AGENUI_ROOT}/playground/harmony"
OUTPUT_DIR="${AGENUI_ROOT}/reports/harmony"
DEVICE_ARG=""
TEST_CLASS=""

# -------------------- Argument Parsing --------------------
show_help() {
    sed -n '5,22p' "$0" | sed 's/^# \?//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --device)     DEVICE_ARG="-t $2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --test-class) TEST_CLASS="$2"; shift 2 ;;
        -h|--help)    show_help ;;
        *)            error "Unknown argument: $1" ;;
    esac
done

# -------------------- Prerequisites Check --------------------
[[ -d "$HARMONY_PLAYGROUND" ]] || error "Harmony Playground directory not found: ${HARMONY_PLAYGROUND}"

DEVECO_HOME="${DEVECO_HOME:-/Applications/DevEco-Studio.app/Contents}"
if [[ ! -d "$DEVECO_HOME" ]]; then
    error "DevEco Studio not found: ${DEVECO_HOME} (override via DEVECO_HOME environment variable)"
fi

export DEVECO_SDK_HOME="${DEVECO_HOME}/sdk"
export PATH="${DEVECO_HOME}/tools/hvigor/bin:${DEVECO_HOME}/tools/ohpm/bin:${DEVECO_HOME}/tools/node/bin:${DEVECO_HOME}/sdk/default/openharmony/toolchains:${PATH}"

command -v hvigorw >/dev/null 2>&1 || error "hvigorw command not found, please ensure DevEco Studio is properly installed"

mkdir -p "$OUTPUT_DIR"
info "Harmony test report output directory: ${OUTPUT_DIR}"

# -------------------- Sync test_fixtures to rawfile --------------------
TEST_FIXTURES_SRC="${AGENUI_ROOT}/tests/integration/fixtures/test_fixtures"
TEST_FIXTURES_DEST="${HARMONY_PLAYGROUND}/entry/src/main/resources/rawfile/test_fixtures"

if [[ -d "$TEST_FIXTURES_SRC" ]]; then
    info "Syncing test_fixtures to rawfile/test_fixtures..."
    rm -rf "$TEST_FIXTURES_DEST"
    cp -r "$TEST_FIXTURES_SRC" "$TEST_FIXTURES_DEST"
    info "test_fixtures synced: ${TEST_FIXTURES_DEST}"
else
    warn "test_fixtures directory not found: ${TEST_FIXTURES_SRC}"
fi

# -------------------- Check Devices --------------------
if ! command -v hdc >/dev/null 2>&1; then
    warn "hdc command not found, will only build the test package"
    DEVICE_AVAILABLE=false
else
    # Ensure hdc server is running and ready (cold start may return [Empty])
    hdc start >/dev/null 2>&1 || true
    sleep 1

    # Retry device detection up to 3 times (handles hdc server cold-start race)
    MAX_RETRY=3
    CONNECTED_DEVICES=()
    for (( _retry=1; _retry<=MAX_RETRY; _retry++ )); do
        CONNECTED_DEVICES=()
        while IFS= read -r line; do
            line="$(echo "$line" | tr -d '[:space:]')"
            [[ -n "$line" ]] && CONNECTED_DEVICES+=("$line")
        done < <(hdc list targets 2>/dev/null | grep -iv "empty\|\[empty\]")

        if [[ ${#CONNECTED_DEVICES[@]} -gt 0 ]]; then
            break
        fi
        if [[ $_retry -lt $MAX_RETRY ]]; then
            info "No device detected, retrying (${_retry}/${MAX_RETRY})..."
            sleep 2
        fi
    done

    if [[ ${#CONNECTED_DEVICES[@]} -eq 0 ]]; then
        warn "No HarmonyOS device detected (retried ${MAX_RETRY} times), will only build the test package (not run)"
        DEVICE_AVAILABLE=false
    else
        DEVICE_AVAILABLE=true

        if [[ -z "$DEVICE_ARG" ]]; then
            if [[ ${#CONNECTED_DEVICES[@]} -eq 1 ]]; then
                # Single device — use it directly
                DEVICE_ARG="-t ${CONNECTED_DEVICES[0]}"
                info "Target device: ${CONNECTED_DEVICES[0]}"
            else
                # Multiple devices — interactive selection with 10s timeout
                echo ""
                info "Detected ${#CONNECTED_DEVICES[@]} devices, please select target device (auto-select in 10s if no input):"
                echo ""
                for i in "${!CONNECTED_DEVICES[@]}"; do
                    # Tag emulators (typically contain "emulator" or start with "127.0.0.1")
                    EMULATOR_TAG=""
                    if [[ "${CONNECTED_DEVICES[$i]}" == *emulator* ]] || [[ "${CONNECTED_DEVICES[$i]}" == 127.0.0.1* ]]; then
                        EMULATOR_TAG=" [emulator]"
                    fi
                    printf "  [%d] %s%s\n" "$((i+1))" "${CONNECTED_DEVICES[$i]}" "$EMULATOR_TAG"
                done
                echo ""

                # Auto-select logic: prefer emulator, fallback to first device
                _auto_select_device() {
                    for d in "${CONNECTED_DEVICES[@]}"; do
                        if [[ "$d" == *emulator* ]] || [[ "$d" == 127.0.0.1* ]]; then
                            echo "$d"
                            return
                        fi
                    done
                    echo "${CONNECTED_DEVICES[0]}"
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
                        else
                            echo "  Invalid input, please enter a number between 1 and ${#CONNECTED_DEVICES[@]}"
                        fi
                    fi
                    ATTEMPTS=$((ATTEMPTS + 1))
                done

                DEVICE_ARG="-t ${TARGET_SERIAL}"
                info "Target device: ${TARGET_SERIAL} (${#CONNECTED_DEVICES[@]} devices detected)"
            fi
        fi
    fi
fi

# -------------------- Check Signing Configuration --------------------
BUILD_PROFILE="${HARMONY_PLAYGROUND}/build-profile.json5"
SIGNING_PATCHED=false

# Ensure build-profile.json5 is restored when script exits
restore_build_profile() {
    if [[ "$SIGNING_PATCHED" == true && -f "${BUILD_PROFILE}.bak" ]]; then
        mv "${BUILD_PROFILE}.bak" "$BUILD_PROFILE"
    fi
}
trap restore_build_profile EXIT

if [[ -f "$BUILD_PROFILE" ]]; then
    STORE_FILE=$(grep -o '"storeFile"[[:space:]]*:[[:space:]]*"[^"]*"' "$BUILD_PROFILE" 2>/dev/null | head -1 | sed 's/.*"\([^"]*\)"$/\1/' || true)
    CERT_FILE=$(grep -o '"certpath"[[:space:]]*:[[:space:]]*"[^"]*"' "$BUILD_PROFILE" 2>/dev/null | head -1 | sed 's/.*"\([^"]*\)"$/\1/' || true)

    PROFILE_FILE=$(grep -o '"profile"[[:space:]]*:[[:space:]]*"[^"]*"' "$BUILD_PROFILE" 2>/dev/null | head -1 | sed 's/.*"\([^"]*\)"$/\1/' || true)

    SIGNING_INVALID=false
    if [[ -n "$STORE_FILE" && ! -f "$STORE_FILE" ]]; then
        warn "Signing key file not found: ${STORE_FILE}"
        SIGNING_INVALID=true
    elif [[ -n "$PROFILE_FILE" && -f "$PROFILE_FILE" ]]; then
        # Check the validity period of the certificate embedded in the provisioning profile (.p7b)
        SIGN_TOOL="${DEVECO_HOME}/sdk/default/openharmony/toolchains/lib/hap-sign-tool.jar"
        if [[ -f "$SIGN_TOOL" ]] && command -v java >/dev/null 2>&1; then
            PROFILE_NOT_AFTER=$(java -jar "$SIGN_TOOL" verify-profile -inFile "$PROFILE_FILE" 2>/dev/null \
                | grep -o '"not-after"[[:space:]]*:[[:space:]]*[0-9]*' | head -1 | grep -o '[0-9]*' || true)
            NOW_TS=$(date "+%s")
            if [[ -n "$PROFILE_NOT_AFTER" && "$NOW_TS" -gt "$PROFILE_NOT_AFTER" ]]; then
                EXPIRY_DATE=$(date -r "$PROFILE_NOT_AFTER" "+%Y-%m-%d %H:%M:%S" 2>/dev/null || date -d "@$PROFILE_NOT_AFTER" "+%Y-%m-%d %H:%M:%S" 2>/dev/null || echo "$PROFILE_NOT_AFTER")
                warn "Signing certificate has expired! Profile: ${PROFILE_FILE}"
                warn "Expired at: ${EXPIRY_DATE}"
                warn "Please open DevEco Studio -> File -> Project Structure -> Signing Configs to regenerate signing"
                SIGNING_INVALID=true
            fi
        fi
    elif [[ -n "$CERT_FILE" && ! -f "$CERT_FILE" ]]; then
        warn "Signing certificate file not found: ${CERT_FILE}"
        SIGNING_INVALID=true
    fi

    if [[ "$SIGNING_INVALID" == true ]]; then
        warn "Signing configuration invalid, removing signing to build unsigned HAP (device install will fail)..."
        cp "$BUILD_PROFILE" "${BUILD_PROFILE}.bak"
        SIGNING_PATCHED=true
        python3 "${SCRIPT_DIR}/patch_signing.py" "$BUILD_PROFILE" 2>&1 || warn "Auto-patching signing configuration failed, build may fail"

        if [[ "$DEVICE_AVAILABLE" == true ]]; then
            error "Signing certificate is invalid or expired. Cannot install on device. Please regenerate signing in DevEco Studio before running."
        fi
    fi
fi

# -------------------- Install Dependencies --------------------
info "Installing ohpm dependencies..."
cd "$HARMONY_PLAYGROUND"

if command -v ohpm >/dev/null 2>&1; then
    ohpm install 2>&1 | cat || warn "ohpm install failed, continuing with build..."
else
    warn "ohpm command not found, skipping dependency installation"
fi

# -------------------- Clean Old Build Cache --------------------
# hvigor's native module build uses cmake cache at platforms/harmony/agenui/.cxx/
# If core source files are renamed/deleted, stale cache causes ninja to fail finding old files
AGENUI_CXX_CACHE="${AGENUI_ROOT}/platforms/harmony/agenui/.cxx"
if [[ -d "$AGENUI_CXX_CACHE" ]]; then
    info "Cleaning native cmake cache: ${AGENUI_CXX_CACHE}"
    rm -rf "$AGENUI_CXX_CACHE"
fi

# -------------------- Build Test HAP --------------------
info "Building ohosTest HAP..."

BUILD_EXIT=0
hvigorw assembleHap \
    --mode module \
    -p module=entry@ohosTest \
    -p product=default \
    -p buildMode=debug \
    --no-daemon 2>&1 | tee "${OUTPUT_DIR}/build_output.txt" || BUILD_EXIT=${PIPESTATUS[0]:-$?}

if [[ "$BUILD_EXIT" -ne 0 ]]; then
    warn "Harmony ohosTest build failed (exit code: ${BUILD_EXIT}), see ${OUTPUT_DIR}/build_output.txt"
    exit "$BUILD_EXIT"
fi

HAP_OUTPUT=$(find "${HARMONY_PLAYGROUND}/entry/build" -name "*.hap" 2>/dev/null | grep -i "ohostest\|test" | head -1)
if [[ -z "$HAP_OUTPUT" ]]; then
    HAP_OUTPUT=$(find "${HARMONY_PLAYGROUND}/entry/build" -name "*.hap" 2>/dev/null | head -1)
fi

[[ -n "$HAP_OUTPUT" ]] || error "Test HAP not found, please check if the build succeeded"
info "Test HAP: ${HAP_OUTPUT}"

# -------------------- Build Main Entry HAP --------------------
info "Building main entry HAP..."

MAIN_BUILD_EXIT=0
hvigorw assembleHap \
    --mode module \
    -p module=entry@default \
    -p product=default \
    -p buildMode=debug \
    --no-daemon 2>&1 | tee "${OUTPUT_DIR}/main_build_output.txt" || MAIN_BUILD_EXIT=${PIPESTATUS[0]:-$?}

if [[ "$MAIN_BUILD_EXIT" -ne 0 ]]; then
    warn "Main entry HAP build failed (exit code: ${MAIN_BUILD_EXIT}), see ${OUTPUT_DIR}/main_build_output.txt"
    exit "$MAIN_BUILD_EXIT"
fi

MAIN_HAP_OUTPUT=$(find "${HARMONY_PLAYGROUND}/entry/build" -name "*.hap" 2>/dev/null | grep -vi "ohostest\|test" | head -1)
[[ -n "$MAIN_HAP_OUTPUT" ]] || error "Main entry HAP not found, please check if the build succeeded"
info "Main entry HAP: ${MAIN_HAP_OUTPUT}"

if [[ "$DEVICE_AVAILABLE" == false ]]; then
    success "Harmony test package built (device unavailable, skipping execution): ${HAP_OUTPUT}"
    exit 0
fi

# -------------------- Unlock Screen --------------------
info "Attempting to unlock device screen..."
hdc ${DEVICE_ARG} shell power-shell wakeup 2>/dev/null || true
hdc ${DEVICE_ARG} shell uinput -K -d 2 -u 2 2>/dev/null || true
sleep 1

# -------------------- Install and Run Tests --------------------
info "Installing main entry HAP..."
hdc ${DEVICE_ARG} install "$MAIN_HAP_OUTPUT" 2>&1 || { warn "Failed to install main entry HAP"; exit 1; }

info "Installing test HAP..."
hdc ${DEVICE_ARG} install "$HAP_OUTPUT" 2>&1 || { warn "Failed to install test HAP"; exit 1; }

BUNDLE_NAME="com.harmony.agenui"
TEST_RUNNER="OpenHarmonyTestRunner"

# Build test arguments
TEST_ARGS=""
if [[ -n "$TEST_CLASS" ]]; then
    TEST_ARGS="class ${TEST_CLASS}"
fi

info "Running ohosTest..."
TEST_EXIT=0
hdc ${DEVICE_ARG} shell aa test \
    -b "$BUNDLE_NAME" \
    -m entry_test \
    -s unittest "$TEST_RUNNER" \
    ${TEST_ARGS:+-s $TEST_ARGS} \
    2>&1 | tee "${OUTPUT_DIR}/raw_output.txt" || TEST_EXIT=${PIPESTATUS[0]:-$?}

# -------------------- Pull Test Reports and Crash Logs --------------------
# Note: Hypium framework does NOT generate test-result files on device; all results
# are output via stdout, already captured to raw_output.txt by tee above.
# The file recv attempt here is kept for backward compatibility.
DEVICE_REPORT_DIR="/data/app/el2/100/base/${BUNDLE_NAME}/files/test-result"
if hdc ${DEVICE_ARG} file recv "$DEVICE_REPORT_DIR" "${OUTPUT_DIR}/" >/dev/null 2>&1; then
    info "Test reports pulled from device"
else
    info "Harmony test results captured in ${OUTPUT_DIR}/raw_output.txt (Hypium outputs via stdout)"
fi

# Pull crash logs (faultlog) if the test crashed
if grep -qE 'App died|app crash|ABORT' "${OUTPUT_DIR}/raw_output.txt" 2>/dev/null; then
    info "Process crash detected, pulling faultlog..."
    mkdir -p "${OUTPUT_DIR}/crashes"
    # Pull cppcrash logs for our bundle
    FAULTLOG_DIR="/data/log/faultlog/faultlogger"
    CRASH_FILES=$(hdc ${DEVICE_ARG} shell "ls ${FAULTLOG_DIR}/ 2>/dev/null | grep -i '${BUNDLE_NAME}' | tail -5" 2>/dev/null || true)
    if [[ -n "$CRASH_FILES" ]]; then
        while IFS= read -r fname; do
            fname="$(echo "$fname" | tr -d '\r\n ')"
            [[ -n "$fname" ]] || continue
            hdc ${DEVICE_ARG} file recv "${FAULTLOG_DIR}/${fname}" "${OUTPUT_DIR}/crashes/${fname}" 2>/dev/null || true
        done <<< "$CRASH_FILES"
        PULLED_COUNT=$(ls -1 "${OUTPUT_DIR}/crashes/" 2>/dev/null | wc -l | tr -d ' ')
        if [[ "$PULLED_COUNT" -gt 0 ]]; then
            info "Pulled ${PULLED_COUNT} crash log(s) to ${OUTPUT_DIR}/crashes/"
            # Print the most recent crash log summary
            LATEST_CRASH=$(ls -t "${OUTPUT_DIR}/crashes/" 2>/dev/null | head -1)
            if [[ -n "$LATEST_CRASH" ]]; then
                echo ""
                warn "===== Latest Crash Log Summary (${LATEST_CRASH}) ====="
                head -50 "${OUTPUT_DIR}/crashes/${LATEST_CRASH}" 2>/dev/null || true
                echo ""
                warn "===== Full log at: ${OUTPUT_DIR}/crashes/${LATEST_CRASH} ====="
            fi
        fi
    else
        info "No faultlog files found for ${BUNDLE_NAME}"
    fi
    # Also pull hilog for context
    info "Pulling recent hilog..."
    hdc ${DEVICE_ARG} shell "hilog -x 2>/dev/null | grep -i 'agenui\|testTag\|ABORT\|SIGSEGV\|SIGABRT' | tail -100" \
        > "${OUTPUT_DIR}/hilog_crash.txt" 2>/dev/null || true
    if [[ -s "${OUTPUT_DIR}/hilog_crash.txt" ]]; then
        info "hilog saved to ${OUTPUT_DIR}/hilog_crash.txt"
        # Extract and highlight JS runtime errors (SyntaxError / RuntimeError / module import failures)
        JS_ERR_LINES=$(grep -E 'Error name:|Error message:|Cannot execute module buffer|does not provide an export' "${OUTPUT_DIR}/hilog_crash.txt" 2>/dev/null | head -5 || true)
        if [[ -n "$JS_ERR_LINES" ]]; then
            echo ""
            warn "===== JS Runtime Error Summary ====="
            echo "$JS_ERR_LINES"
            warn "Hint: Usually caused by entry/src/mock/Libentry.mock.ets missing required named exports from the SDK. Please add the missing exports."
            echo ""
        fi
    fi
fi

# Check test results for failures
if [[ -f "${OUTPUT_DIR}/raw_output.txt" ]]; then
    if grep -q 'OHOS_REPORT_STATUS_CODE: -1\|OHOS_REPORT_STATUS_CODE: -2' "${OUTPUT_DIR}/raw_output.txt" 2>/dev/null; then
        warn "Harmony: some test cases failed"
        TEST_EXIT=1
    fi
    # Detect test process interruption/crash (App died, ResultCode: -1, etc.)
    if grep -qE 'TestFinished-ResultCode:\s*-1|App died|app crash|ABORT|Test runner died' "${OUTPUT_DIR}/raw_output.txt" 2>/dev/null; then
        warn "Harmony test process terminated abnormally (App died / ResultCode: -1)"
        TEST_EXIT=1
    fi
    # raw_output too short (<200 bytes) usually means tests didn't actually run
    RAW_SIZE=$(wc -c <"${OUTPUT_DIR}/raw_output.txt" 2>/dev/null | tr -d ' ')
    if [[ -n "$RAW_SIZE" && "$RAW_SIZE" -lt 200 && "$TEST_EXIT" -eq 0 ]]; then
        warn "Harmony raw_output too short (${RAW_SIZE} bytes), tests may not have actually executed"
        TEST_EXIT=1
    fi
fi

if [[ "$TEST_EXIT" -ne 0 ]]; then
    warn "Harmony integration tests have failures (exit code: ${TEST_EXIT})"
    exit "$TEST_EXIT"
fi

success "Harmony integration tests completed"
