#!/usr/bin/env bash
#
# Build and run the AGenUI C++ test suite.
#
# By default this script exercises BOTH AddressSanitizer and ThreadSanitizer
# in two separate build trees. The two sanitizers cannot live in the same
# binary, so they are split as:
#
#   tests/cpp/build/host/   ← ASan + UBSan
#   tests/cpp/build/tsan/   ← TSan
#
# Usage:
#   ./tests/cpp/ci/run_tests.sh                  # default: ASan + TSan
#   ./tests/cpp/ci/run_tests.sh --asan-only      # skip TSan
#   ./tests/cpp/ci/run_tests.sh --tsan-only      # skip ASan
#   ./tests/cpp/ci/run_tests.sh --no-san         # plain build, no sanitizer
#   ./tests/cpp/ci/run_tests.sh --coverage       # coverage build (no sanitizer)
#   ./tests/cpp/ci/run_tests.sh --strict-tsan    # treat TSan races as failure
#   ./tests/cpp/ci/run_tests.sh --jobs 8         # parallel build jobs
#   ./tests/cpp/ci/run_tests.sh --clean          # remove all cmake/build artifacts and exit
#   ./tests/cpp/ci/run_tests.sh --clean-first    # clean before running tests
#   ./tests/cpp/ci/run_tests.sh --verbose        # show ALL engine logs (default: filtered)
#
# Notes:
#   * By default the noisy engine printf/logger lines (timestamped lines
#     that start with `[YYYY-MM-DD ...]`) are filtered from the live console
#     so that gtest verdicts and sanitizer reports are not drowned. Full
#     unfiltered output is always saved to ${BUILD_DIR}/<binary>.log, and
#     a summary table is printed at the end of the run.
#   * TSan currently reports known engine races; by default these are
#     informational (script exit is not affected). Pass --strict-tsan
#     to gate on TSan output.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS_CPP_ROOT="$(cd "${HERE}/.." && pwd)"

DO_ASAN=1
DO_TSAN=1
DO_PLAIN=0
DO_COVERAGE=0
STRICT_TSAN=0
DO_CLEAN=0
CLEAN_FIRST=0
VERBOSE=0
JOBS="${JOBS:-4}"

# Per-binary log files appended here so the final summary can pull from them.
ALL_LOGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --asan-only)   DO_ASAN=1; DO_TSAN=0; DO_PLAIN=0; DO_COVERAGE=0; shift ;;
        --tsan-only)   DO_ASAN=0; DO_TSAN=1; DO_PLAIN=0; DO_COVERAGE=0; shift ;;
        --no-san)      DO_ASAN=0; DO_TSAN=0; DO_PLAIN=1; DO_COVERAGE=0; shift ;;
        --coverage)    DO_ASAN=0; DO_TSAN=0; DO_PLAIN=0; DO_COVERAGE=1; shift ;;
        --strict-tsan) STRICT_TSAN=1; shift ;;
        --clean)       DO_CLEAN=1; shift ;;
        --clean-first) CLEAN_FIRST=1; shift ;;
        --verbose)     VERBOSE=1; shift ;;
        --jobs)        JOBS="$2"; shift 2 ;;
        -h|--help)
            grep "^#" "$0" | sed 's/^#//'
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

# Remove all cmake/build artifacts produced by run_variant().
# Wipes the entire tests/cpp/build/ tree (host/, tsan/, plain/, coverage/, xcode/, ...).
clean_all() {
    local build_root="${TESTS_CPP_ROOT}/build"
    echo ""
    echo "============================================================"
    echo "==> clean"
    echo "    target: ${build_root}"
    echo "============================================================"
    if [[ -d "${build_root}" ]]; then
        rm -rf "${build_root}"
        echo "   removed: ${build_root}"
    else
        echo "   nothing to clean (${build_root} does not exist)"
    fi
}

if [[ ${DO_CLEAN} -eq 1 ]]; then
    clean_all
    echo ""
    echo "==> Done (clean)"
    exit 0
fi

if [[ ${CLEAN_FIRST} -eq 1 ]]; then
    clean_all
fi

