# AGenUI SDK Stability Test

## Overview

The stability test verifies the reliability of the AGenUI SDK under long-running, high-pressure scenarios. The test system supports two modes:

- **Stress Mode**: Rapid synchronous execution of extreme operations (no UI rendering), covering multi-surface, long conversations, high-frequency updates, and other extreme scenarios
- **Realistic Mode**: Full-screen rendering of real UI components, simulating normal user workflows while also covering stress mode patterns (abnormal data injection, interrupt recovery, etc.)

Both modes support automatic crash detection, scenario skipping, memory monitoring, and auto-restart.

Supported platforms: **Android**, **iOS**, **HarmonyOS**.

## Quick Start

```bash
# === Android ===
# Simplest usage (default all_combined scenario, 8 hours)
./tests/stability/run.sh --android

# Short test (30 minutes)
./tests/stability/run.sh --android --duration 30

# Build + install then run
./tests/stability/run.sh --android --install --duration 60

# Specify a single scenario
./tests/stability/run.sh --android --scenario extreme_render --duration 120

# Run only realistic rendering scenarios
./tests/stability/run.sh --android --scenario all_realistic --duration 120

# Run a single realistic scenario
./tests/stability/run.sh --android --scenario realistic_article_stream --duration 30

# Custom crash threshold (skip after 3 crashes)
./tests/stability/run.sh --android --crash-threshold 3 --duration 60

# === iOS ===
# Use iOS Simulator
./tests/stability/run.sh --ios --simulator --duration 30

# Use physical device
./tests/stability/run.sh --ios --duration 60

# Specify device UDID
./tests/stability/run.sh --ios --device-id <UDID> --duration 60

# Build + install then run
./tests/stability/run.sh --ios --simulator --install --duration 30

# === HarmonyOS ===
# Default run
./tests/stability/run.sh --harmony --duration 60

# Build + install then run
./tests/stability/run.sh --harmony --install --duration 30
```

## Prerequisites

### Android
- Android device/emulator connected (`adb devices` visible)
- APK installed on device (or use `--install` to auto-build and install)
- `adb` added to PATH

### iOS
- Physical device: `ios-deploy` or Xcode 15+ (`xcrun devicectl`)
- Simulator: Xcode installed and simulator booted (`xcrun simctl`)
- Device trust established
- App installed (or use `--install` to auto-build and install)

### HarmonyOS
- HarmonyOS device connected (`hdc list targets` visible)
- HAP installed on device (or use `--install` to auto-build and install)
- `hdc` added to PATH
- DevEco Studio / hvigorw available (only required for `--install`)

## Command Line Options

| Parameter | Default | Description |
|------|--------|------|
| `--android` | (default) | Platform: Android |
| `--ios` | - | Platform: iOS |
| `--harmony` | - | Platform: HarmonyOS |
| `--simulator` | - | (iOS) Use iOS Simulator |
| `--device-id <id>` | - | (iOS) Specify device UDID |
| `--scenario <name>` | `all_combined` | Test scenario (see scenario list below, supports stress/realistic/meta) |
| `--duration <minutes>` | `480` | Run duration (minutes) |
| `--rounds <n>` | `0` | Max rounds (0=unlimited) |
| `--interval <ms>` | `100` | Delay between rounds (milliseconds) |
| `--crash-threshold <n>` | `5` | Auto-skip scenario after N crashes |
| `--install` | - | Build and install before running |
| `-h, --help` | - | Show help |

## Test Scenarios

### Stress Scenarios

Synchronous execution, no UI rendering, focused on extreme operational pressure:

