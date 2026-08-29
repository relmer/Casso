<#
.SYNOPSIS
    Regenerates docs/cycle-reference.md from the emulator's instruction tables.

.DESCRIPTION
    The cycle reference is GENERATED, not written. Its numbers are the
    `baseCycles` values in Casso's own NMOS and 65C02 `Microcode` tables, so the
    document cannot describe a machine other than the one the build emulates,
    and a changed cycle count cannot leave a stale reference behind it.

    The generator lives in `CassoCore/CycleReference.cpp` where `UnitTest` can
    link it. This script is the crank that turns it: it builds, runs the guard
    test, and copies the document the test produced into `docs/`.

    THE TEST WRITES THE DOCUMENT, NOT THIS SCRIPT, and it writes it outside the
    repository -- to `%TEMP%\Casso\cycle-reference.md` -- on every run, pass or
    fail. A test that wrote into the working tree would make the comparison it
    performs vacuous (it would be checking its own output against itself) and
    would trip the tree-modification guard in RunTests.ps1 besides.

    The copy is written with CRLF endings to match the rest of docs/.

    Run this after any change to a cycle count, an addressing mode, or the
    instruction tables, then commit the regenerated document alongside the code
    change. The guard test fails until you do.

.PARAMETER Configuration
    Build configuration used to run the generator. Default: Debug.

.PARAMETER Platform
    Target platform. Default: x64.

.PARAMETER SkipBuild
    Use the test assembly already on disk. Only sensible immediately after a
    build; a stale assembly generates the previous build's document, which is
    the exact failure this whole mechanism exists to prevent.

.PARAMETER Check
    Report whether the document is up to date and change nothing. Exits 1 when
    it is stale. This is what the guard test does, from the command line.

.NOTES
    Exit codes: 0 = success, 1 = failure (or, with -Check, stale).
#>
[CmdletBinding()]
param (
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [switch]$SkipBuild,

    [switch]$Check
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot  = Split-Path -Parent $PSScriptRoot
$document  = Join-Path $repoRoot 'docs\cycle-reference.md'
$generated = Join-Path ([System.IO.Path]::GetTempPath()) 'Casso\cycle-reference.md'
$filter    = 'FullyQualifiedName~CycleReferenceTests'

#
#  Run the guard test. Its verdict is INFORMATION here rather than an error:
#  a red run is the ordinary case when the document is stale, which is the
#  case this script exists to fix. What must not be tolerated is the test not
#  running at all, and that is what the missing-artifact check below catches.
#
$runTests = Join-Path $PSScriptRoot 'RunTests.ps1'

if (-not (Test-Path -LiteralPath $runTests)) {
    Write-Host "Test runner not found: $runTests" -ForegroundColor Red
    exit 1
}

if (Test-Path -LiteralPath $generated) {
    Remove-Item -LiteralPath $generated -Force
}

$arguments = @{ Configuration = $Configuration; Platform = $Platform; Filter = $filter }

if (-not $SkipBuild) {
    $arguments['Build'] = $true
}

& $runTests @arguments | Out-Host

#
#  A missing artifact means the generator never ran -- a build failure, a
#  filter that matched nothing, a renamed test. Reporting "up to date" here
#  would be a confident pass over an inspection that never happened.
#
if (-not (Test-Path -LiteralPath $generated)) {
    Write-Host ''
    Write-Host 'The guard test produced no document.' -ForegroundColor Red
    Write-Host "  expected: $generated" -ForegroundColor DarkGray
    Write-Host '  The test did not run. Check the output above for a build or filter failure.' -ForegroundColor DarkGray
    exit 1
}

$fresh   = [System.IO.File]::ReadAllText($generated) -replace "`r", ''
$current = ''

if (Test-Path -LiteralPath $document) {
    $current = [System.IO.File]::ReadAllText($document) -replace "`r", ''
}

if ($fresh -eq $current) {
    Write-Host ''
    Write-Host 'docs/cycle-reference.md is up to date.' -ForegroundColor Green
    exit 0
}

if ($Check) {
    Write-Host ''
    Write-Host 'docs/cycle-reference.md is STALE.' -ForegroundColor Red
    Write-Host "  Regenerate it: pwsh scripts/UpdateCycleReference.ps1" -ForegroundColor DarkGray
    exit 1
}

[System.IO.File]::WriteAllText($document, ($fresh -replace "`n", "`r`n"))

Write-Host ''
Write-Host 'docs/cycle-reference.md regenerated.' -ForegroundColor Green
Write-Host '  Review the diff and commit it with the change that caused it.' -ForegroundColor DarkGray
exit 0