# Run one gtest binary, capturing its full output to a log file and
# streaming a filtered view to the console (suppresses the noisy
# engine printf/logger lines that start with `[YYYY-MM-DD `).
#
# Args:
#   $1  variant label (e.g. host, tsan)
#   $2  absolute path to the test binary
run_one_binary() {
    local variant="$1"
    local bin_path="$2"
    local bin_name
    bin_name="$(basename "${bin_path}")"
    local log_file="${bin_path}.log"

    echo ""
    echo "---- [${variant}] ${bin_name} ----"

    # Always keep --gtest_color=no for log-friendly output. Drop
    # --gtest_brief so failed-assertion bodies are kept in the log
    # (we still hide them on the live console below).
    set +e
    local rc
    if [[ ${VERBOSE} -eq 1 ]]; then
        "${bin_path}" --gtest_color=no ${GTEST_EXTRA_ARGS:-} 2>&1 | tee "${log_file}"
        rc=${PIPESTATUS[0]}
    else
        # Filter out engine logger lines (start with `[YYYY-MM-DD`),
        # but keep gtest verdicts, sanitizer reports, fatal errors.
        # NOTE: capture PIPESTATUS *before* any other command runs, since
        # PIPESTATUS is reset by every subsequent command/builtin.
        "${bin_path}" --gtest_color=no ${GTEST_EXTRA_ARGS:-} 2>&1 \
            | tee "${log_file}" \
            | grep -v -E '^\[[0-9]{4}-[0-9]{2}-[0-9]{2}'
        rc=${PIPESTATUS[0]}
    fi
    set -e

    ALL_LOGS+=("${variant}|${bin_name}|${log_file}|${rc}")
    return ${rc}
}

