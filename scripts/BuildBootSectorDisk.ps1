<#
.SYNOPSIS
    Builds a single-boot-sector .dsk from one .a65 source.

.DESCRIPTION
    The small hardware smoke tests under Apple2/Demos (mockingboard-test,
    mockingboard-irq-test, mockingboard-speech-test) are one-boot-sector
    programs: the slot-6 Disk2.rom firmware reads track 0 sector 0 into
    $0800-$08FF and JMPs $0801, and the program never touches the disk
    again. This script assembles such a source with CassoCli and lays out
    the standard 143360-byte DOS-order .dsk image with the code in the
    boot sector and $00 fill everywhere else.

    Boot it with:  Casso.exe --disk1 <output.dsk>

.PARAMETER Source
    The .a65 source under Apple2/Demos. Default: mockingboard-speech-test.a65.

.PARAMETER Configuration
    Which CassoCli build to use. Default: Debug.
#>
[CmdletBinding()]
param(
    [string]$Source = 'mockingboard-speech-test.a65',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$demoDir  = Join-Path $repoRoot 'Apple2\Demos'
$cli      = Join-Path $repoRoot "x64\$Configuration\CassoCli.exe"

if (-not (Test-Path $cli)) {
    throw "CassoCli.exe not found at $cli. Run scripts/Build.ps1 first."
}

$kBytesPerSector = 256
$kImageSize      = 143360     # 35 tracks x 16 sectors x 256 bytes
$kBootOrg        = 0x0800     # boot ROM loads the sector here; .a65 .org $0801

$srcPath = Join-Path $demoDir $Source
if (-not (Test-Path $srcPath)) {
    throw "Source not found: $srcPath"
}

$outDsk = [System.IO.Path]::ChangeExtension($srcPath, '.dsk')
$outBin = [System.IO.Path]::ChangeExtension($srcPath, '.bin')

# CassoCli exits non-zero on warnings (e.g. unused labels) but still writes
# the .bin. Pre-delete so genuine failure (no .bin) is distinguishable.
if (Test-Path $outBin) { Remove-Item $outBin }

# --flat asks for the full 64 KB padded image. The default output is the
# unpadded span from the lowest address the source used to the highest, which
# for these sources is about 64 bytes starting at $0801 -- indexing it at
# $0800 below would read past the end.
& $cli as65 $srcPath -o $outBin -q -z --flat | Out-Null

if (-not (Test-Path $outBin)) {
    throw "Assembly failed: $srcPath (no output produced)"
}

$full = [System.IO.File]::ReadAllBytes($outBin)
if ($full.Length -lt ($kBootOrg + $kBytesPerSector)) {
    throw "$outBin too short ($($full.Length) bytes) for a boot sector at `$0800."
}

# Logical sector 0 is physical sector 0, so the boot sector is simply the
# first 256 bytes of the image; the rest is $00 fill.
$image = New-Object byte[] $kImageSize
[Array]::Copy($full, $kBootOrg, $image, 0, $kBytesPerSector)

[System.IO.File]::WriteAllBytes($outDsk, $image)
Remove-Item $outBin

Write-Host "Wrote $outDsk" -ForegroundColor Green
