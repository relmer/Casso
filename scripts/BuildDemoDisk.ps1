<#
.SYNOPSIS
    Rebuilds the casso-rocks demo disk image from sources.

.DESCRIPTION
    The casso-rocks demo is a hand-crafted bootable Apple //e disk
    image with two stages of 6502 code plus image data laid out
    across specific tracks. This script:

      1. Assembles both stage .a65 files with CassoCli
      2. Extracts the populated code regions from CassoCli's 64KB
         output (CassoCli writes the whole address space FF-filled)
      3. Lays out the standard 143360-byte DOS-order .dsk image:

           Track 0, sector 0  ($0000-$00FF) : stage 1 (boot sector)
           Tracks 1-2         ($1000-$2FFF) : DHGR aux pattern
           Track 3, sector 0  ($3000-$30FF) : stage 2
           Track 3, sectors 1-4 ($3100-$34FF) : LoRes test pattern
           Tracks 4-5         ($4000-$5FFF) : DHGR main pattern
           Tracks 6-7         ($6000-$7FFF) : HGR1 cassowary
           Tracks 8-9         ($8000-$9FFF) : HGR2 test bands
           Everything else                  : $FF fill

      4. Writes the assembled image to Apple2/Demos/casso-rocks.dsk

    Requires CassoCli.exe under x64/Debug or x64/Release. Run
    scripts/Build.ps1 first if it's missing.

.PARAMETER Configuration
    Which CassoCli build to use. Default: Debug.
