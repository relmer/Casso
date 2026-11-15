<#
.SYNOPSIS
    Profiles a full solution build: an MSBuild binary log plus a C++ Build
    Insights trace.

.DESCRIPTION
    Runs Build.ps1 at normal priority, so the timing is not skewed by the
    BelowNormal default, with -BinaryLog. Around it, vcperf collects a C++
    Build Insights trace without elevation (/noadmin). The output directory
    receives:

      build.binlog    MSBuild binary log. Open it in MSBuild Structured Log
                      Viewer (https://msbuildlog.com). Its Timeline tab has
                      each project's start and end times, the gaps between
                      dependent projects, and idle cores.
      build.etl       Build Insights trace for Windows Performance Analyzer
                      (wpa.exe). The Build Explorer, Files, Functions and
                      Template Instantiations views list PCH creation time,
                      per-header parse cost across all files, and front-end
                      versus back-end time per file.
      build.json      The same trace in Chrome trace format. Open it at
                      https://ui.perfetto.dev for a timeline without WPA.
      summary.txt     Commit, configuration, core count and elapsed time.

    To compare two commits, run it in each checkout and compare the two
    summary.txt files, then the two traces.

.PARAMETER Configuration
    Debug or Release. Default: Debug

.PARAMETER Platform
    x64, ARM64 or Auto. Default: Auto

.PARAMETER Target
    Rebuild (default) measures the whole build. Build measures an incremental
    build from the current state of the output directories.

.PARAMETER OutputDirectory
    Where the logs go. Default: %TEMP%\CassoBuildProfile\<time>-<commit>,
    outside the working tree.

.PARAMETER Property
    Extra MSBuild properties, each as Name=Value, passed through to Build.ps1.
    summary.txt lists them.

.PARAMETER NoTrace
    Skip the vcperf trace and write only the binary log.

.PARAMETER Templates
    Include template instantiation data in the trace. Analysis takes longer
    and the trace is larger.

.EXAMPLE
    .\ProfileBuild.ps1
    Rebuilds Debug x64 with both logs.

.EXAMPLE
    .\ProfileBuild.ps1 -Configuration Release -Templates
    Rebuilds Release, with template instantiations in the trace.
#>
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64', 'Auto')]
    [string]$Platform = 'Auto',

    [ValidateSet('Rebuild', 'Build')]
    [string]$Target = 'Rebuild',

    [string]$OutputDirectory,

    [string[]]$Property = @(),

    [switch]$NoTrace,

    [switch]$Templates
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'VSTools.ps1')





#
#  Count running compiler and linker processes. Called only while this
#  script's build is not running, so every one belongs to another build.
#  MSBuild.exe is not counted: idle worker nodes left by node reuse or by
#  Visual Studio stay alive for minutes without compiling anything.
#
function Get-OtherCompilerCount {
    $processes = Get-Process -Name 'cl', 'link' -ErrorAction SilentlyContinue
    return @($processes).Count
}

$repoRoot = Split-Path $PSScriptRoot -Parent
$commit   = (git -C $repoRoot rev-parse --short HEAD 2>$null)
$branch   = (git -C $repoRoot rev-parse --abbrev-ref HEAD 2>$null)

if (-not $commit) {
    $commit = 'nogit'
}

if (-not $OutputDirectory) {
    $stamp           = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputDirectory = Join-Path $env:TEMP "CassoBuildProfile\$stamp-$commit"
}

$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null

$binaryLogPath = Join-Path $OutputDirectory 'build.binlog'
$rawTracePath  = Join-Path $OutputDirectory 'build-raw.etl'
$tracePath     = Join-Path $OutputDirectory 'build.etl'
$timeTracePath = Join-Path $OutputDirectory 'build.json'
$summaryPath   = Join-Path $OutputDirectory 'summary.txt'

#
#  The trace session is machine-wide: it records every MSVC compile on the
#  machine, including another checkout's build. That build also competes for
#  the cores, so the elapsed time is wrong too. Check before and after the
#  run, and write the result into the summary so a contaminated profile is
#  marked as one.
#
$otherBuildsBefore = Get-OtherCompilerCount

if ($otherBuildsBefore -gt 0) {
    Write-Host "WARNING: $otherBuildsBefore compiler or linker process(es) are already running. The trace and the timing will include them." -ForegroundColor Yellow
}

$vcperfPath  = $null
$sessionName = "CassoBuild-$PID"

if (-not $NoTrace) {
    $vcperfPath = Get-VS2026ToolPath -Find 'Common7\IDE\CommonExtensions\Platform\CppBuildInsights\vcperf.exe'

    if (-not $vcperfPath) {
        throw 'vcperf.exe not found in the VS 2026 install. Install the C++ Build Insights component, or pass -NoTrace.'
    }

    & $vcperfPath /start /noadmin /nocpusampling $sessionName | Out-Null

    if ($LASTEXITCODE -ne 0) {
        throw "vcperf /start failed with exit code $LASTEXITCODE."
    }
}

#
#  Stop the trace session however the build ends. An ETW session left
#  running outlives this script and keeps collecting until reboot or a
#  manual vcperf /stop.
#
$buildExitCode = 0
$stopwatch     = [System.Diagnostics.Stopwatch]::StartNew()

try {
    & (Join-Path $PSScriptRoot 'Build.ps1') -Configuration $Configuration `
                                           -Platform $Platform `
                                           -Target $Target `
                                           -BinaryLog $binaryLogPath `
                                           -Property $Property `
                                           -NormalPriority
    $buildExitCode = $LASTEXITCODE
}
finally {
    $stopwatch.Stop()
    $otherBuildsAfter = Get-OtherCompilerCount

    if ($vcperfPath) {
        Write-Host 'Stopping the trace...' -ForegroundColor Cyan
        & $vcperfPath /stopnoanalyze $sessionName $rawTracePath | Out-Null
    }
}

if ($vcperfPath) {
    $templatesArg = @()

    if ($Templates) {
        $templatesArg = @('/templates')
    }

    Write-Host 'Analyzing the trace...' -ForegroundColor Cyan
    & $vcperfPath /analyze @templatesArg $rawTracePath $tracePath | Out-Null
    & $vcperfPath /analyze @templatesArg $rawTracePath /timetrace $timeTracePath | Out-Null
    Remove-Item $rawTracePath -ErrorAction SilentlyContinue
}

$elapsed     = $stopwatch.Elapsed
$cleanText   = 'yes'

if ($otherBuildsBefore -gt 0 -or $otherBuildsAfter -gt 0) {
    $cleanText = "NO: other compiler or linker processes ($otherBuildsBefore at start, $otherBuildsAfter at end)"
}

$summary = @(
    "Clean run:      $cleanText"
    "Commit:         $commit ($branch)"
    "Configuration:  $Configuration|$Platform"
    "Target:         $Target"
    "Properties:     $($Property -join ' ')"
    "Logical cores:  $([Environment]::ProcessorCount)"
    "Elapsed:        {0:00}:{1:00}.{2:000}" -f [int][Math]::Floor($elapsed.TotalMinutes), $elapsed.Seconds, $elapsed.Milliseconds
    "Build exit:     $buildExitCode"
    "Started:        $((Get-Date) - $elapsed)"
)

$summary | Set-Content $summaryPath

Write-Host ''
$summary | ForEach-Object { Write-Host $_ }
Write-Host ''
Write-Host "Logs: $OutputDirectory" -ForegroundColor Green

Get-ChildItem $OutputDirectory | ForEach-Object {
    Write-Host ("  {0,-14} {1,10:N0} KB" -f $_.Name, ($_.Length / 1KB))
}

exit $buildExitCode
