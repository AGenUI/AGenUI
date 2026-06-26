#!/bin/bash
set -euo pipefail

# ============================================================================
# tests/integration/platforms/android/android.sh
# Run Android Playground integration tests (androidTest / instrumented tests)
#
# Prerequisites:
#   - Android device connected or emulator running
#   - ANDROID_HOME / ANDROID_SDK_ROOT set
#
# Usage:
#   ./tests/integration/platforms/android/android.sh [options]
#
# Options:
#   --device <serialNo>   Specify device serial number (default: first device from adb devices)
#   --output-dir <dir>    JUnit XML report output directory (default: reports/android)
#   --class <className>   Run only specified test class (fully qualified name)
#   -h, --help            Show help
#
# Examples:
#   ./tests/integration/platforms/android/android.sh
#   ./tests/integration/platforms/android/android.sh --output-dir /tmp/reports/android
#   ./tests/integration/platforms/android/android.sh --class com.amap.agenui.tests.InitializationTest
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../../../../scripts/common/_common.sh
source "${SCRIPT_DIR}/../../../../scripts/common/_common.sh"

# -------------------- Default parameters --------------------
ANDROID_PLAYGROUND="${AGENUI_ROOT}/playground/android"
OUTPUT_DIR="${AGENUI_ROOT}/reports/android"
DEVICE_ARG=""
CLASS_ARG=""

# Clear inherited ANDROID_SERIAL to avoid parent shell env interference with device selection.
# This script will export ANDROID_SERIAL after device selection is complete.
unset ANDROID_SERIAL

# -------------------- Argument parsing --------------------
show_help() {
    sed -n '5,20p' "$0" | sed 's/^# \?//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --device)     DEVICE_ARG="-s $2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --class)      CLASS_ARG="-e class $2"; shift 2 ;;
        -h|--help)    show_help ;;
        *)            error "Unknown argument: $1" ;;
    esac
done

# -------------------- Prerequisites --------------------
# Auto-detect ANDROID_HOME (macOS default path)
if [[ -z "${ANDROID_HOME:-}" ]]; then
    if [[ -z "${ANDROID_SDK_ROOT:-}" ]]; then
        if [[ -d "${HOME}/Library/Android/sdk" ]]; then
            export ANDROID_HOME="${HOME}/Library/Android/sdk"
            info "Auto-detected ANDROID_HOME: ${ANDROID_HOME}"
        else
            error "ANDROID_HOME not set and Android SDK not found at default path"
        fi
    else
        export ANDROID_HOME="${ANDROID_SDK_ROOT}"
    fi
fi

# Ensure JAVA_HOME points to Java 11~23 (required by Gradle 8.x / AGP 8.x)
_ensure_java_compatible() {
    local ver
    # If JAVA_HOME is set, check version is within compatible range [11, 23]
    if [[ -n "${JAVA_HOME:-}" && -x "${JAVA_HOME}/bin/java" ]]; then
        ver=$("${JAVA_HOME}/bin/java" -version 2>&1 | head -1 | sed -E 's/.*"([0-9]+).*/\1/')
        if [[ "$ver" -ge 11 && "$ver" -le 23 ]] 2>/dev/null; then
            return 0
        fi
        warn "Current JAVA_HOME (${JAVA_HOME}) uses Java ${ver}, Gradle 8.x requires 11~23"
    fi
    # macOS: search for compatible version by LTS priority (17 > 21 > 11), avoid too new versions
    if [[ -x /usr/libexec/java_home ]]; then
        local jh=""
        for v in 17 21 11; do
            jh=$(/usr/libexec/java_home -v ${v} 2>/dev/null) && break
        done
        if [[ -n "$jh" && -d "$jh" ]]; then
            export JAVA_HOME="$jh"
            info "Auto-set JAVA_HOME: ${JAVA_HOME}"
            return 0
        fi
    fi
    warn "No compatible Java runtime found (11~23), Gradle build may fail"
}
_ensure_java_compatible

[[ -d "$ANDROID_PLAYGROUND" ]] || error "Android Playground directory not found: ${ANDROID_PLAYGROUND}"
[[ -x "${ANDROID_PLAYGROUND}/gradlew" ]] || error "Executable gradlew not found: ${ANDROID_PLAYGROUND}/gradlew"

mkdir -p "$OUTPUT_DIR"

info "Android test report output directory: ${OUTPUT_DIR}"