# Print a final summary by parsing each per-binary log file: pass/fail
# counts, the name of every failed test, and any sanitizer findings.
print_test_summary() {
    echo ""
    echo "============================================================"
    echo "==> Test Result Summary"
    echo "============================================================"

    if [[ ${#ALL_LOGS[@]} -eq 0 ]]; then
        echo "  (no test binaries were executed)"
        return
    fi

    local total_failed=0
    local entry variant bin_name log_file bin_rc
    for entry in "${ALL_LOGS[@]}"; do
        variant="${entry%%|*}"
        local rest="${entry#*|}"
        bin_name="${rest%%|*}"
        rest="${rest#*|}"
        log_file="${rest%%|*}"
        bin_rc="${rest##*|}"

        local pass_count=0 fail_count=0
        if [[ -f "${log_file}" ]]; then
            pass_count="$(grep -oE '\[  PASSED  \] [0-9]+ test' "${log_file}" \
                | tail -1 | grep -oE '[0-9]+' || echo 0)"
            fail_count="$(grep -oE '\[  FAILED  \] [0-9]+ test' "${log_file}" \
                | head -1 | grep -oE '[0-9]+' || echo 0)"
        fi
        pass_count="${pass_count:-0}"
        fail_count="${fail_count:-0}"

        local status_tag="OK"
        if [[ "${fail_count}" -gt 0 ]]; then
            status_tag="FAIL"
        elif [[ "${bin_rc}" -ne 0 ]]; then
            status_tag="ABORT"
        fi

        printf '  [%-5s] %-30s passed=%-4s failed=%-4s rc=%-4s -> %s\n' \
            "${variant}" "${bin_name}" "${pass_count}" "${fail_count}" "${bin_rc}" "${status_tag}"

        # List failed test names (dedup, only "Suite.Test" lines).
        if [[ -f "${log_file}" && "${fail_count}" -gt 0 ]]; then
            echo "      failed cases:"
            grep -E '^\[  FAILED  \] [A-Za-z_][A-Za-z0-9_]*\.' "${log_file}" \
                | sed -E 's/^\[  FAILED  \] //' \
                | sort -u \
                | sed 's/^/        - /'
        fi

        # If the binary aborted (rc != 0) but gtest summary did not
        # report failed cases, surface the most likely root cause.
        if [[ -f "${log_file}" && "${bin_rc}" -ne 0 && "${fail_count}" -eq 0 ]]; then
            local last_run
            last_run="$(grep -E '^\[ RUN      \]' "${log_file}" | tail -1 | sed -E 's/^\[ RUN      \] //')"
            if [[ -n "${last_run}" ]]; then
                echo "      last started case (likely culprit): ${last_run}"
            fi
        fi

        # Sanitizer findings (ASan / UBSan / TSan).
        if [[ -f "${log_file}" ]]; then
            local san_lines
            san_lines="$(grep -E 'AddressSanitizer:|UndefinedBehaviorSanitizer:|ThreadSanitizer:|runtime error:|SUMMARY: (Address|Undefined|Thread)Sanitizer' "${log_file}" \
                | head -8 || true)"
            if [[ -n "${san_lines}" ]]; then
                echo "      sanitizer findings:"
                while IFS= read -r line; do
                    echo "        ${line}"
                done <<<"${san_lines}"
            fi
        fi

        total_failed=$((total_failed + fail_count))
    done

    echo ""
    if [[ ${OVERALL_RC} -eq 0 && ${total_failed} -eq 0 ]]; then
        echo "  Overall: PASS  (failed_cases=0)"
    else
        echo "  Overall: FAIL  (script_rc=${OVERALL_RC}, failed_cases=${total_failed})"
        echo "  See full logs:"
        local entry2
        for entry2 in "${ALL_LOGS[@]}"; do
            local lf="${entry2}"
            lf="${lf#*|}"; lf="${lf#*|}"; lf="${lf%%|*}"
            echo "    - ${lf}"
        done
    fi
}

run_variant() {
    local name="$1"
    local cmake_args="$2"
    local treat_tests_failure_as="$3"   # "fatal" | "informational"
    local build_dir="${TESTS_CPP_ROOT}/build/${name}"

    echo ""
    echo "============================================================"
    echo "==> ${name}"
    echo "    cmake : ${cmake_args}"
    echo "    out   : ${build_dir}"
    echo "============================================================"

    # If a stale CMakeCache.txt points to a different source/build dir
    # (e.g. the workspace was renamed/moved), wipe the build dir so cmake
    # can regenerate cleanly instead of failing with a path-mismatch error.
    local cache_file="${build_dir}/CMakeCache.txt"
    if [[ -f "${cache_file}" ]]; then
        local cached_src
        cached_src="$(grep -E '^CMAKE_HOME_DIRECTORY:INTERNAL=' "${cache_file}" | cut -d= -f2- || true)"
        if [[ -n "${cached_src}" && "${cached_src}" != "${TESTS_CPP_ROOT}" ]]; then
            echo "   !! stale CMake cache detected (was: ${cached_src}); cleaning ${build_dir}"
            rm -rf "${build_dir}"
        fi
    fi

    cmake -S "${TESTS_CPP_ROOT}" -B "${build_dir}" ${cmake_args}
    cmake --build "${build_dir}" -j "${JOBS}"

    local rc=0
    run_one_binary "${name}" "${build_dir}/agenui_unit_tests"        || rc=$?
    run_one_binary "${name}" "${build_dir}/agenui_concurrency_tests" || rc=$?
    if [[ -x "${build_dir}/agenui_tsan_tests" ]]; then
        run_one_binary "${name}" "${build_dir}/agenui_tsan_tests"   || rc=$?
    fi

    if [[ ${rc} -ne 0 ]]; then
        if [[ "${treat_tests_failure_as}" == "informational" ]]; then
            echo ""
            echo "!! [${name}] non-zero exit (rc=${rc}) — treated as INFORMATIONAL." \
                 "(use --strict-tsan to gate)"
            return 0
        fi
        echo ""
        echo "!! [${name}] FAILED (rc=${rc})"
        return ${rc}
    fi
    return 0
}

OVERALL_RC=0

if [[ ${DO_ASAN} -eq 1 ]]; then
    run_variant "host" \
        "-DAGENUI_TESTS_ENABLE_ASAN=ON -DAGENUI_TESTS_ENABLE_TSAN=OFF" \
        "fatal" || OVERALL_RC=$?
fi

if [[ ${DO_TSAN} -eq 1 ]]; then
    if [[ ${STRICT_TSAN} -eq 1 ]]; then
        run_variant "tsan" \
            "-DAGENUI_TESTS_ENABLE_ASAN=OFF -DAGENUI_TESTS_ENABLE_TSAN=ON" \
            "fatal" || OVERALL_RC=$?
    else
        run_variant "tsan" \
            "-DAGENUI_TESTS_ENABLE_ASAN=OFF -DAGENUI_TESTS_ENABLE_TSAN=ON" \
            "informational"
    fi
fi

if [[ ${DO_PLAIN} -eq 1 ]]; then
    run_variant "plain" \
        "-DAGENUI_TESTS_ENABLE_ASAN=OFF -DAGENUI_TESTS_ENABLE_TSAN=OFF" \
        "fatal" || OVERALL_RC=$?
fi

if [[ ${DO_COVERAGE} -eq 1 ]]; then
    run_variant "coverage" \
        "-DAGENUI_TESTS_ENABLE_ASAN=OFF -DAGENUI_TESTS_ENABLE_COVERAGE=ON" \
        "fatal" || OVERALL_RC=$?
fi

print_test_summary

echo ""
echo "============================================================"
echo "==> Done (rc=${OVERALL_RC})"
echo "============================================================"
exit ${OVERALL_RC}
