<#
.SYNOPSIS
    Builds the Casso65 solution using MSBuild.

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

    [switch]$RunCodeAnalysis
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
$solutionPath = Join-Path $repoRoot 'Casso65.sln'

$toolsScript = Join-Path $PSScriptRoot 'VSTools.ps1'
if (-not (Test-Path $toolsScript)) {
    throw "Tool helper script not found: $toolsScript"
}

. $toolsScript

if (-not (Test-Path $solutionPath)) {
    throw "Solution not found: $solutionPath"
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
                "-p:Configuration=$config",
                "-p:Platform=$platformToBuild",
                "-p:PreferredToolArchitecture=$preferredArch",
                '-p:EnableCppCoreCheck=true',
                '-p:RunCodeAnalysis=true',
                "-t:$msbuildTarget"
            )

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
    $msbuildArgs = @(
        $solutionPath,
        "-p:Configuration=$Configuration",
        "-p:Platform=$Platform",
        "-p:PreferredToolArchitecture=$preferredArch",
        '-p:EnableCppCoreCheck=true',
        '-p:RunCodeAnalysis=true'
    )

    if ($RunCodeAnalysis) {
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

if ($scriptExitCode -ne 0) {
    Write-Host "FAILED ($timeText)" -ForegroundColor Red
    exit $scriptExitCode
}

Write-Host "SUCCEEDED ($timeText)" -ForegroundColor Green
