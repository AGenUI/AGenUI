#!/usr/bin/env bash
#
# Generate a single Xcode project for the AGenUI C++ test suite.
#
# Xcode is a multi-config generator, so ASan and TSan are exposed as two
# build configurations inside the SAME project. Switch between them via
# Edit Scheme → Run → Build Configuration:
#
#   Debug    → AddressSanitizer + UBSan   (default)
#   Tsan     → ThreadSanitizer
#   Release  → no sanitizer
#
# Output: tests/cpp/build/xcode/agenui_cpp_tests.xcodeproj
#
# Usage:
#   ./tests/cpp/ide/xcode/generate.sh         # generate
#   ./tests/cpp/ide/xcode/generate.sh --open  # generate + open in Xcode

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS_CPP_ROOT="$(cd "${HERE}/../.." && pwd)"
BUILD_DIR="${TESTS_CPP_ROOT}/build/xcode"

OPEN_AFTER=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --open) OPEN_AFTER=1; shift ;;
        -h|--help)
            grep "^#" "$0" | sed 's/^#//'
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

echo "==> Generating Xcode project"
echo "    Source : ${TESTS_CPP_ROOT}"
echo "    Build  : ${BUILD_DIR}"

cmake -S "${TESTS_CPP_ROOT}" -B "${BUILD_DIR}" -G Xcode

PROJECT="${BUILD_DIR}/agenui_cpp_tests.xcodeproj"
if [[ ! -d "${PROJECT}" ]]; then
    echo "ERROR: Xcode project not generated at ${PROJECT}" >&2
    exit 1
fi

cat <<EOF

============================================================
==> Xcode project ready
============================================================

  ${PROJECT}

Build configurations (switch via Edit Scheme → Run → Build Configuration):
  - Debug    AddressSanitizer + UBSan   (default)
  - Tsan     ThreadSanitizer
  - Release  no sanitizer

Schemes:
  - agenui_unit_tests           integration / unit / stress / asan
  - agenui_concurrency_tests    concurrency + chaos
  - agenui_tsan_tests           TSan-dedicated suite (use Tsan config)

CMD+R to run, CMD+Y to debug.
EOF

if [[ "${OPEN_AFTER}" == "1" ]]; then
    echo ""
    echo "==> open ${PROJECT}"
    open "${PROJECT}"
fi
