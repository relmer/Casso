<#
.SYNOPSIS
    Builds every disk in this folder.

.DESCRIPTION
    Two smoke tests for the Mockingboard C's sound side: the AY-3-8910 tone
    generators, and the 6522 timer interrupts that drive them. Both are single
    boot sectors -- the drive ROM reads track 0 sector 0 to $0800 and jumps to
    $0801, and neither program touches the disk again -- so there is no loader
    and nothing to generate. The third disk, mockingboard-music, is a tune for
    both AY chips. It is too large for a boot sector, so it is built with
    BuildDirectBootDisk.ps1, which adds the loader.

    The finished .dsk images go up to Apple2\Demos, which is where every
    machine profile and launch command names them.

    Each build is skipped when the .dsk is already newer than its source.
    Pass -Force to rebuild regardless.

.PARAMETER Force
    Rebuild even when the source has not changed.

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
$repoRoot = (Resolve-Path (Join-Path $here '..\..\..\..')).Path
$demoDir  = Join-Path $repoRoot 'Apple2\Demos'
$rel      = 'Mockingboard/Audio'

#  source and the script that builds it: a single boot sector, or a direct-boot
#  payload loaded from track 1 on (the music disk is too large for one sector)
$disks = @(
    @{ Src = 'mockingboard-test.a65';     Script = 'BuildBootSectorDisk.ps1' },
    @{ Src = 'mockingboard-irq-test.a65'; Script = 'BuildBootSectorDisk.ps1' },
    @{ Src = 'mockingboard-music.a65';    Script = 'BuildDirectBootDisk.ps1' }
)

foreach ($d in $disks)
{
    $src    = $d.Src
    $stem   = [IO.Path]::GetFileNameWithoutExtension($src)
    $target = Join-Path $demoDir "$stem.dsk"
    $source = Join-Path $here $src

    if (-not $Force -and (Test-Path $target) -and
        (Get-Item $source).LastWriteTimeUtc -le (Get-Item $target).LastWriteTimeUtc)
    {
        Write-Host "  up to date: $stem.dsk"
        continue
    }

    & (Join-Path $repoRoot "scripts\$($d.Script)") `
        -Source "$rel/$src" -Configuration $Configuration
}
