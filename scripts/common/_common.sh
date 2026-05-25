#!/bin/bash
# =============================================================================
# scripts/common/_common.sh
# Shared utilities for all platform build scripts: logging + repo path constants.
#
# Monorepo structure (Plan B):
#   All platform build configs directly reference core/ path, no symlinks needed.
#   This file only provides shared logging functions and path constants for build.sh scripts.
#
# Path constants:
#   AGENUI_ROOT     = agenui repo root directory
#   CORE_DIR        = agenui/core (C++ core source code)
#   PLATFORMS_DIR   = agenui/platforms (platform projects)
# =============================================================================

if [[ -z "${_AGENUI_COMMON_LOADED:-}" ]]; then
    _AGENUI_COMMON_LOADED=1

    # -------------------- Colors and logging --------------------
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m'

    info()    { echo -e "${BLUE}[INFO]${NC} $*" >&2; }
    warn()    { echo -e "${YELLOW}[WARN]${NC} $*" >&2; }
    error()   { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }
    success() { echo -e "${GREEN}[SUCCESS]${NC} $*" >&2; }

    # -------------------- Repo path resolution --------------------
    # This file is at agenui/scripts/common/_common.sh, go up 2 levels for repo root
    _COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    AGENUI_ROOT="$(cd "${_COMMON_DIR}/../.." && pwd)"
    CORE_DIR="${AGENUI_ROOT}/core"
    PLATFORMS_DIR="${AGENUI_ROOT}/platforms"
fi

# Verify C++ core source integrity
ensure_core_dir() {
    if [[ ! -d "${CORE_DIR}/src" ]] || [[ ! -d "${CORE_DIR}/include" ]]; then
        error "C++ core source directory not found: ${CORE_DIR} (missing src/ or include/)"
    fi
}

