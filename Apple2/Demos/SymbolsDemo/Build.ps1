<#
.SYNOPSIS
    Builds symbols-demo.dsk and symbols-demo.dbg.

.DESCRIPTION
    A direct-boot disk for trying the debugger's symbol tips and its source
    pane. The source is assembled with -g, which writes symbols-demo.dbg
    beside symbols-demo.a65. The debugger finds the source from the debug
    file, so the two stay in this folder together. The disk goes up to
    Apple2\Demos, like every other demo disk.

.PARAMETER Force
    Rebuild even when the .dsk is newer than the source.

.PARAMETER Configuration
    Which CassoCli build to use. Default: Release.
#>
[CmdletBinding()]
param(
    [switch]$Force,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$here     = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $here '..\..\..')).Path
$demoDir  = Join-Path $repoRoot 'Apple2\Demos'
$cli      = Join-Path $repoRoot "x64\$Configuration\CassoCli.exe"
$source   = Join-Path $here 'symbols-demo.a65'
$outBin   = Join-Path $here 'symbols-demo.bin'
$outDbg   = Join-Path $here 'symbols-demo.dbg'
$outDsk   = Join-Path $demoDir 'symbols-demo.dsk'

if (-not (Test-Path $cli))
{
    throw "CassoCli.exe not found at $cli. Run scripts/Build.ps1 first."
}

if (-not $Force -and (Test-Path $outDsk) -and (Test-Path $outDbg) -and
    (Get-Item $source).LastWriteTimeUtc -le (Get-Item $outDsk).LastWriteTimeUtc)
{
    Write-Host "  up to date: symbols-demo.dsk"
    return
}

#  `disk create` refuses to overwrite, so clear the artifacts first.
Remove-Item $outBin, $outDbg, $outDsk -ErrorAction SilentlyContinue

#  CassoCli exits non-zero on warnings (an unused label) but still writes the
#  binary, so check for the files rather than the exit code.
Push-Location $here

try
{
    & $cli as65 symbols-demo.a65 -g -o symbols-demo.bin -q | Out-Null
}
finally
{
    Pop-Location
}

if (-not (Test-Path $outBin) -or -not (Test-Path $outDbg))
{
    throw "Assembly produced no output for symbols-demo.a65."
}

& $cli disk create $outDsk --boot $outBin --load 0x6000
if ($LASTEXITCODE -ne 0)
{
    throw "disk create failed ($LASTEXITCODE)."
}

Remove-Item $outBin
Write-Host "Wrote $outDsk and $outDbg"