| Scenario | Identifier | Description |
|------|------|------|
| Session Storm | `session_storm` | Rapidly create/destroy SurfaceManager instances (10 per round) |
| Stream Marathon | `stream_marathon` | Single surface long-duration streaming (100 chunks per round) |
| Multi Surface | `multi_surface` | 5 surfaces exist simultaneously with random updates |
| Action Flood | `action_flood` | Rapidly submit 50 UI data changes (simulating high-frequency user operations) |
| Theme Switch | `theme_switch` | Rapid light/dark mode toggling (20 times per round) |
| Interrupt Recover | `interrupt_recover` | Interrupt mid-stream and recover |
| Extreme Render | `extreme_render` | Randomly load 36 extreme fixture files for rendering |
| SDK Robustness | `sdk_robustness` | 12 API misuse defense sub-cases (access after destroy, null params, malformed JSON, etc.) |
| SDK Interface Stability | `sdk_interface_stability` | Global config, listener, stream, surface, and utility API stability under repeated, interleaved, and edge-case usage |
| JNI Bridge Race | `jni_bridge_race` | Concurrent set/clear of `nativeSetSurfaceSizeProvider` / `nativeClearSurfaceSizeProvider` while engine worker thread is mid-call into the JNI surface-size bridge — targets cross-thread UAF on the bridge's `_javaHost` global ref |

### Realistic Scenarios

Asynchronous execution, full-screen rendering of real UI components, with a floating overlay showing real-time metrics. Each scenario also covers one or more stress modes:

| Scenario | Identifier | Stress Coverage | Description |
|------|------|-------------|------|
| Article Stream | `realistic_article_stream` | STREAM_MARATHON | Markdown article streamed paragraph by paragraph |
| Multi Card | `realistic_multi_card` | MULTI_SURFACE + SESSION_STORM | 4 surfaces displayed simultaneously (chat bubbles), created/destroyed one by one |
| Form Fill | `realistic_form_fill` | ACTION_FLOOD + abnormal data | Form filling with malformed JSON injection |
| Chart Refresh | `realistic_chart_refresh` | EXTREME_RENDER + abnormal data | Dashboard chart refresh + empty component injection |
| Long List | `realistic_long_list` | STREAM_MARATHON + abnormal data | News list incremental loading + unknown component type injection |
| Page Switch | `realistic_page_switch` | SESSION_STORM + INTERRUPT_RECOVER | Rapid surface create/destroy simulating page switching |
| Tab Navigation | `realistic_tab_navigation` | THEME_SWITCH + abnormal data | Tab switching + empty components injection |
| Lottie Carousel | `realistic_lottie_carousel` | INTERRUPT_RECOVER + EXTREME_RENDER | Carousel + Lottie playback with stream interrupt recovery |
| Mixed Dashboard | `realistic_mixed_dashboard` | MULTI_SURFACE + ACTION_FLOOD | Complex layout + 6 rapid dataModel updates |
| Error Recovery | `realistic_error_recovery` | All abnormal data injection | 5 types of abnormal data injection followed by normal recovery |

### Meta Scenarios

| Scenario | Identifier | Description |
|------|------|------|
| All Combined | `all_combined` | Randomly selects from 7 stress + 10 realistic scenarios (recommended) |
| All Stress | `all_stress` | Randomly selects from 7 stress scenarios only |
| All Realistic | `all_realistic` | Randomly selects from 10 realistic scenarios only |

## Architecture

```
tests/stability/
├── run.sh                     # Main entry: argument parsing, dispatch, result collection
├── generate_report.py         # HTML report generator (universal, supports all platforms)
├── symbolicate.py             # Crash log symbolication
├── platforms/
│   ├── android/
│   │   ├── launch.sh          # Build/install APK, start StabilityTestActivity
│   │   ├── monitor.sh         # Monitor process health, crash/freeze detection with auto-restart
│   │   └── collect.sh         # Pull logs from device, generate summary statistics
│   ├── ios/
│   │   ├── launch.sh          # Build/install IPA, start StabilityTestViewController
│   │   ├── monitor.sh         # Monitor process health (simctl/devicectl), crash/freeze detection
│   │   └── collect.sh         # Pull logs from device/simulator
│   └── harmony/
│       ├── launch.sh          # Build/install HAP, start StabilityTestAbility
│       ├── monitor.sh         # Monitor process health (hdc), crash/freeze detection
│       ├── collect.sh         # Pull logs from device (hdc file recv)
│       └── symbolicate_crash.sh
```

### Platform Tool Comparison

