#!/bin/bash
set -euo pipefail

# ============================================================================
# scripts/sync-version.sh
# Synchronize version from C++ header to all platform build configurations.
#
# Single source of truth: core/include/agenui_version.h
#   #define AGENUI_VERSION "x.y.z"
#
# Target files:
#   - platforms/android/gradle.properties        (SDK_VERSION=x.y.z)
#   - AGenUI.podspec                             (s.version = 'x.y.z')
#   - AGenUI_ForSDK.podspec                      (s.version = 'x.y.z')
#   - platforms/ios/AGenUI_ForSDK.podspec         (s.version = 'x.y.z')
#   - platforms/harmony/agenui/oh-package.json5   ("version": "x.y.z")
#
# Usage:
#   ./scripts/sync-version.sh
#
# The script is idempotent: files already matching the target version are
# left untouched.
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common/_common.sh
source "${SCRIPT_DIR}/common/_common.sh"

# -------------------- Parse version from C++ header --------------------
VERSION_HEADER="${CORE_DIR}/include/agenui_version.h"

if [[ ! -f "$VERSION_HEADER" ]]; then
    error "Version header not found: ${VERSION_HEADER}"
fi

VERSION=$(grep -o '#define AGENUI_VERSION "[^"]*"' "$VERSION_HEADER" | sed 's/#define AGENUI_VERSION "//;s/"//')

if [[ -z "$VERSION" ]]; then
    error "Failed to parse AGENUI_VERSION from ${VERSION_HEADER}"
fi

info "Source version (from C++ header): ${VERSION}"

# -------------------- Target file definitions --------------------
declare -a TARGET_FILES=(
    "${PLATFORMS_DIR}/android/gradle.properties"
    "${AGENUI_ROOT}/AGenUI.podspec"
    "${AGENUI_ROOT}/AGenUI_ForSDK.podspec"
    "${PLATFORMS_DIR}/ios/AGenUI_ForSDK.podspec"
    "${PLATFORMS_DIR}/harmony/agenui/oh-package.json5"
)

# -------------------- Update functions --------------------
updated_count=0
skipped_count=0

update_gradle_properties() {
    local file="$1"
    local current
    current=$(grep -o 'SDK_VERSION=.*' "$file" | sed 's/SDK_VERSION=//')
    if [[ "$current" == "$VERSION" ]]; then
        info "  [skip] ${file} (already ${VERSION})"
        skipped_count=$((skipped_count + 1))
        return
    fi
    sed -i '' "s/^SDK_VERSION=.*/SDK_VERSION=${VERSION}/" "$file"
    info "  [updated] ${file}: ${current} -> ${VERSION}"
    updated_count=$((updated_count + 1))
}

update_podspec() {
    local file="$1"
    local current
    current=$(grep "s\.version" "$file" | head -1 | sed "s/.*= *'//;s/'.*//")
    if [[ "$current" == "$VERSION" ]]; then
        info "  [skip] ${file} (already ${VERSION})"
        skipped_count=$((skipped_count + 1))
        return
    fi
    sed -i '' "s/\(s\.version[[:space:]]*=[[:space:]]*'\)[^']*'/\1${VERSION}'/" "$file"
    info "  [updated] ${file}: ${current} -> ${VERSION}"
    updated_count=$((updated_count + 1))
}

update_oh_package_json5() {
    local file="$1"
    local current
    current=$(grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' "$file" | sed 's/.*"version"[[:space:]]*:[[:space:]]*"//;s/"//')
    if [[ "$current" == "$VERSION" ]]; then
        info "  [skip] ${file} (already ${VERSION})"
        skipped_count=$((skipped_count + 1))
        return
    fi
    sed -i '' "s/\(\"version\"[[:space:]]*:[[:space:]]*\"\)[^\"]*\"/\1${VERSION}\"/" "$file"
    info "  [updated] ${file}: ${current} -> ${VERSION}"
    updated_count=$((updated_count + 1))
}

# -------------------- Execute updates --------------------
info "Syncing version to platform build configurations..."

# Android
update_gradle_properties "${PLATFORMS_DIR}/android/gradle.properties"

# iOS podspecs
update_podspec "${AGENUI_ROOT}/AGenUI.podspec"
update_podspec "${AGENUI_ROOT}/AGenUI_ForSDK.podspec"
update_podspec "${PLATFORMS_DIR}/ios/AGenUI_ForSDK.podspec"

# HarmonyOS
update_oh_package_json5 "${PLATFORMS_DIR}/harmony/agenui/oh-package.json5"

# -------------------- Summary --------------------
echo ""
info "Sync complete: ${updated_count} file(s) updated, ${skipped_count} file(s) already up-to-date."
if [[ "$updated_count" -gt 0 ]]; then
    info "Remember to commit the updated files."
fi

success "Version sync done (${VERSION})"
