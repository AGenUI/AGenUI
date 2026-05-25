# Xcode debugging

Generate a single Xcode project for the AGenUI C++ test suite, run the tests,
and step through `core/src/**/*.cpp` with the LLDB debugger.

## Quick start

```bash
# From the repo root:
./tests/cpp/ide/xcode/generate.sh
open tests/cpp/build/xcode/agenui_cpp_tests.xcodeproj
```

The generator produces **one** Xcode project that contains both sanitizer
modes as build configurations:

| Configuration | Sanitizer | When to use |
|---|---|---|
| **Debug** (default) | AddressSanitizer + UBSan | Day-to-day debugging |
| **Tsan** | ThreadSanitizer | Race investigation |
| **Release** | none | Performance work |

## Switching configuration in Xcode

1. Top-left scheme drop-down: pick a scheme
   - `agenui_unit_tests` — integration / unit / stress / sanitizer-asan (~157 cases)
   - `agenui_concurrency_tests` — concurrency + chaos (~48 cases)
   - `agenui_tsan_tests` — TSan-dedicated suite (run under the `Tsan` config)
2. **Product → Scheme → Edit Scheme...** (or hold `Option` while clicking ▶)
3. **Run → Info → Build Configuration**: pick `Debug` (ASan) or `Tsan` (TSan)
4. ⌘R runs with the selected configuration

You can keep two windows open with the same project — one with each
configuration — to compare side-by-side.

Tip: duplicate a scheme via **Manage Schemes... → ⚙ → Duplicate**, rename
to `agenui_unit_tests-tsan`, and pin its Build Configuration to `Tsan`.
That gives you a one-click TSan run.

## Profiling

Use **Product → Profile** (⌘I) to launch Instruments (Time Profiler, Allocations,
Leaks, …).

## Common issues

| Symptom | Fix |
|---|---|
| `code object is not signed`, process killed (exit 137) | The build now ad-hoc signs every test binary automatically. If you build inside Xcode and it still fires, run `codesign --force --sign - <binary>` manually. |
| `gtest_discover_tests: Subprocess killed` after build | This is the same code-signing issue and is harmless for development; the build artifact is still good. |
| Re-generate after adding new `.cpp` test files | Run `./tests/cpp/ide/xcode/generate.sh` again — `file(GLOB)` is only re-evaluated at CMake configure time. |

## Working directory

Tests use `TESTS_FIXTURE_DIR` (compile-time macro pointing at
`tests/cpp/fixtures/`). No runtime CWD setup is required in Xcode.
