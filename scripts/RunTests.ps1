<#
.SYNOPSIS
    Runs the My6502 unit tests using vstest.console.

.PARAMETER Configuration
    The build configuration whose test DLL to run.
    Default: Debug

.PARAMETER Platform
    The target platform. Valid values are 'x64' or 'Auto'.
    'Auto' defaults to x64.
    Default: Auto
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64', 'Auto')]
    [string]$Platform = 'Auto'
)

if ($Platform -eq 'Auto') {
    if ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq [System.Runtime.InteropServices.Architecture]::Arm64) {
        $Platform = 'ARM64'
    } else {
        $Platform = 'x64'
    }
}

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

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

$testAssembly = Join-Path -Path $repoRoot -ChildPath "$Platform\$Configuration\UnitTest.dll"
if (-not (Test-Path -Path $testAssembly)) {
    throw "Test assembly not found at $testAssembly. Build the tests before running them."
}

Write-Host "Running tests from $testAssembly" -ForegroundColor Cyan
Write-Host "vstest.console path: $vstestPath" -ForegroundColor DarkGray

& $vstestPath $testAssembly
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
