#!/usr/bin/env bash
# ============================================================================
# HarmonyOS Crash Symbolication Tool
#
# Usage:
#   ./symbolicate_crash.sh <crash_file>
#
# Input: crash log file (either format):
#   - reports/stability/runs/.../crashes/crash_*.txt
#   - cppcrash-com.harmony.agenui-*.txt (raw faultlog)
#
# Requires:
#   - DevEco Studio installed (for llvm-addr2line)
#   - Unstripped liba2ui-capi.so in build output
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

ADDR2LINE_CANDIDATES=(
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/llvm-addr2line"
    "/Applications/DevEco-Studio.app/Contents/sdk/default/hms/native/BiSheng/bin/llvm-addr2line"
)

SO_SEARCH_PATHS=(
    "playground/harmony/entry/build/default/intermediates/libs/default/arm64-v8a"
    "playground/harmony/entry/build/default/intermediates/libs/ohosTest/arm64-v8a"
    "platforms/harmony/agenui/build/default/intermediates/libs/default/arm64-v8a"
)

die() { echo "ERROR: $*" >&2; exit 1; }

find_addr2line() {
    for candidate in "${ADDR2LINE_CANDIDATES[@]}"; do
        [[ -x "$candidate" ]] && { echo "$candidate"; return 0; }
    done
    command -v llvm-addr2line 2>/dev/null && return 0
    return 1
}

find_so() {
    local target_buildid="${1:-}"
    for rel_path in "${SO_SEARCH_PATHS[@]}"; do
        local so_path="$REPO_ROOT/$rel_path/liba2ui-capi.so"
        if [[ -f "$so_path" ]]; then
            if [[ -n "$target_buildid" ]] && file "$so_path" 2>/dev/null | grep -q "$target_buildid"; then
                echo "$so_path"; return 0
            fi
        fi
    done
    for rel_path in "${SO_SEARCH_PATHS[@]}"; do
        local so_path="$REPO_ROOT/$rel_path/liba2ui-capi.so"
        [[ -f "$so_path" ]] && { echo "$so_path"; return 0; }
    done
    return 1
}

# Extract fault thread stack frames only (stop at next "Tid:" or empty line after frames)
extract_fault_thread_frames() {
    local file="$1"
    awk '
    /^Fault thread info:/ { in_fault=1; next }
    in_fault && /^Tid:/ { in_frames=1; next }
    in_frames && /^#[0-9]+ pc / { print; next }
    in_frames && !/^#[0-9]+ pc / { exit }
    ' "$file"
}

main() {
    if [[ $# -lt 1 ]]; then
        echo "Usage: $0 <crash_file>"
        echo ""
        echo "Examples:"
        echo "  $0 reports/stability/.../crashes/crash_7_20260513_105016.txt"
        echo "  $0 ~/Downloads/cppcrash-com.harmony.agenui-20260513105030.503.txt"
        exit 1
    fi

    local crash_file="$1"
    [[ -f "$crash_file" ]] || die "File not found: $crash_file"

    # 1. Find tool
    local addr2line
    addr2line=$(find_addr2line) || die "llvm-addr2line not found. Install DevEco Studio."
    echo "Tool: $addr2line" >&2

    # 2. Find .so with BuildID match
    local buildid
    buildid=$(sed -n 's/.*(\([0-9a-f]\{40\}\)).*/\1/p' "$crash_file" | head -1)
    [[ -n "$buildid" ]] && echo "BuildID: $buildid" >&2

    local so_path
    so_path=$(find_so "$buildid") || die "Unstripped liba2ui-capi.so not found. Build the project first."
    file "$so_path" | grep -q "not stripped" || echo "WARNING: .so may be stripped" >&2
    echo "Symbols: $so_path" >&2

    # 3. Extract fault thread frames
    local all_frames
    all_frames=$(extract_fault_thread_frames "$crash_file")
    [[ -n "$all_frames" ]] || die "No fault thread frames found in crash file"

    # 4. Separate our .so frames for symbolication
    local our_frames
    our_frames=$(echo "$all_frames" | grep 'liba2ui-capi\.so' || true)

    local addrs=()
    if [[ -n "$our_frames" ]]; then
        while IFS= read -r line; do
            local addr
            addr=$(echo "$line" | sed -n 's/.*pc \([0-9a-fA-F]*\).*/\1/p')
            [[ -n "$addr" ]] && addrs+=("0x$addr")
        done <<< "$our_frames"
    fi

    echo "Fault thread: $(echo "$all_frames" | wc -l | tr -d ' ') frames, ${#addrs[@]} in liba2ui-capi.so" >&2
    echo "" >&2

    # 5. Batch symbolicate
    local symbols=""
    if [[ ${#addrs[@]} -gt 0 ]]; then
        symbols=$("$addr2line" -Cfpie "$so_path" "${addrs[@]}" 2>/dev/null)
    fi

    # 6. Output merged stack
    echo "=== Symbolicated Fault Thread Stack ==="
    echo ""

    local sym_idx=0
    while IFS= read -r line; do
        local frame_num addr
        frame_num=$(echo "$line" | sed -n 's/.*#\([0-9]*\) pc.*/\1/p')
        addr=$(echo "$line" | sed -n 's/.*pc \([0-9a-fA-F]*\).*/\1/p')

        if echo "$line" | grep -q 'liba2ui-capi\.so'; then
            # Resolve from symbols output
            local sym_line
            sym_line=$(echo "$symbols" | sed -n "$((sym_idx+1))p")
            sym_idx=$((sym_idx + 1))

            # Shorten for readability
            local short_sym
            short_sym=$(echo "$sym_line" | sed \
                -e 's/std::__n1::basic_string<char, std::__n1::char_traits<char>, std::__n1::allocator<char>>/std::string/g' \
                -e 's/std::__n1:://g' \
                -e 's/\[abi:v[0-9]*\]//g')

            echo "#${frame_num} pc ${addr}  liba2ui-capi.so  ${short_sym}"
        else
            # System frame: extract lib name and existing symbol if any
            local lib_name existing_sym
            lib_name=$(echo "$line" | sed -n 's|.*/\([^/(]*\.so[^(]*\).*|\1|p')
            existing_sym=$(echo "$line" | sed -n 's/.*\.so(\([^)]*\)).*/\1/p' | head -1)
            if [[ -n "$existing_sym" && "$existing_sym" != *"$buildid"* ]]; then
                echo "#${frame_num} pc ${addr}  ${lib_name}  ${existing_sym}"
            else
                echo "#${frame_num} pc ${addr}  ${lib_name:-unknown}"
            fi
        fi
    done <<< "$all_frames"
}

main "$@"
