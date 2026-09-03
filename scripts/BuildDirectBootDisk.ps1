<#
.SYNOPSIS
    Builds a bootable .dsk from one .a65 source using CassoCli's direct boot.

.DESCRIPTION
    For demos too large for a single boot sector. `disk create --boot` writes a
    small loader into track 0 sector 0 and lays the payload down from track 1,
    one sector per memory page, then jumps to it. No DOS, no filesystem, and no
    hand-written loader -- two commands, assemble and create.

    The source must `.org` at the load address and put its entry point in the
    first emitted byte.

    Boot the result with:
        Casso.exe --machine Apple2e --disk1 <output.dsk>
    (--disk1 is required; a positional path is silently ignored.)

.PARAMETER Source
    The .a65 source under Apple2/Demos. Default: mockingboard-speech-demo.a65.

.PARAMETER LoadAddress
    Where the payload loads and is entered. Default: 0x6000, which is page
    aligned (so no lead-in padding) and leaves $C000 upward alone.

.PARAMETER Configuration
    Which CassoCli build to use. Default: Release.
#>
[CmdletBinding()]
param(
    [string]$Source        = 'mockingboard-speech-demo.a65',
    [string]$LoadAddress   = '0x6000',
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$demoDir  = Join-Path $repoRoot 'Apple2\Demos'
$cli      = Join-Path $repoRoot "x64\$Configuration\CassoCli.exe"

if (-not (Test-Path $cli))
{
    throw "CassoCli.exe not found at $cli. Run scripts/Build.ps1 first."
}

$srcPath = Join-Path $demoDir $Source
if (-not (Test-Path $srcPath))
{
    throw "Source not found: $srcPath"
}

$stem   = [IO.Path]::GetFileNameWithoutExtension($Source)
$outBin = Join-Path $demoDir "$stem.bin"
$outDsk = Join-Path $demoDir "$stem.dsk"

# `disk create` refuses to overwrite, so clear both artifacts first.
Remove-Item $outBin, $outDsk -ErrorAction SilentlyContinue

# CassoCli exits non-zero on warnings (e.g. an unused label) but still writes
# the binary, so check for the file rather than trusting the exit code.
& $cli as65 $srcPath -o $outBin -q | Out-Null
if (-not (Test-Path $outBin))
{
    throw "Assembly produced no output for $Source."
}

& $cli disk create $outDsk --boot $outBin --load $LoadAddress
if ($LASTEXITCODE -ne 0)
{
    throw "disk create failed ($LASTEXITCODE)."
}

$binSize = (Get-Item $outBin).Length
Write-Host "Wrote $outDsk ($binSize byte payload loading at $LoadAddress)"
