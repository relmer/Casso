<#
.SYNOPSIS
    Packs the staged release payloads into a single signed-ready
    Casso-<version>.msixbundle.

.DESCRIPTION
    WHY THERE IS AN MSIX AT ALL. winget's validation refused the portable zip
    and would refuse any zip built the same way. A portable package is
    installed by dropping symlinks into a Links folder on PATH, and the
    validator then launches the binaries through those links. Windows resolves
    a DLL import against the directory of the LINK, not of its target, so the
    loader looked in Links, found no vcruntime140.dll there, fell through to
    System32 and came up empty on a machine without the Visual C++
    redistributable. Both executables exited with STATUS_DLL_NOT_FOUND on both
    architectures. ArchiveBinariesDependOnPath does not change where the
    loader looks and did not help.

    An MSIX has no links. The executables are launched from the folder they
    live in, beside the runtime DLLs the package carries, and the terminal
    commands come from app execution aliases, which are entries in the
    app-paths list rather than files on PATH.

    THE PAYLOAD IS THE ZIP'S PAYLOAD, taken from the same staging folders the
    release job already built and -- in a release -- already signed. That is
    deliberate: the two downloads are then the same bytes in two wrappers,
    and there is no second list of files to keep in step with the first. It
    also means this script must run AFTER signing, never before; packing
    unsigned executables and signing the bundle afterwards would leave the
    executables inside it unsigned, because signing a container does not
    reach into it.

    THE C RUNTIME STAYS IN THE PACKAGE. The tidy alternative is a framework
    dependency on Microsoft.VCLibs, which Windows would resolve at install
    time. winget's validator installs with --skip-dependencies, so that
    resolution never happens there and the package fails validation the same
    way the zip did, one layer further in.

    The per-architecture packages are built and then bundled. A bundle is one
    download that installs the right architecture on each machine, which is
    what winget's single-installer-per-package model wants; shipping two
    .msix files would mean two Installer entries and two hashes.

.PARAMETER PayloadRoot
    Folder holding the per-architecture staging directories -- Casso-x64 and
    Casso-ARM64 -- as the release job's "Stage release payloads" step builds
    them. Default: staging, beside the repository root.

.PARAMETER OutputDirectory
    Where the .msixbundle is written. Default: artifacts.

.PARAMETER Version
    Three-part version to stamp. Default: read from CassoCore/Version.h, the
    same source the release tag comes from.

.PARAMETER Platforms
    Which architectures to pack. Default: whichever of x64 and ARM64 have a
    staging folder, so a local run with one build works without arguments.
    A release names both explicitly, so a missing one fails rather than
    quietly shipping half a bundle.

.PARAMETER IntermediateDirectory
    Where the per-architecture layouts and .msix files are built. Default: a
    fresh folder under the system temp directory, removed on the way in.

.OUTPUTS
    The path of the bundle, last line. Exit code 0 on success.
