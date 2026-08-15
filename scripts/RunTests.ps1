<#
.SYNOPSIS
    Runs the Casso unit tests using vstest.console.

.DESCRIPTION
    This script does NOT build by default -- every VS Code task that invokes it
    is labeled "(no build)" and the composite "Build + Test" tasks chain a
    build task in front of it. That contract lives in the task label, though,
    so a bare call from a terminal used to run happily against whatever
    UnitTest.dll happened to be lying around and report a confident green.

    Two guards close that hole: -Build runs the build first, and a staleness
    check refuses to run at all when the test assembly is older than the newest
    source file that compiles into it.

.PARAMETER Configuration
    The build configuration whose test DLL to run.
    Default: Debug

.PARAMETER Platform
    The target platform. Valid values are 'x64' or 'Auto'.
    'Auto' defaults to x64.
    Default: Auto

.PARAMETER Build
    Build before running, so the test assembly cannot be stale. An up-to-date
    incremental build costs about a second, so this is cheap to pass always.

.PARAMETER AllowStale
    Run even when the test assembly is older than the newest source file.
    For deliberately re-running a previous build's results.

.EXAMPLE
    ./scripts/RunTests.ps1 -Build
    Builds, then runs the full suite.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64', 'Auto')]
    [string]$Platform = 'Auto',

    [switch]$Build,

    [switch]$AllowStale
)

if ($Platform -eq 'Auto') {
    if ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq [System.Runtime.InteropServices.Architecture]::Arm64) {
        $Platform = 'ARM64'
    } else {
        $Platform = 'x64'
    }
}

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

#
#  The newest write time among tracked sources that compile into UnitTest.dll.
#
#  Enumerated via `git ls-files` rather than the filesystem so build output can
#  never be mistaken for a source, and narrowed to the projects the test DLL
#  actually links -- CassoCli builds a separate executable, so editing it does
#  not make the test assembly stale and must not trip the guard.
#
#  Returns $null when the answer cannot be determined (no git, no matches), and
#  the caller treats that as "cannot judge" rather than as "stale".
#
function Get-NewestSourceTimestamp {
    param([string]$RepoRoot)

    $projects   = @('CassoCore/', 'CassoEmuCore/', 'Dxui/', 'Casso/', 'UnitTest/')
    $extensions = @('.cpp', '.c', '.h', '.hpp', '.inl', '.vcxproj')

    Push-Location $RepoRoot
    try {
        $tracked = & git ls-files 2>$null
        if ($LASTEXITCODE -ne 0 -or -not $tracked) { return $null }
    } catch {
        return $null
    } finally {
        Pop-Location
    }

    $relevant = $tracked | Where-Object {
        $path = $_
        ($projects   | Where-Object { $path.StartsWith($_, 'OrdinalIgnoreCase') }) -and
        ($extensions | Where-Object { $path.EndsWith($_,   'OrdinalIgnoreCase') })
    }

    if (-not $relevant) { return $null }

    $full = $relevant | ForEach-Object { Join-Path $RepoRoot $_ }
    $stat = Get-Item -LiteralPath $full -ErrorAction SilentlyContinue |
            Measure-Object -Property LastWriteTime -Maximum

    return $stat.Maximum
}

if ($Build) {
    $buildScript = Join-Path $PSScriptRoot 'Build.ps1'
    if (-not (Test-Path $buildScript)) {
        throw "Build script not found: $buildScript"
    }

    & $buildScript -Configuration $Configuration -Platform $Platform
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$toolsScript = Join-Path $PSScriptRoot 'VSTools.ps1'
if (-not (Test-Path $toolsScript)) {
    throw "Tool helper script not found: $toolsScript"
}

. $toolsScript

$vstestPath = Get-VS2026VSTestPath
if (-not $vstestPath) {
    $vstestPath = Get-VS2026VSTestPath -IncludePrerelease
}

if (-not $vstestPath) {
    throw 'vstest.console.exe not found in VS 2026 (v18.x). Install VS 2026 with Test workload.'
}

$testAssembly = Join-Path -Path $repoRoot -ChildPath "$Platform\$Configuration\UnitTest.dll"
if (-not (Test-Path -Path $testAssembly)) {
    throw "Test assembly not found at $testAssembly. Build the tests before running them, or pass -Build."
}

# A stale assembly reports a full, confident pass against code that is not the
# code on disk -- and a brand-new test file that never compiled in simply is
# not in the count, with nothing to say so. Refuse rather than mislead.
if (-not $AllowStale) {
    $assemblyStamp = (Get-Item -LiteralPath $testAssembly).LastWriteTime
    $newestSource  = Get-NewestSourceTimestamp -RepoRoot $repoRoot

    if ($null -ne $newestSource -and $newestSource -gt $assemblyStamp) {
        Write-Host ''
        Write-Host 'Test assembly is STALE.' -ForegroundColor Red
        Write-Host "  $testAssembly" -ForegroundColor DarkGray
        Write-Host "  built  $assemblyStamp" -ForegroundColor DarkGray
        Write-Host "  source $newestSource" -ForegroundColor DarkGray
        Write-Host ''
        throw "Source has changed since the test assembly was built. Re-run with -Build (or -AllowStale to run the previous build anyway)."
    }
}

Write-Host "Running tests from $testAssembly" -ForegroundColor Cyan
Write-Host "vstest.console path: $vstestPath" -ForegroundColor DarkGray

& $vstestPath $testAssembly
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
