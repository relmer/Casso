<#
.SYNOPSIS
    Builds the casso-rocks demo disk from the sources in this folder.

.DESCRIPTION
    A hand-laid bootable Apple //e disk: a boot sector, a second stage, and the
    same cassowary photograph shipped four ways, in HGR and double hi-res, each
    in color and in monochrome. Nothing on the disk can tell which monitor is
    attached, so all four are carried and the demo cycles them.

    The finished .dsk goes up to Apple2\Demos, which is where every machine
    profile, document and launch command already points. The sources, the image
    assets and the five scripts that generate those assets stay here.

    THE IMAGE ASSETS ARE NOT REBUILT BY THIS SCRIPT, and that is deliberate.
    DhgrCassowaryGen.py and HgrCassowaryGen.py read a 4000-pixel photograph out
    of Assets\ and spend real time on dithering; their output is committed
    because it is an authored artifact, not a derived one. Run them by hand
    after changing the photograph or the layout:

        python Apple2\Demos\CassoRocks\HgrCassowaryGen.py
        python Apple2\Demos\CassoRocks\DhgrCassowaryGen.py

.PARAMETER Force
    Accepted so this folder answers the same call as the other demo folders.
    The disk builder has no incremental path of its own, so a build is always
    a full one and this switch changes nothing.

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

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path

& (Join-Path $repoRoot 'scripts\BuildDemoDisk.ps1') -Configuration $Configuration
