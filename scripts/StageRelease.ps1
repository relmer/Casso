<#
.SYNOPSIS
    Assembles the per-architecture release payloads -- staging/Casso-x64 and
    staging/Casso-ARM64 -- from built Release binaries.

.DESCRIPTION
    What a release ships, in one place: the release job and the packaging dry
    run both stage from here.

    Stage BEFORE signing, so what gets signed is what gets shipped. The zips
    and the MSIX are both built from this tree afterwards.

    THE DEMO DISKS ARE LISTED, NEVER GLOBBED. Apple2\Demos also holds
    commercial titles that may not be redistributed, so the list below is an
    allowlist. Apple2\Demos beside the exe is where
    AssetBootstrap::AppendBundledDemoDisks looks, so a shipped build offers
    them with no code change.

    The directory names carry no version, so the path inside the zip is
    stable across releases. The zip's own name carries it.

    THE C RUNTIME SHIPS BESIDE THE EXE. The build links the dynamic CRT, so a
    Windows without the Visual C++ redistributable fails to start Casso with
    STATUS_DLL_NOT_FOUND. The list comes from dumpbin rather than a hand
    written table, and a DLL it cannot find fails the run.

.PARAMETER Platforms
    Which architectures to stage. Default: x64 and ARM64.

.PARAMETER FromRelease
    Unpack a published release's zips instead of building. The zips already
    hold what this script assembles, signed, so a packaging change can be
    tested against the exact bytes that shipped without a rebuild. Only valid
    while the code is unchanged: it stages the release's binaries, not the
    working tree's.

.PARAMETER Repository
    owner/name to take -FromRelease zips from. Default: relmer/Casso.

.PARAMETER BuildRoot
    Where the built binaries are, as <BuildRoot>/<platform>/Release. Default:
    the repository root, which is where MSBuild puts them.

.PARAMETER Destination
    Where to write the payloads. Default: staging, beside the repository
    root.

.OUTPUTS
    One line per staged file. Exit code 0 on success.
#>
param(
    [string[]]$Platforms   = @('x64', 'ARM64'),
    [string]  $BuildRoot   = "",
    [string]  $Destination = "",
    [string]  $FromRelease = "",
    [string]  $Repository  = "relmer/Casso"
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

if ([string]::IsNullOrEmpty($BuildRoot))   { $BuildRoot   = $repoRoot }
if ([string]::IsNullOrEmpty($Destination)) { $Destination = Join-Path $repoRoot 'staging' }

if (-not [string]::IsNullOrEmpty($FromRelease))
{
    $version = $FromRelease -replace '^v', ''

    if (Test-Path $Destination) { Remove-Item $Destination -Recurse -Force }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null

    foreach ($plat in $Platforms)
    {
        $zip = Join-Path $Destination "Casso-$version-$plat.zip"

        gh release download $FromRelease --repo $Repository `
            --pattern "Casso-$version-$plat.zip" --dir $Destination
        if ($LASTEXITCODE -ne 0) { throw "Could not download Casso-$version-$plat.zip from $FromRelease." }

        # The zip already carries a Casso-<plat> folder at its root.
        Expand-Archive -Path $zip -DestinationPath $Destination -Force
        Remove-Item $zip

        $staged = Join-Path $Destination "Casso-$plat"
        if (-not (Test-Path (Join-Path $staged 'Casso.exe')))
        {
            throw "Casso-$version-$plat.zip did not unpack to Casso-$plat\Casso.exe."
        }

        $sig = Get-AuthenticodeSignature (Join-Path $staged 'Casso.exe')
        Write-Host "  $plat from ${FromRelease}: Casso.exe $($sig.Status)"
    }

    Get-ChildItem $Destination -Recurse -File |
        ForEach-Object { Write-Host "  staged: $($_.FullName)" }

    return
}

$demos = @(
    'casso-rocks.dsk',
    'mockingboard-speech-demo-hgr.dsk',
    'mockingboard-speech-demo-dhgr.dsk',
    'mockingboard-test.dsk'
)

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsRoot  = & $vswhere -latest -products * -property installationPath
$dumpbin = & $vswhere -latest -products * -find '**\Hostx64\x64\dumpbin.exe' | Select-Object -First 1
if (-not $vsRoot -or -not $dumpbin) { throw 'Cannot locate Visual Studio or dumpbin.exe on this machine.' }

$redist = Get-ChildItem (Join-Path $vsRoot 'VC\Redist\MSVC') -Directory |
    Where-Object { $_.Name -match '^\d+(\.\d+)+$' } |
    Sort-Object { [version] $_.Name } -Descending |
    Select-Object -First 1
if (-not $redist) { throw "No versioned redist folder under $vsRoot\VC\Redist\MSVC." }

foreach ($plat in $Platforms)
{
    $binaries   = Join-Path $BuildRoot "$plat/Release"
    $stagingDir = Join-Path $Destination "Casso-$plat"
    $demoDir    = Join-Path $stagingDir 'Apple2/Demos'

    New-Item -ItemType Directory -Path $demoDir -Force | Out-Null

    Copy-Item (Join-Path $binaries 'Casso.exe')    $stagingDir
    Copy-Item (Join-Path $binaries 'CassoCli.exe') $stagingDir
    Copy-Item (Join-Path $repoRoot 'README.md')    $stagingDir
    Copy-Item (Join-Path $repoRoot 'CHANGELOG.md') $stagingDir
    Copy-Item (Join-Path $repoRoot 'LICENSE')      $stagingDir

    $crtDir = Get-ChildItem (Join-Path $redist.FullName $plat) -Directory -Filter 'Microsoft.VC*.CRT' |
        Select-Object -First 1
    if (-not $crtDir) { throw "No Microsoft.VC*.CRT folder for $plat under $($redist.FullName)." }

    $crtDlls = foreach ($exe in @('Casso.exe', 'CassoCli.exe'))
    {
        & $dumpbin /DEPENDENTS (Join-Path $binaries $exe) |
            Where-Object { $_ -match '^\s+((?:vcruntime|msvcp|concrt|vccorlib)\S*\.dll)\s*$' } |
            ForEach-Object { $Matches[1].ToLowerInvariant() }
    }

    foreach ($dll in ($crtDlls | Sort-Object -Unique))
    {
        $src = Join-Path $crtDir.FullName $dll
        if (-not (Test-Path $src)) { throw "$plat/$dll is imported but $($crtDir.FullName) has no copy of it." }
        Copy-Item $src $stagingDir
        Write-Host "  runtime: $plat/$dll from $($crtDir.Name)"
    }

    foreach ($disk in $demos)
    {
        $src = Join-Path $repoRoot (Join-Path 'Apple2/Demos' $disk)
        if (-not (Test-Path $src)) { throw "Demo disk missing from the tree: $src" }
        Copy-Item $src $demoDir
    }
}

Get-ChildItem $Destination -Recurse -File |
    ForEach-Object { Write-Host "  staged: $($_.FullName)" }
