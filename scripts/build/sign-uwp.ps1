[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$CertificatePath,
    [Parameter(Mandatory)]
    [string]$CertificatePassword,
    [Parameter(Mandatory)]
    [ValidatePattern("^[A-Za-z0-9._-]+$")]
    [string]$ArchiveName,
    [string]$PackageRoot,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))
if (-not $PackageRoot) {
    $PackageRoot = Join-Path $repositoryRoot "ports/uwp/build/uwp-release/AppPackages/SF3UWP"
}
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $repositoryRoot "dist/xbox-uwp"
}
if (-not (Test-Path -LiteralPath $CertificatePath -PathType Leaf)) {
    throw "Signing certificate was not found: $CertificatePath"
}

[xml]$sourceManifest = Get-Content -Raw (Join-Path $repositoryRoot "ports/uwp/Package.appxmanifest")
$version = $sourceManifest.Package.Identity.Version
$packageDirectory = Join-Path $PackageRoot "SF3UWP_${version}_x64_Test"
$package = Join-Path $packageDirectory "SF3UWP_${version}_x64.msix"
if (-not (Test-Path -LiteralPath $package -PathType Leaf)) {
    throw "Expected MSIX was not found: $package"
}

$flags = [Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet
$certificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new(
    $CertificatePath,
    $CertificatePassword,
    $flags
)
if (-not $certificate.HasPrivateKey) {
    throw "The signing certificate does not contain a private key."
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$msix = [System.IO.Compression.ZipFile]::OpenRead($package)
try {
    $entry = $msix.GetEntry("AppxManifest.xml")
    if (-not $entry) {
        throw "AppxManifest.xml is missing from the MSIX."
    }
    $reader = [System.IO.StreamReader]::new($entry.Open())
    try {
        [xml]$packageManifest = $reader.ReadToEnd()
    } finally {
        $reader.Dispose()
    }
} finally {
    $msix.Dispose()
}

$publisher = $packageManifest.Package.Identity.Publisher
if ($publisher -ne $certificate.Subject) {
    throw "Manifest publisher '$publisher' does not match certificate subject '$($certificate.Subject)'."
}

$kitsRoot = ${env:ProgramFiles(x86)}
if (-not $kitsRoot) {
    throw "ProgramFiles(x86) is not defined."
}
$signTool = Get-ChildItem -LiteralPath (Join-Path $kitsRoot "Windows Kits/10/bin") `
    -Filter signtool.exe -File -Recurse |
    Where-Object { $_.FullName -match "\\x64\\signtool\.exe$" } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $signTool) {
    throw "SignTool was not found in the Windows SDK."
}

& $signTool.FullName sign /fd SHA256 /f $CertificatePath /p $CertificatePassword $package
if ($LASTEXITCODE -ne 0) {
    throw "SignTool failed with exit code $LASTEXITCODE."
}
& $signTool.FullName verify /pa $package
if ($LASTEXITCODE -ne 0) {
    throw "Signature verification failed with exit code $LASTEXITCODE."
}

New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$stage = Join-Path $OutputRoot ".stage-$([guid]::NewGuid())"
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    Copy-Item -LiteralPath $package -Destination (Join-Path $stage "$ArchiveName.msix")

    $dependency = Join-Path $packageDirectory "Dependencies/x64/Microsoft.VCLibs.x64.14.00.appx"
    if (-not (Test-Path -LiteralPath $dependency -PathType Leaf)) {
        throw "Expected x64 framework dependency was not found: $dependency"
    }
    $dependencyStage = Join-Path $stage "Dependencies/x64"
    New-Item -ItemType Directory -Path $dependencyStage -Force | Out-Null
    Copy-Item -LiteralPath $dependency -Destination $dependencyStage

    [IO.File]::WriteAllBytes(
        (Join-Path $stage "3SXW-UWP.cer"),
        $certificate.Export([Security.Cryptography.X509Certificates.X509ContentType]::Cert)
    )
    Copy-Item -LiteralPath (Join-Path $repositoryRoot "docs/THIRD_PARTY_NOTICES.txt") -Destination $stage

    $install = @"
3SXW for Xbox Dev Mode

You will need an Xbox running in Developer Mode and your own legally obtained SF33RD.AFS file. Original game content is not included with 3SXW.

INSTALL THE APP

1. Open Xbox Device Portal and go to the Apps page.
2. Add ${ArchiveName}.msix as the application package.
3. Add Dependencies\x64\Microsoft.VCLibs.x64.14.00.appx as a dependency, then install.

ADD THE GAME DATA

1. Open File Explorer in Xbox Device Portal and select 3SXW's LocalState.
2. Create a folder named resources inside LocalState.
3. Upload your legally obtained SF33RD.AFS file into that folder.

The completed path must be:

LocalState\resources\SF33RD.AFS

CHANGE THE CONFIGURATION

3SXW creates its configuration file here:

LocalState\data\config

Use Xbox Device Portal's File Explorer to download the config file, edit it in Notepad, then upload it to the same location and replace the old file.

The available settings and values are documented here:

https://github.com/guimaraf/3sxw/blob/main/docs/config.md

SAVES AND UPDATES

Saves and other writable game files are stored under LocalState\data.
"@
    [IO.File]::WriteAllText(
        (Join-Path $stage "INSTALL.txt"),
        $install,
        [Text.UTF8Encoding]::new($false)
    )

    $archive = Join-Path $OutputRoot "$ArchiveName.zip"
    Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $archive -CompressionLevel Optimal -Force
} finally {
    Remove-Item -LiteralPath $stage -Recurse -Force
}

Write-Host "Staged $archive"
