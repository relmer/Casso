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

.PARAMETER Filter
    Run only matching tests. A bare word is treated as a substring of the fully
    qualified name, so -Filter Merlin does what you expect; anything containing
    a filter operator is passed to vstest verbatim, so the full
    /TestCaseFilter: grammar remains available.

    The Debug suite takes roughly 15 minutes, which is long enough that people
    route around it. This exists so the edit-test loop does not have to.

.PARAMETER Scenario
    Run ScenarioTests.dll instead of UnitTest.dll. This is the ONE deliberate
    way to run the scenario suite: those cases need external inputs (the stock
    DOS 3.3 System Master, fetched rather than committed) and boot real guests,
    so they live in a separate binary that CI never names and this script never
    runs by default. A scenario run is not the unit-test suite, and the banner
    says so.

.EXAMPLE
    ./scripts/RunTests.ps1 -Build
    Builds, then runs the full suite.

.EXAMPLE
    ./scripts/RunTests.ps1 -Build -Filter CommandLine
    Builds, then runs only tests whose qualified name contains "CommandLine".

.EXAMPLE
    ./scripts/RunTests.ps1 -Filter "FullyQualifiedName~Merlin&Name!~Slow"
    Passes a full vstest filter expression through unchanged.

.EXAMPLE
    ./scripts/RunTests.ps1 -Build -Scenario
    Builds, then runs the scenario suite -- the system tests that need the
    DOS 3.3 System Master and a booted guest.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64', 'Auto')]
    [string]$Platform = 'Auto',

    [switch]$Build,

    [switch]$AllowStale,

    [string]$Filter = '',

    [switch]$Scenario,

    [switch]$NormalPriority,

    [switch]$LowPriority
)

#
#  OFF THE FOREGROUND'S BACK. This saturates every core and the disk with
#  it, and nothing about it is latency-sensitive -- nobody watches a build.
#  Lowered here rather than around the tool because Windows hands a child
#  its parent's priority class, so this reaches MSBuild, every cl.exe it
#  fans out, and vstest and its hosts. See scripts/HostLoad.ps1.
#
. (Join-Path $PSScriptRoot 'HostLoad.ps1')

$priorityWas = $null

if (-not $NormalPriority) {
    $priorityWas = Set-CassoHostLoad -Priority ($LowPriority ? 'Idle' : 'BelowNormal')
}