| Function | Android | iOS (Device) | iOS (Simulator) | HarmonyOS |
|------|---------|--------------|-----------------|-----------|
| Device Connection | `adb` | `devicectl` / `ios-deploy` | `simctl` | `hdc` |
| Process Check | `adb shell pidof` | `devicectl info processes` | `simctl spawn launchctl` | `hdc shell pidof` |
| Log Collection | `adb logcat` | `idevicesyslog` | `simctl spawn log show` | `hdc hilog` |
| Crash Reports | tombstone / dropbox | `.ips` crash reports | DiagnosticReports | faultlog |
| File Pull | `adb pull` | `ios-deploy --download` | Direct file system | `hdc file recv` |
| Launch App | `adb shell am start` | `devicectl process launch` | `simctl openurl` | `hdc shell aa start` |
| Force Stop | `adb shell am force-stop` | `devicectl process terminate` | `simctl terminate` | `hdc shell aa force-stop` |

playground/android/app/src/main/java/.../stability/
├── StabilityTestActivity.java # Self-driving test Activity (Handler-based loop)
├── StabilityScenarioEngine.java # Implementation of 8 scenarios
├── StabilityLogger.java       # JSONL format test metrics logger
├── MemoryMonitor.java         # Memory monitoring (baseline/peak/growth)
└── CrashTracker.java          # Crash tracking and scenario skip mechanism

playground/ios/Playground/.../Stability/
├── StabilityTestViewController.swift  # Dual-mode test VC (stress labels / realistic full-screen rendering)
├── StabilityScenarioEngine.swift      # Stress scenario enum + synchronous execution engine
├── RealisticScenarioEngine.swift      # Realistic scenario async execution engine (SurfaceManager-driven)
├── MetricsOverlayView.swift           # Floating real-time metrics panel
├── StabilityLogger.swift              # JSONL logger
├── StabilityMemoryMonitor.swift       # Memory monitoring
└── StabilityCrashTracker.swift        # Crash tracking

playground/resource/stability_fixtures/   # 46 test fixtures
├── extreme_components/        # 100 components, deep nesting, long text, etc.
├── extreme_data/              # Large arrays, complex bindings, large datamodel
├── extreme_stream/            # Large payload, tiny chunks, rapid updates
├── extreme_lifecycle/         # Concurrent operations, rapid create/destroy
├── extreme_interaction/       # Validation storm, complex action params
└── realistic_scenarios/       # 10 realistic user scenarios (with UI rendering)
```

## Realistic Mode

When executing realistic scenarios, the iOS `StabilityTestViewController` automatically switches to full-screen rendering mode:

```
┌────────────────────────────────┐
│  ┌──────────────────────┐      │
│  │  MetricsOverlay      │      │  ← Top-right overlay (semi-transparent black)
│  │  R:42  00:05:30      │      │     Shows round, time, memory, error count
│  │  128MB (pk:145)      │      │
│  └──────────────────────┘      │
│                                │
│  ┌────────────────────────────┐│
│  │                            ││
│  │   Real rendered Surface    ││  ← Full-screen ScrollView hosting Surface
│  │   (Markdown/Card/Chart...) ││     Multiple surfaces stacked vertically
│  │                            ││
│  └────────────────────────────┘│
└────────────────────────────────┘
```

### Workflow

1. `RealisticScenarioEngine` loads fixture JSON and creates a `SurfaceManager` instance
2. Executes each action asynchronously in `steps[]` array order, with `delay_ms` delays
3. When `SurfaceManager` calls back `onCreateSurface`, adds `surface.view` to the full-screen ScrollView
4. Overlay `MetricsOverlayView` refreshes every second, displaying current metrics
5. After all steps complete, cleans up surface views, records duration, schedules next round

### Hybrid Mode Execution

When using `all_combined` (default), the system randomly selects stress or realistic scenarios:
- Selected **stress scenario** → switches to label UI, synchronous execution
- Selected **realistic scenario** → switches to full-screen rendering, returns to scheduling loop after async execution completes

## Crash Detection & Auto-Skip

### How It Works

1. **Write-ahead**: Before each round, writes the current scenario name to `crash_state.json` on the device
2. **Crash attribution**: After process crash and restart, detects `crash_state.json` exists → attributes crash to that scenario
3. **Registry**: Accumulates crash count for each scenario in `crash_registry.json`
4. **Blacklist**: Automatically skips scenarios when crash count >= threshold

### Example Flow

```
Round 100: EXTREME_RENDER → crash_state.json written → SIGSEGV → process killed
    ↓
