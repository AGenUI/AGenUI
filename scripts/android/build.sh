#!/bin/bash
set -euo pipefail

# ============================================================================
# scripts/android/build.sh
# Build the Android AGenUI AAR / publish to local Maven.
#
# Prerequisites: core/ directory must exist at the repo root (C++ core source).
# AGENUI_CPP_ROOT in CMakeLists.txt already points to core/ directly;
# no additional preparation is required.
#
# Usage:
#   ./scripts/android/build.sh [options]
#
# Options:
#   --task <gradleTask>     Gradle task to run, default: assembleRelease
#                           Other common values:
#                             assembleDebug
#                             publishReleasePublicationToLocalMavenRepository
#   --debug                 Equivalent to --task assembleDebug
#   --publish-local         Equivalent to --task publishReleasePublicationToLocalMavenRepository
#   --publish-maven         Publish AAR to remote Maven repository.
#                           Requires MAVEN_URL / MAVEN_USERNAME / MAVEN_PASSWORD env vars.
#                           Optionally set AGENUI_MAVEN_GROUP / AGENUI_MAVEN_ARTIFACT env vars
#                           to override groupId/artifactId (defaults: SDK_GROUP_ID/SDK_ARTIFACT_ID).
#   --yoga-prebuilt <dir>   Path to prebuilt yoga artifacts directory.
#                           Expected structure: {dir}/include/yoga/ + {dir}/libs/libyoga.so
#                           If not specified, yoga is fetched from GitHub via FetchContent.
#   --no-yoga-in-aar        Exclude libyoga.so from AAR output (use with --yoga-prebuilt)
#   --with-symbols          Emit lib*.so.debug companion files for the release build
#                           AND package them into <name>-symbols.aar. Wires up
#                           CMake (-DAGENUI_EMIT_DEBUG_SYMBOLS=ON) + Gradle
#                           (-PagenuiEmitDebugSymbols=true). Release builds only;
#                           no effect for --debug.
#   --clean                 Run ./gradlew clean before building
#   -h, --help              Show this help message
#
# Examples:
#   ./scripts/android/build.sh                       # default assembleRelease
#   ./scripts/android/build.sh --debug --clean
#   ./scripts/android/build.sh --publish-local
#   ./scripts/android/build.sh --yoga-prebuilt ./yoga_prebuilt/android/arm64-v8a/Test
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../common/_common.sh
source "${SCRIPT_DIR}/../common/_common.sh"
# shellcheck source=../common/_build_id.sh
source "${SCRIPT_DIR}/../common/_build_id.sh"

# -------------------- Defaults --------------------
GRADLE_TASK="assembleRelease"
DO_CLEAN=false
YOGA_PREBUILT_DIR=""
YOGA_INCLUDE_IN_AAR=""
PUBLISH_MAVEN=false
WITH_SYMBOLS=false

ANDROID_PROJECT_ROOT="${PLATFORMS_DIR}/android"

# -------------------- Argument parsing --------------------
show_help() {
    sed -n '5,42p' "$0" | sed -E 's/^#( |$)//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --task)            GRADLE_TASK="$2"; shift 2 ;;
        --debug)           GRADLE_TASK="assembleDebug"; shift ;;
        --publish-local)   GRADLE_TASK="publishReleasePublicationToLocalMavenRepository"; shift ;;
        --publish-maven)   PUBLISH_MAVEN=true; shift ;;
        --yoga-prebuilt)   YOGA_PREBUILT_DIR="$2"; shift 2 ;;
        --no-yoga-in-aar)  YOGA_INCLUDE_IN_AAR="false"; shift ;;
        --with-symbols)    WITH_SYMBOLS=true; shift ;;
        --clean)           DO_CLEAN=true; shift ;;
        -h|--help)         show_help ;;
        *)                 error "Unknown argument: $1" ;;
    esac
done

[[ -d "$ANDROID_PROJECT_ROOT" ]] || error "Android project directory not found: ${ANDROID_PROJECT_ROOT}"
[[ -x "${ANDROID_PROJECT_ROOT}/gradlew" ]] || error "Executable gradlew not found: ${ANDROID_PROJECT_ROOT}/gradlew"

