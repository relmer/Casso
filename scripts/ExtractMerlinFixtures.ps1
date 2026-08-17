<#
.SYNOPSIS
    Extracts the Merlin dialect test fixtures from the Merlin Pro disk into
    UnitTest/Fixtures/Merlin/.

.DESCRIPTION
    Reads vendor source files and their shipped object code out of the Merlin
    Pro 2.23 DOS 3.3 disk and writes them, byte for byte, as test fixtures.

    Each extracted pair is an independent oracle: assemble the source with
    Casso's Merlin dialect and the bytes must match the object Glen Bredon
    shipped in 1984. That is a stronger statement than any hand-written test,
    because the expected bytes were produced by the assembler being emulated,
    on code nobody here chose.

    Files are extracted VERBATIM -- the exact bytes DOS 3.3 stores, including
    the 4-byte BIN header, with the high-bit ASCII left as it sits on the disk.
    Two reasons. It keeps the fixtures provably identical to the vendor
    artifact, so any of them can be re-derived from the disk and hash-compared
    at any time; and verbatim copying is unambiguously what the disk's license
    permits, where a transcoded copy invites an argument about derivative
    works. Consumers decode; see UnitTest/Fixtures/Merlin/README.md.

    Run scripts/FetchMerlin.ps1 first to obtain the disk.

.PARAMETER Force
    Overwrite fixtures that already exist.

.PARAMETER Disk
    Path to the Merlin Pro disk image. Defaults to the location FetchMerlin.ps1
    writes.

.NOTES
    Exit codes: 0 = success, 1 = failure
