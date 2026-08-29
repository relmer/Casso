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
           Tracks 1-2         ($1000-$2FFF) : DHGR mono aux half
           Track 3, sectors 0-2 ($3000-$32FF) : stage 2
           Track 3, sectors 3-15                : unused
           Tracks 4-5         ($4000-$5FFF) : DHGR mono main half
           Tracks 6-7         ($6000-$7FFF) : HGR mono cassowary
           Tracks 8-9         ($8000-$9FFF) : DHGR color aux half
           Tracks 10-11       ($A000-$BFFF) : DHGR color main half
           Tracks 12-13       ($C000-$DFFF) : HGR color cassowary
           Everything else                  : $FF fill

       The four cassowaries are one photo encoded four ways. Both DHGR
       and HGR framebuffers are decoded differently depending on the
       monitor -- color cells versus dots -- and the two decodes want
       opposite things from the encoder, so each image is authored for
       one monitor and reads as noise on the other. Nothing on the disk
       can tell which is attached, so all four ship, monochrome pair
       first. See scripts/DhgrCassowaryGen.py and
       scripts/HgrCassowaryGen.py.

       The tracks are in the order the demo reads them, which is the
       order the modes are cycled in, so whatever the user reaches next
       is whatever finished loading last.

      4. Writes the assembled image to Apple2/Demos/casso-rocks.dsk

    Requires CassoCli.exe under x64/Debug or x64/Release. Run
    scripts/Build.ps1 first if it's missing.

.PARAMETER Configuration
    Which CassoCli build to use. Default: Debug.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    # Lay the image out in PowerShell, the way this script always did, rather
    # than with `CassoCli disk sectorwrite`. Kept as the second witness: the
    # CLI path exercises `disk create` and `disk sectorwrite` end to end,
    # the legacy path writes raw file offsets, and -Compare diffs the two.
    [switch]$LegacyLayout,

    # Build the image both ways and report whether they are identical.
    [switch]$Compare,

    #  Rebuild and COMPARE, writing nothing.
    #
    #  The drift check used to live in BootDiskTests, which read the committed
    #  image and failed when it did not match what the test had just built.
    #  That made a unit test report on the state of the working tree: it failed
    #  on a tree that was perfectly correct except that nobody had re-run this
    #  script. The question belongs here, where the disk is built, and CI can
    #  ask it without a test touching the file at all.
    [switch]$Verify
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
$kStage2Length    = $kBytesPerSector * 3   # stage 2 spans three sectors now
$kImageLength     = 0x2000   # each cassowary image asset is 8 KB = 2 tracks

