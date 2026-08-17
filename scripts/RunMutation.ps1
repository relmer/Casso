<#
.SYNOPSIS
    Runs one mutation test: break a thing, prove the suite notices, put it back.

.DESCRIPTION
    Writing a test proves the test passes. Breaking the code it covers is what
    proves the test is a test. This script does that one step, and enforces the
    five conditions that make the result mean anything -- every one of which has
    been got wrong at least once in this repository, and each of which fails
    QUIETLY in the direction of a false green.

      1. THE MUTATION MUST APPLY. An anchor that does not match leaves the tree
         untouched, the suite passes, and the mutation is scored "not caught"
         when it was never made. Multi-line anchors are the usual cause: the
         file is CRLF and the anchor is LF. Reported as ANCHOR NOT FOUND, never
         as a result.

      2. THE MUTATION MUST COMPILE. A mutation that does not build is not
         evidence about the tests; it is a badly formed mutation and wants
         reformulating. Reported as DID NOT COMPILE.

      3. THE RUN MUST COMPLETE. A mutation that crashes or hangs the test host
         produces NO tally, and a harness that scores by looking for a "Failed:"
         line reads no-failures as success. One mutation here made the emulated
         6502 execute garbage and wrote 3.2 GB of trace before anything noticed.
         A short or absent tally is reported as RUN DID NOT COMPLETE -- which
         means treat it as caught-by-crash, NOT as a miss.

      4. THE TREE MUST BE REBUILT AFTER RESTORING. Restoring the source is not
         enough: the binaries still contain the mutation, so the next suite run
         reports a confident green against code that is not on disk. This script
         restores AND rebuilds, and verifies the restore by hash.

      5. THE STALENESS GUARD MUST NOT BE BYPASSED. RunTests.ps1 refuses to run
         against a test assembly older than its sources for exactly this reason.
         This script never passes -AllowStale, and never will.

    The verdict is CAUGHT only when the run completed with the expected number
    of tests AND at least one of them failed. Anything else is reported as what
    it is.

.PARAMETER File
    Source file to mutate. Absolute path, or relative to the repository root.

.PARAMETER Anchor
    Exact text to replace. Must occur EXACTLY ONCE -- an ambiguous anchor
    mutates a place you did not choose.

.PARAMETER Mutation
    The text to put in its place. Empty string deletes the anchor.

.PARAMETER Configuration
    Debug or Release. Debug is the default: assertions are compiled away in
    Release, so a mutation that trips one is invisible there.

.PARAMETER Platform
    Defaults to x64. ARM64 is build-only on this project.

.PARAMETER Filter
    Optional test filter, passed straight to RunTests.ps1. A filtered run
    cannot tell you the mutation broke something ELSEWHERE, so run unfiltered
    before believing a NOT CAUGHT.

.PARAMETER ExpectedTotal
    Test count the unmutated tree produces. Omit and the script measures it
    first, which costs one extra suite run and is the safer default.

.EXAMPLE
    ./scripts/RunMutation.ps1 -File CassoCore/AssemblySession.cpp `
        -Anchor 'BAIL_OUT_IF (!isMutable, S_OK);' `
        -Mutation 'BAIL_OUT_IF (true, S_OK);'

    Proves whether anything actually covers the pass-2 rebind.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$File,

    [Parameter(Mandatory = $true)]
    [string]$Anchor,

    [string]$Mutation = '',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [string]$Platform = 'x64',

    [string]$Filter = '',

    [int]$ExpectedTotal = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
$buildPs1  = Join-Path $scriptDir 'Build.ps1'
$testPs1   = Join-Path $scriptDir 'RunTests.ps1'

if (-not [System.IO.Path]::IsPathRooted($File))
{
    $File = Join-Path $repoRoot $File
}

if (-not (Test-Path $File))
{
    throw "No such file: $File"
}

#  Build and test output goes to a file, never through a pipeline filter.
#  Select-Object -First N closes the pipeline while MSBuild keeps running, and
#  the next build then collides with it over the PDB and reports a bogus
#  C1041 "Build FAILED" that looks exactly like a real one.
$logDir = Join-Path ([System.IO.Path]::GetTempPath()) 'CassoMutation'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Invoke-Build
{
    param([string]$Why)

    $log = Join-Path $logDir "build-$Why.log"
    & $buildPs1 -Configuration $Configuration -Platform $Platform *>&1 |
        Out-File $log -Encoding utf8

    return (Select-String -Path $log -Pattern 'Build succeeded' -Quiet) -eq $true
}

function Invoke-Suite
{
    param([string]$Why)

    $log = Join-Path $logDir "test-$Why.log"

    #  A HASHTABLE, not an array. Splatting an array passes its elements
    #  POSITIONALLY, so '-Configuration' arrives as the value of the first
    #  positional parameter rather than naming one -- which fails validation
    #  in a way that reads like the caller passed nonsense.
    #
    #  Also not named $args: inside a function that is the automatic variable
    #  holding unbound arguments.
    $suiteArgs = @{ Configuration = $Configuration; Platform = $Platform }

    if ($Filter)
    {
        $suiteArgs['Filter'] = $Filter
    }

    #  NOTE: -AllowStale is deliberately never passed. See condition 5.
    & $testPs1 @suiteArgs *>&1 | Out-File $log -Encoding utf8

    $text    = Get-Content $log -Raw
    $result  = [pscustomobject]@{ Complete = $false; Total = -1; Passed = 0; Failed = 0; Skipped = 0; Log = $log }

    if ($text -match 'Total tests:\s*(\d+)')   { $result.Total   = [int]$Matches[1] }
    if ($text -match '\bPassed:\s*(\d+)')      { $result.Passed  = [int]$Matches[1] }
    if ($text -match '\bFailed:\s*(\d+)')      { $result.Failed  = [int]$Matches[1] }
    if ($text -match '\bSkipped:\s*(\d+)')     { $result.Skipped = [int]$Matches[1] }

    #  A tally that does not add up is a truncated run wearing a result's
    #  clothing. Both halves are required.
    if ($result.Total -ge 0 -and ($result.Passed + $result.Failed + $result.Skipped) -eq $result.Total)
    {
        $result.Complete = $true
    }

    return $result
}

