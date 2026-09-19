<#
.SYNOPSIS
    Packs the staged release payloads into a single signed-ready
    Casso-<version>.msixbundle.

.DESCRIPTION
    An MSIX installs the executables and the DLLs they need as one unit, in
    one folder, and launches them from it. A portable install launches
    through symlinks, where the loader searches beside the link and never
    finds the C runtime.

    Packs the same staging folders the zip ships, so RUN IT AFTER SIGNING:
    signing the bundle does not reach the executables inside it.

    The C runtime stays in the package rather than becoming a Microsoft.VCLibs
    dependency, because winget's validator installs with --skip-dependencies.

.PARAMETER PayloadRoot
    Folder holding Casso-x64 and Casso-ARM64, as scripts/StageRelease.ps1
    builds them. Default: staging.

.PARAMETER OutputDirectory
    Where the .msixbundle is written. Default: artifacts.

.PARAMETER Version
    Three-part version to stamp. Default: read from CassoCore/Version.h.

.PARAMETER Platforms
    Which architectures to pack. Default: whichever of x64 and ARM64 have a
    staging folder. A release passes both, so a missing one fails rather than
    shipping half a bundle.

.PARAMETER IntermediateDirectory
    Where the layouts and per-architecture .msix files are built. Default: a
    fresh folder under the system temp directory.

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

# Repository spelling -> the lowercase the appx schema requires.
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

# MSIX versions are four parts; the fourth is always zero here.
$packageVersion = "$Version.0"

#-----------------------------------------------------------------------------
# Tools
#-----------------------------------------------------------------------------

# Newest Windows SDK wins; pinning a version breaks on the next SDK update.
function Find-SdkTool
{
    param([string] $name)

    $kitRoots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    ) | Where-Object { Test-Path $_ }

    $hostArch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' } else { 'x64' }

    # Sorted across both roots: either can hold the newer SDK.
    $tool = @(foreach ($root in $kitRoots)
    {
        Get-ChildItem $root -Directory |
            Where-Object { $_.Name -match '^10\.\d+\.\d+\.\d+$' } |
            ForEach-Object { [pscustomobject] @{
                Version = [version] $_.Name
                Path    = Join-Path $_.FullName "$hostArch\$name"
            } }
    }) | Where-Object { Test-Path $_.Path } |
         Sort-Object Version -Descending |
         Select-Object -First 1 -ExpandProperty Path
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

# A missing logo packs without complaint and fails at install. The scale-100
# stands in for its family; they are generated together.
$namedLogos = @(
    'StoreLogo', 'Square44x44Logo', 'Square71x71Logo',
    'Square150x150Logo', 'Square310x310Logo', 'Wide310x150Logo'
)

foreach ($logo in $namedLogos)
{
    if (-not (Test-Path (Join-Path $assetSource "$logo.scale-100.png")))
    {
        throw "The manifest refers to $logo but $assetSource has no $logo.scale-100.png. " +
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

# From scratch: makeappx packs whatever it finds, including leftovers.
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

    # UTF-8, no BOM.
    $manifest = $template.Replace('{VERSION}', $packageVersion).Replace('{ARCHITECTURE}', $architecture)
    [IO.File]::WriteAllText((Join-Path $layout 'AppxManifest.xml'), $manifest, (New-Object Text.UTF8Encoding $false))

    # Without resources.pri the scale-qualified logos are invisible and the
    # install fails. The config stays outside the layout, which gets packed.
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

# makeappx bundle takes a directory, not a list of files.
$bundleInput = Join-Path $IntermediateDirectory 'bundle'
New-Item -ItemType Directory -Path $bundleInput -Force | Out-Null
foreach ($package in $packagePaths) { Move-Item $package $bundleInput }

$bundle = Join-Path $OutputDirectory "Casso-$Version.msixbundle"

& $makeappx bundle /bv $packageVersion /d $bundleInput /p $bundle /o | Out-Null
if ($LASTEXITCODE -ne 0) { throw "makeappx bundle failed ($LASTEXITCODE)." }

$size = [Math]::Round((Get-Item $bundle).Length / 1MB, 1)

Write-Host ""
Write-Host "Bundle: $bundle ($size MB)"
Write-Host ""
Write-Host $bundle
