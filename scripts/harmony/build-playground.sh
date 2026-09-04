#!/usr/bin/env bash
#
# Build, sign, and verify the HarmonyOS Playground release app package (.app).
#
# The output is the App Pack that AppGallery expects, signed with the release
# certificate and release profile. Signing credentials are read only from
# environment variables:
#   HARMONY_PLAYGROUND_KEYSTORE_PATH      release .p12
#   HARMONY_PLAYGROUND_KEYSTORE_PASSWORD  keystore password
#   HARMONY_PLAYGROUND_KEY_ALIAS          key alias (default: release)
#   HARMONY_PLAYGROUND_KEY_PASSWORD       key password
#   HARMONY_PLAYGROUND_CERT_PATH          release .cer
#   HARMONY_PLAYGROUND_PROFILE_PATH       release .p7b
#   HARMONY_PLAYGROUND_BUNDLE_NAME        optional, default com.agenui.playground
#
# The bundle name bound in the release profile differs from the one committed
# in AppScope/app.json5, so this script injects the release identity plus the
# signing config for the duration of the build and restores both files
# afterwards. Nothing secret is ever written into the repository.
#
# Usage:
#   ./scripts/harmony/build-playground.sh --version 1.4.0 --version-code 1004000

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../common/_common.sh
source "${SCRIPT_DIR}/../common/_common.sh"

PLAYGROUND_ROOT="${AGENUI_ROOT}/playground/harmony"
BUILD_PROFILE="${PLAYGROUND_ROOT}/build-profile.json5"
APP_JSON5="${PLAYGROUND_ROOT}/AppScope/app.json5"
BUILD_OUTPUT_DIR="${PLAYGROUND_ROOT}/build/outputs/default"

VERSION=""
VERSION_CODE=""
ARTIFACT_NAME=""
OUTPUT_DIR="${AGENUI_ROOT}/dist/harmony/playground"
CLEAN_BUILD=false

usage() {
    echo "Usage: $0 [options]"
    echo
    echo "Options:"
    echo "  --version <name>       Artifact and app version name"
    echo "                         (default: AGENUI_VERSION from core/include/agenui_version.h)"
    echo "  --version-code <code>  Positive integer app version code"
    echo "                         (default: major*1000000 + minor*1000 + patch)"
    echo "  --output-dir <path>    Output directory (default: dist/harmony/playground)"
    echo "  --artifact-name <name> Artifact filename"
    echo "                         (default: AGenUI-Playground-<version>-harmony.app)"
    echo "  --clean                Run hvigorw clean before building"
    echo "  -h, --help             Show this help"
}

require_environment_variable() {
    local variable_name="$1"
    [[ -n "${!variable_name:-}" ]] ||
        error "Required environment variable is missing: ${variable_name}"
}

