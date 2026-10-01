<#
.SYNOPSIS
    Regenerates docs/Debugger-Commands.md from the debugger's help table.

.DESCRIPTION
    The debugger command reference is GENERATED, not written. Each mode's
    section is that mode's `help all`, built by `CommandModeHelp::BuildReference`
    in `CassoCore/Debugger/CommandModeHelp.cpp`, so the document cannot list a
    command help does not.

    This script builds, runs the guard test (DebuggerCommandsDocTests), and
    copies the document the test wrote to `%TEMP%\Casso\Debugger-Commands.md`
    into `docs/` with CRLF endings. The test never writes into the tree.

    Run it after any change to a command's syntax, description or category,
    then commit the regenerated document with the change. The guard test fails
    until you do.
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
$document  = Join-Path $repoRoot 'docs\Debugger-Commands.md'
$generated = Join-Path ([System.IO.Path]::GetTempPath()) 'Casso\Debugger-Commands.md'
$filter    = 'FullyQualifiedName~DebuggerCommandsDocTests'

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
    Write-Host 'docs/Debugger-Commands.md is up to date.' -ForegroundColor Green
    exit 0
}

if ($Check) {
    Write-Host ''
    Write-Host 'docs/Debugger-Commands.md is STALE.' -ForegroundColor Red
    Write-Host "  Regenerate it: pwsh scripts/UpdateDebuggerCommands.ps1" -ForegroundColor DarkGray
    exit 1
}

[System.IO.File]::WriteAllText($document, ($fresh -replace "`n", "`r`n"))

Write-Host ''
Write-Host 'docs/Debugger-Commands.md regenerated.' -ForegroundColor Green
Write-Host '  Review the diff and commit it with the change that caused it.' -ForegroundColor DarkGray
exit 0
