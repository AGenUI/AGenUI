# AGenUI C++ test suite

Standalone gtest-based test suite for the platform-agnostic C++ core in
`core/`. Tests cover:

- **integration** — end-to-end through the public engine + SurfaceManager API
- **unit** — white-box tests for individual modules (parsers, dispatchers, …)
- **concurrency** — thread-safety + lifecycle race tests
- **stress** — short in-process pressure tests (~minutes)
- **sanitizer** — tests purpose-built to surface ASan / UBSan / TSan findings

The suite is **standalone**: it pulls gtest 1.14 and yoga 2.0 via
`FetchContent` and compiles `core/src/**` as a static library. It does
not modify or rebuild `platforms/`, `playground/`, or any existing test
tree.

## Quick start (host)

By default the convenience script runs **both ASan and TSan** suites in two
separate build trees:

```bash
./tests/cpp/ci/run_tests.sh                       # ASan + TSan (default)
./tests/cpp/ci/run_tests.sh --asan-only           # only ASan + UBSan
./tests/cpp/ci/run_tests.sh --tsan-only           # only TSan
./tests/cpp/ci/run_tests.sh --no-san              # plain build
./tests/cpp/ci/run_tests.sh --strict-tsan         # TSan failures gate the script
```

Or invoke CMake directly for finer control:

```bash
cmake -S tests/cpp -B tests/cpp/build/host        # default ASan + UBSan
cmake --build tests/cpp/build/host -j 4
ctest --test-dir tests/cpp/build/host --output-on-failure
```

## Build options

| CMake option | Default | Notes |
|---|---|---|
| `AGENUI_TESTS_ENABLE_ASAN` | `ON` | ASan + UBSan; mutex with TSAN |
| `AGENUI_TESTS_ENABLE_TSAN` | `OFF` | ThreadSanitizer; mutex with ASAN |
| `AGENUI_TESTS_ENABLE_COVERAGE` | `OFF` | gcov instrumentation |
| `AGENUI_TESTS_USE_LOCAL_YOGA` | `OFF` | Skip GitHub fetch; supply `AGENUI_TESTS_LOCAL_YOGA_DIR` |

See [`BUILD.md`](./BUILD.md) for full build details, sanitizer matrix,
and coverage report generation.

## What's covered

| Suite | Files | Tests |
|---|---|---|
| `integration/` | 9 | 85+ |
| `unit/` | 7 | 60+ |
| `concurrency/` | 3 | 11 |
| `stress/` | 1 | 2 |
| `sanitizer/memory_safety_test.cpp` | 1 | 4 |
| `sanitizer/thread_sanitizer_test.cpp` (TSan only) | 1 | 2 |

Total: **~165 tests**, all green under host build with ASan + UBSan
enabled. (See [DESIGN.md](./DESIGN.md) Appendix A for the complete
case list.)

### Known TSan findings

The dedicated TSan target reports **11 data races** in the engine,
all on the path between main thread and the shared worker thread. They
respect the documented "all engine APIs are called on the main thread"
contract but make multi-threaded callers fragile. These are diagnostic
output, not test failures — see [DESIGN.md §5](./DESIGN.md#5--known-tsan-findings)
for the proposed fixes.

## IDE debugging

| IDE | Doc |
|---|---|
| Xcode (macOS host) | [`ide/xcode/README.md`](./ide/xcode/README.md) |

Quick recipe:

```bash
# Xcode (defaults to BOTH ASan and TSan projects):
./tests/cpp/ide/xcode/generate.sh
open tests/cpp/build/xcode/agenui_cpp_tests.xcodeproj        # ASan + UBSan
open tests/cpp/build/xcode-tsan/agenui_cpp_tests.xcodeproj   # TSan
# pick scheme `agenui_unit_tests`, ⌘R to run, set breakpoints in core/src/**.

```

## Adding a new test

1. Create a new `.cpp` file in the right subdirectory:
   - Black-box, public API only → `integration/`
   - White-box, internal class → `unit/<area>/`
   - Multi-thread → `concurrency/`
2. Use the test fixtures in `support/`:
   - `ScopedSurfaceManager` for per-test SurfaceManager isolation
   - `MockMessageListener` to capture callbacks
   - `WaitForWorkerIdle()` to synchronize with the engine's worker thread
3. Re-run CMake configure (file globs are resolved once):
   ```bash
   cmake -S tests/cpp -B tests/cpp/build
   ```
4. Build & run; if you added a new fixture file, update `fixtures/`.

## Layout

```
tests/cpp/
├── CMakeLists.txt          # build
├── main.cpp                # gtest entry + global engine env
├── support/                # mocks, helpers, fixture loader
├── fixtures/               # protocol/json fixtures
├── integration/            # 9 files, 85+ tests
├── unit/{module,stream,style_parser,function_call}/
├── concurrency/            # 3 files, 11 tests
├── stress/                 # in-process stress
├── sanitizer/              # ASan + TSan dedicated
├── ide/                    # Xcode
├── ci/                     # run_tests.sh + GitHub Actions template
├── DESIGN.md               # reviewed design with full case list
└── BUILD.md                # build / sanitizer / coverage details
```
