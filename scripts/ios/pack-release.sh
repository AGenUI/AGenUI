#!/bin/bash
set -euo pipefail

# ============================================================================
# scripts/ios/pack-release.sh
# Package the pre-built XCFramework + resource Bundle into a distributable zip.
#
# This zip is uploaded to GitHub Releases as an asset tagged AGenUI-<version>.
# The publish/AGenUI.podspec references this zip via its :http source, so
# consumers who run `pod 'AGenUI'` get the pre-built binary directly.
#
# Prerequisites:
#   - XCFramework must exist at dist/ios/release/AGenUI.xcframework
#     (run ./scripts/ios/build.sh first, or use --build flag)
#   - Resource Bundle must exist at platforms/ios/AGenUI/Assets/AGenUI.bundle
#
# Usage:
#   ./scripts/ios/pack-release.sh [options]
#
# Options:
#   --build              Run ./scripts/ios/build.sh before packaging
#   --clean              Pass --clean to build.sh (implies --build)
#   -o, --output <dir>   Output directory (default: dist/ios/publish/)
#   -h, --help           Show this help message
#
# Output:
#   dist/ios/publish/AGenUI-<version>-ios.zip
#   Contains: AGenUI.xcframework/ + AGenUI.bundle/ + LICENSE
#
# Examples:
#   ./scripts/ios/pack-release.sh                  # package existing build
#   ./scripts/ios/pack-release.sh --build          # build then package
#   ./scripts/ios/pack-release.sh --build --clean  # clean build then package
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../common/_common.sh
source "${SCRIPT_DIR}/../common/_common.sh"
# shellcheck source=../common/_build_id.sh
source "${SCRIPT_DIR}/../common/_build_id.sh"

# -------------------- Defaults --------------------
DO_BUILD=false
DO_CLEAN=false
OUTPUT_DIR=""

# -------------------- Argument parsing --------------------
show_help() {
    sed -n '5,28p' "$0" | sed -E 's/^#( |$)//'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)    DO_BUILD=true; shift ;;
        --clean)    DO_CLEAN=true; DO_BUILD=true; shift ;;
        -o|--output) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)  show_help ;;
        *)          error "Unknown argument: $1" ;;
    esac
done

# -------------------- Version check --------------------
check_version_consistency
fetch_build_id "ios"

if [[ -z "${AGENUI_SDK_VERSION:-}" ]]; then
    error "Failed to determine SDK version"
fi

info "AGenUI SDK version: ${AGENUI_SDK_VERSION}"

# -------------------- Optional build --------------------
XCFRAMEWORK_PATH="${AGENUI_ROOT}/dist/ios/release/AGenUI.xcframework"

if [[ "$DO_BUILD" == true ]]; then
    info "Building XCFramework via scripts/ios/build.sh..."
    BUILD_ARGS=()
    [[ "$DO_CLEAN" == true ]] && BUILD_ARGS+=(--clean)
    bash "${SCRIPT_DIR}/build.sh" ${BUILD_ARGS[@]+"${BUILD_ARGS[@]}"}
fi

# -------------------- Validate inputs --------------------
if [[ ! -d "$XCFRAMEWORK_PATH" ]]; then
    error "XCFramework not found: ${XCFRAMEWORK_PATH}
Run './scripts/ios/build.sh' first, or use '--build' flag."
fi

BUNDLE_SRC="${PLATFORMS_DIR}/ios/AGenUI/Assets/AGenUI.bundle"
if [[ ! -d "$BUNDLE_SRC" ]]; then
    error "Resource bundle not found: ${BUNDLE_SRC}"
fi

info "XCFramework: ${XCFRAMEWORK_PATH}"
info "Bundle:      ${BUNDLE_SRC}"

# -------------------- Prepare output directory --------------------
if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="${AGENUI_ROOT}/dist/ios/publish"
fi
mkdir -p "$OUTPUT_DIR"

# -------------------- Stage files --------------------
STAGE_DIR="${OUTPUT_DIR}/.stage"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

info "Staging XCFramework..."
cp -R "$XCFRAMEWORK_PATH" "$STAGE_DIR/"

info "Staging resource bundle..."
cp -R "$BUNDLE_SRC" "$STAGE_DIR/"

info "Staging LICENSE..."
cp "${AGENUI_ROOT}/LICENSE" "$STAGE_DIR/"

# -------------------- Create zip --------------------
ZIP_NAME="AGenUI-${AGENUI_SDK_VERSION}-ios.zip"
ZIP_PATH="${OUTPUT_DIR}/${ZIP_NAME}"

info "Creating ${ZIP_NAME}..."
cd "$STAGE_DIR"
zip -r -q "$ZIP_PATH" AGenUI.xcframework AGenUI.bundle LICENSE
cd "$AGENUI_ROOT"

# -------------------- Cleanup stage --------------------
rm -rf "$STAGE_DIR"

# -------------------- Summary --------------------
ZIP_SIZE=$(du -h "$ZIP_PATH" | awk '{print $1}')
echo ""
success "Package created successfully"
info "  Path: ${ZIP_PATH}"
info "  Size: ${ZIP_SIZE}"
info "  Version: ${AGENUI_SDK_VERSION}"
echo ""
info "Next steps:"
info "  1. Upload to GitHub Release:"
info "     gh release create AGenUI-${AGENUI_SDK_VERSION} ${ZIP_PATH} --title 'AGenUI ${AGENUI_SDK_VERSION}' --notes 'Release ${AGENUI_SDK_VERSION}'"
info "  2. Lint the binary podspec:"
info "     pod spec lint ${PLATFORMS_DIR}/ios/publish/AGenUI.podspec --allow-warnings --skip-import-validation --verbose"
info "  3. Publish to CocoaPods trunk:"
info "     pod trunk push ${PLATFORMS_DIR}/ios/publish/AGenUI.podspec --allow-warnings --skip-import-validation"

print_build_version