# DOS 3.3 physical-to-file sector interleave, indexed by physical sector --
# the number in the address field the drive presents at that position, which
# is how the demo's own RWTS files what it reads. A .dsk holds its sectors in
# DOS logical order, so placing payload page S under address mark S means
# writing it at file offset (T * 16 + PhysicalToFile[S]) * 256. Only the
# LEGACY layout reads this copy; the default path says `sectorwrite
# --physical` and lets the engine's own table answer, which is what makes
# -Compare an independent witness of the same sixteen numbers.
$kDsk_PhysicalToFile = @(0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Get-PhysicalSectorOffset {
    param(
        [int]$Track,
        [int]$PhysicalSector
    )
    return ($Track * $kSectorsPerTrack + $kDsk_PhysicalToFile[$PhysicalSector]) * $kBytesPerSector
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

    # The dialect is named, because assembling no longer guesses: an
    # unrecognized first argument is refused rather than taken as a source file.
    #
    # --flat is REQUIRED and was not always. This script reads its regions
    # out of the output at their ORIGIN, so it needs the whole 64 KB address
    # space, not just the bytes the source filled. Spec 020 made the
    # assembled bytes the default and retired the flag that used to name
    # them, which left this script reading offset $0800 of a 253-byte file.
    & $cli as65 $SourcePath -o $outBin -q -z --flat | Out-Null

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

$dhgrAux  = Read-AssetFile 'dhgr-cassowary-aux.bin'       $kImageLength
$dhgrMain = Read-AssetFile 'dhgr-cassowary-main.bin'      $kImageLength
$monoAux  = Read-AssetFile 'dhgr-cassowary-mono-aux.bin'  $kImageLength
$monoMain = Read-AssetFile 'dhgr-cassowary-mono-main.bin' $kImageLength
$hgr      = Read-AssetFile 'cassowary.hgr'                $kImageLength
$hgrMono  = Read-AssetFile 'cassowary-mono.hgr'           $kImageLength

function Build-LayoutInPowerShell {
    #  The original method: allocate the image, place every region at a
    #  computed file offset, write it out.
    #
    #  KEPT AS AN INDEPENDENT WITNESS, not as a fallback. It shares no code
    #  with the sectorwrite path, so the two agreeing byte for byte is evidence
    #  rather than a tautology. Run -Compare to check them against each other.

    # $00-filled blank disk (matches the test fixture; nibblizer doesn't
    # care, but a zero fill keeps unused sectors clean).
    $image = New-Object byte[] $kImageSize

    # Track 0 logical sector 0: boot sector = stage 1 ($0800..$08FF)
    Write-Bytes-At $image (Get-PhysicalSectorOffset 0 0) $stage1

    # Tracks 1-2: DHGR mono aux half, stitched in logical-sector order
    for ($trackOff = 0; $trackOff -lt 2; $trackOff++) {
        for ($sector = 0; $sector -lt $kSectorsPerTrack; $sector++) {
            $fileOff    = Get-PhysicalSectorOffset (1 + $trackOff) $sector
            $payloadOff = ($trackOff * $kSectorsPerTrack + $sector) * $kBytesPerSector
            $slice      = New-Object byte[] $kBytesPerSector
            [Array]::Copy($monoAux, $payloadOff, $slice, 0, $kBytesPerSector)
            Write-Bytes-At $image $fileOff $slice
        }
    }

    # Track 3 logical sectors 0-2: stage 2 ($1000..$12FF). The sectors
    # are NOT adjacent in the file -- the interleave puts them a long way
    # apart -- so each is placed at its own offset.
    for ($sector = 0; $sector -lt 3; $sector++) {
        $slice = New-Object byte[] $kBytesPerSector
        [Array]::Copy($stage2, $sector * $kBytesPerSector, $slice, 0, $kBytesPerSector)
        Write-Bytes-At $image (Get-PhysicalSectorOffset 3 $sector) $slice
    }

    # Tracks 4-5: DHGR mono main half
    for ($trackOff = 0; $trackOff -lt 2; $trackOff++) {
        for ($sector = 0; $sector -lt $kSectorsPerTrack; $sector++) {
            $fileOff    = Get-PhysicalSectorOffset (4 + $trackOff) $sector
            $payloadOff = ($trackOff * $kSectorsPerTrack + $sector) * $kBytesPerSector
            $slice      = New-Object byte[] $kBytesPerSector
            [Array]::Copy($monoMain, $payloadOff, $slice, 0, $kBytesPerSector)
            Write-Bytes-At $image $fileOff $slice
        }
    }

    # Tracks 6-7: HGR mono cassowary, 8-9: DHGR color aux half,
    # 10-11: DHGR color main half, 12-13: HGR color cassowary. Same
    # two-track stitch each time, so the payloads drive the loop.
    $twoTrackRegions = @(
        @{ StartTrack =  6; Payload = $hgrMono  },
        @{ StartTrack =  8; Payload = $dhgrAux  },
        @{ StartTrack = 10; Payload = $dhgrMain },
        @{ StartTrack = 12; Payload = $hgr      }
    )

    foreach ($region in $twoTrackRegions) {
        for ($trackOff = 0; $trackOff -lt 2; $trackOff++) {
            for ($sector = 0; $sector -lt $kSectorsPerTrack; $sector++) {
                $fileOff    = Get-PhysicalSectorOffset ($region.StartTrack + $trackOff) $sector
                $payloadOff = ($trackOff * $kSectorsPerTrack + $sector) * $kBytesPerSector
                $slice      = New-Object byte[] $kBytesPerSector
                [Array]::Copy($region.Payload, $payloadOff, $slice, 0, $kBytesPerSector)
                Write-Bytes-At $image $fileOff $slice
            }
        }
    }

    #  BUILDS AND RETURNS, NEVER WRITES. It used to write the image here as
    #  well, which made -Verify compare the file against what it had just
    #  put there: it reported a match on a disk with a whole track zeroed.
    #  Every caller that wants the bytes on disk writes them itself.
    return ,$image
}


function Build-LayoutWithCassoCli {
    #  The same layout as eight `disk sectorwrite --physical` calls.
    #
    #  --physical says exactly what the demo needs said: its RWTS files each
    #  sector by address-mark number, so page N of every region must sit
    #  under mark N, and a physical run-on advances mark by mark. The
    #  interleave stays in the layer that owns it -- this path never touches
    #  the sixteen numbers, which is what makes -Compare an independent
    #  witness against the legacy layout that spells them out.

    $dsk = Join-Path $demoDir "casso-rocks.dsk"

    #  An unformatted image, because this disk has no filesystem: it boots
    #  its own loader, which reads fixed tracks. --format none is that.
    if (Test-Path $dsk) { Remove-Item $dsk -Force }

    & $cli disk create $dsk --type dsk --format none | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "disk create failed ($LASTEXITCODE)" }

    #  Each stage goes to a scratch file first: sectorwrite takes a file, and
    #  the assembled regions are in memory at this point.
    $tmp1 = Join-Path $demoDir "stage1.tmp"
    $tmp2 = Join-Path $demoDir "stage2.tmp"

    [System.IO.File]::WriteAllBytes($tmp1, $stage1)
    [System.IO.File]::WriteAllBytes($tmp2, $stage2)

    #  Track, physical sector, and what goes there: the layout the demo
    #  documents, in the order its boot loader reads it.
    $plan = @(
        @{ Track = 0; Sector = 0; Path = $tmp1 },
        @{ Track = 1; Sector = 0; Path = (Join-Path $demoDir "dhgr-cassowary-mono-aux.bin") },
        @{ Track = 3; Sector = 0; Path = $tmp2 },
        @{ Track = 4; Sector = 0; Path = (Join-Path $demoDir "dhgr-cassowary-mono-main.bin") },
        @{ Track = 6; Sector = 0; Path = (Join-Path $demoDir "cassowary-mono.hgr") },
        @{ Track = 8; Sector = 0; Path = (Join-Path $demoDir "dhgr-cassowary-aux.bin") },
        @{ Track = 10; Sector = 0; Path = (Join-Path $demoDir "dhgr-cassowary-main.bin") },
        @{ Track = 12; Sector = 0; Path = (Join-Path $demoDir "cassowary.hgr") }
    )

    foreach ($step in $plan) {
        & $cli disk sectorwrite $dsk $step.Path --physical --track $step.Track --sector $step.Sector | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "disk sectorwrite failed: $($step.Path) at track $($step.Track) physical sector $($step.Sector)"
        }
    }

    Remove-Item $tmp1 -Force
    Remove-Item $tmp2 -Force

    return ,([System.IO.File]::ReadAllBytes($dsk))
}