#>
param(
    [string]  $PayloadRoot           = "",
    [string]  $OutputDirectory       = "",
    [string]  $Version               = "",
    [string[]]$Platforms             = @(),
    [string]  $IntermediateDirectory = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

if ([string]::IsNullOrEmpty($PayloadRoot))     { $PayloadRoot     = Join-Path $repoRoot 'staging' }
if ([string]::IsNullOrEmpty($OutputDirectory)) { $OutputDirectory = Join-Path $repoRoot 'artifacts' }

# The manifest wants x64 and arm64 in the lowercase spellings the schema
# defines; the staging folders and the zip names use the repository's own
# ARM64. One table rather than two conversions scattered about.
$architectures = [ordered] @{
    'x64'   = 'x64'
    'ARM64' = 'arm64'
}

#-----------------------------------------------------------------------------
# Version
#-----------------------------------------------------------------------------

if ([string]::IsNullOrEmpty($Version))
{
    $versionHeader = Join-Path $repoRoot 'CassoCore/Version.h'
    $content       = Get-Content $versionHeader -Raw

    if ($content -notmatch '#define VERSION_MAJOR (\d+)') { throw "VERSION_MAJOR missing from $versionHeader" }
    $major = $Matches[1]
    if ($content -notmatch '#define VERSION_MINOR (\d+)') { throw "VERSION_MINOR missing from $versionHeader" }
    $minor = $Matches[1]
    if ($content -notmatch '#define VERSION_PATCH (\d+)') { throw "VERSION_PATCH missing from $versionHeader" }
    $patch = $Matches[1]

    $Version = "$major.$minor.$patch"
}

if ($Version -notmatch '^\d+\.\d+\.\d+$')
{
    throw "Version must be three parts, e.g. 1.25.0. Got: $Version"
}

# MSIX has no three-part version. The fourth field is the Store's to
# increment on a repackage, so it is always zero here.
$packageVersion = "$Version.0"

#-----------------------------------------------------------------------------
# Tools
#-----------------------------------------------------------------------------

# makeappx and makepri ship in the Windows SDK, which installs one folder per
# SDK version. Take the newest: an older one cannot pack anything the newest
# cannot, and pinning a version would break the first time the SDK is updated.
function Find-SdkTool
{
    param([string] $name)

    $kitRoots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    ) | Where-Object { Test-Path $_ }

    $hostArch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' } else { 'x64' }

    $candidates = foreach ($root in $kitRoots)
    {
        Get-ChildItem $root -Directory |
            Where-Object { $_.Name -match '^10\.\d+\.\d+\.\d+$' } |
            Sort-Object { [version] $_.Name } -Descending |
            ForEach-Object { Join-Path $_.FullName "$hostArch\$name" } |
            Where-Object { Test-Path $_ }
    }

    $tool = $candidates | Select-Object -First 1
    if (-not $tool)
    {
        throw "$name not found. Install the Windows 10 SDK (it ships in the " +
              'Visual Studio installer under "Windows 10/11 SDK").'
    }

    return $tool
}

$makeappx = Find-SdkTool 'makeappx.exe'
$makepri  = Find-SdkTool 'makepri.exe'

Write-Host "makeappx: $makeappx"
Write-Host "makepri:  $makepri"
Write-Host "version:  $packageVersion"
Write-Host ""

#-----------------------------------------------------------------------------
# What to pack
#-----------------------------------------------------------------------------

if (-not (Test-Path $PayloadRoot))
{
    throw "No staging root at $PayloadRoot. Build and stage the release payloads first."
}

if ($Platforms.Count -eq 0)
{
    $Platforms = @($architectures.Keys | Where-Object { Test-Path (Join-Path $PayloadRoot "Casso-$_") })

    if ($Platforms.Count -eq 0)
    {
        throw "No Casso-<platform> staging folders under $PayloadRoot."
    }

    Write-Host "Packing what is staged: $($Platforms -join ', ')"
}

foreach ($platform in $Platforms)
{
    if (-not $architectures.Contains($platform))
    {
        throw "Unknown platform '$platform'. Expected one of: $($architectures.Keys -join ', ')"
    }
}

$manifestTemplate = Join-Path $repoRoot 'Installer/Package.appxmanifest'
$assetSource      = Join-Path $repoRoot 'Installer/Assets'

foreach ($required in @($manifestTemplate, $assetSource))
{
    if (-not (Test-Path $required)) { throw "Missing: $required" }
}

# A logo the manifest names but the asset folder does not hold packs without
# complaint and fails at install, so check the five up front. The scale-100
# of each stands in for its whole family: they are generated together, by
# scripts/GenerateMsixAssets.ps1, and never one at a time.
$namedLogos = @(
    'StoreLogo', 'Square44x44Logo', 'Square71x71Logo',
    'Square150x150Logo', 'Square310x310Logo', 'Wide310x150Logo'
)

foreach ($logo in $namedLogos)
{
    if (-not (Test-Path (Join-Path $assetSource "$logo.scale-100.png")))
    {
        throw "The manifest names $logo but $assetSource has no $logo.scale-100.png. " +
              'Run scripts/GenerateMsixAssets.ps1.'
    }
}