# -------------------- Verify device available & lock single device --------------------
# Get list of all connected devices (exclude offline / unauthorized)
CONNECTED_DEVICES=($(adb devices 2>/dev/null | grep -E '\s(device|emulator)$' | awk '{print $1}'))
if [[ ${#CONNECTED_DEVICES[@]} -eq 0 ]]; then
    error "No available Android device or emulator detected, please connect a device or start an emulator"
fi

# Determine target device: --device arg > multi-device interactive selection (10s timeout) > single device auto-select
if [[ -n "${DEVICE_ARG}" ]]; then
    # --device parameter format: "-s <serial>", extract serial
    TARGET_SERIAL="${DEVICE_ARG#-s }"
elif [[ ${#CONNECTED_DEVICES[@]} -gt 1 ]]; then
    # Multiple devices without explicit selection, interactive with 10s timeout
    echo ""
    info "Detected ${#CONNECTED_DEVICES[@]} devices, please select target device (auto-select in 10s):"
    echo ""
    for i in "${!CONNECTED_DEVICES[@]}"; do
        # Get device model as supplementary info
        MODEL=$(adb -s "${CONNECTED_DEVICES[$i]}" shell getprop ro.product.model 2>/dev/null | tr -d '\r' || echo "unknown")
        # Mark emulators
        EMULATOR_TAG=""
        if [[ "${CONNECTED_DEVICES[$i]}" == emulator-* ]]; then
            EMULATOR_TAG=" [emulator]"
        fi
        printf "  [%d] %s  (%s)%s\n" "$((i+1))" "${CONNECTED_DEVICES[$i]}" "$MODEL" "$EMULATOR_TAG"
    done
    echo ""

    # Timeout auto-select logic: prefer emulator, fallback to first device
    _auto_select_device() {
        # Prefer emulator
        for d in "${CONNECTED_DEVICES[@]}"; do
            if [[ "$d" == emulator-* ]]; then
                echo "$d"
                return
            fi
        done
        # No emulator, select first device
        echo "${CONNECTED_DEVICES[0]}"
    }

    TARGET_SERIAL=""
    TIMEOUT_SEC=10
    ATTEMPTS=0
    while [[ -z "$TARGET_SERIAL" ]]; do
        if [[ $ATTEMPTS -ge $TIMEOUT_SEC ]]; then
            # Timeout, auto-select
            TARGET_SERIAL=$(_auto_select_device)
            echo ""
            info "Timeout, auto-selected: ${TARGET_SERIAL}"
            break
        fi
        # Use read -t 1 for per-second timeout loop with visual feedback
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
else
    TARGET_SERIAL="${CONNECTED_DEVICES[0]}"
fi

# Export ANDROID_SERIAL to ensure all subsequent adb calls and Gradle connectedTest use the same device
export ANDROID_SERIAL="$TARGET_SERIAL"
info "Target device: ${ANDROID_SERIAL} (${#CONNECTED_DEVICES[@]} device(s) detected)"

# Verify target device is actually online
if ! printf '%s\n' "${CONNECTED_DEVICES[@]}" | grep -qx "$ANDROID_SERIAL"; then
    error "Specified device ${ANDROID_SERIAL} not in connected device list: ${CONNECTED_DEVICES[*]}"
fi

# -------------------- Run integration tests via Gradle --------------------
info "Running Android integration tests..."
cd "$ANDROID_PLAYGROUND"

# Clean old Gradle test reports to prevent stale XML from polluting new reports
GRADLE_REPORT_DIR="${ANDROID_PLAYGROUND}/app/build/outputs/androidTest-results/connected"
if [[ -d "$GRADLE_REPORT_DIR" ]]; then
    info "Cleaning old Gradle test reports: ${GRADLE_REPORT_DIR}"
    rm -rf "$GRADLE_REPORT_DIR"
fi

GRADLE_CMD=(./gradlew app:connectedDebugAndroidTest)
if [[ -n "$CLASS_ARG" ]]; then
    # CLASS_ARG format: "-e class com.xxx.TestClass", extract class name
    CLASS_NAME="${CLASS_ARG#-e class }"
    GRADLE_CMD+=(-Pandroid.testInstrumentationRunnerArguments.class="${CLASS_NAME}")
fi

GRADLE_EXIT=0
"${GRADLE_CMD[@]}" 2>&1 | tee "${OUTPUT_DIR}/raw_output.txt" || GRADLE_EXIT=${PIPESTATUS[0]:-$?}

# Copy Gradle-generated XML reports (only if Gradle produced reports)
if [[ -d "$GRADLE_REPORT_DIR" ]]; then
    cp -r "$GRADLE_REPORT_DIR"/* "${OUTPUT_DIR}/" 2>/dev/null || true
    success "Android test reports copied to: ${OUTPUT_DIR}"
else
    warn "Gradle did not generate test reports (build may have failed), see ${OUTPUT_DIR}/raw_output.txt"
fi

if [[ "$GRADLE_EXIT" -ne 0 ]]; then
    warn "Android integration tests failed (exit code: ${GRADLE_EXIT})"
    exit "$GRADLE_EXIT"
fi

success "Android integration tests completed"