#>
[CmdletBinding()]
param(
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


# ---------------------------------------------------------------------------
# Constants for the .dsk layout. Anything that would otherwise be a
# magic number lives here so the layout stays inspectable.
# ---------------------------------------------------------------------------

$kBytesPerSector  = 256
$kSectorsPerTrack = 16
$kBytesPerTrack   = $kBytesPerSector * $kSectorsPerTrack
$kTrackCount      = 35
$kImageSize       = $kBytesPerTrack * $kTrackCount    # 143360
$kStage1Org       = 0x0800   # boot ROM loads boot sector here; .a65 .org $0801
$kStage2Org       = 0x1000   # stage 1 jmp $1000 after loading track 3
$kStage1Length    = $kBytesPerSector
$kStage2Length    = $kBytesPerSector
$kImageLength     = 0x2000   # each cassowary image asset is 8 KB = 2 tracks

# DOS 3.3 logical-to-physical sector interleave. Casso's nibblization
# layer expects .dsk files in PHYSICAL sector order; writing logical
# sector S of track T means stamping it at file offset
# (T * 16 + LtoP[S]) * 256.
$kDsk_LtoP = @(0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Get-LogicalSectorOffset {
    param(
        [int]$Track,
        [int]$LogicalSector
    )
    return ($Track * $kSectorsPerTrack + $kDsk_LtoP[$LogicalSector]) * $kBytesPerSector
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Get-AssembledRegion {
    param(
        [string]$SourcePath,
        [int]$Origin,
        [int]$Length
    )

    $outBin = [System.IO.Path]::ChangeExtension($SourcePath, '.bin')

    # CassoCli exits non-zero on warnings (e.g. unused labels) but still
    # writes the .bin. Pre-delete so we can tell genuine failure (no
    # .bin written) from warnings-only (.bin written, exit 1).
    if (Test-Path $outBin) { Remove-Item $outBin }

    & $cli $SourcePath -o $outBin -q -z | Out-Null

    if (-not (Test-Path $outBin)) {
        throw "Assembly failed: $SourcePath (no output produced)"
    }

    $full = [System.IO.File]::ReadAllBytes($outBin)
    if ($full.Length -lt ($Origin + $Length)) {
        throw "$outBin too short ($($full.Length) bytes); cannot read $Length from offset $Origin."
    }

    $region = New-Object byte[] $Length
    [Array]::Copy($full, $Origin, $region, 0, $Length)

    Remove-Item $outBin
    return ,$region
}


function Write-Bytes-At {
    param(
        [byte[]]$Destination,
        [int]$Offset,
        [byte[]]$Source
    )

    if (($Offset + $Source.Length) -gt $Destination.Length) {
        throw "Source ($($Source.Length)b @ $Offset) exceeds destination ($($Destination.Length)b)."
    }
    [Array]::Copy($Source, 0, $Destination, $Offset, $Source.Length)
}


function Read-AssetFile {
    param(
        [string]$Name,
        [int]$ExpectedLength
    )

    $path = Join-Path $demoDir $Name
    if (-not (Test-Path $path)) {
        throw "Asset not found: $path"
    }

    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ne $ExpectedLength) {
        throw "$Name is $($bytes.Length) bytes, expected $ExpectedLength."
    }

    return ,$bytes
}


# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

Write-Host "Assembling stages..." -ForegroundColor Cyan

$stage1 = Get-AssembledRegion `
    (Join-Path $demoDir 'casso-rocks.a65') $kStage1Org $kStage1Length

$stage2 = Get-AssembledRegion `
    (Join-Path $demoDir 'casso-rocks-stage2.a65') $kStage2Org $kStage2Length

Write-Host "Loading image assets..." -ForegroundColor Cyan

$dhgrAux  = Read-AssetFile 'dhgr-cassowary-aux.bin'  $kImageLength
$dhgrMain = Read-AssetFile 'dhgr-cassowary-main.bin' $kImageLength
$hgr      = Read-AssetFile 'cassowary.hgr'           $kImageLength
$bands    = Read-AssetFile 'test-bands.hgr'          $kImageLength
$lores    = Read-AssetFile 'lores-bars.lores'        ($kBytesPerSector * 4)

Write-Host "Laying out .dsk image..." -ForegroundColor Cyan

# $00-filled blank disk (matches the test fixture; nibblizer doesn't
# care, but a zero fill keeps unused sectors clean).
$image = New-Object byte[] $kImageSize

# Track 0 logical sector 0: boot sector = stage 1 ($0800..$08FF)
Write-Bytes-At $image (Get-LogicalSectorOffset 0 0) $stage1

# Tracks 1-2: DHGR aux pattern, stitched in logical-sector order
for ($trackOff = 0; $trackOff -lt 2; $trackOff++) {
    for ($sector = 0; $sector -lt $kSectorsPerTrack; $sector++) {
        $fileOff    = Get-LogicalSectorOffset (1 + $trackOff) $sector
        $payloadOff = ($trackOff * $kSectorsPerTrack + $sector) * $kBytesPerSector
        $slice      = New-Object byte[] $kBytesPerSector
        [Array]::Copy($dhgrAux, $payloadOff, $slice, 0, $kBytesPerSector)
        Write-Bytes-At $image $fileOff $slice
    }
}

# Track 3 logical sector 0: stage 2 ($1000..$10FF)
Write-Bytes-At $image (Get-LogicalSectorOffset 3 0) $stage2

# Track 3 logical sectors 1-4: LoRes pattern (4 sectors of 256 bytes)
for ($sector = 0; $sector -lt 4; $sector++) {
    $slice = New-Object byte[] $kBytesPerSector
    [Array]::Copy($lores, $sector * $kBytesPerSector, $slice, 0, $kBytesPerSector)
    Write-Bytes-At $image (Get-LogicalSectorOffset 3 (1 + $sector)) $slice
}

# Tracks 4-5: DHGR main pattern
for ($trackOff = 0; $trackOff -lt 2; $trackOff++) {
    for ($sector = 0; $sector -lt $kSectorsPerTrack; $sector++) {
        $fileOff    = Get-LogicalSectorOffset (4 + $trackOff) $sector
        $payloadOff = ($trackOff * $kSectorsPerTrack + $sector) * $kBytesPerSector
        $slice      = New-Object byte[] $kBytesPerSector
        [Array]::Copy($dhgrMain, $payloadOff, $slice, 0, $kBytesPerSector)
        Write-Bytes-At $image $fileOff $slice
    }
}

# Tracks 6-7: HGR1 cassowary
for ($trackOff = 0; $trackOff -lt 2; $trackOff++) {
    for ($sector = 0; $sector -lt $kSectorsPerTrack; $sector++) {
        $fileOff    = Get-LogicalSectorOffset (6 + $trackOff) $sector
        $payloadOff = ($trackOff * $kSectorsPerTrack + $sector) * $kBytesPerSector
        $slice      = New-Object byte[] $kBytesPerSector
        [Array]::Copy($hgr, $payloadOff, $slice, 0, $kBytesPerSector)
        Write-Bytes-At $image $fileOff $slice
    }
}

# Tracks 8-9: HGR2 test bands
for ($trackOff = 0; $trackOff -lt 2; $trackOff++) {
    for ($sector = 0; $sector -lt $kSectorsPerTrack; $sector++) {
        $fileOff    = Get-LogicalSectorOffset (8 + $trackOff) $sector
        $payloadOff = ($trackOff * $kSectorsPerTrack + $sector) * $kBytesPerSector
        $slice      = New-Object byte[] $kBytesPerSector
        [Array]::Copy($bands, $payloadOff, $slice, 0, $kBytesPerSector)
        Write-Bytes-At $image $fileOff $slice
    }
}

$dskPath = Join-Path $demoDir 'casso-rocks.dsk'
[System.IO.File]::WriteAllBytes($dskPath, $image)

Write-Host "Wrote $dskPath ($($image.Length) bytes)" -ForegroundColor Green
