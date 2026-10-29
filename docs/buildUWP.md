# Build Guide: Xbox UWP

This guide builds the x64 UWP package for Xbox Dev Mode. The port keeps its
host, compatibility code, manifest, and assets under `ports/uwp`. The main
source tree still owns the game loop, SDL D3D11 renderer, input, audio, and
content handling.

Rendering, controller input, WASAPI playback, LocalState content loading, and
save-file creation were reported working on Xbox by the original PR author. This integration still
requires a local build and testing on hardware.

## Requirements

Install Visual Studio 2022 with the C++ UWP tools and Windows SDK
`10.0.19041.0`, plus LLVM/Clang, Ninja, CMake 3.24 or newer, and vcpkg.

Set the vcpkg checkout before building:

```powershell
$env:VCPKG_ROOT = '<path-to-vcpkg>'
```

Use vcpkg commit `80f9bcfa455e875d9c1bf7a7c6692d7e1e481061`. The same
revision is pinned in the workflow and `ports/uwp/vcpkg.json`. It builds FFmpeg,
mbedTLS, and zlib for `x64-uwp-static-md`. The patched SDL3-UWP 3.4.14 source is
committed under `ports/uwp/third_party/SDL3`.

## Build

Run the build script from the repository root:

```powershell
.\scripts\build\build-uwp.ps1
```

`Release` is the default. To include symbols, use:

```powershell
.\scripts\build\build-uwp.ps1 -Configuration RelWithDebInfo
```

CMake configures the matching preset and restores the vcpkg dependencies. The
generated package is placed under:

```text
ports/uwp/build/<preset>/AppPackages/SF3UWP
```

The script uses the pinned vcpkg revision declared in the manifest.
No build has been validated for this integration yet.

## Packaging and signing

This branch does not change the desktop release workflow. The local build produces
an unsigned package. Sign it with a code-signing certificate whose subject is
`CN=3SXW` before deployment when required by the console. Keep private keys and
passwords outside the repository. Install the matching x64 VCLibs framework dependency.

## Game data and storage

Do not package `SF33RD.AFS`, disc images, ROMs, or saves. Transfer a legally
obtained `SF33RD.AFS` through Xbox Device Portal to:

```text
LocalState/resources/SF33RD.AFS
```

Configuration, saves, and logs are written under `LocalState/data`. Press View
and Menu together to save screenshots under `LocalState/prints`.
On first launch, the bundled bezels are copied to `LocalState/data/img`. Existing
files in that folder are left unchanged.
Runtime code remains in the installed package.

## Xbox deployment

Install the signed MSIX and its framework dependencies through Xbox Device
Portal. Import the included public certificate if the console does not already
trust it.

## GitHub Actions: UWP branch

Push commits to `UWP` to run only `Build (Xbox UWP)`. Desktop push workflows
remain restricted to `main`; their jobs also skip PRs originating from `UWP`.
Do not create release tags for experimental builds: release tags still trigger
the existing multi-platform release workflow.

The Windows runner builds only `SF3UWP`, restores cached vcpkg dependencies,
and cancels older runs on the same branch. The first dependency build can take
substantially longer than later cached runs.

Download `3SXW-Xbox-UWP-test` from the completed Actions run. Extract the artifact
ZIP and then its inner ZIP. The package includes the signed MSIX, x64 VCLibs,
public test certificate and installation instructions. Private keys are not uploaded.
Each run uses a new temporary test certificate and version 1.0.0.0; this is for
fresh test installations, not a stable update channel. Back up LocalState before
uninstalling an earlier test package. A persistent certificate and incrementing
versions will be needed for dependable updates preserving installed app data.

If compilation fails, inspect the failed step and `Xbox-UWP-diagnostics` artifact.
No local build is required for this workflow.
