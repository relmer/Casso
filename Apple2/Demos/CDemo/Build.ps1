<#
.SYNOPSIS
    Builds cdemo.dsk and cdemo.dbg.

.DESCRIPTION
    A direct-boot disk for trying the debugger's source-level debugging of a
    cc65 C program. cdemo.c and bar.c are compiled with cc65 -g and linked
    with boot.s at $6000 by cdemo.cfg, which writes cdemo.dbg beside the
    sources. The debug file has the paths of the sources relative to itself,
    so they stay in this folder together. The disk goes up to Apple2\Demos, like every
    other demo disk.

    The compiler, assembler and linker run one at a time rather than through
    cl65, so the generated assembly files appear in the debug file as cdemo.s
    and bar.s rather than as cl65's temporary files. The generated files are
    deleted afterward.

.PARAMETER Cc65Home
    The cc65 folder, with bin\cc65.exe and lib\apple2enh.lib in it. Default:
    $env:CC65_HOME when it is set, otherwise C:\Users\relmer\Tools\cc65.

.PARAMETER Force
    Rebuild even when the .dsk is newer than the sources.

.PARAMETER Configuration
    Which CassoCli build to use. Default: Release.
#>
[CmdletBinding()]
param(
    [string]$Cc65Home = $(if ($env:CC65_HOME) { $env:CC65_HOME } else { 'C:\Users\relmer\Tools\cc65' }),
    [switch]$Force,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$here     = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $here '..\..\..')).Path
$demoDir  = Join-Path $repoRoot 'Apple2\Demos'
$cli      = Join-Path $repoRoot "x64\$Configuration\CassoCli.exe"
$cc65     = Join-Path $Cc65Home 'bin\cc65.exe'
$ca65     = Join-Path $Cc65Home 'bin\ca65.exe'
$ld65     = Join-Path $Cc65Home 'bin\ld65.exe'
$runtime  = Join-Path $Cc65Home 'lib\apple2enh.lib'
$cSources = @('cdemo.c', 'bar.c')
$inputs   = @('cdemo.c', 'bar.c', 'bar.h', 'boot.s', 'cdemo.cfg') | ForEach-Object { Join-Path $here $_ }
$outBin   = Join-Path $here 'cdemo.bin'
$outDbg   = Join-Path $here 'cdemo.dbg'
$outDsk   = Join-Path $demoDir 'cdemo.dsk'

foreach ($tool in @($cli, $cc65, $ca65, $ld65, $runtime))
{
    if (-not (Test-Path $tool))
    {
        throw "Not found: $tool. Run scripts/Build.ps1 for CassoCli, or pass -Cc65Home for cc65."
    }
}

$newest = ($inputs | ForEach-Object { (Get-Item $_).LastWriteTimeUtc } | Measure-Object -Maximum).Maximum

if (-not $Force -and (Test-Path $outDsk) -and (Test-Path $outDbg) -and
    $newest -le (Get-Item $outDsk).LastWriteTimeUtc)
{
    Write-Host "  up to date: cdemo.dsk"
    return
}

#  `disk create` does not overwrite an image, so clear the artifacts first.
Remove-Item $outBin, $outDbg, $outDsk -ErrorAction SilentlyContinue

$generated = @('boot.o') + ($cSources | ForEach-Object { [IO.Path]::ChangeExtension($_, '.s'), [IO.Path]::ChangeExtension($_, '.o') })

#  Relative paths on the command lines become relative paths in cdemo.dbg,
#  so the sources resolve wherever the repository is.
Push-Location $here

try
{
    foreach ($source in $cSources)
    {
        $asm = [IO.Path]::ChangeExtension($source, '.s')

        & $cc65 -t apple2enh -g -o $asm $source
        if ($LASTEXITCODE -ne 0) { throw "cc65 failed on $source ($LASTEXITCODE)." }

        & $ca65 -t apple2enh -g -o ([IO.Path]::ChangeExtension($source, '.o')) $asm
        if ($LASTEXITCODE -ne 0) { throw "ca65 failed on $asm ($LASTEXITCODE)." }
    }

    & $ca65 -t apple2enh -g -o boot.o boot.s
    if ($LASTEXITCODE -ne 0) { throw "ca65 failed on boot.s ($LASTEXITCODE)." }

    & $ld65 -C cdemo.cfg --dbgfile cdemo.dbg -o cdemo.bin boot.o cdemo.o bar.o $runtime
    if ($LASTEXITCODE -ne 0) { throw "ld65 failed ($LASTEXITCODE)." }
}
finally
{
    Remove-Item $generated -ErrorAction SilentlyContinue
    Pop-Location
}

& $cli disk create $outDsk --boot $outBin --load 0x6000
if ($LASTEXITCODE -ne 0)
{
    throw "disk create failed ($LASTEXITCODE)."
}

Remove-Item $outBin
Write-Host "Wrote $outDsk and $outDbg"