# -------------------- Ensure local.properties exists --------------------
if [[ ! -f "${ANDROID_PROJECT_ROOT}/local.properties" ]]; then
    SDK_DIR="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
    if [[ -z "$SDK_DIR" ]] && command -v python3 &>/dev/null; then
        # Try common default paths
        for candidate in "$HOME/Library/Android/sdk" "$HOME/Android/Sdk" "/opt/android-sdk"; do
            if [[ -d "$candidate" ]]; then SDK_DIR="$candidate"; break; fi
        done
    fi
    if [[ -n "${SDK_DIR:-}" ]]; then
        echo "sdk.dir=${SDK_DIR}" > "${ANDROID_PROJECT_ROOT}/local.properties"
        info "Auto-generated local.properties (sdk.dir=${SDK_DIR})"
    else
        warn "${ANDROID_PROJECT_ROOT}/local.properties not found and ANDROID_HOME not set."
        warn "Please set ANDROID_HOME or create local.properties with sdk.dir=<path>"
    fi
fi

ensure_core_dir
check_version_consistency
fetch_build_id "android"

# -------------------- Run Gradle --------------------
cd "$ANDROID_PROJECT_ROOT"

if [[ "$DO_CLEAN" == true ]]; then
    info "Running clean..."
    ./gradlew clean | cat
fi

# -------------------- Yoga prebuilt configuration --------------------
GRADLE_EXTRA_ARGS=""
if [[ -n "$YOGA_PREBUILT_DIR" ]]; then
    # Resolve to absolute path
    YOGA_PREBUILT_DIR="$(cd "$YOGA_PREBUILT_DIR" 2>/dev/null && pwd || echo "$YOGA_PREBUILT_DIR")"
    if [[ ! -f "${YOGA_PREBUILT_DIR}/libs/libyoga.so" ]]; then
        error "Yoga prebuilt directory invalid: ${YOGA_PREBUILT_DIR}/libs/libyoga.so not found"
    fi
    info "Using prebuilt yoga from: ${YOGA_PREBUILT_DIR}"
    GRADLE_EXTRA_ARGS="-PYOGA_PREBUILT_DIR=${YOGA_PREBUILT_DIR}"
fi
if [[ -n "$YOGA_INCLUDE_IN_AAR" ]]; then
    GRADLE_EXTRA_ARGS="${GRADLE_EXTRA_ARGS} -PYOGA_INCLUDE_IN_AAR=${YOGA_INCLUDE_IN_AAR}"
    info "Yoga include in AAR: ${YOGA_INCLUDE_IN_AAR}"
fi

# -------------------- Native debug symbols --------------------
# --with-symbols turns on the symbol-splitting POST_BUILD step in CMake AND the
# assembleReleaseSymbols Gradle task (finalized after assembleRelease). One
# property pipes through both layers. Effective only for release builds; for
# --debug, CMake skips the split internally.
if [[ "$WITH_SYMBOLS" == true ]]; then
    GRADLE_EXTRA_ARGS="${GRADLE_EXTRA_ARGS} -PagenuiEmitDebugSymbols=true"
    info "Native debug symbols: enabled (lib*.so.debug companion + <name>-symbols.aar)"
fi

info "Running Gradle task: ${GRADLE_TASK}"
./gradlew $GRADLE_EXTRA_ARGS "$GRADLE_TASK" | cat