#  Put it back on the way out however this ends -- these are run from an
#  interactive shell as often as from a fresh one, and a session left at
#  BelowNormal for the rest of the day is a slow shell nobody can explain.
trap { Restore-CassoHostLoad -Priority $priorityWas; break }

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
#  The newest write time among tracked sources that compile into the selected
#  test DLL.
#
#  Enumerated via `git ls-files` rather than the filesystem so build output can
#  never be mistaken for a source, and narrowed to the projects the test DLL
#  actually links -- CassoCli builds a separate executable, so editing it does
#  not make the test assembly stale and must not trip the guard. The scenario
#  DLL compiles UnitTest's shared guest helpers, so UnitTest/ stays on its
#  list; it never links Dxui or the Casso exe's objects, so those stay off.
#
#  Returns $null when the answer cannot be determined (no git, no matches), and
#  the caller treats that as "cannot judge" rather than as "stale".
#
function Get-NewestSourceTimestamp {
    param([string]$RepoRoot, [string[]]$Projects)

    $projects   = $Projects
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

$assemblyName    = 'UnitTest.dll'
$sourceProjects  = @('CassoCore/', 'CassoEmuCore/', 'Dxui/', 'Casso/', 'UnitTest/')

if ($Scenario) {
    $assemblyName   = 'ScenarioTests.dll'
    $sourceProjects = @('CassoCore/', 'CassoEmuCore/', 'UnitTest/', 'ScenarioTests/')
}

$testAssembly = Join-Path -Path $repoRoot -ChildPath "$Platform\$Configuration\$assemblyName"
if (-not (Test-Path -Path $testAssembly)) {
    throw "Test assembly not found at $testAssembly. Build the tests before running them, or pass -Build."
}

# A stale assembly reports a full, confident pass against code that is not the
# code on disk -- and a brand-new test file that never compiled in simply is
# not in the count, with nothing to say so. Refuse rather than mislead.
if (-not $AllowStale) {
    $assemblyStamp = (Get-Item -LiteralPath $testAssembly).LastWriteTime
    $newestSource  = Get-NewestSourceTimestamp -RepoRoot $repoRoot -Projects $sourceProjects

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

if ($Scenario) {
    # A scenario run is not the unit-test suite, and a green one says nothing
    # about it: these cases need the DOS 3.3 System Master on this machine and
    # boot real guests, which is exactly why they live in their own DLL.
    Write-Host ''
    Write-Host 'SCENARIO SUITE -- system tests, NOT the unit-test suite.' -ForegroundColor Yellow
    Write-Host '  Needs external inputs (the stock DOS 3.3 System Master and ProDOS Users Disk)' -ForegroundColor DarkGray
    Write-Host '  and boots real guests.' -ForegroundColor DarkGray
    Write-Host ''

    # Fetch whatever this machine is missing, so invoking the suite is enough
    # to run it. A developer should not have to discover an undocumented
    # manual step, and a rule requiring these cases before a merge is only
    # worth writing if obeying it is cheap.
    #
    # NOT fatal, deliberately. Offline with the images already here is a
    # working run, and when one really is absent the case that needs it fails
    # with a message listing both locations and how to fill them -- better
    # than anything this script could report from out here.
    $fetchScript = Join-Path $PSScriptRoot 'FetchStockDisks.ps1'

    & $fetchScript
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Continuing: the cases needing a missing image will report it themselves.' -ForegroundColor DarkGray
        Write-Host ''
    }
}

$vstestArgs = @($testAssembly)

if ($Filter) {
    # A bare word is the common case and the awkward one to type, so promote it
    # to a name-substring match; anything already containing filter grammar is
    # the caller's own expression and passes through untouched.
    $expression = $Filter
    if ($Filter -notmatch '[~=!&|()]') {
        $expression = "FullyQualifiedName~$Filter"
    }

    $vstestArgs += "/TestCaseFilter:$expression"

    # A filtered run is not a suite run. Say so, loudly and every time -- the
    # failure mode this guards against is reading a green partial result as
    # though the suite had passed.
    Write-Host ''
    Write-Host "FILTERED RUN -- this is NOT the full suite." -ForegroundColor Yellow
    Write-Host "  filter: $expression" -ForegroundColor DarkGray
    Write-Host ''
}

# A TEST RUN MUST LEAVE THE WORKING TREE EXACTLY AS IT FOUND IT.
#
# Measured before and after, because one test did not. BootDiskTests wrote the
# image it had just built over Apple2/Demos/casso-rocks.dsk on every run,
# called out in the code as a deliberate side effect. It normally wrote
# byte-identical content -- until the day it wrote a different image, with a
# whole track zeroed, and a corrupted binary asset landed in the tree from a
# test run.
#
# MTIMES, NOT `git status`. Measured: rewriting a tracked file with its own
# bytes moves the mtime and `git status --porcelain` still reports nothing,
# because git re-hashes and finds the content identical. That is exactly the
# case that hid this write for as long as it hid: a test that scribbles the
# same bytes every run looks clean right up until the run where it does not.
# The rule is that a unit test never touches system state, so what is compared
# is whether any tracked file was TOUCHED.
$stateBefore = $null
$insideGit   = $false

function Get-TrackedFileState {
    $state = @{}

    foreach ($relative in (& git ls-files)) {
        if (-not $relative) { continue }

        $item = Get-Item -LiteralPath $relative -ErrorAction SilentlyContinue

        if ($item -and -not $item.PSIsContainer) {
            $state[$relative] = "$($item.LastWriteTimeUtc.Ticks):$($item.Length)"
        }
    }

    return $state
}

try {
    $null      = & git rev-parse --is-inside-work-tree 2>$null
    $insideGit = ($LASTEXITCODE -eq 0)
} catch {
    $insideGit = $false
}

if ($insideGit) {
    $stateBefore = Get-TrackedFileState
}

& $vstestPath @vstestArgs
$testExit = $LASTEXITCODE

if ($insideGit) {
    $stateAfter = Get-TrackedFileState
    $touched    = @()

    foreach ($relative in $stateAfter.Keys) {
        if (-not $stateBefore.ContainsKey($relative)) {
            $touched += "created  $relative"
        }
        elseif ($stateBefore[$relative] -ne $stateAfter[$relative]) {
            $touched += "written  $relative"
        }
    }

    foreach ($relative in $stateBefore.Keys) {
        if (-not $stateAfter.ContainsKey($relative)) {
            $touched += "deleted  $relative"
        }
    }

    if ($touched.Count -gt 0) {
        Write-Host ''
        Write-Host 'THE TEST RUN TOUCHED THE SOURCE TREE.' -ForegroundColor Red
        Write-Host 'A unit test may not write into it, even the same bytes back:' -ForegroundColor Red
        Write-Host ''

        foreach ($line in ($touched | Sort-Object)) {
            Write-Host "  $line" -ForegroundColor Yellow
        }

        Write-Host ''
        Restore-CassoHostLoad -Priority $priorityWas
        exit 1
    }
}

Restore-CassoHostLoad -Priority $priorityWas

if ($testExit -ne 0) {
    exit $testExit
}