#-----------------------------------------------------------------------------
# Build
#-----------------------------------------------------------------------------

if ([string]::IsNullOrEmpty($IntermediateDirectory))
{
    $IntermediateDirectory = Join-Path ([IO.Path]::GetTempPath()) 'CassoMsix'
}

# From scratch every time. makeappx packs the folder as it finds it, so a
# file left over from a previous version's layout would ship in this one.
if (Test-Path $IntermediateDirectory) { Remove-Item $IntermediateDirectory -Recurse -Force }
New-Item -ItemType Directory -Path $IntermediateDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $OutputDirectory        -Force | Out-Null

$template     = Get-Content $manifestTemplate -Raw
$packagePaths = @()

foreach ($platform in $Platforms)
{
    $architecture = $architectures[$platform]
    $payload      = Join-Path $PayloadRoot "Casso-$platform"

    if (-not (Test-Path $payload)) { throw "No staged payload at $payload." }

    Write-Host "--- $platform ---"

    $layout = Join-Path $IntermediateDirectory $platform
    New-Item -ItemType Directory -Path $layout -Force | Out-Null

    Copy-Item "$payload\*" $layout -Recurse -Force
    Copy-Item $assetSource (Join-Path $layout 'Assets') -Recurse -Force

    # UTF-8 without a BOM: the appx manifest parser reads the encoding from
    # the XML declaration and a BOM ahead of it is one more thing that has to
    # agree with it.
    $manifest = $template.Replace('{VERSION}', $packageVersion).Replace('{ARCHITECTURE}', $architecture)
    [IO.File]::WriteAllText((Join-Path $layout 'AppxManifest.xml'), $manifest, (New-Object Text.UTF8Encoding $false))

    # The resource index. Without it the scale-qualified logo files are
    # invisible to Windows, which looks for the unqualified names the
    # manifest gives and fails the install rather than falling back.
    #
    # The config goes OUTSIDE the layout. Anything in the layout is packed,
    # and a build configuration is not part of what ships.
    $priConfig = Join-Path $IntermediateDirectory "priconfig-$platform.xml"

    & $makepri createconfig /ConfigXml $priConfig /Default en-US /Overwrite | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "makepri createconfig failed ($LASTEXITCODE)." }

    & $makepri new /ProjectRoot $layout /ConfigXml $priConfig `
                   /OutputFile (Join-Path $layout 'resources.pri') `
                   /Manifest (Join-Path $layout 'AppxManifest.xml') /Overwrite | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "makepri new failed ($LASTEXITCODE) for $platform." }

    $package = Join-Path $IntermediateDirectory "Casso-$Version-$platform.msix"

    & $makeappx pack /d $layout /p $package /o | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "makeappx pack failed ($LASTEXITCODE) for $platform." }

    $size = [Math]::Round((Get-Item $package).Length / 1MB, 1)
    Write-Host "  packed: $(Split-Path $package -Leaf) ($size MB)"

    $packagePaths += $package
}

#-----------------------------------------------------------------------------
# Bundle
#-----------------------------------------------------------------------------

# makeappx bundle takes a DIRECTORY of packages, not a list of files, so the
# .msix files are gathered where nothing else can be swept in with them.
$bundleInput = Join-Path $IntermediateDirectory 'bundle'
New-Item -ItemType Directory -Path $bundleInput -Force | Out-Null
foreach ($package in $packagePaths) { Move-Item $package $bundleInput }

$bundle = Join-Path $OutputDirectory "Casso-$Version.msixbundle"

# The bundle carries its own version, distinct from the packages inside it.
# Same number: they are released together and a bundle that said anything
# else would only ever confuse a reader.
& $makeappx bundle /bv $packageVersion /d $bundleInput /p $bundle /o | Out-Null
if ($LASTEXITCODE -ne 0) { throw "makeappx bundle failed ($LASTEXITCODE)." }

$size = [Math]::Round((Get-Item $bundle).Length / 1MB, 1)

Write-Host ""
Write-Host "Bundle: $bundle ($size MB)"
Write-Host ""
Write-Host $bundle