# -------------------- Publish to remote Maven (if --publish-maven) --------------------
if [[ "$PUBLISH_MAVEN" == true ]]; then
    if [[ -z "${AGENUI_FULL_VERSION:-}" ]]; then
        error "AGENUI_FULL_VERSION is not set (fetch_build_id must be called before --publish-maven)"
    fi
    if [[ -z "${MAVEN_URL:-}" ]]; then
        error "MAVEN_URL env var is required for --publish-maven. Set MAVEN_URL / MAVEN_USERNAME / MAVEN_PASSWORD before running this script."
    fi

    # Optional Maven coordinates override (caller sets these env vars)
    MAVEN_GRADLE_ARGS="-PmavenVersion=${AGENUI_FULL_VERSION}"
    if [[ -n "${AGENUI_MAVEN_GROUP:-}" ]]; then
        MAVEN_GRADLE_ARGS="${MAVEN_GRADLE_ARGS} -PmavenGroupId=${AGENUI_MAVEN_GROUP}"
    fi
    if [[ -n "${AGENUI_MAVEN_ARTIFACT:-}" ]]; then
        MAVEN_GRADLE_ARGS="${MAVEN_GRADLE_ARGS} -PmavenArtifactId=${AGENUI_MAVEN_ARTIFACT}"
    fi

    info "Publishing to Maven: ${MAVEN_URL}"
    info "  version=${AGENUI_FULL_VERSION}${AGENUI_MAVEN_GROUP:+, groupId=${AGENUI_MAVEN_GROUP}}${AGENUI_MAVEN_ARTIFACT:+, artifactId=${AGENUI_MAVEN_ARTIFACT}}"
    info "Running Gradle publish task: publishReleasePublicationToRemoteMavenRepository"
    if ./gradlew $GRADLE_EXTRA_ARGS $MAVEN_GRADLE_ARGS publishReleasePublicationToRemoteMavenRepository | cat; then
        success "Maven publish complete: ${AGENUI_MAVEN_GROUP:-default}:${AGENUI_MAVEN_ARTIFACT:-default}:${AGENUI_FULL_VERSION}"
        # Export published version to a temp file for caller to read (wrapper script needs this for amap version bump)
        echo "$AGENUI_FULL_VERSION" > "${SCRIPT_DIR}/.published_version"
    else
        error "Maven publish FAILED. Check the error output above."
    fi
fi

# -------------------- Print output artifact path --------------------
AAR_DIR="${ANDROID_PROJECT_ROOT}/build/outputs/aar"
if [[ -d "$AAR_DIR" ]]; then
    info "Gradle AAR intermediate output directory: ${AAR_DIR}"
    find "$AAR_DIR" -name '*.aar' -maxdepth 1 -type f -exec ls -lh {} \; 2>/dev/null || true
fi

# -------------------- Copy artifacts to unified output directory --------------------
# Unified output layout: AGenUI/dist/<plat>/<config>/...
# All platforms follow the same layout so CI can collect artifacts
# and downstream scripts (e.g. ext repos) can locate them at fixed paths.
case "$GRADLE_TASK" in
    *Debug*|*debug*)   BUILD_CONFIG="debug" ;;
    *)                 BUILD_CONFIG="release" ;;
esac

DIST_DIR="${AGENUI_ROOT}/dist/android/${BUILD_CONFIG}"
mkdir -p "$DIST_DIR"

# Filter by filename suffix so cross-config leftovers in AAR_DIR (which Gradle
# accumulates across runs) don't bleed into this dist. The symbols sidecar is
# only picked up when THIS invocation asked for it via --with-symbols — without
# the gate, a stale <name>-symbols.aar from a previous --with-symbols run would
# still match the glob and end up in the dist of a plain release build.
shopt -s nullglob
copied_count=0
aar_list=("${AAR_DIR}"/*-"${BUILD_CONFIG}".aar)
if [[ "$BUILD_CONFIG" == "release" && "$WITH_SYMBOLS" == true ]]; then
    aar_list+=("${AAR_DIR}"/*-symbols.aar)
fi
for aar in "${aar_list[@]}"; do
    cp -f "$aar" "$DIST_DIR/"
    copied_count=$((copied_count + 1))
    info "Published to dist: ${DIST_DIR}/$(basename "$aar")"
done
shopt -u nullglob

if [[ "$copied_count" -eq 0 ]]; then
    warn "No AAR artifact found to copy (GRADLE_TASK=${GRADLE_TASK} may not produce an AAR, e.g. publishToLocalMaven)"
else
    info "Unified artifact directory: ${DIST_DIR}"
fi

print_build_version
success "Android build complete (${GRADLE_TASK})"
