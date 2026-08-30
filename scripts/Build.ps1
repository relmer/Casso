<#
.SYNOPSIS
    Builds the Casso solution using MSBuild.

.PARAMETER Configuration
    The build configuration. Valid values are 'Debug' or 'Release'.
    Default: Debug

.PARAMETER Platform
    The target platform. Valid values are 'x64', 'ARM64', or 'Auto'.
    'Auto' detects the current OS architecture.
    Default: Auto

.PARAMETER Target
    The build target. Valid values are:
      - Build            Build the solution (default)
      - Clean            Clean build outputs
      - Rebuild          Clean and rebuild
      - BuildAllRelease  Build Release for all platforms (x64 and ARM64)
      - CleanAll         Clean all configurations and platforms
      - RebuildAllRelease  Rebuild Release for all platforms
    Default: Build

.PARAMETER RunCodeAnalysis
    If set, enables C++ Core Check code analysis during build.

.PARAMETER NormalPriority
    Run at the shell's own priority instead of below it. For CI, which has
    no foreground to protect.

.PARAMETER LowPriority
    Run at Idle instead of BelowNormal: no progress at all while anything
    else wants the CPU. For a run you genuinely do not care about finishing
    soon.

.EXAMPLE
    .\Build.ps1
    Builds Debug configuration for x64.

.EXAMPLE
    .\Build.ps1 -Configuration Release -Target Rebuild
    Rebuilds Release configuration for x64.
#>
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64', 'Auto')]
    [string]$Platform = 'Auto',

    [ValidateSet('Build', 'Clean', 'Rebuild', 'BuildAllRelease', 'CleanAll', 'RebuildAllRelease')]
    [string]$Target = 'Build',

    [switch]$RunCodeAnalysis,

    [switch]$NormalPriority,

    [switch]$LowPriority
)

if ($Platform -eq 'Auto') {
    if ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq [System.Runtime.InteropServices.Architecture]::Arm64) {
        $Platform = 'ARM64'
    } else {
        $Platform = 'x64'
    }
}

$ErrorActionPreference = 'Stop'

$repoRoot     = Split-Path $PSScriptRoot -Parent
$solutionPath = Join-Path $repoRoot 'Casso.sln'

#
#  OFF THE FOREGROUND'S BACK. This saturates every core and the disk with
#  it, and nothing about it is latency-sensitive -- nobody watches a build.
#  Lowered here rather than around the tool because Windows hands a child
#  its parent's priority class, so this reaches MSBuild, every cl.exe it
#  fans out, and vstest and its hosts. See scripts/HostLoad.ps1.
#
. (Join-Path $PSScriptRoot 'HostLoad.ps1')

$priorityWas = $null

#
#  There are exactly two ways this script runs:
#
#      default            BelowNormal (or Idle with -LowPriority), and
#                         MSBuild node reuse OFF
#      -NormalPriority    the shell's own priority, node reuse ON
#
#  Nothing else decides it -- no CI sniffing, no environment variable. If
#  you want a build at full priority you ask for one.
#
#  NODE REUSE IS NOT A SEPARATE PERFORMANCE KNOB HERE. It is off only
#  because it is what makes the lowering reach the compiler, and it rides
#  the same flag for that reason alone. A child inherits its parent's
#  priority AT CREATION, and MSBuild's reusable workers are not children of
#  this build: they outlive the build that made them, were started at
#  Normal by whichever build came first, and are handed this build's work
#  as they already are. Sampled mid-build with reuse on, exactly one
#  MSBuild was BelowNormal -- the one we launched -- while six workers and
#  every cl.exe under them ran at Normal, which is to say the entire
#  compile. With reuse off: seven MSBuild and eighteen cl.exe, all
#  BelowNormal.
#
#  So when we are NOT lowering there is nothing for reuse to defeat, and it
#  stays on and keeps its roughly one second per build.
#
$msbuildNodeArgs = @()

if (-not $NormalPriority) {
    $priorityWas      = Set-CassoHostLoad -Priority ($LowPriority ? 'Idle' : 'BelowNormal')
    $msbuildNodeArgs += '-nr:false'
}

#  Put it back on the way out however this ends -- these are run from an
#  interactive shell as often as from a fresh one, and a session left at
#  BelowNormal for the rest of the day is a slow shell nobody can explain.
trap { Restore-CassoHostLoad -Priority $priorityWas; break }

$toolsScript = Join-Path $PSScriptRoot 'VSTools.ps1'
if (-not (Test-Path $toolsScript)) {
    throw "Tool helper script not found: $toolsScript"
}

. $toolsScript

if (-not (Test-Path $solutionPath)) {
    throw "Solution not found: $solutionPath"
}

#
#  Point this clone at the repo's hooks, so the pre-push style gate actually
#  runs.
#
#  Git deliberately refuses to let a repository configure its own clones --
#  core.hooksPath arriving with a checkout would make cloning any repo an
#  arbitrary-code-execution hazard -- so .git/config never syncs and the
#  setting has to be applied locally, once per clone. Documenting that step
#  is not enough: a clone whose owner did not read the docs pushes style
#  violations that CI then catches after the fact, which is the slow way to
#  learn something the hook reports in a second.
#
#  Doing it here rather than in a setup script means it self-heals. Everyone
#  builds, so every clone and every new worktree acquires the hook without
#  anyone remembering to. It announces itself the one time it changes
#  anything, because a build script quietly rewriting git config is the sort
#  of thing that should be findable later.
#
#  Not fatal on failure: a missing git, a detached checkout, or an exported
#  tree with no .git are all reasons the build should still run. CI's
#  tree-wide style job is the real backstop regardless.
#
$hooksDir = '.githooks'
if (Test-Path (Join-Path $repoRoot '.git')) {
    try {
        $currentHooks = git -C $repoRoot config --local --get core.hooksPath 2>$null
        if ($currentHooks -ne $hooksDir) {
            git -C $repoRoot config --local core.hooksPath $hooksDir 2>$null
            if ($LASTEXITCODE -eq 0) {
                Write-Host "Enabled the pre-push style hook for this clone (core.hooksPath -> $hooksDir)." -ForegroundColor DarkGray
            }
        }
    } catch {
        Write-Host "Could not set core.hooksPath; pushes will not be style-gated locally." -ForegroundColor DarkYellow
    }
}

