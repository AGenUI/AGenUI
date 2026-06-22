#!/bin/bash
set -euo pipefail

# ============================================================================
# scripts/harmony/build.sh
# Builds the AGenUI HarmonyOS HAR (including the lite variant and debug symbols).
#
# Prerequisite: the core/ directory (C++ core sources) must exist at the
# repository root. build-profile.json5 already references it directly via
# -DAMAP_AGENUI_DIR=../../../core, so no extra setup is required.
#
# Usage:
#   ./scripts/harmony/build.sh [options]
#
# Options:
#   --mode <release|debug>   Build mode, default: release
#   -o, --output <dir>       Artifact output directory,
#                            default: <repo_root>/dist/harmony/<mode>/
#   --no-package             Skip packaging of the lite HAR and symbol files;
#                            only produce the .har
#   -h, --help               Show this help
#
# Requirements:
#   1. DevEco Studio is installed (default path: /Applications/DevEco-Studio.app),
#      overridable via the DEVECO_HOME environment variable.
#   2. The project has been initialized once through DevEco IDE so that the
#      signing configuration and other generated assets exist.
#
# Examples:
#   ./scripts/harmony/build.sh
#   ./scripts/harmony/build.sh --mode debug --no-package
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../common/_common.sh
source "${SCRIPT_DIR}/../common/_common.sh"
# shellcheck source=../common/_build_id.sh
source "${SCRIPT_DIR}/../common/_build_id.sh"

# -------------------- Default parameters --------------------
BUILD_MODE="release"
CUSTOM_OUTPUT_DIR=""
DO_PACKAGE=true

HARMONY_PROJECT_ROOT="${PLATFORMS_DIR}/harmony"
HARMONY_MODULE="agenui"
HAR_OUTPUT="${HARMONY_PROJECT_ROOT}/${HARMONY_MODULE}/build/default/outputs/default/agenui.har"

# Third-party shared libraries stripped from the lite HAR variant
# (kept consistent with the original build_har.sh).
THIRDPARTY_LIBS=("libyoga.so" "libiksemel.so")

# Debug-log toggle header (LOG_ENABLE is disabled for release builds).
DEBUG_LOG_HEADER="${HARMONY_PROJECT_ROOT}/${HARMONY_MODULE}/src/main/cpp/log/hm_debug_log.h"

# -------------------- Argument parsing --------------------
show_help() {
    sed -n '6,29p' "$0" | sed 's/^# \?//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode)        BUILD_MODE="$2"; shift 2 ;;
        -o|--output)   CUSTOM_OUTPUT_DIR="$2"; shift 2 ;;
        --no-package)  DO_PACKAGE=false; shift ;;
        -h|--help)     show_help ;;
        *)             error "Unknown argument: $1" ;;
    esac
done

case "$BUILD_MODE" in
    release|debug) ;;
    *) error "Invalid --mode: ${BUILD_MODE} (only 'release' or 'debug' are supported)" ;;
esac

# Resolve relative -o path to absolute before build_har() changes cwd.
if [[ -n "$CUSTOM_OUTPUT_DIR" ]]; then
    CUSTOM_OUTPUT_DIR="$(cd "$(dirname "$CUSTOM_OUTPUT_DIR")" 2>/dev/null && pwd)/$(basename "$CUSTOM_OUTPUT_DIR")"
fi

[[ -d "$HARMONY_PROJECT_ROOT" ]] || error "Harmony project directory not found: ${HARMONY_PROJECT_ROOT}"
ensure_core_dir
check_version_consistency
fetch_build_id "harmony"

# Resolved after argument parsing so that BUILD_MODE is final.
SYMBOL_OUTPUT_DIR="${HARMONY_PROJECT_ROOT}/${HARMONY_MODULE}/build/default/outputs/default/symbol/${BUILD_MODE}"

# -------------------- DevEco toolchain --------------------
DEVECO_HOME="${DEVECO_HOME:-/Applications/DevEco-Studio.app/Contents}"
if [[ ! -d "$DEVECO_HOME" ]]; then
    error "DevEco Studio not found: ${DEVECO_HOME} (override via the DEVECO_HOME environment variable)"
fi
export DEVECO_SDK_HOME="${DEVECO_HOME}/sdk"
export PATH="${DEVECO_HOME}/tools/hvigor/bin:${DEVECO_HOME}/tools/ohpm/bin:${DEVECO_HOME}/tools/node/bin:${PATH}"

