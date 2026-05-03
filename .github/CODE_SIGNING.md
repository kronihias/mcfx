# GitHub Actions: Release Builds & Code Signing

This document explains how to configure the [release workflow](workflows/release.yml) so it can build, sign, notarize and publish mcfx VST3 installers for Windows 64-bit and macOS (universal arm64+x86_64).

## Overview

The workflow runs on two triggers:

- **`release: published`** — when you publish a GitHub release, both jobs run automatically and attach the resulting `.pkg` (macOS) and `.exe` (Windows) installers to the release.
- **`workflow_dispatch`** — manual trigger from the *Actions* tab. Lets you pick any branch and choose whether to sign. Outputs are uploaded as workflow artifacts (not attached to a release).

Both jobs are independent and run in parallel (`build-macos` on `macos-14`, `build-windows` on `windows-2022`).

## VST2 SDK (required)

`mcfx_anything` hosts VST2 plugins via the JUCE `JUCE_PLUGINHOST_VST` flag, which requires Steinberg's VST2 SDK headers (`pluginterfaces/vst2.x/aeffectx.h`). Steinberg discontinued public distribution of the VST2 SDK in 2018 and the headers cannot legally be redistributed. The workflow expects you to keep your own copy in a **private GitHub repo** and grant CI read access via an SSH deploy key.

The mcfx build scripts pass `-DBUILD_VST=TRUE` even for VST3-only builds (so `mcfx_anything` gets VST2 hosting), so the workflow will fail if the SDK isn't available — `BUILD_VST=TRUE` makes JUCE's `juce_set_vst2_sdk_path` fatal-error when the SDK is missing.

### One-time setup