# -------------------- Argument parsing --------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            [[ $# -ge 2 ]] || error "--version requires a value"
            VERSION="$2"; shift 2 ;;
        --version-code)
            [[ $# -ge 2 ]] || error "--version-code requires a value"
            VERSION_CODE="$2"; shift 2 ;;
        --output-dir)
            [[ $# -ge 2 ]] || error "--output-dir requires a value"
            OUTPUT_DIR="$2"; shift 2 ;;
        --artifact-name)
            [[ $# -ge 2 ]] || error "--artifact-name requires a value"
            ARTIFACT_NAME="$2"; shift 2 ;;
        --clean)
            CLEAN_BUILD=true; shift ;;
        -h|--help)
            usage; exit 0 ;;
        *)
            error "Unknown argument: $1" ;;
    esac
done

[[ -d "$PLAYGROUND_ROOT" ]] || error "Harmony playground project not found: ${PLAYGROUND_ROOT}"
[[ -f "$BUILD_PROFILE" ]]   || error "Project build profile not found: ${BUILD_PROFILE}"
[[ -f "$APP_JSON5" ]]       || error "App config not found: ${APP_JSON5}"
ensure_core_dir

require_environment_variable HARMONY_PLAYGROUND_KEYSTORE_PATH
require_environment_variable HARMONY_PLAYGROUND_KEYSTORE_PASSWORD
require_environment_variable HARMONY_PLAYGROUND_KEY_PASSWORD
require_environment_variable HARMONY_PLAYGROUND_CERT_PATH
require_environment_variable HARMONY_PLAYGROUND_PROFILE_PATH

KEY_ALIAS="${HARMONY_PLAYGROUND_KEY_ALIAS:-release}"
BUNDLE_NAME="${HARMONY_PLAYGROUND_BUNDLE_NAME:-com.agenui.playground}"

for _credential in HARMONY_PLAYGROUND_KEYSTORE_PATH \
                   HARMONY_PLAYGROUND_CERT_PATH \
                   HARMONY_PLAYGROUND_PROFILE_PATH; do
    [[ -f "${!_credential}" ]] || error "${_credential} points to a missing file: ${!_credential}"
done

# -------------------- Version resolution --------------------
if [[ -z "$VERSION" ]]; then
    VERSION="$(sed -n 's/^#define AGENUI_VERSION "\([^"]*\)"/\1/p' \
        "${CORE_DIR}/include/agenui_version.h")"
    [[ -n "$VERSION" ]] || error "Could not read AGENUI_VERSION from ${CORE_DIR}/include/agenui_version.h; pass --version"
fi
[[ "$VERSION" =~ ^[0-9A-Za-z][0-9A-Za-z._-]*$ ]] ||
    error "Invalid version for an artifact filename: ${VERSION}"

# AppGallery rejects a versionCode that is not strictly greater than the one
# already uploaded, so derive a stable monotonic value from the version name.
if [[ -z "$VERSION_CODE" ]]; then
    if [[ "$VERSION" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
        VERSION_CODE=$(( ${BASH_REMATCH[1]} * 1000000 + ${BASH_REMATCH[2]} * 1000 + ${BASH_REMATCH[3]} ))
    else
        error "Cannot derive a version code from '${VERSION}'; pass --version-code explicitly"
    fi
fi
[[ "$VERSION_CODE" =~ ^[1-9][0-9]*$ ]] ||
    error "Version code must be a positive integer: ${VERSION_CODE}"
(( VERSION_CODE <= 2147483647 )) ||
    error "Version code exceeds the HarmonyOS maximum: ${VERSION_CODE}"

if [[ -z "$ARTIFACT_NAME" ]]; then
    ARTIFACT_NAME="AGenUI-Playground-${VERSION}-harmony.app"
fi
[[ "$ARTIFACT_NAME" =~ ^[0-9A-Za-z][0-9A-Za-z._-]*$ ]] ||
    error "Invalid artifact filename: ${ARTIFACT_NAME}"

# -------------------- DevEco toolchain --------------------
# Same resolution order as scripts/harmony/build.sh: explicit DEVECO_HOME, then
# the default macOS install, then the headless Command Line Tools.
if [[ -z "${DEVECO_HOME:-}" ]]; then
    if [[ -d "/Applications/DevEco-Studio.app/Contents" ]]; then
        DEVECO_HOME="/Applications/DevEco-Studio.app/Contents"
    elif [[ -n "${COMMAND_LINE_TOOLS_HOME:-}" && -d "${COMMAND_LINE_TOOLS_HOME}" ]]; then
        DEVECO_HOME="${COMMAND_LINE_TOOLS_HOME}"
    else
        error "DevEco Studio or Command Line Tools not found. Set DEVECO_HOME or COMMAND_LINE_TOOLS_HOME."
    fi
fi
info "Using DevEco toolchain at: ${DEVECO_HOME}"
export DEVECO_SDK_HOME="${DEVECO_HOME}/sdk"

for _tool_dir in "${DEVECO_HOME}/tools/hvigor/bin" \
                 "${DEVECO_HOME}/tools/ohpm/bin" \
                 "${DEVECO_HOME}/tools/node/bin" \
                 "${DEVECO_HOME}/hvigor/bin" \
                 "${DEVECO_HOME}/ohpm/bin" \
                 "${DEVECO_HOME}/node/bin"; do
    [[ -d "$_tool_dir" ]] && PATH="${_tool_dir}:${PATH}"
done
export PATH

command -v hvigorw >/dev/null 2>&1 ||
    error "hvigorw not found; verify that DevEco Studio or Command Line Tools is installed correctly"

NODE_BIN="$(command -v node || true)"
[[ -n "$NODE_BIN" ]] || error "node not found; it is required to generate the hvigor signing material"

# -------------------- Scratch dir + restore trap --------------------
# Holds a copy of the private key and the generated key material, so it is
# removed on every exit path, not just the happy one.
SIGN_WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/agenui-hm-sign.XXXXXX")"
chmod 700 "$SIGN_WORKDIR"

cleanup() {
    local exit_code=$?
    if [[ -f "${SIGN_WORKDIR}/build-profile.json5.bak" ]]; then
        cp "${SIGN_WORKDIR}/build-profile.json5.bak" "$BUILD_PROFILE"
        info "Restored: ${BUILD_PROFILE}"
    fi
    if [[ -f "${SIGN_WORKDIR}/app.json5.bak" ]]; then
        cp "${SIGN_WORKDIR}/app.json5.bak" "$APP_JSON5"
        info "Restored: ${APP_JSON5}"
    fi
    rm -rf "$SIGN_WORKDIR"
    return $exit_code
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

cp "$BUILD_PROFILE" "${SIGN_WORKDIR}/build-profile.json5.bak"
cp "$APP_JSON5"     "${SIGN_WORKDIR}/app.json5.bak"

# -------------------- Inject release signing + app identity --------------------
info "Preparing release signing material for ${BUNDLE_NAME} ${VERSION} (${VERSION_CODE})"

# The request goes through stdin so that neither password appears in the process
# argument list.
REQUEST_JSON="$(
    SIGN_WORKDIR="$SIGN_WORKDIR" BUILD_PROFILE="$BUILD_PROFILE" APP_JSON5="$APP_JSON5" \
    KEY_ALIAS="$KEY_ALIAS" BUNDLE_NAME="$BUNDLE_NAME" VERSION="$VERSION" VERSION_CODE="$VERSION_CODE" \
    HARMONY_PLAYGROUND_KEYSTORE_PASSWORD="$HARMONY_PLAYGROUND_KEYSTORE_PASSWORD" \
    HARMONY_PLAYGROUND_KEY_PASSWORD="$HARMONY_PLAYGROUND_KEY_PASSWORD" \
    "$NODE_BIN" -e '
        const read = (name) => process.env[name];
        process.stdout.write(JSON.stringify({
            workDir: read("SIGN_WORKDIR"),
            buildProfile: read("BUILD_PROFILE"),
            appJson5: read("APP_JSON5"),
            storeFile: read("HARMONY_PLAYGROUND_KEYSTORE_PATH"),
            certPath: read("HARMONY_PLAYGROUND_CERT_PATH"),
            profilePath: read("HARMONY_PLAYGROUND_PROFILE_PATH"),
            keyAlias: read("KEY_ALIAS"),
            storePassword: read("HARMONY_PLAYGROUND_KEYSTORE_PASSWORD"),
            keyPassword: read("HARMONY_PLAYGROUND_KEY_PASSWORD"),
            bundleName: read("BUNDLE_NAME"),
            versionName: read("VERSION"),
            versionCode: Number(read("VERSION_CODE")),
        }));
    '
)"

SIGN_RESULT="$(printf '%s' "$REQUEST_JSON" | "$NODE_BIN" "${SCRIPT_DIR}/hvigor-sign-material.js")"

if ! printf '%s' "$SIGN_RESULT" | grep -q '"ok":true'; then
    error "Failed to prepare signing material: $(printf '%s' "$SIGN_RESULT" | sed -n 's/.*"error":"\([^"]*\)".*/\1/p')"
fi
info "Signing material ready in scratch dir"

# -------------------- Build --------------------
cd "$PLAYGROUND_ROOT"

if [[ ! -d "${PLAYGROUND_ROOT}/oh_modules" ]]; then
    info "Installing ohpm dependencies"
    ohpm install --all
fi

if [[ "$CLEAN_BUILD" == true ]]; then
    info "Cleaning previous build output"
    hvigorw clean --no-daemon | cat
fi

rm -rf "$BUILD_OUTPUT_DIR"

info "Building release app package"
hvigorw assembleApp \
    --mode project \
    -p product=default \
    -p buildMode=release \
    --no-daemon | cat

SIGNED_APP="$(ls -1 "${BUILD_OUTPUT_DIR}"/*-signed.app 2>/dev/null | head -n 1 || true)"
[[ -n "$SIGNED_APP" && -f "$SIGNED_APP" ]] ||
    error "assembleApp did not produce a signed app package under ${BUILD_OUTPUT_DIR}"
info "App package built: $(du -h "$SIGNED_APP" | cut -f1) (${SIGNED_APP})"

# -------------------- Verify --------------------
PACK_INFO="${BUILD_OUTPUT_DIR}/pack.info"
[[ -f "$PACK_INFO" ]] || error "pack.info not found: ${PACK_INFO}"

"$NODE_BIN" -e '
    const fs = require("fs");
    const [packInfoPath, expectedBundle, expectedName, expectedCode] = process.argv.slice(1);
    const app = JSON.parse(fs.readFileSync(packInfoPath, "utf8")).summary.app;
    const problems = [];
    if (app.bundleName !== expectedBundle) {
        problems.push(`bundleName is ${app.bundleName}, expected ${expectedBundle}`);
    }
    if (app.version.name !== expectedName) {
        problems.push(`versionName is ${app.version.name}, expected ${expectedName}`);
    }
    if (app.version.code !== Number(expectedCode)) {
        problems.push(`versionCode is ${app.version.code}, expected ${expectedCode}`);
    }
    if (problems.length > 0) {
        console.error("[ERROR] pack.info mismatch: " + problems.join("; "));
        process.exit(1);
    }
    console.log(`[INFO] pack.info verified: ${app.bundleName} ${app.version.name} (${app.version.code})`);
' "$PACK_INFO" "$BUNDLE_NAME" "$VERSION" "$VERSION_CODE"

# A release build signed with a debug profile installs on a registered device
# but is rejected by AppGallery, and hvigor does not treat that as an error.
# Confirm the embedded profile really is a release/app_gallery one.
SIGN_TOOL="$(find "${DEVECO_SDK_HOME}" -name hap-sign-tool.jar -type f 2>/dev/null | head -n 1 || true)"
if [[ -n "$SIGN_TOOL" ]] && command -v java >/dev/null 2>&1 && command -v openssl >/dev/null 2>&1; then
    info "Verifying app signature"
    VERIFY_DIR="${SIGN_WORKDIR}/verify"
    mkdir -p "$VERIFY_DIR"
    java -jar "$SIGN_TOOL" verify-app \
        -inFile "$SIGNED_APP" \
        -outCertChain "${VERIFY_DIR}/chain.cer" \
        -outProfile "${VERIFY_DIR}/profile.p7b" >/dev/null 2>&1 ||
        error "Signature verification failed for ${SIGNED_APP}"

    PROFILE_TYPE="$(openssl smime -verify -in "${VERIFY_DIR}/profile.p7b" -inform DER \
        -noverify -nosigs 2>/dev/null | head -c 2000 |
        sed -n 's/.*"type":"\([^"]*\)".*/\1/p' | head -n 1)"
    DISTRIBUTION_TYPE="$(openssl smime -verify -in "${VERIFY_DIR}/profile.p7b" -inform DER \
        -noverify -nosigs 2>/dev/null | head -c 2000 |
        sed -n 's/.*"app-distribution-type":"\([^"]*\)".*/\1/p' | head -n 1)"

    [[ "$PROFILE_TYPE" == "release" ]] ||
        error "Embedded profile type is '${PROFILE_TYPE:-unknown}', expected 'release'; the app would be rejected by AppGallery"
    [[ "$DISTRIBUTION_TYPE" == "app_gallery" ]] ||
        error "Embedded distribution type is '${DISTRIBUTION_TYPE:-unknown}', expected 'app_gallery'"
    info "Signature verified: profile type=release, distribution=app_gallery"
else
    warn "Skipped signature verification (needs hap-sign-tool.jar, java and openssl)"
    warn "Verify manually that the package is release-signed before uploading"
fi

# -------------------- Package --------------------
mkdir -p "$OUTPUT_DIR"
OUTPUT_APP="${OUTPUT_DIR}/${ARTIFACT_NAME}"
cp "$SIGNED_APP" "$OUTPUT_APP"

(
    cd "$OUTPUT_DIR"
    shasum -a 256 "$ARTIFACT_NAME" > "${ARTIFACT_NAME}.sha256"
    shasum -a 256 --check "${ARTIFACT_NAME}.sha256" >/dev/null
)

# Crash-symbol table for the AGC "crash analysis" upload; optional.
SYMBOL_ZIP="${BUILD_OUTPUT_DIR}/symbol/release/app-symbol.zip"
if [[ -f "$SYMBOL_ZIP" ]]; then
    SYMBOL_NAME="${ARTIFACT_NAME%.app}-symbol.zip"
    cp "$SYMBOL_ZIP" "${OUTPUT_DIR}/${SYMBOL_NAME}"
    info "Symbols: ${OUTPUT_DIR}/${SYMBOL_NAME}"
fi

info "App package: ${OUTPUT_APP}  $(du -h "$OUTPUT_APP" | cut -f1)"
info "SHA-256:     ${OUTPUT_APP}.sha256"
success "HarmonyOS Playground ${VERSION} (${VERSION_CODE}) built and signed for AppGallery"
