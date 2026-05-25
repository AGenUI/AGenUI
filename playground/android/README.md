# AGenUI Android Playground

Sample Android project for the AGenUI SDK, used to demonstrate SDK features and provide a component debugging and validation environment.

## Project Structure

```
playground/android/
├── app/
│   ├── src/main/java/        # Playground source code
│   ├── src/main/res/         # Resource files
│   └── libs/                 # AAR artifact directory (used in AAR mode)
├── build.gradle              # Root project build configuration
├── settings.gradle           # Gradle project configuration
└── gradle.properties         # SDK dependency mode switch
```

This project lives inside a monorepo. The dependency hierarchy is:

```
playground/android/app
    └── platforms/android/        # Android SDK (Java + JNI)
            └── core/             # C++ core (compiled via CMake + NDK)
```

---

## Quick Start

Open the `playground/android/` directory directly in Android Studio, sync Gradle, then build and run. No additional initialization steps are required.

**Requirements:**

| Item | Requirement |
|---|---|
| Android Studio | Hedgehog or later |
| Android SDK | API 35 |
| NDK | 27.3.13750724 |
| minSdk | 21 |
| Java | 11 |

---

## SDK Dependency Mode

Switch between modes via `agenui.sdk.source` in `gradle.properties`:

### `source=false` (default — AAR mode)

```properties
agenui.sdk.source=false
```

At build time, Gradle automatically:
1. Runs `assembleRelease` in `platforms/android/` to compile the SDK (including the C++ engine)
2. Copies the produced AAR to `app/libs/AGenUI-Client-Android-release.aar`
3. Builds the Playground APK

Best for: modifying only Playground code while the SDK is relatively stable, or validating the SDK release artifact.

### `source=true` (source mode)

```properties
agenui.sdk.source=true
```

`platforms/android/` participates directly as a Gradle subproject, allowing the SDK and Playground to be compiled incrementally within the same Gradle build.

Best for: simultaneously modifying the SDK and the Playground for rapid iteration.

---

## Yoga Configuration

The SDK uses [yoga](https://github.com/aspect-build/rules_js) for layout. By default it is fetched from GitHub via CMake FetchContent. You can override this through `local.properties`:

```properties
# (Optional) Path to prebuilt yoga artifacts (libyoga.so + headers)
yoga.prebuilt.dir=/path/to/yoga_prebuilt/android/arm64-v8a/Release

# (Optional) Whether to bundle libyoga.so into the AAR (default: true)
yoga.include.in.aar=true
```

**Priority:** Gradle `-P` parameter > `local.properties` > default (FetchContent).
