#!/bin/bash
# =============================================================================
# scripts/common/_build_id.sh
# Build ID management: generate a build ID for the current build and export
# AGENUI_BUILD_ID / AGENUI_FULL_VERSION for downstream use.
#
# Two modes:
#   1. Service mode (recommended for team collaboration):
#      Set AGENUI_BUILD_ID_URL to point to a build ID service that supports
#      atomic increment per (sdk_version, platform) pair, ensuring globally
#      unique and monotonically increasing build IDs across all developers.
#
#      Required service API:
#        POST /api/next-build-id
#        Request:  {"sdk_version": "x.y.z", "platform": "ios|android|harmony"}
#        Response: <integer> (the next build ID as plain text)
#
#      The service MUST:
#        - Atomic increment: concurrent requests always get distinct, increasing IDs
#        - Per (sdk_version, platform) isolation: different versions or platforms
#          maintain independent counters
#        - Persistence: IDs survive service restarts
#
#   2. Local mode (default, for open-source or standalone use):
#      When AGENUI_BUILD_ID_URL is not set, a random build ID is generated
#      locally based on the current timestamp. This ensures each build gets
#      a unique ID without requiring any external service.
#
# Configuration:
#   AGENUI_BUILD_ID_URL - Set to the build ID service URL (e.g. http://host:port)
#                         to enable service mode. Leave unset for local mode.
#
# Idempotency:
#   fetch_build_id is a no-op when AGENUI_FULL_VERSION is already exported in
#   the environment. This lets wrappers that drive multiple build.sh invocations
#   in sequence (e.g. publishing a main artifact + a same-version symbols sidecar)
#   pin both to the same base version without consuming an extra build ID from
#   the service. Standalone build.sh invocations are unaffected — the variable
#   is empty by default.
# =============================================================================

if [[ -z "${_AGENUI_BUILD_ID_LOADED:-}" ]]; then
    _AGENUI_BUILD_ID_LOADED=1

    # Service URL. Set AGENUI_BUILD_ID_URL to override.
    # When empty (after open-source cleanup), local random build ID generation is used.
    AGENUI_BUILD_ID_URL="${AGENUI_BUILD_ID_URL:-}"

    # fetch_build_id <platform>
    #   Fetches the next build ID for the given sdk_version + platform.
    #   Exports: AGENUI_BUILD_ID, AGENUI_FULL_VERSION
    fetch_build_id() {
        local platform="$1"
        if [[ -z "$platform" ]]; then
            error "fetch_build_id: platform argument is required (ios|android|harmony)"
        fi

        if [[ -z "${AGENUI_SDK_VERSION:-}" ]]; then
            error "fetch_build_id: AGENUI_SDK_VERSION is not set (call check_version_consistency first)"
        fi

        # Idempotent: if the caller already set AGENUI_FULL_VERSION (e.g. a wrapper
        # that wants to publish multiple artifacts under the same base version),
        # reuse it instead of consuming a fresh build ID from the service.
        if [[ -n "${AGENUI_FULL_VERSION:-}" ]]; then
            info "Reusing pre-set AGENUI_FULL_VERSION=${AGENUI_FULL_VERSION} (${platform})"
            return 0
        fi

        local build_id=""

        if [[ -n "${AGENUI_BUILD_ID_URL}" ]]; then
            # Service mode: fetch from remote service (5s timeout)
            local response
            response=$(curl -s -m 5 -X POST "${AGENUI_BUILD_ID_URL}/api/next-build-id" \
                -H "Content-Type: application/json" \
                -d "{\"sdk_version\":\"${AGENUI_SDK_VERSION}\",\"platform\":\"${platform}\"}" 2>/dev/null) || true

            # Validate: response should be a positive integer
            if [[ -n "$response" && "$response" =~ ^[1-9][0-9]*$ ]]; then
                build_id="$response"
                info "Build ID fetched from service: ${AGENUI_SDK_VERSION}.${build_id} (${platform})"
            else
                warn "Build ID service unavailable or returned invalid response (got: '${response:-empty}')"
                warn "Falling back to local random build ID"
            fi
        fi

        # Local mode or service fallback: generate a random build ID from timestamp
        if [[ -z "$build_id" ]]; then
            # Seconds since epoch modulo 100000 gives a compact, increasing number
            # that wraps every ~27 hours. Sufficient for local builds.
            build_id=$(( $(date +%s) % 100000 ))
            info "Build ID generated locally: ${AGENUI_SDK_VERSION}.${build_id} (${platform})"
        fi

        export AGENUI_BUILD_ID="${build_id}"
        export AGENUI_FULL_VERSION="${AGENUI_SDK_VERSION}.${build_id}"
    }

    # print_build_version
    #   Prints the full version string prominently after a successful build.
    print_build_version() {
        if [[ -z "${AGENUI_FULL_VERSION:-}" ]]; then
            return 0
        fi
        echo ""
        echo -e "${GREEN}========================================${NC}"
        echo -e "${GREEN}  Build Version: ${AGENUI_FULL_VERSION}${NC}"
        echo -e "${GREEN}========================================${NC}"
        echo ""
    }
fi