# -------------------- Version consistency check --------------------
# Reads AGENUI_VERSION from the C++ header and verifies it matches the
# platform build config for the calling script. Emits a warning on mismatch.
# Exports AGENUI_SDK_VERSION for downstream use.
check_version_consistency() {
    local version_header="${CORE_DIR}/include/agenui_version.h"
    if [[ ! -f "$version_header" ]]; then
        warn "Version header not found: ${version_header}, skipping consistency check"
        return 0
    fi

    AGENUI_SDK_VERSION=$(grep -o '#define AGENUI_VERSION "[^"]*"' "$version_header" | sed 's/#define AGENUI_VERSION "//;s/"//')
    export AGENUI_SDK_VERSION

    if [[ -z "$AGENUI_SDK_VERSION" ]]; then
        warn "Failed to parse AGENUI_VERSION from ${version_header}"
        return 0
    fi

    info "AGenUI SDK version: ${AGENUI_SDK_VERSION}"

    # Determine which platform config to check based on the calling script path
    local caller="${0:-}"
    local config_version=""
    local config_file=""

    if [[ "$caller" == *"android"* ]]; then
        config_file="${PLATFORMS_DIR}/android/gradle.properties"
        if [[ -f "$config_file" ]]; then
            config_version=$(grep -o 'SDK_VERSION=.*' "$config_file" | sed 's/SDK_VERSION=//')
        fi
    elif [[ "$caller" == *"ios"* ]]; then
        config_file="${AGENUI_ROOT}/AGenUI.podspec"
        if [[ -f "$config_file" ]]; then
            config_version=$(grep "s\.version" "$config_file" | head -1 | sed "s/.*= *'//;s/'.*//")
        fi
    elif [[ "$caller" == *"harmony"* ]]; then
        config_file="${PLATFORMS_DIR}/harmony/agenui/oh-package.json5"
        if [[ -f "$config_file" ]]; then
            config_version=$(grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' "$config_file" | sed 's/.*"version"[[:space:]]*:[[:space:]]*"//;s/"//')
        fi
    fi

    if [[ -n "$config_version" && "$config_version" != "$AGENUI_SDK_VERSION" ]]; then
        warn "Version mismatch! C++ header: ${AGENUI_SDK_VERSION}, ${config_file}: ${config_version}"
        info "Auto-syncing version via sync-version.sh..."
        bash "${AGENUI_ROOT}/scripts/sync-version.sh"
    fi
}

# -------------------- Cleanup utilities --------------------
# Calculate path size (human-readable); returns - if not exists
human_size() {
    local target="$1"
    if [[ -e "$target" ]]; then
        du -sh "$target" 2>/dev/null | awk '{print $1}'
    else
        echo "-"
    fi
}

# -------------------- Run Key & Git Metadata --------------------
# Generate unique run key: {sanitized_branch}_{commit_short}_{YYYYMMDD_HHMMSS}
generate_run_key() {
    local branch commit ts branch_safe

    if git -C "$AGENUI_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        branch=$(git -C "$AGENUI_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
        [[ "$branch" == "HEAD" ]] && branch="detached"
        commit=$(git -C "$AGENUI_ROOT" rev-parse --short=7 HEAD 2>/dev/null || echo "0000000")
    else
        branch="nogit"
        commit="0000000"
    fi

    ts=$(date +%Y%m%d_%H%M%S)

    # Sanitize branch name: keep alnum/underscore/hyphen, truncate to 40 chars
    branch_safe=$(printf '%s' "$branch" | tr -c 'a-zA-Z0-9_-' '-' | cut -c1-40 | sed 's/-*$//')
    [[ -z "$branch_safe" ]] && branch_safe="unknown"

    echo "${branch_safe}_${commit}_${ts}"
}

# Collect git metadata and output JSON (no jq dependency)
# Usage: collect_git_metadata <run_key>
collect_git_metadata() {
    local run_key="${1:-}"
    local branch commit_short commit_full dirty ts

    if git -C "$AGENUI_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        branch=$(git -C "$AGENUI_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
        [[ "$branch" == "HEAD" ]] && branch="detached"
        commit_short=$(git -C "$AGENUI_ROOT" rev-parse --short=7 HEAD 2>/dev/null || echo "0000000")
        commit_full=$(git -C "$AGENUI_ROOT" rev-parse HEAD 2>/dev/null || echo "0000000000000000000000000000000000000000")
        if [[ -n "$(git -C "$AGENUI_ROOT" status --porcelain 2>/dev/null)" ]]; then
            dirty="true"
        else
            dirty="false"
        fi
    else
        branch="nogit"
        commit_short="0000000"
        commit_full="0000000000000000000000000000000000000000"
        dirty="false"
    fi

    ts=$(date +%Y-%m-%dT%H:%M:%S%z)

    printf '{"run_key":"%s","git":{"branch":"%s","commit_short":"%s","commit_full":"%s","dirty":%s},"timestamp":"%s"}' \
        "$run_key" "$branch" "$commit_short" "$commit_full" "$dirty" "$ts"
}

# Safe delete: must be an absolute path within AGENUI_ROOT (or explicit HOME subpath),
# prevents accidental deletion outside repo or root-level dirs. Dry-run mode only displays, no actual deletion.
#
# Usage: safe_rm <absolute_path> [<dry_run: true|false>]
safe_rm() {
    local target="$1"
    local dry_run="${2:-false}"

    if [[ -z "$target" ]]; then
        warn "safe_rm: target path is empty, skipping"
        return 0
    fi

    # Absolute path validation
    if [[ "${target:0:1}" != "/" ]]; then
        error "safe_rm: refusing to delete non-absolute path: ${target}"
    fi

    # Path whitelist: must be within AGENUI_ROOT or HOME, and cannot equal either
    local allowed=false
    if [[ "$target" == "${AGENUI_ROOT}/"* ]]; then allowed=true; fi
    if [[ "$target" == "${HOME}/"* ]]; then allowed=true; fi
    if [[ "$allowed" != "true" ]]; then
        error "safe_rm: refusing to delete path outside AGENUI_ROOT/HOME: ${target}"
    fi
    if [[ "$target" == "${AGENUI_ROOT}" ]] || [[ "$target" == "${HOME}" ]]; then
        error "safe_rm: refusing to delete root directory: ${target}"
    fi

    if [[ ! -e "$target" ]]; then
        info "  Skipped (not exists): ${target}"
        return 0
    fi

    local size
    size=$(human_size "$target")

    if [[ "$dry_run" == "true" ]]; then
        info "  [dry-run] Would delete (${size}): ${target}"
    else
        info "  Deleting (${size}): ${target}"
        rm -rf "$target"
    fi
}
