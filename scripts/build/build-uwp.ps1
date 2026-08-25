param(
    [ValidateSet("Release", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [ValidatePattern("^[0-9]+\.[0-9]+\.[0-9]+$")]
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = "Stop"

if (-not $env:VCPKG_ROOT) {
    throw "Set VCPKG_ROOT to a vcpkg checkout before building."
}

$vcpkgToolchain = Join-Path $env:VCPKG_ROOT "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path -LiteralPath $vcpkgToolchain)) {
    throw "VCPKG_ROOT does not contain scripts/buildsystems/vcpkg.cmake."
}

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")
$uwpRoot = Join-Path $repositoryRoot "ports/uwp"
$preset = if ($Configuration -eq "Release") {
    "uwp-release"
} else {
    "uwp-relwithdebinfo"
}

Push-Location $uwpRoot
try {
    & cmake --preset $preset "-DSF3_VERSION=$Version"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed."
    }

    & cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) {
        throw "UWP build failed."
    }
} finally {
    Pop-Location
}