1. Create a **private** GitHub repo (e.g. `your-user/vstsdk2.4`) containing the VST2 SDK with this layout:
   ```
   pluginterfaces/
     vst2.x/
       aeffect.h
       aeffectx.h
       vstfxstore.h
   ```
   (this matches the path checked at [CMakeLists.txt:95](../CMakeLists.txt#L95)).

2. Generate a deploy keypair locally:
   ```bash
   ssh-keygen -t ed25519 -f vst2sdk-deploy -N "" -C "mcfx CI deploy"
   ```

3. Add the **public** key (`vst2sdk-deploy.pub`) to the private SDK repo:
   *Settings → Deploy keys → Add deploy key* (read-only is sufficient).

4. Add the **private** key (`vst2sdk-deploy`) to the **mcfx** repo as a secret named `VST2_SDK_DEPLOY_KEY`:
   *Settings → Secrets and variables → Actions → New repository secret*. Paste the entire file contents including the `-----BEGIN OPENSSH PRIVATE KEY-----` header.

5. Add a repository **variable** (not a secret) named `VST2_SDK_REPO` with the SSH URL of your private repo:
   *Settings → Secrets and variables → Actions → Variables → New repository variable*. Example value:
   ```
   git@github.com:your-user/vstsdk2.4.git
   ```

6. Delete the local `vst2sdk-deploy*` keypair (it now lives in GitHub).

## macOS code signing & notarization

Add the following **repository secrets** in *Settings → Secrets and variables → Actions*:

| Secret | Contents |
|---|---|
| `CODESIGN_CERTIFICATE_P12` | Base64-encoded `.p12` containing both your Developer ID Application and Developer ID Installer certificates (with private keys). |
| `CODESIGN_CERTIFICATE_PASSWORD` | Password for the `.p12` above. |
| `KEYCHAIN_PASSWORD` | Arbitrary password for the temporary CI keychain (any random string). |
| `CODESIGN_APP` | Exact certificate identity string, e.g. `Developer ID Application: Matthias Kronlachner (W52ZCCWU2C)`. |
| `CODESIGN_INSTALLER` | Exact certificate identity string, e.g. `Developer ID Installer: Matthias Kronlachner (W52ZCCWU2C)`. |
| `NOTARIZE_APPLE_ID` | Apple ID email used for notarization. |
| `NOTARIZE_PASSWORD` | App-specific password generated at https://appleid.apple.com (NOT your Apple ID password). |
| `NOTARIZE_TEAM_ID` | Apple Developer Team ID (e.g. `W52ZCCWU2C`). |

### Exporting the `.p12`

In Keychain Access, expand each certificate to confirm the private key is present, then `Cmd-click` *both* certificate rows (Application + Installer), right-click → *Export 2 items…* → format **Personal Information Exchange (.p12)** → set a strong password.

Then base64-encode it for GitHub:

```bash
base64 -i certificate.p12 | pbcopy
```

Paste into the `CODESIGN_CERTIFICATE_P12` secret.

You can find the exact identity strings with:
```bash
security find-identity -v -p codesigning
```

## Windows code signing

| Secret | Contents |
|---|---|
| `WINDOWS_CODESIGN_PFX` | Base64-encoded `.p12` / `.pfx` containing your code-signing certificate for plugin binaries (corresponds to the local `scripts/DevIDApplication.p12`). |
| `WINDOWS_CODESIGN_PFX_PASSWORD` | Password for the certificate above. |
| `WINDOWS_INSTALLER_PFX` | Base64-encoded `.p12` / `.pfx` for signing the NSIS installer (corresponds to the local `scripts/DevIDInstaller.p12`). |
| `WINDOWS_INSTALLER_PFX_PASSWORD` | Password for the installer certificate. |

### Encoding the `.pfx`

```bash
base64 -i scripts/DevIDApplication.p12 | pbcopy   # then paste into WINDOWS_CODESIGN_PFX
base64 -i scripts/DevIDInstaller.p12  | pbcopy    # then paste into WINDOWS_INSTALLER_PFX
```

(If you only have a single certificate for both plugin and installer signing, supply the same value to both secrets.)

## How the workflow handles missing secrets

| Secret state | Result |
|---|---|
| `VST2_SDK_REPO` or `VST2_SDK_DEPLOY_KEY` missing | Workflow fails fast with a clear error before any build steps run. |
| Signing requested (`sign=true` or release event) but `CODESIGN_CERTIFICATE_P12` (macOS) / `WINDOWS_CODESIGN_PFX` (Windows) missing | Workflow logs a warning and falls back to an unsigned build. |
| `workflow_dispatch` with `sign=false` | Always produces unsigned installers — useful for verifying the pipeline without burning notarization quota. |

Unsigned macOS installers will trip Gatekeeper on user machines and unsigned Windows installers will get a SmartScreen warning, so always sign for actual releases.

## Publishing a release

1. Bump the [`VERSION`](../VERSION) file (single line, no `v` prefix, e.g. `0.7.3`).
2. Commit and push to `master`.
3. Create a tag and GitHub release:
   ```bash
   gh release create v0.7.3 --generate-notes
   ```
   The workflow triggers on **publish**, so make sure you don't leave the release as a draft.
4. Watch the *Actions* tab. Both jobs typically take 20–40 minutes (most of macOS time is notarization wait).
5. The signed `.pkg` and `.exe` installers appear as release assets when both jobs finish.

## Doing a dry run

To verify the pipeline on a feature branch without using your signing certificates:

1. Push your branch.
2. Go to *Actions* → *Release* → *Run workflow*.
3. Pick the branch, set `sign` to `false`.
4. Wait for the run; download the workflow artifacts to inspect the unsigned installers.

## Local equivalents

The workflow shells out to the existing scripts so anything that builds in CI also builds locally:

- macOS: `VST2SDKPATH=~/SDKs/vstsdk2.4 ./scripts/build_osx.sh --vst3` (signed) or add `--no-sign` for unsigned.
- Windows: edit/create `scripts/codesign.env` if needed, then `cd scripts && build_all_win64.bat vst3 sign`.

## Security notes

- Never commit `.p12` / `.pfx` files or `scripts/codesign.env` — `.gitignore` already excludes them.
- App-specific passwords can be revoked at https://appleid.apple.com if leaked.
- The temporary CI keychain is destroyed when the runner VM is recycled.
- Use a dedicated Apple ID for CI if you want to compartmentalize notarization access.
