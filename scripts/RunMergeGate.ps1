<#
.SYNOPSIS
    Runs the pre-merge gate: the style sweep, all four configurations rebuilt
    with code analysis, the Debug and Release unit suites, the scenario suite
    and the AddressSanitizer scenario run.

.DESCRIPTION
    By default the independent steps overlap. The style sweep runs beside
    everything else, the x64 builds go first so their tests run while the
    ARM64 configurations build, and the AddressSanitizer build runs beside
    the ARM64 builds. The unit suites are sharded across processes by
    RunTests.ps1.

    -Serial runs every step in one process after another, and the unit suites
    in one vstest process each. Use it when a parallel run runs out of
    commit memory (C3859, C1060, or a linker that runs out of heap), or to
    rule out two steps interfering.

    The scenario suites run one after the other in both modes, since both
    boot guests from the same fetched disks.

    Each step writes a log to the output directory. The gate stops when a
    step fails and prints which.

.PARAMETER Serial
    One step at a time, unit suites unsharded.

.PARAMETER OutputDirectory
    Where the step logs go. Default: %TEMP%\CassoMergeGate\<time>.

.EXAMPLE
    .\RunMergeGate.ps1
#>
param(
    [switch]$Serial,

    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $env:TEMP ("CassoMergeGate\" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}

New-Item -ItemType Directory -Force $OutputDirectory | Out-Null

$shardArg = $Serial ? @('-Serial') : @()

#
#  A step is a script and its arguments. Each runs in its own pwsh, so steps
#  can run as background jobs in parallel mode and in sequence in serial mode.
#
function New-Step {
    param([string]$Name, [string]$Script, [string[]]$Arguments)

    [pscustomobject]@{ Name = $Name; Script = (Join-Path $PSScriptRoot $Script); Arguments = $Arguments }
}

$style        = New-Step 'style-tree'       'CheckStyle.ps1' @('-Mode', 'Tree')
$buildX64Dbg  = New-Step 'build-x64-debug'  'Build.ps1'      @('-Configuration', 'Debug',   '-Platform', 'x64',   '-Target', 'Rebuild', '-RunCodeAnalysis')
$buildX64Rel  = New-Step 'build-x64-release' 'Build.ps1'     @('-Configuration', 'Release', '-Platform', 'x64',   '-Target', 'Rebuild', '-RunCodeAnalysis')
$buildArmDbg  = New-Step 'build-arm64-debug' 'Build.ps1'     @('-Configuration', 'Debug',   '-Platform', 'ARM64', '-Target', 'Rebuild', '-RunCodeAnalysis')
$buildArmRel  = New-Step 'build-arm64-release' 'Build.ps1'   @('-Configuration', 'Release', '-Platform', 'ARM64', '-Target', 'Rebuild', '-RunCodeAnalysis')
$testsDbg     = New-Step 'tests-debug'      'RunTests.ps1'   (@('-Configuration', 'Debug') + $shardArg)
$testsRel     = New-Step 'tests-release'    'RunTests.ps1'   (@('-Configuration', 'Release') + $shardArg)
$buildAsan    = New-Step 'build-asan'       'Build.ps1'      @('-Configuration', 'Release', '-Platform', 'x64', '-Sanitize')
$scenario     = New-Step 'scenario-debug'   'RunTests.ps1'   (@('-Configuration', 'Debug', '-Scenario') + $shardArg)
$asanScenario = New-Step 'asan-scenario'    'RunTests.ps1'   (@('-Configuration', 'Release', '-Sanitize', '-Scenario') + $shardArg)

#
#  Stages run one after another; the steps inside a stage run at the same
#  time. Serial mode flattens them into one step per stage.
#
$stages = @(
    , @($style, $buildX64Dbg)
    , @($buildX64Rel, $testsDbg)
    , @($buildArmDbg, $testsRel, $scenario)
    , @($buildArmRel, $buildAsan)
    , @($asanScenario)
)

if ($Serial) {
    $stages = @($stages | ForEach-Object { $_ } | ForEach-Object { , @($_) })
}

$gateTimer = [System.Diagnostics.Stopwatch]::StartNew()
$failed    = $null

foreach ($stage in $stages) {
    $running = foreach ($step in $stage) {
        $log = Join-Path $OutputDirectory "$($step.Name).log"

        [pscustomobject]@{
            Step    = $step
            Log     = $log
            Timer   = [System.Diagnostics.Stopwatch]::StartNew()
            Process = Start-Process -FilePath 'pwsh' -WorkingDirectory $repoRoot -NoNewWindow -PassThru `
                                    -ArgumentList (@('-NoProfile', '-File', "`"$($step.Script)`"") + $step.Arguments) `
                                    -RedirectStandardOutput $log -RedirectStandardError "$log.err"
        }
    }

    foreach ($r in $running) {
        $r.Process.WaitForExit()
        $r.Timer.Stop()

        $code   = $r.Process.ExitCode
        $status = ($code -eq 0) ? 'ok' : 'FAILED'

        Write-Host ("{0,-20} {1,-6} {2,5:N1} min" -f $r.Step.Name, $status, $r.Timer.Elapsed.TotalMinutes)

        if ($code -ne 0 -and -not $failed) {
            $failed = $r
        }
    }

    if ($failed) {
        break
    }
}

$gateTimer.Stop()

if ($failed) {
    Write-Host ''
    Write-Host ("GATE FAILED at {0}. Log: {1}" -f $failed.Step.Name, $failed.Log) -ForegroundColor Red
    Get-Content $failed.Log, "$($failed.Log).err" -ErrorAction SilentlyContinue | Select-Object -Last 30 | ForEach-Object { Write-Host $_ }
    exit 1
}

Write-Host ''
Write-Host ("GATE PASSED in {0:N1} min. Logs: {1}" -f $gateTimer.Elapsed.TotalMinutes, $OutputDirectory) -ForegroundColor Green
exit 0