Write-Host "Laying out .dsk image..." -ForegroundColor Cyan

$dskPath = Join-Path $demoDir "casso-rocks.dsk"

if ($Verify) {
    #  Built in memory and compared. The PowerShell layout is used because it
    #  is the one that writes nothing: the CassoCli path lays the image down
    #  with `disk create` and `disk sectorwrite`, which is a write by construction.
    $expected = Build-LayoutInPowerShell

    if (-not (Test-Path $dskPath)) {
        throw "$dskPath is missing. Run scripts/BuildDemoDisk.ps1 to build it."
    }

    $actual = [System.IO.File]::ReadAllBytes($dskPath)
    $same   = ($expected.Length -eq $actual.Length)
    $firstDifference = -1

    if ($same) {
        for ($i = 0; $i -lt $expected.Length; $i++) {
            if ($expected[$i] -ne $actual[$i]) {
                $same = $false
                $firstDifference = $i
                break
            }
        }
    }

    if (-not $same) {
        Write-Host ''
        Write-Host "casso-rocks.dsk is not what the sources build." -ForegroundColor Red

        if ($firstDifference -ge 0) {
            $track  = [int][Math]::Floor($firstDifference / 4096)
            $sector = [int][Math]::Floor(($firstDifference % 4096) / 256)
            Write-Host ("  first difference at byte {0}: track {1}, sector {2}" -f `
                        $firstDifference, $track, $sector) -ForegroundColor Yellow
        }
        else {
            Write-Host ("  committed {0} bytes, sources build {1}" -f `
                        $actual.Length, $expected.Length) -ForegroundColor Yellow
        }

        Write-Host '  Run scripts/BuildDemoDisk.ps1 to rebuild it.' -ForegroundColor Yellow
        Write-Host ''
        exit 1
    }

    Write-Host "casso-rocks.dsk matches what the sources build." -ForegroundColor Green
    exit 0
}

if ($Compare) {
    #  Both methods, and whether they agree. Checking them against each
    #  other is the reason the old one is still here.
    $viaCli   = Build-LayoutWithCassoCli
    $viaShell = Build-LayoutInPowerShell

    [System.IO.File]::WriteAllBytes($dskPath, $viaCli)

    $same = ($viaCli.Length -eq $viaShell.Length)

    if ($same) {
        for ($i = 0; $i -lt $viaCli.Length; $i++) {
            if ($viaCli[$i] -ne $viaShell[$i]) { $same = $false; break }
        }
    }

    if (-not $same) {
        throw "The two layout methods disagree. One of them has the sector skew wrong."
    }

    Write-Host "Both methods agree, byte for byte." -ForegroundColor Green
}
elseif ($LegacyLayout) {
    $image = Build-LayoutInPowerShell
    [System.IO.File]::WriteAllBytes($dskPath, $image)
}
else {
    Build-LayoutWithCassoCli | Out-Null
}

$image = [System.IO.File]::ReadAllBytes($dskPath)

Write-Host "Wrote $dskPath ($($image.Length) bytes)" -ForegroundColor Green
