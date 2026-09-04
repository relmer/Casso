<#
.SYNOPSIS
    Builds every disk in this folder.

.DESCRIPTION
    Two demos and three probes for the Mockingboard C's SSI 263A voice chip.
    The finished .dsk images go up to Apple2\Demos, which is where every
    machine profile and launch command names them; the sources, the generator
    and the shared engine stay here.

    THE GENERATOR IS THE STEP PEOPLE MISS. mockingboard-speech-demo-*.inc are
    not hand-written: gen-speech-demo-data.py emits the phoneme streams, the
    song, HAL's eye and the paint-cost constants, and it emits BOTH versions
    from one description so the speech cannot drift between them. Editing a
    line of speech means running it, which this script does for you.

    Each build is skipped when the .dsk is already newer than everything that
    feeds it. Pass -Force to rebuild regardless.

.PARAMETER Force
    Rebuild even when nothing that feeds a disk has changed.

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
$rel      = 'Mockingboard/Speech'

function Test-Stale
{
    param([string]$Target, [string[]]$Sources)

    if ($Force -or -not (Test-Path $Target)) { return $true }

    $built = (Get-Item $Target).LastWriteTimeUtc

    foreach ($s in $Sources)
    {
        if ((Test-Path $s) -and (Get-Item $s).LastWriteTimeUtc -gt $built) { return $true }
    }

    return $false
}

#  The generator writes both .inc files, so either being older than it is
#  reason enough to run it again.
$gen  = Join-Path $here 'gen-speech-demo-data.py'
$incs = @('mockingboard-speech-demo-hgr.inc', 'mockingboard-speech-demo-dhgr.inc') |
        ForEach-Object { Join-Path $here $_ }

if ($Force -or ($incs | Where-Object { Test-Stale $_ @($gen) }))
{
    Write-Host "Generating the data tables" -ForegroundColor Cyan
    Push-Location $here
    try { python $gen } finally { Pop-Location }
}

$engine = Join-Path $here 'mockingboard-speech-engine.inc'

#  source, the extra files it is built from, and which loader it needs
$disks = @(
    @{ Src = 'mockingboard-speech-demo-hgr.a65'
       Dep = @($engine, (Join-Path $here 'mockingboard-speech-demo-hgr.inc'))
       Boot = 'Direct' },
    @{ Src = 'mockingboard-speech-demo-dhgr.a65'
       Dep = @($engine, (Join-Path $here 'mockingboard-speech-demo-dhgr.inc'))
       Boot = 'Direct' },
    @{ Src = 'phoneme-probe.a65';          Dep = @(); Boot = 'Direct' },
    @{ Src = 'pulse-probe.a65';            Dep = @(); Boot = 'Direct' },
    @{ Src = 'mockingboard-speech-test.a65'; Dep = @(); Boot = 'Sector' }
)

foreach ($d in $disks)
{
    $stem   = [IO.Path]::GetFileNameWithoutExtension($d.Src)
    $target = Join-Path $demoDir "$stem.dsk"
    $srcs   = @((Join-Path $here $d.Src)) + $d.Dep

    if (-not (Test-Stale $target $srcs))
    {
        Write-Host "  up to date: $stem.dsk"
        continue
    }

    $script = if ($d.Boot -eq 'Direct') { 'BuildDirectBootDisk.ps1' } else { 'BuildBootSectorDisk.ps1' }

    & (Join-Path $repoRoot "scripts\$script") -Source "$rel/$($d.Src)" -Configuration $Configuration
}
