# AGenUI Integration Tests

## Overview

Three-platform integration test runner: builds each platform's playground app,
installs it on a device/simulator, runs instrumented tests, collects results,
and generates a unified HTML summary report.

## Usage

```bash
# Run all three platforms
./tests/integration/run.sh

# Run a single platform
./tests/integration/run.sh --android
./tests/integration/run.sh --ios
./tests/integration/run.sh --harmony

# Skip a platform
./tests/integration/run.sh --skip-harmony

# Custom output directory
./tests/integration/run.sh --output-dir /tmp/reports

# Open HTML report after completion
./tests/integration/run.sh --open-report

# Show full help
./tests/integration/run.sh --help
```

## Directory Structure

```
integration/
├── run.sh              Main orchestrator (dispatches to platform scripts)
├── generate_report.sh  Parses JUnit XML → unified HTML report
├── parse_reports.py    JUnit/xcresult/Harmony output parser
├── fixtures/           Shared test fixtures
├── README.md           This file
└── platforms/
    ├── android/
    │   └── android.sh          Gradle connectedDebugAndroidTest
    ├── ios/
    │   ├── ios.sh              xcodebuild test (XCTest)
    │   └── add_test_target.rb  Injects XCTest target into the iOS playground
    └── harmony/
        ├── harmony.sh          hvigorw assembleHap + hdc shell aa test
        └── patch_signing.py    Patches HAP signing config for CI
```

## Platform Scripts

Each platform script (`platforms/<platform>.sh`):

1. Validates prerequisites (SDK, device connectivity)
2. Builds the test target
3. Installs and runs tests on the connected device/simulator
4. Copies test results (JUnit XML or raw output) to `--output-dir`

They can be invoked standalone:

```bash
./tests/integration/platforms/android/android.sh --output-dir ./reports/android
./tests/integration/platforms/ios/ios.sh --simulator "iPhone 16"
./tests/integration/platforms/harmony/harmony.sh --test-class InitializationTest
```

## Reports

Results are written to `reports/runs/<run_key>/` with a `latest` symlink:

```
reports/
├── latest -> runs/<run_key>     (symlink to most recent run)
├── index.html                   (historical runs index)
└── runs/<run_key>/
    ├── metadata.json
    ├── summary.html
    ├── android/                 (JUnit XML + raw output)
    ├── ios/                     (xcresult + raw output)
    └── harmony/                 (raw output)
```

## Dependencies

- **Android**: `ANDROID_HOME`, `adb`, Java 11-23, connected device/emulator
- **iOS**: macOS, Xcode, CocoaPods, iOS Simulator or physical device
- **Harmony**: DevEco Studio, `hdc`, connected HarmonyOS device
