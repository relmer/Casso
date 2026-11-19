<#
.SYNOPSIS
    Run the full Tom Harte SingleStepTests vector set against the CPU cores.

.DESCRIPTION
    A unit test run checks a small subset of the vectors, compiled into the
    test DLL (scripts/EmbedHarteVectors.py). Run this script for the full set
    after a change that could reasonably break it: the CPU cores, opcode
    tables, addressing, flags, decimal mode, cycle counts, interrupts, or the
    MMU and memory bus. See docs/testing.md.

    1. Runs GenerateHarteTests.py to download the JSON and write the binary
       vector files into -HarteDir, unless -SkipGenerate is given
    2. Builds the solution
    3. Runs only the Harte tests, with CASSO_HARTE_DIR set to -HarteDir

.PARAMETER Opcode
    Optional: single opcode (hex) to generate and test, e.g. "A9"

.PARAMETER MaxVectors
    Optional: limit vectors per opcode when generating

.PARAMETER Cpu
    CPU type folder name to generate (default: "6502")

.PARAMETER HarteDir
    Where the full set is kept (default: %LOCALAPPDATA%\Casso\HarteTests),
    shared by every checkout on the machine

.PARAMETER SkipGenerate
    Skip the generation step and use the vector files already in -HarteDir

.PARAMETER Configuration
    Build configuration (default: Release)

.PARAMETER Platform
    Build platform (default: x64)

.NOTES
    Exit codes: 0 = success, 1 = failure
#>
[CmdletBinding()]
param (
    [string] $Opcode,
    [int]    $MaxVectors = 0,
    [string] $Cpu = "6502",
    [string] $HarteDir = (Join-Path $env:LOCALAPPDATA 'Casso\HarteTests'),
    [switch] $SkipGenerate,
    [string] $Configuration = "Release",
    [string] $Platform = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition


# Step 1: Generate the binary vector files
if (-not $SkipGenerate) {
    Write-Host "=== Generating Harte test data ===" -ForegroundColor Cyan

    $genArgs = @("$scriptDir\GenerateHarteTests.py", "--cpu", $Cpu, "--output-dir", (Join-Path $HarteDir $Cpu))

    if ($Opcode) {
        $genArgs += @("--opcode", $Opcode)
    }

    if ($MaxVectors -gt 0) {
        $genArgs += @("--max-vectors", $MaxVectors)
    }

    & python @genArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Generation failed"
        exit 1
    }
}


# Step 2: Build
Write-Host "`n=== Building ($Configuration|$Platform) ===" -ForegroundColor Cyan
& pwsh -NoProfile -ExecutionPolicy Bypass -File "$scriptDir\Build.ps1" `
    -Target Build -Configuration $Configuration -Platform $Platform

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}


# Step 3: Run the Harte tests against the full set
Write-Host "`n=== Running Harte tests from $HarteDir ===" -ForegroundColor Cyan
$env:CASSO_HARTE_DIR = $HarteDir

try {
    & pwsh -NoProfile -ExecutionPolicy Bypass -File "$scriptDir\RunTests.ps1" `
        -Configuration $Configuration -Platform $Platform -Filter Harte
}
finally {
    Remove-Item Env:CASSO_HARTE_DIR -ErrorAction SilentlyContinue
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Tests failed"
    exit 1
}

Write-Host "`nSUCCESS" -ForegroundColor Green
exit 0
