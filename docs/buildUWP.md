# Build Guide: Xbox UWP

This guide builds the x64 UWP package for Xbox Dev Mode. The port keeps its
host, compatibility code, manifest, and assets under `ports/uwp`. The main
source tree still owns the game loop, SDL D3D11 renderer, input, audio, and
content handling.

Rendering, controller input, WASAPI playback, LocalState content loading, and
save-file creation have been tested on Xbox.

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

The GitHub Actions UWP build workflow uses the same script and pinned vcpkg
revision. Its artifacts are unsigned.

## Release signing

The release workflow signs the MSIX with these GitHub Actions secrets:

- `XBOX_UWP_SIGNING_CERTIFICATE`: the PFX file encoded as Base64
- `XBOX_UWP_SIGNING_PASSWORD`: the PFX password

The certificate must contain its private key and use the subject `CN=3SXW`,
matching `Package.appxmanifest`. The private certificate is used only by the
Windows release job. The release ZIP contains the signed MSIX, its x64 framework
dependency, the public certificate, installation notes, and third-party notices.

## Game data and storage

Do not package `SF33RD.AFS`, disc images, ROMs, or saves. Transfer a legally
obtained `SF33RD.AFS` through Xbox Device Portal to:

```text
LocalState/resources/SF33RD.AFS
```

Configuration, saves, logs, and screenshots are written under `LocalState/data`.
Runtime code remains in the installed package.

## Xbox deployment

Install the signed MSIX and its framework dependencies through Xbox Device
Portal. Import the included public certificate if the console does not already
trust it.
