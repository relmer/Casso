<#
.SYNOPSIS
    Assembles the per-architecture release payloads -- staging/Casso-x64 and
    staging/Casso-ARM64 -- from built Release binaries.

.DESCRIPTION
    What a release ships, in one place. Both the release job and the
    packaging dry run stage from here, so there is one list of files rather
    than two that drift.

    STAGED BEFORE SIGNING, because what gets signed has to be what gets
    shipped. Signing the build output and then copying it is the same mistake
    as testing a binary you then rebuild. The signing step points at the
    staging tree this writes, and both the zips and the MSIX are built from
    it afterwards.

    THE DEMO DISKS ARE NAMED, NEVER GLOBBED. Apple2\Demos also holds
    commercial titles that are there to be tested against and may not be
    redistributed, so the list below is an allowlist and a wildcard would
    ship them. Anything added to it has to be a disk Casso's own sources
    build.

    Apple2\Demos beside the exe is not an arbitrary layout:
    AssetBootstrap::AppendBundledDemoDisks looks for exactly that directory
    starting at the executable's own, so a shipped build offers these in the
    disk picker with no code change.

    THE DIRECTORY NAMES CARRY NO VERSION, deliberately. The winget manifest
    addressed the binaries through them while the package was a portable zip
    (NestedInstallerFiles: RelativeFilePath), and a version in that path
    would have meant every release changed it. Per-architecture so that
    extracting both zips still gives two directories. The zip's own name
    keeps the version, which is what identifies a download.

    THE C RUNTIME SHIPS BESIDE THE EXE. The build links the dynamic CRT, so
    Casso.exe imports vcruntime140.dll, msvcp140.dll and their companions,
    and a Windows without the Visual C++ Redistributable fails to start it
    with STATUS_DLL_NOT_FOUND. That is exactly how winget's validation VM saw
    1.23.2 (winget-pkgs #431697), and what any user without that redist
    installed gets from the zip. Microsoft supports copying those DLLs next
    to the executable (app-local deployment), so the payload carries them.

    THE LIST COMES FROM THE BINARY, NOT FROM A HAND-WRITTEN TABLE: every CRT
    import dumpbin reports is copied from this machine's redist folder, and
    one it cannot find fails the run rather than shipping a payload that
    cannot start. The redist is the newest one in the Visual Studio install,
    so the DLLs are never older than the toolset that built the binaries.

.PARAMETER Platforms
    Which architectures to stage. Default: x64 and ARM64.

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
    [string]  $Destination = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

if ([string]::IsNullOrEmpty($BuildRoot))   { $BuildRoot   = $repoRoot }
if ([string]::IsNullOrEmpty($Destination)) { $Destination = Join-Path $repoRoot 'staging' }

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