#>
[CmdletBinding()]
param (
    [switch]$Force,
    [string]$Disk
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir

if (-not $Disk) {
    $Disk = Join-Path $repoRoot 'DevDisks\Merlin-proDos2.23.dsk'
}

$destDir = Join-Path $repoRoot 'UnitTest\Fixtures\Merlin'

# Must match the pin in FetchMerlin.ps1. Extracting from an unverified disk
# would launder unknown bytes into the test suite as an authoritative oracle,
# which is the one thing this fixture set cannot afford.
$kDiskSha256 = 'CB7FD9522A3B90792ACBB00D6C811323DC046DC2920FC05A640858BFE611F0E6'

#
#  What comes off the disk, and why.
#
#  Pairs are byte-identical oracles. CLOCK is one source producing two objects
#  through DO/ELSE/FIN, so it covers conditional assembly and yields two
#  independent checks.
#
#  The two PI files carry no object on purpose: they are the linker demo, and
#  the objects beside them on the disk are post-link, so comparing against
#  those would be meaningless. They are the subset-boundary specimens, and
#  there are two because the boundary has two sides. PI.START.S imports --
#  three EXT references -- which nothing can resolve without a linker.
#  PI.ADD.S only exports (ENT, no EXT), which a user can work around by
#  assembling absolutely. One specimen would exercise one refusal and leave
#  the other unexamined, and the two must not produce the same diagnostic:
#  offering the absolute-assembly workaround for a file that imports would be
#  advice that cannot work.
#
$fixtures = @(
    @{ Source = 'LABELS.S';       Objects = @('LABELS')               ; Role = 'Oracle -- 105x DCI, the string-encoding probe' },
    @{ Source = 'KEYMAC.S';       Objects = @('KEYMAC')               ; Role = 'Oracle' },
    @{ Source = 'PRINTFILER.S';   Objects = @('PRINTFILER')           ; Role = 'Oracle' },
    @{ Source = 'MAKE DUMP.S';    Objects = @('MAKE DUMP')            ; Role = 'Oracle' },
    @{ Source = 'CLOCK.S';        Objects = @('CLOCK.24','CLOCK.12')  ; Role = 'Oracle -- DO/ELSE/FIN, one source, two objects' },
    @{ Source = 'PI.START.S';     Objects = @()                       ; Role = 'Boundary -- imports (REL/ENT/EXT/USE), no workaround exists' },
    @{ Source = 'PI.ADD.S';       Objects = @()                       ; Role = 'Boundary -- exports only (REL/ENT/USE, no EXT), workaround exists' }
)

#
#  Macro libraries the boundary specimens include.
#
#  PI.START.S says `USE PI.MACS` and PI.ADD.S adds `PUT SENDMSG`, but the files
#  on disk are named T.PI.MACS and T.SENDMSG -- Merlin prefixes `T.` to a
#  PUT/USE operand to find the file. Without these committed, a refusal test
#  would be resolving an include that cannot be found, and would report a
#  missing-file diagnostic while appearing to prove something about REL/ENT/EXT.
#
#  They are type T, not type B: no 4-byte header, and the file ends at the
#  first $00 rather than at a declared length. That difference is the reason
#  they are listed separately rather than folded in above.
#
#  T.MACRO LIBRARY is the distribution's general-purpose macro library and is
#  here for a different reason from the other two: nothing on the disk includes
#  it, so its oracle is GENERATED rather than shipped. A source authored in this
#  project puts it and invokes its macros, that source is assembled under real
#  Merlin Pro, and the object it produced is the expectation -- an ordinary
#  source/object pair whose object simply did not exist in 1984.
#
#  It is the only evidence anywhere that the first-character conditional and
#  definition fall-through are ORDINARY Merlin rather than exotic; between them
#  those two constructs appear throughout it.
#
$textFixtures = @(
    @{ Name = 'T.PI.MACS';        Role = 'Include target of `USE PI.MACS`' },
    @{ Name = 'T.SENDMSG';        Role = 'Include target of `PUT SENDMSG`' },
    @{ Name = 'T.MACRO LIBRARY';  Role = 'The vendor macro library -- oracle half of a generated pair' }
)

function Get-SectorOffset([int]$Track, [int]$Sector) {
    return ((($Track * 16) + $Sector) * 256)
}

#
#  Reads one DOS 3.3 file by walking the catalog to its track/sector list and
#  then the list to its data sectors. Returns the whole sectors; the caller
#  truncates to the logical length, which for a BIN file lives in the header.
#
function Read-DosFile {
    param([byte[]]$Image, [string]$Name)

    $track = 17
    $sector = 15

    while ($true) {
        $catalog = Get-SectorOffset $track $sector

        for ($i = 0; $i -lt 7; $i++) {
            $entry = $catalog + 0x0B + ($i * 0x23)

            if ($Image[$entry] -eq 0 -or $Image[$entry] -eq 0xFF) { continue }

            $entryName = (-join (0..29 | ForEach-Object {
                [char]([int]$Image[$entry + 3 + $_] -band 0x7F)
            })).TrimEnd()

            if ($entryName -ne $Name) { continue }

            $bytes = New-Object System.Collections.Generic.List[byte]
            $listTrack = [int]$Image[$entry]
            $listSector = [int]$Image[$entry + 1]

            while ($listTrack -ne 0) {
                $list = Get-SectorOffset $listTrack $listSector

                for ($p = 0x0C; $p -lt 256; $p += 2) {
                    $dataTrack = [int]$Image[$list + $p]
                    $dataSector = [int]$Image[$list + $p + 1]

                    if ($dataTrack -eq 0) { continue }

                    $data = Get-SectorOffset $dataTrack $dataSector
                    for ($k = 0; $k -lt 256; $k++) { $bytes.Add($Image[$data + $k]) }
                }

                $listTrack = [int]$Image[$list + 1]
                $listSector = [int]$Image[$list + 2]
            }

            return , $bytes.ToArray()
        }

        $nextTrack = [int]$Image[$catalog + 1]
        $nextSector = [int]$Image[$catalog + 2]

        if ($nextTrack -eq 0) { break }

        $track = $nextTrack
        $sector = $nextSector
    }

    return $null
}

if (-not (Test-Path $Disk)) {
    Write-Host "Disk not found: $Disk" -ForegroundColor Red
    Write-Host "Run scripts/FetchMerlin.ps1 first." -ForegroundColor Red
    exit 1
}

$actualHash = (Get-FileHash $Disk -Algorithm SHA256).Hash

if ($actualHash -ne $kDiskSha256) {
    Write-Host "Disk hash mismatch -- refusing to extract." -ForegroundColor Red
    Write-Host "  expected $kDiskSha256" -ForegroundColor Red
    Write-Host "  got      $actualHash" -ForegroundColor Red
    exit 1
}

Write-Host "Disk verified: $(Split-Path -Leaf $Disk)" -ForegroundColor DarkGray

if (-not (Test-Path $destDir)) {
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}

$image = [System.IO.File]::ReadAllBytes($Disk)
$written = 0
$skipped = 0
$failed = 0
$inventory = @()

foreach ($fixture in $fixtures) {
    $names = @($fixture.Source) + $fixture.Objects

    foreach ($name in $names) {
        $destPath = Join-Path $destDir $name

        if ((Test-Path $destPath) -and -not $Force) {
            Write-Host "  SKIP  $name -- already extracted" -ForegroundColor DarkGray
            $skipped++
            $bytes = [System.IO.File]::ReadAllBytes($destPath)
        }
        else {
            $raw = Read-DosFile -Image $image -Name $name

            if (-not $raw) {
                Write-Host "  FAIL  $name -- not found in catalog" -ForegroundColor Red
                $failed++
                continue
            }

            # BIN files carry load address and length in the first four bytes.
            # Everything on this disk that we extract is type B, so the logical
            # length is always header + declared length; the rest is sector
            # padding that was never part of the file.
            $declared = [int]$raw[2] + ([int]$raw[3] * 256)
            $total = $declared + 4

            if ($total -gt $raw.Length) {
                Write-Host "  FAIL  $name -- declared length $declared exceeds $($raw.Length) bytes read" -ForegroundColor Red
                $failed++
                continue
            }

            $bytes = New-Object byte[] $total
            [Array]::Copy($raw, 0, $bytes, 0, $total)
            [System.IO.File]::WriteAllBytes($destPath, $bytes)

            Write-Host "  OK    $name ($total bytes)" -ForegroundColor Green
            $written++
        }

        $load = [int]$bytes[0] + ([int]$bytes[1] * 256)
        $length = [int]$bytes[2] + ([int]$bytes[3] * 256)

        $inventory += [pscustomobject]@{
            Name   = $name
            Bytes  = $bytes.Length
            Load   = '${0:X4}' -f $load
            Length = $length
            Sha256 = (Get-FileHash $destPath -Algorithm SHA256).Hash
        }
    }
}

foreach ($textFixture in $textFixtures) {
    $name = $textFixture.Name
    $destPath = Join-Path $destDir $name

    if ((Test-Path $destPath) -and -not $Force) {
        Write-Host "  SKIP  $name -- already extracted" -ForegroundColor DarkGray
        $skipped++
        $bytes = [System.IO.File]::ReadAllBytes($destPath)
    }
    else {
        $raw = Read-DosFile -Image $image -Name $name

        if (-not $raw) {
            Write-Host "  FAIL  $name -- not found in catalog" -ForegroundColor Red
            $failed++
            continue
        }

        # A DOS 3.3 text file has no header and no stored length. It ends at
        # the first $00, and everything after that is sector padding.
        $end = [Array]::IndexOf($raw, [byte]0)

        if ($end -lt 0) { $end = $raw.Length }

        $bytes = New-Object byte[] $end
        [Array]::Copy($raw, 0, $bytes, 0, $end)
        [System.IO.File]::WriteAllBytes($destPath, $bytes)

        Write-Host "  OK    $name ($end bytes, type T)" -ForegroundColor Green
        $written++
    }

    $inventory += [pscustomobject]@{
        Name   = $name
        Bytes  = $bytes.Length
        Load   = 'n/a'
        Length = $bytes.Length
        Sha256 = (Get-FileHash $destPath -Algorithm SHA256).Hash
    }
}

Write-Host ""
Write-Host "Written: $written  Skipped: $skipped  Failed: $failed"

if ($failed -gt 0) {
    Write-Host "Extraction incomplete." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Inventory (paste into UnitTest/Fixtures/Merlin/README.md):" -ForegroundColor Cyan
foreach ($row in $inventory) {
    "| ``{0}`` | {1} | {2} | {3} | ``{4}`` |" -f `
        $row.Name, $row.Bytes, $row.Load, $row.Length, $row.Sha256.Substring(0, 16)
}

Write-Host ""
Write-Host "Fixtures in: $destDir" -ForegroundColor Green
Write-Host "Vendor material -- see that directory's README.md for license terms." -ForegroundColor DarkYellow
