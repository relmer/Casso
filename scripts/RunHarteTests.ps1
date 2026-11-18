<#
.SYNOPSIS
    Generate Tom Harte SingleStepTests binary data and run the test suite.

.DESCRIPTION
    1. Runs GenerateHarteTests.py to download JSON and produce binary test files
    2. Builds the UnitTest project
    3. Runs only the Harte tests, so the HarteVectorDepth line (FULL or REDUCED) is visible

.PARAMETER Opcode
    Optional: single opcode (hex) to generate and test, e.g. "A9"

.PARAMETER MaxVectors
    Optional: limit vectors per opcode for quick dev runs

.PARAMETER Cpu
    CPU type folder name (default: "6502")

.PARAMETER SkipGenerate
    Skip the generation step (use existing binary files)

.PARAMETER Configuration
    Build configuration (default: Debug)

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
    [switch] $SkipGenerate,
    [string] $Configuration = "Debug",
    [string] $Platform = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir


# Step 1: Generate binary test data
if (-not $SkipGenerate) {
    Write-Host "=== Generating Harte test data ===" -ForegroundColor Cyan

    $cacheDir = Join-Path $env:LOCALAPPDATA "Casso\HarteTests\$Cpu"
    $genArgs  = @("$scriptDir\GenerateHarteTests.py", "--cpu", $Cpu, "--output-dir", $cacheDir)

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
Write-Host "`n=== Building UnitTest ($Configuration|$Platform) ===" -ForegroundColor Cyan
& pwsh -NoProfile -ExecutionPolicy Bypass -File "$scriptDir\Build.ps1" `
    -Target Build -Configuration $Configuration -Platform $Platform

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}


# Step 3: Run tests
Write-Host "`n=== Running Harte tests ===" -ForegroundColor Cyan
& pwsh -NoProfile -ExecutionPolicy Bypass -File "$scriptDir\RunTests.ps1" `
    -Configuration $Configuration -Platform $Platform -Filter Harte

if ($LASTEXITCODE -ne 0) {
    Write-Error "Tests failed"
    exit 1
}

Write-Host "`nSUCCESS" -ForegroundColor Green
exit 0