$original = [System.IO.File]::ReadAllText($File)
$before   = (Get-FileHash $File -Algorithm SHA256).Hash

#  Condition 1. Try the anchor as given, then with line endings normalized to
#  the file's -- a multi-line anchor typed as LF against a CRLF file is the
#  single most common way a mutation silently does nothing.
$anchorText = $Anchor

if (-not $original.Contains($anchorText))
{
    $anchorText = $Anchor -replace "`r`n", "`n" -replace "`n", "`r`n"
}

if (-not $original.Contains($anchorText))
{
    $anchorText = $Anchor -replace "`r`n", "`n"
}

if (-not $original.Contains($anchorText))
{
    Write-Host "ANCHOR NOT FOUND -- nothing was mutated, so there is no result." -ForegroundColor Red
    Write-Host "  file:   $File"
    Write-Host "  anchor: $($Anchor.Substring(0, [Math]::Min(70, $Anchor.Length)))"
    Write-Host "  If the anchor spans lines, check CRLF versus LF."
    exit 2
}

$occurrences = ([regex]::Matches($original, [regex]::Escape($anchorText))).Count

if ($occurrences -ne 1)
{
    Write-Host "ANCHOR IS AMBIGUOUS -- found $occurrences occurrences, need exactly 1." -ForegroundColor Red
    Write-Host "  An ambiguous anchor mutates a site you did not choose."
    exit 2
}

if ($ExpectedTotal -le 0)
{
    Write-Host "Measuring the baseline (unmutated) suite..." -ForegroundColor Cyan

    if (-not (Invoke-Build 'baseline'))
    {
        throw "The tree does not build BEFORE mutating. Fix that first."
    }

    $baseline = Invoke-Suite 'baseline'

    if (-not $baseline.Complete)
    {
        throw "The baseline run did not complete ($($baseline.Total) reported). See $($baseline.Log)."
    }

    if ($baseline.Failed -gt 0)
    {
        throw "The baseline suite is RED ($($baseline.Failed) failing). A mutation result would mean nothing."
    }

    $ExpectedTotal = $baseline.Total
    Write-Host "  baseline: $ExpectedTotal tests, all passing." -ForegroundColor Cyan
}

$verdict  = ''
$exitCode = 0

try
{
    [System.IO.File]::WriteAllText($File, $original.Replace($anchorText, $Mutation))

    Write-Host "Mutation applied. Building..." -ForegroundColor Cyan

    if (-not (Invoke-Build 'mutated'))
    {
        #  Condition 2.
        $verdict  = "DID NOT COMPILE -- reformulate the mutation; this is not a result about the tests."
        $exitCode = 3
    }
    else
    {
        Write-Host "Running the suite against the mutated tree..." -ForegroundColor Cyan
        $run = Invoke-Suite 'mutated'

        if (-not $run.Complete -or $run.Total -ne $ExpectedTotal)
        {
            #  Condition 3. This is the verdict the whole script exists for.
            $verdict  = "RUN DID NOT COMPLETE (reported $($run.Total) of $ExpectedTotal) -- " +
                        "treat as CAUGHT-BY-CRASH, not as a miss. Log: $($run.Log)"
            $exitCode = 4
        }
        elseif ($run.Failed -gt 0)
        {
            $verdict  = "CAUGHT -- $($run.Failed) of $($run.Total) tests failed."
            $exitCode = 0
        }
        else
        {
            $verdict  = "NOT CAUGHT -- all $($run.Total) tests passed with the code broken. " +
                        "Suspect fixture depth before suspecting the assertion."
            $exitCode = 1
        }
    }
}
finally
{
    #  Condition 4. Restore, PROVE the restore, and rebuild -- leaving mutated
    #  binaries behind is how a later green gets reported against code that is
    #  not on disk.
    [System.IO.File]::WriteAllText($File, $original)

    $after = (Get-FileHash $File -Algorithm SHA256).Hash

    if ($after -ne $before)
    {
        Write-Host "RESTORE FAILED -- $File does not match its original content." -ForegroundColor Red
        Write-Host "  expected $before"
        Write-Host "  found    $after"
        exit 5
    }

    Write-Host "Restored. Rebuilding so the binaries match the tree..." -ForegroundColor Cyan

    if (-not (Invoke-Build 'restored'))
    {
        Write-Host "REBUILD AFTER RESTORE FAILED -- the binaries do NOT match the tree." -ForegroundColor Red
        exit 5
    }
}

$color = if ($exitCode -eq 0) { 'Green' } else { 'Yellow' }
Write-Host ''
Write-Host $verdict -ForegroundColor $color

exit $exitCode