monitor.sh detects crash → reads crash_state.json → "Crashed scenario: EXTREME_RENDER"
    ↓
monitor.sh restarts app → StabilityTestActivity.onCreate()
    ↓
CrashTracker reads crash_state.json → EXTREME_RENDER crash_count: 1→2→3→4→5
    ↓
crash_count >= threshold(5) → EXTREME_RENDER blacklisted
    ↓
ALL_COMBINED mode: future rounds skip EXTREME_RENDER, test other scenarios
```

### Device State Files

**Android:**
```
/sdcard/Android/data/com.amap.agenuiplayground/files/stability/
├── stability_log.jsonl     # Test metrics log
├── crash_state.json        # Currently executing scenario (auto-deleted after completion)
└── crash_registry.json     # Crash count and blacklist registry
```

**iOS (Simulator):**
```
<AppContainer>/Documents/stability/
├── stability_log.jsonl
├── crash_state.json
└── crash_registry.json
```

**HarmonyOS:**
```
/data/app/el2/100/base/com.harmony.agenui/files/stability/
├── stability_log.jsonl
├── crash_state.json
└── crash_registry.json
```

### Reset Crash Counts

Reset via intent extra:
```bash
adb shell am start -n com.amap.agenuiplayground/.stability.StabilityTestActivity \
    --es scenario "all_combined" --ei reset_crash_counts 1 --ei duration_minutes 60
```

## Output Structure

Each run generates an independent result directory:

```
reports/stability/runs/<branch>_<commit>_<timestamp>/
├── metadata.json               # Run parameters (scenario, duration, git info)
└── android/
    ├── stability_log.jsonl     # Per-round detailed metrics (JSONL)
    ├── monitor_summary.json    # Crash summary
    ├── collection_summary.json # Round statistics
    ├── final_logcat.txt        # Last 1000 lines of logcat
    └── crashes/
        ├── crash_1_20260511_143000.txt
        └── crash_2_20260511_144500.txt
