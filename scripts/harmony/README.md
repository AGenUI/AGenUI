# HarmonyOS Playground Release Build

Builds the AppGallery-ready `.app` package for the HarmonyOS Playground demo
(`playground/harmony`), signed with the release certificate and release profile.

This is the app-package counterpart of `build.sh`, which builds the SDK `.har`.

## Prerequisites

| Item | Notes |
|---|---|
| DevEco Studio 6.0.x | Or the headless Command Line Tools |
| `node` | Generates the hvigor signing material |
| `java` + `openssl` | Only for signature verification |

The toolchain is auto-detected at `/Applications/DevEco-Studio.app/Contents`.
Override with `DEVECO_HOME`, or point `COMMAND_LINE_TOOLS_HOME` at an unpacked
Command Line Tools directory on a headless machine.

Only macOS with DevEco Studio has been verified end to end.

## Signing credentials

Credentials are read from environment variables and never from a config file,
so nothing secret can be committed.

| Variable | Required | Default |
|---|---|---|
| `HARMONY_PLAYGROUND_KEYSTORE_PATH` | Yes | — |
| `HARMONY_PLAYGROUND_KEYSTORE_PASSWORD` | Yes | — |
| `HARMONY_PLAYGROUND_KEY_PASSWORD` | Yes | — |
| `HARMONY_PLAYGROUND_CERT_PATH` | Yes | — |
| `HARMONY_PLAYGROUND_PROFILE_PATH` | Yes | — |
| `HARMONY_PLAYGROUND_KEY_ALIAS` | No | `release` |
| `HARMONY_PLAYGROUND_BUNDLE_NAME` | No | `com.agenui.playground` |

The four files come from AppGallery Connect → Certificates, APP IDs and
Profiles:

| Extension | Role |
|---|---|
| `.p12` | PKCS#12 private key store |
| `.csr` | Certificate request; unused after the cert is issued |
| `.cer` | Huawei-issued release certificate chain |
| `.p7b` | CMS-signed Release Profile |

The `.p7b` hard-binds a `bundle-name`. Keep
`HARMONY_PLAYGROUND_BUNDLE_NAME` equal to it, or signing succeeds and the
package is still rejected at upload.

## Usage

```bash
HARMONY_PLAYGROUND_KEYSTORE_PATH=/path/to/release.p12 \
HARMONY_PLAYGROUND_KEYSTORE_PASSWORD=<keystore-password> \
HARMONY_PLAYGROUND_KEY_PASSWORD=<key-password> \
HARMONY_PLAYGROUND_CERT_PATH=/path/to/release.cer \
HARMONY_PLAYGROUND_PROFILE_PATH=/path/to/release.p7b \
  ./scripts/harmony/build-playground.sh
```

Every command-line argument is optional:

| Argument | Default |
|---|---|
| `--version` | `AGENUI_VERSION` in `core/include/agenui_version.h` |
| `--version-code` | `major*1000000 + minor*1000 + patch` |
| `--output-dir` | `dist/harmony/playground` |
| `--artifact-name` | `AGenUI-Playground-<version>-harmony.app` |
| `--clean` | Run `hvigorw clean` first |

AppGallery requires a strictly increasing `versionCode`. The derived value is
monotonic in the version name, so bumping `AGENUI_VERSION` is normally enough.
Pass `--version-code` only to override that scheme.

## Output

| File | Purpose |
|---|---|
| `AGenUI-Playground-<v>-harmony.app` | Upload this to AppGallery |
| `...harmony.app.sha256` | Checksum |
| `...harmony-symbol.zip` | Optional, for AGC crash analysis |

## What the script does

1. Copies `.p12`/`.cer`/`.p7b` into a `mktemp -d` scratch directory (mode 700).
2. Generates the `material/{fd,ac,ce}` key store next to the copied `.p12` and
   encrypts both passwords into it.
3. Injects a `signingConfigs` entry plus the release `bundleName`,
   `versionName` and `versionCode` into `build-profile.json5` and
   `AppScope/app.json5`.
4. Runs `hvigorw assembleApp --mode project -p product=default -p buildMode=release`.
5. Verifies `pack.info`, then extracts the embedded profile and fails unless it
   reports `type=release` and `app-distribution-type=app_gallery`.
6. Copies the artifacts to the output directory.
7. Restores both project files and deletes the scratch directory, on every exit
   path including `Ctrl-C`.

Steps 3 and 7 are why the repository keeps `com.harmony.agenui` and an empty
`signingConfigs`: contributors get a clean debug project, and the release
identity exists only for the duration of the build.

### Why `hvigor-sign-material.js` exists

hvigor rejects plaintext `storePassword`/`keyPassword` with error `00303116`
("length ... is less than 32"). It expects the AES-128-GCM ciphertext that
DevEco Studio writes, and derives the key from a `material/{fd,ac,ce}`
directory located at `dirname(storeFile)/material` — see
`hvigor-ohos-plugin/src/utils/decipher-util.js`. The helper reproduces that
format so a release package can be built without opening the IDE.

Passwords reach the helper through stdin, not argv, so they do not appear in
the process list.

## Notes

- `buildMode=release` and release signing are independent. A release-mode build
  signed with a debug profile installs on registered devices and is silently
  rejected by AppGallery, which is why step 5 checks the embedded profile.
- `Ohos BundleTool [Warning]: Hap debug is true or Hsp verify infos is empty`
  is emitted for release-signed packages too. The second clause applies
  because this project has no HSP modules. Judge the package by the profile
  the script verifies, not by this warning.
- Upload the `.app`, not the `.hap` inside it.
- `dist/` is not gitignored. Do not `git add -A` after a build.
- The script leaves the working tree untouched. Confirm with
  `git status --short playground/harmony/` if a build was interrupted.

## Troubleshooting

| Symptom | Cause |
|---|---|
| `Required environment variable is missing` | One of the five credentials is unset |
| `points to a missing file` | Credential path is wrong |
| `Cannot derive a version code` | `--version` is not `x.y.z`; pass `--version-code` |
| `hvigorw not found` | Set `DEVECO_HOME` or `COMMAND_LINE_TOOLS_HOME` |
| `Failed to prepare signing material` | See the reported field; usually a bad path |
| `Embedded profile type is 'debug'` | The `.p7b` is a debug profile |
| `pack.info mismatch` | Injection did not take; re-run and check the log |

## Related

- `build.sh` — builds the SDK `.har` (`dist/harmony/release`)
- `.github/workflows/release-harmony.yml` — publishes the `.har` on release
- `.github/workflows/release-playground-android.yml` — the Android equivalent