command -v hvigorw >/dev/null 2>&1 || error "hvigorw command not found; please verify that DevEco Studio is installed correctly"

# -------------------- Pre-release file backup / restore --------------------
# Source files temporarily mutated during the build (e.g. flipping LOG_ENABLE)
# are backed up here and restored on normal exit or interruption.
MODIFIED_FILES=()
restore_modified_files() {
    # Skip when no file has been backed up. Under `set -u`, expanding an empty
    # array as "${arr[@]}" triggers an unbound-variable error on bash 3.x; use
    # the `${arr[@]+...}` form so that empty arrays expand to nothing.
    if [[ ${#MODIFIED_FILES[@]} -eq 0 ]]; then
        return
    fi
    local file
    for file in "${MODIFIED_FILES[@]+"${MODIFIED_FILES[@]}"}"; do
        if [[ -f "${file}.bak" ]]; then
            mv "${file}.bak" "$file"
            info "Restored: ${file}"
        fi
    done
    MODIFIED_FILES=()
}
trap restore_modified_files EXIT

backup_file() {
    local file="$1"
    if [[ -f "$file" ]]; then
        cp "$file" "${file}.bak"
        MODIFIED_FILES+=("$file")
    fi
}

prepare_release_env() {
    if [[ "$BUILD_MODE" != "release" ]]; then
        return
    fi
    if [[ -f "$DEBUG_LOG_HEADER" ]]; then
        backup_file "$DEBUG_LOG_HEADER"
        sed -i '' 's/#define LOG_ENABLE 1/#define LOG_ENABLE 0/' "$DEBUG_LOG_HEADER"
        info "Disabled LOG_ENABLE (${DEBUG_LOG_HEADER})"
    fi
}

# -------------------- Resolve git commit short SHA --------------------
# Echoes the 7-char short SHA of HEAD (matches the convention in _common.sh's
# generate_run_key / collect_git_metadata). Echoes nothing and warns when the
# value cannot be resolved (no git, no commits, etc.), letting callers decide
# how to degrade. Also warns once when the working tree is dirty so the user
# knows the commit no longer uniquely identifies the artifact.
resolve_commit_short_sha() {
    if ! git -C "$AGENUI_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        warn "Not a git repository; commit-id suffix will be skipped"
        return 0
    fi

    local commit
    commit=$(git -C "$AGENUI_ROOT" rev-parse --short=7 HEAD 2>/dev/null || true)
    if [[ -z "$commit" ]]; then
        warn "Failed to resolve git commit short SHA; commit-id suffix will be skipped"
        return 0
    fi

    if [[ -n "$(git -C "$AGENUI_ROOT" status --porcelain 2>/dev/null)" ]]; then
        warn "Working tree is dirty; lite HAR version will reference commit ${commit} but may include uncommitted changes"
    fi

    echo "$commit"
}

# Rewrites the "version" field in the given oh-package.json5 to "<current>.<commit>".
# No-ops (with a warn) when the file or the version field cannot be read.
stamp_commit_into_oh_package() {
    local file="$1"
    local commit="$2"

    if [[ ! -f "$file" ]]; then
        warn "oh-package.json5 not found in extracted HAR (${file}); leaving version untouched"
        return 0
    fi

    local current
    current=$(grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' "$file" \
        | sed 's/.*"version"[[:space:]]*:[[:space:]]*"//;s/"//' || true)
    if [[ -z "$current" ]]; then
        warn "Failed to read existing version from ${file}; leaving version untouched"
        return 0
    fi

    # Use SemVer 2.0.0 build-metadata syntax ("+<identifier>"). Build metadata
    # MUST be ignored when determining version precedence (semver §10), which
    # matches the intent here: tag the artifact with its source commit without
    # affecting dependency resolution. A "." separator would yield a 4-segment
    # version that ohpm's validator rejects as non-semver.
    local new_version="${current}+${commit}"
    sed -i '' "s/\(\"version\"[[:space:]]*:[[:space:]]*\"\)[^\"]*\"/\1${new_version}\"/" "$file"
    info "Stamped lite HAR version with commit short SHA: ${current} -> ${new_version}"
}

# -------------------- Build the HAR --------------------
build_har() {
    info "Building ${BUILD_MODE} HAR (module=${HARMONY_MODULE})"
    cd "$HARMONY_PROJECT_ROOT"

    # Wipe the previous module build cache to guarantee a clean rebuild.
    rm -rf "${HARMONY_MODULE}/build"

    hvigorw assembleHar \
        --mode module \
        -p "module=${HARMONY_MODULE}@default" \
        -p product=default \
        -p "buildMode=${BUILD_MODE}" \
        --no-daemon | cat

    [[ -f "$HAR_OUTPUT" ]] || error "HAR build failed; artifact not found: ${HAR_OUTPUT}"
    info "HAR build succeeded: $(du -h "$HAR_OUTPUT" | cut -f1) (${HAR_OUTPUT})"
}

# -------------------- Generate the lite HAR (third-party .so stripped) --------------------
# The lite HAR (and only the lite HAR) is stamped with the current commit's
# short SHA in its embedded oh-package.json5 (e.g. version "1.0.0+91084fe"),
# so downstream consumers of the lite variant can trace the artifact back to
# its source commit. The full agenui.har keeps the version from the source
# oh-package.json5 unchanged. The "+<sha>" form is SemVer build metadata,
# which ohpm accepts and which does NOT affect version precedence.
create_lite_har() {
    local target_dir="$1"
    local work_dir
    work_dir="$(mktemp -d)"

    tar xzf "$HAR_OUTPUT" -C "$work_dir"
    local lib
    for lib in "${THIRDPARTY_LIBS[@]}"; do
        rm -f "${work_dir}/package/libs/arm64-v8a/${lib}"
        info "Removed from lite HAR: ${lib}"
    done

    local commit
    commit="$(resolve_commit_short_sha)"
    if [[ -n "$commit" ]]; then
        stamp_commit_into_oh_package "${work_dir}/package/oh-package.json5" "$commit"
    fi

    tar czf "${target_dir}/agenui-lite.har" -C "$work_dir" package
    rm -rf "$work_dir"
}

# -------------------- Package artifacts (HAR + lite + symbols) --------------------
# Unified output rule: AGenUI/dist/<plat>/<config>/...
# All platforms follow the same layout so that CI and downstream scripts (e.g.
# the ext repo) can locate artifacts at fixed paths. Overwrites on each build;
# use git tags / CI artifacts to retain history.
package_output() {
    local output_dir
    if [[ -n "$CUSTOM_OUTPUT_DIR" ]]; then
        output_dir="$CUSTOM_OUTPUT_DIR"
    else
        output_dir="${AGENUI_ROOT}/dist/harmony/${BUILD_MODE}"
    fi
    local symbols_dir="${output_dir}/symbols"

    # Wipe previous run so stale artifacts cannot linger.
    rm -rf "$output_dir"
    mkdir -p "$output_dir"

    info "Packaging artifacts into: ${output_dir}"

    cp "$HAR_OUTPUT" "${output_dir}/agenui.har"
    create_lite_har "$output_dir"

    # Debug builds keep symbols in the .so itself; only collect stripped
    # symbol artifacts for release builds.
    if [[ "$BUILD_MODE" == "release" ]]; then
        mkdir -p "$symbols_dir"

        local symbol_so="${SYMBOL_OUTPUT_DIR}/arm64-v8a/liba2ui-capi.so"
        if [[ -f "$symbol_so" ]]; then
            cp "$symbol_so" "$symbols_dir/"
        else
            warn "Unstripped .so not found (${symbol_so})"
        fi

        local sourcemap="${SYMBOL_OUTPUT_DIR}/agenui-sourceMaps.map"
        if [[ -f "$sourcemap" ]]; then
            cp "$sourcemap" "$symbols_dir/"
        fi
    fi

    info "Unified artifact directory: ${output_dir}"
    echo "  ${output_dir}/agenui.har        $(du -h "${output_dir}/agenui.har" | cut -f1)"
    echo "  ${output_dir}/agenui-lite.har   $(du -h "${output_dir}/agenui-lite.har" | cut -f1)"
    [[ -f "${symbols_dir}/liba2ui-capi.so" ]] && 
        echo "  ${symbols_dir}/liba2ui-capi.so       $(du -h "${symbols_dir}/liba2ui-capi.so" | cut -f1)" || true
    [[ -f "${symbols_dir}/agenui-sourceMaps.map" ]] && 
        echo "  ${symbols_dir}/agenui-sourceMaps.map $(du -h "${symbols_dir}/agenui-sourceMaps.map" | cut -f1)" || true
}

# -------------------- Main --------------------
prepare_release_env
build_har
if [[ "$DO_PACKAGE" == true ]]; then
    package_output
fi

print_build_version
success "Harmony build finished (${BUILD_MODE})"