# GPL/copyleft guard for vendored shader ports. Runs before the
# msbuild invocation so a license drift fails the build with a clear
# error before any compile output is produced. Skipped for Clean/CleanAll
# targets (no source compile -> nothing to taint).
if ($Target -ne 'Clean' -and $Target -ne 'CleanAll')
{
    $shaderCheckScript = Join-Path $PSScriptRoot 'CheckShaderLicenses.ps1'
    if (Test-Path $shaderCheckScript)
    {
        & $shaderCheckScript
        if ($LASTEXITCODE -ne 0)
        {
            Write-Host "CheckShaderLicenses pre-build step failed (see errors above)." -ForegroundColor Red
            exit $LASTEXITCODE
        }
    }
}

$msbuildPath = Get-VS2026MSBuildPath
if (-not $msbuildPath) {
    $msbuildPath = Get-VS2026MSBuildPath -IncludePrerelease
}

if (-not $msbuildPath) {
    throw 'VS 2026 (v18.x) MSBuild not found (via vswhere). Install VS 2026 with MSBuild.'
}

Write-Host "Using MSBuild: $msbuildPath"
Write-Host "Building: $solutionPath ($Configuration|$Platform) Target=$Target"

$preferredArch = 'x64'
if ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq [System.Runtime.InteropServices.Architecture]::Arm64) {
    $preferredArch = 'ARM64'
}

$scriptExitCode = 0

if ($Target -eq 'BuildAllRelease' -or $Target -eq 'CleanAll' -or $Target -eq 'RebuildAllRelease') {
    $platformsToBuild = @('x64', 'ARM64')

    if ($Target -eq 'CleanAll') {
        $configsToBuild = @('Debug', 'Release')
        $msbuildTarget = 'Clean'
    }
    elseif ($Target -eq 'RebuildAllRelease') {
        $configsToBuild = @('Release')
        $msbuildTarget = 'Rebuild'
    }
    else {
        $configsToBuild = @('Release')
        $msbuildTarget = 'Build'
    }

    foreach ($config in $configsToBuild) {
        foreach ($platformToBuild in $platformsToBuild) {
            $msbuildArgs = @(
                $solutionPath,
                "-m",
                "-p:Configuration=$config",
                "-p:Platform=$platformToBuild",
                "-p:PreferredToolArchitecture=$preferredArch",
                "-t:$msbuildTarget"
            ) + $msbuildNodeArgs

            if ($RunCodeAnalysis) {
                $msbuildArgs += '-p:EnableCppCoreCheck=true'
                $msbuildArgs += '-p:RunCodeAnalysis=true'
                $msbuildArgs += '-p:CodeAnalysisTreatWarningsAsErrors=true'
            }

            Write-Host "Building: $solutionPath ($config|$platformToBuild) Target=$msbuildTarget" -ForegroundColor Cyan

            $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
            & $msbuildPath @msbuildArgs
            $stopwatch.Stop()

            if ($LASTEXITCODE -ne 0) {
                $scriptExitCode = $LASTEXITCODE
                break
            }
        }

        if ($scriptExitCode -ne 0) { break }
    }
}
else {
    #  -m builds projects that do not depend on each other at the same time;
    #  /MP (Directory.Build.props) parallelizes the files within a project.
    #  Neither was set, so a full build used one core out of eight.
    $msbuildArgs = @(
        $solutionPath,
        "-m",
        "-p:Configuration=$Configuration",
        "-p:Platform=$Platform",
        "-p:PreferredToolArchitecture=$preferredArch"
    ) + $msbuildNodeArgs

    if ($RunCodeAnalysis) {
        $msbuildArgs += '-p:EnableCppCoreCheck=true'
        $msbuildArgs += '-p:RunCodeAnalysis=true'
        $msbuildArgs += '-p:CodeAnalysisTreatWarningsAsErrors=true'
    }

    if ($Target -ne 'Build') {
        $msbuildArgs += "-t:$Target"
    }

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    & $msbuildPath @msbuildArgs
    $stopwatch.Stop()

    if ($LASTEXITCODE -ne 0) {
        $scriptExitCode = $LASTEXITCODE
    }
}

$minutes  = [int][Math]::Floor($stopwatch.Elapsed.TotalMinutes)
$timeText = "{0:00}:{1:00}.{2:000}" -f $minutes, $stopwatch.Elapsed.Seconds, $stopwatch.Elapsed.Milliseconds

Restore-CassoHostLoad -Priority $priorityWas

if ($scriptExitCode -ne 0) {
    Write-Host "FAILED ($timeText)" -ForegroundColor Red
    exit $scriptExitCode
}

Write-Host "SUCCEEDED ($timeText)" -ForegroundColor Green