```

### stability_log.jsonl Format

One JSON object per line:

```json
{"ts":"2026-05-11T14:30:45.123","round":1,"scenario":"SESSION_STORM","duration_ms":142,"memory_native_mb":45,"memory_java_mb":23,"memory_total_mb":68.0,"status":"ok","fixture":null}
{"ts":"2026-05-11T14:30:45.300","round":2,"scenario":"EXTREME_RENDER","duration_ms":89,"memory_native_mb":46,"memory_java_mb":24,"memory_total_mb":70.0,"status":"error","fixture":"extreme_stream/mega_payload.json","error":"OutOfMemoryError: ..."}
{"ts":"2026-05-11T14:30:50.000","event":"crash_detected","detail":"crashed_scenario=EXTREME_RENDER,blacklist=EXTREME_RENDER(5)"}
```

### monitor_summary.json Format

```json
{
  "duration_minutes": 480,
  "crash_count": 3,
  "crash_threshold": 5,
  "crash_scenarios": ["EXTREME_RENDER", "EXTREME_RENDER", "SESSION_STORM"],
  "end_time": "2026-05-11T22:30:00Z"
}
```

## Fixture System

46 test fixture files located in `playground/resource/stability_fixtures/`, divided into 6 categories:

| Category | Files | Description |
|------|--------|------|
| `extreme_components/` | 10 | 100-component hierarchy, deep nesting (50 levels), 50 buttons, long text (10KB) |
| `extreme_data/` | 8 | 1000-element arrays, complex binding paths, large datamodel |
| `extreme_stream/` | 6 | 124KB payload, 1-byte chunks, high-frequency updates |
| `extreme_lifecycle/` | 6 | Concurrent operations, rapid create/destroy, double create |
| `extreme_interaction/` | 6 | Validation storm, complex action params, concurrent sync |
| `realistic_scenarios/` | 10 | Realistic user scenarios with visible UI rendering and timed steps |

### Extreme Fixture Format

Extreme fixtures used by stress scenarios support two modes:
- `"messages": [...]` — sent one message at a time
- `"payload": [...]` — concatenated and streamed in 100-byte chunks

### Realistic Fixture Format

Realistic scenario fixtures use a steps format, where each step contains an action type and delay:

```json
{
  "description": "Scenario description",
  "surfaceId": "realistic-xxx",
  "multi_surface": false,
  "steps": [
    {"action": "createSurface", "delay_ms": 0, "message": {...}},
    {"action": "updateComponents", "delay_ms": 300, "message": {...}},
    {"action": "rawChunk", "delay_ms": 1000, "raw": "malformed data..."},
    {"action": "beginNewStream", "delay_ms": 3000, "description": "interrupt"}
  ],
  "stability_meta": {
    "category": "realistic_scenarios",
    "expected_duration_ms": 8000,
    "components_used": ["Markdown", "Card", "Text"],
    "stress_coverage": "STREAM_MARATHON+ABNORMAL_DATA"
  }
}
```

Supported action types:

| action | Description |
|--------|------|
| `createSurface` | Create a new surface; message is a complete JSON protocol message |
| `updateComponents` | Update the component tree |
| `updateDataModel` | Update the data model |
| `deleteSurface` | Delete a surface |
| `rawChunk` | Inject raw text (can be malformed JSON to test error handling) |
| `beginNewStream` | Interrupt current stream and start a new one (simulates crash recovery) |

## Interpreting Results

### Key Metrics

1. **crash_count** — Process-level crash count (SIGSEGV, etc.)
2. **error_count** — Java/application-layer exception count (caught, did not cause crash)
3. **memory_total_mb trend** — Continuous growth indicates memory leak
4. **Frequent crashes in specific scenarios** — Check `crash_scenarios` array to identify problematic scenarios

### Suggested Success Criteria

| Metric | Suggested Threshold |
|------|---------|
| crash_count | 0 (no native crash) |
| error_count / total_rounds | < 1% |
| memory_growth | < 50MB / 8h |
| blacklisted scenarios | 0 |

## Troubleshooting

### Android

| Issue | Troubleshooting |
|------|---------|
| Test not started | Check process with `adb shell pidof com.amap.agenuiplayground` |
| Logs empty | Check `adb shell ls /sdcard/Android/data/com.amap.agenuiplayground/files/stability/` |
| Scenario crashes repeatedly | Check `crash_registry.json` for count, run that scenario alone to reproduce |
| All scenarios blacklisted | App auto-exits with "All scenarios blacklisted"; lower `--crash-threshold` or fix and reset |
| Memory keeps growing | Compare `memory_start_mb` and `memory_end_mb`, use Android Profiler for deep analysis |

### iOS

| Issue | Troubleshooting |
|------|---------|
| Simulator not found | Confirm booted simulator with `xcrun simctl list devices booted` |
| Device not connected | Check with `xcrun devicectl list devices` or `ios-deploy --detect` |
| Log pull failed | Confirm `ios-deploy` is installed (`brew install ios-deploy`) |
| Crash reports empty | Check `~/Library/Logs/DiagnosticReports/` for Playground-related `.ips` files |
| URL Scheme not registered | Confirm App's Info.plist has registered `agenui-stability://` URL Scheme |

### HarmonyOS

| Issue | Troubleshooting |
|------|---------|
| Device not connected | Confirm device online with `hdc list targets` |
| HAP install failed | Check signing configuration, confirm `hvigorw` is available |
| hilog no output | Manually verify with `hdc hilog -x` that logs are readable |
| faultlog no permission | Generally accessible in developer mode; production devices may require root |
| Process detection abnormal | Manually verify with `hdc shell pidof com.harmony.agenui` |
