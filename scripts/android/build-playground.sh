#!/usr/bin/env bash
#
# Build, sign, and verify the Android Playground release APK.
#
# Signing credentials are read only from environment variables:
#   ANDROID_PLAYGROUND_KEYSTORE_PATH
#   ANDROID_PLAYGROUND_KEYSTORE_PASSWORD
#   ANDROID_PLAYGROUND_KEY_ALIAS
#   ANDROID_PLAYGROUND_KEY_PASSWORD
#
# Usage:
#   ./scripts/android/build-playground.sh \
#     --version 1.2.1 \
#     --version-code 123

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PLAYGROUND_ROOT="${REPO_ROOT}/playground/android"

VERSION=""
VERSION_CODE=""
OUTPUT_DIR="${REPO_ROOT}/dist/android/playground"
CLEAN_BUILD=false

usage() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  --version <name>       Artifact and Android version name"
    echo "  --version-code <code>  Positive Android version code"
    echo "  --output-dir <path>    Output directory (default: dist/android/playground)"
    echo "  --clean                Clean before building"
    echo "  -h, --help             Show this help"
}

fail() {
    echo "[ERROR] $*" >&2
    exit 1
}

info() {
    echo "[INFO] $*"
}

require_environment_variable() {
    local variable_name="$1"
    [[ -n "${!variable_name:-}" ]] ||
        fail "Required environment variable is missing: ${variable_name}"
}

find_apksigner() {
    local sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
    local apksigner_path=""

    if command -v apksigner >/dev/null 2>&1; then
        command -v apksigner
        return
    fi

    [[ -n "${sdk_root}" ]] ||
        fail "ANDROID_HOME or ANDROID_SDK_ROOT is required to locate apksigner"
    [[ -d "${sdk_root}/build-tools" ]] ||
        fail "Android build-tools directory does not exist: ${sdk_root}/build-tools"

    apksigner_path="$(
        find "${sdk_root}/build-tools" -mindepth 2 -maxdepth 2 \
            -type f -name apksigner -print | sort | tail -n 1
    )"
    [[ -n "${apksigner_path}" ]] || fail "apksigner was not found in ${sdk_root}/build-tools"
    printf '%s\n' "${apksigner_path}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            [[ $# -ge 2 ]] || fail "--version requires a value"
            VERSION="$2"
            shift 2
            ;;
        --version-code)
            [[ $# -ge 2 ]] || fail "--version-code requires a value"
            VERSION_CODE="$2"
            shift 2
            ;;
        --output-dir)
            [[ $# -ge 2 ]] || fail "--output-dir requires a value"
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "Unknown argument: $1"
            ;;
    esac
done

if [[ -z "${VERSION}" ]]; then
    VERSION="$(
        sed -n 's/^#define AGENUI_VERSION "\([^"]*\)"/\1/p' \
            "${REPO_ROOT}/core/include/agenui_version.h"
    )"
fi
[[ "${VERSION}" =~ ^[0-9A-Za-z][0-9A-Za-z._-]*$ ]] ||
    fail "Invalid version for an artifact filename: ${VERSION}"

if [[ -z "${VERSION_CODE}" ]]; then
    VERSION_CODE="$(($(date +%s) / 60))"
fi
[[ "${VERSION_CODE}" =~ ^[1-9][0-9]*$ ]] ||
    fail "Version code must be a positive integer: ${VERSION_CODE}"
((VERSION_CODE <= 2100000000)) ||
    fail "Version code exceeds the Android maximum: ${VERSION_CODE}"

require_environment_variable ANDROID_PLAYGROUND_KEYSTORE_PATH
require_environment_variable ANDROID_PLAYGROUND_KEYSTORE_PASSWORD
require_environment_variable ANDROID_PLAYGROUND_KEY_ALIAS
require_environment_variable ANDROID_PLAYGROUND_KEY_PASSWORD
[[ -f "${ANDROID_PLAYGROUND_KEYSTORE_PATH}" ]] ||
    fail "Keystore does not exist: ${ANDROID_PLAYGROUND_KEYSTORE_PATH}"

APKSIGNER="$(find_apksigner)"
GRADLE_TASKS=()
if [[ "${CLEAN_BUILD}" == true ]]; then
    GRADLE_TASKS+=("clean")
fi
GRADLE_TASKS+=(":app:assembleRelease")

info "Building Android Playground ${VERSION} (${VERSION_CODE})"
(
    cd "${PLAYGROUND_ROOT}"
    ANDROID_PLAYGROUND_REQUIRE_SIGNING=true \
    ANDROID_PLAYGROUND_VERSION_NAME="${VERSION}" \
    ANDROID_PLAYGROUND_VERSION_CODE="${VERSION_CODE}" \
        ./gradlew "${GRADLE_TASKS[@]}" -Pagenui.sdk.source=true
)

SOURCE_APK="${PLAYGROUND_ROOT}/app/build/outputs/apk/release/app-release.apk"
[[ -f "${SOURCE_APK}" ]] || fail "Signed release APK was not produced: ${SOURCE_APK}"

mkdir -p "${OUTPUT_DIR}"
ARTIFACT_NAME="AGenUI-Playground-${VERSION}-android.apk"
OUTPUT_APK="${OUTPUT_DIR}/${ARTIFACT_NAME}"
CHECKSUM_FILE="${OUTPUT_APK}.sha256"

cp "${SOURCE_APK}" "${OUTPUT_APK}"
"${APKSIGNER}" verify --verbose --print-certs "${OUTPUT_APK}"

(
    cd "${OUTPUT_DIR}"
    shasum -a 256 "${ARTIFACT_NAME}" > "${ARTIFACT_NAME}.sha256"
    shasum -a 256 --check "${ARTIFACT_NAME}.sha256"
)

info "APK: ${OUTPUT_APK}"
info "SHA-256: ${CHECKSUM_FILE}"
