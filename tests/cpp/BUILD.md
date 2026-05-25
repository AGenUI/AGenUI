# Build & sanitizer guide

Detailed instructions for building and running the AGenUI C++ test suite
in different configurations.

## Prerequisites

- CMake 3.18 or newer
- C++17 compiler (Apple clang 14+, GCC 11+, or clang 14+)
- git (for FetchContent of gtest + yoga)

The first configure downloads ~30 MB of source (gtest 1.14, yoga 2.0)
into the build directory. Subsequent configures reuse the cached download.

## Default build

```bash
cmake -S tests/cpp -B tests/cpp/build           # ASan + UBSan ON by default
cmake --build tests/cpp/build -j 4
ctest --test-dir tests/cpp/build --output-on-failure
```

`ctest` enumerates each gtest case as an individual test and runs them
in parallel.

## Sanitizer matrix

| Configuration | CMake flags | Notes |
|---|---|---|
| **Default (ASan + UBSan)** | (none) | Recommended day-to-day |
| Plain | `-DAGENUI_TESTS_ENABLE_ASAN=OFF` | Fastest; lowest signal |
| ThreadSanitizer | `-DAGENUI_TESTS_ENABLE_ASAN=OFF -DAGENUI_TESTS_ENABLE_TSAN=ON` | Mutually exclusive with ASan; uses a different build dir |
| Coverage | `-DAGENUI_TESTS_ENABLE_COVERAGE=ON` | Adds `--coverage`; combine with `--no-san` for clean reports |

### TSan recipe

```bash
cmake -S tests/cpp -B tests/cpp/build/tsan \
    -DAGENUI_TESTS_ENABLE_ASAN=OFF \
    -DAGENUI_TESTS_ENABLE_TSAN=ON
cmake --build tests/cpp/build/tsan -j 4
./tests/cpp/build/tsan/agenui_tsan_tests
```

The TSan target builds an additional `agenui_tsan_tests` executable that
includes the dedicated suite under `sanitizer/thread_sanitizer_test.cpp`.

> **Heads up**: The current AGenUI engine has 11 known data races between
> the main thread and the shared worker thread. They are documented in
> `DESIGN.md` §5. Until those are resolved, treat TSan output as
> diagnostic — failures here do not gate merges.

## Coverage report

```bash
cmake -S tests/cpp -B tests/cpp/build/coverage \
    -DAGENUI_TESTS_ENABLE_ASAN=OFF \
    -DAGENUI_TESTS_ENABLE_COVERAGE=ON
cmake --build tests/cpp/build/coverage -j 4
./tests/cpp/build/coverage/agenui_unit_tests > /dev/null
./tests/cpp/build/coverage/agenui_concurrency_tests > /dev/null

# Generate HTML report (requires lcov):
lcov --capture --directory tests/cpp/build/coverage --output-file cov.info
lcov --remove cov.info '*/_deps/*' '*/tests/cpp/*' --output-file cov.info
genhtml cov.info --output-directory tests/cpp/build/coverage/cov_html
open tests/cpp/build/coverage/cov_html/index.html
```

## Cross-compile (Android / Harmony)

The top-level CMake is cross-compile-friendly:

- `gtest_discover_tests` is skipped automatically when `CMAKE_CROSSCOMPILING`
  or `ANDROID` is set. Tests still build, but ctest doesn't enumerate
  them on the host.
- `target_link_libraries(... PUBLIC log)` is added on Android to satisfy
  `__android_log_print` references in agenui_core's default logger and
  in yoga.

The Android Studio module (`ide/android-studio/`) is the canonical
on-device build harness; see its README for details.

## Local yoga (skipping GitHub fetch)

If you run on an air-gapped CI, place a yoga 2.0 source tree somewhere
on disk and point CMake at it:

```bash
cmake -S tests/cpp -B tests/cpp/build \
    -DAGENUI_TESTS_USE_LOCAL_YOGA=ON \
    -DAGENUI_TESTS_LOCAL_YOGA_DIR=/path/to/yoga
```

## Faster iteration

- Use `ninja` instead of make:
  `cmake -S tests/cpp -B tests/cpp/build -G Ninja && ninja -C tests/cpp/build`
- Filter to a single suite during development:
  `./tests/cpp/build/agenui_unit_tests --gtest_filter='EngineLifecycleTest.*'`
- Sanitizer error message is **not enough** — set `ASAN_OPTIONS=halt_on_error=1:abort_on_error=1`
  to stop at the first error and get a usable core dump.

## Output paths

Everything generated lives under `tests/cpp/build/<variant>/`:

- `tests/cpp/build/host/`       — default (ASan + UBSan)
- `tests/cpp/build/tsan/`       — TSan
- `tests/cpp/build/coverage/`   — coverage
- `tests/cpp/build/xcode/`      — Xcode generator

All are covered by the repository root `.gitignore` rule `build/`.

**Source clones** (gtest, yoga) live in
`~/.cache/agenui-tests-deps/` by default — outside any `build/` tree
and outside the repo, so `git add tests/` never picks them up. Override
via `-DFETCHCONTENT_BASE_DIR=...` or `AGENUI_TESTS_DEPS_DIR=...`.
