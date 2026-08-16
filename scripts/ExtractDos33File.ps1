<#
.SYNOPSIS
    Lists and extracts files from a flat DOS-order DOS 3.3 disk image.

.DESCRIPTION
    Throwaway corpus-capture tooling, NOT a product feature.

    This exists only because the Merlin 8 development disk happens to be a flat
    DOS-order image, where sector N of track T sits at a fixed offset and needs
    no nibble decoding. It is roughly the minimum code that can walk a VTOC,
    follow a catalog chain, and concatenate a file's data sectors.

    It deliberately does NOT duplicate `020-disk-file-access`, whose `disk get`
    is a tested C++ capability spanning every mountable format including WOZ.
    When that lands, DELETE THIS SCRIPT and capture through it instead.

    Supports the two file types Merlin source and object land as: BINARY, whose
    first four bytes are a DOS load address and length that are stripped, and
    TEXT, which is high-bit ASCII terminated by a NUL.

.PARAMETER Image
    Path to the .do / .dsk image. Must be 143,360 bytes (35 tracks x 16 sectors
    x 256 bytes).

.PARAMETER Name
    File to extract. Omit to list the catalog instead.

.PARAMETER OutFile
    Where to write the extracted payload. Omit to write beside the image.

.PARAMETER Raw
    Skip type-specific processing and emit the concatenated sectors verbatim,
    including the DOS BIN header. For diagnosing a mismatch.

.EXAMPLE
    ./scripts/ExtractDos33File.ps1 -Image Merlin8-v2.47.do
    Lists the catalog.

.EXAMPLE
    ./scripts/ExtractDos33File.ps1 -Image Merlin8-v2.47.do -Name PI.ADD
    Extracts PI.ADD, reporting its load address and length.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Image,

    [string]$Name = '',

    [string]$OutFile = '',

    [switch]$Raw
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$SectorSize      = 256
$SectorsPerTrack = 16
$TrackCount      = 35
$ExpectedSize    = $SectorSize * $SectorsPerTrack * $TrackCount

$VtocTrack       = 17
$VtocSector      = 0

# Catalog sector layout
$CatNextTrack    = 0x01
$CatNextSector   = 0x02
$CatFirstEntry   = 0x0B
$CatEntrySize    = 35
$CatEntryCount   = 7

# File descriptive entry layout, relative to the entry start
$FdeTsTrack      = 0x00
$FdeTsSector     = 0x01
$FdeTypeFlags    = 0x02
$FdeName         = 0x03
$FdeNameLength   = 30
$FdeSectorCount  = 0x21

# Track/sector list layout
$TslNextTrack    = 0x01
$TslNextSector   = 0x02
$TslFirstPair    = 0x0C
$TslPairCount    = 122

$FdeDeleted      = 0xFF
$FdeNeverUsed    = 0x00
$LockedFlag      = 0x80

# DOS BIN payloads open with a two-byte load address and a two-byte length.
$BinHeaderSize   = 4



function Get-Sector
{
    param([byte[]]$Bytes, [int]$Track, [int]$Sector)

    $offset = (($Track * $SectorsPerTrack) + $Sector) * $SectorSize

    if ($offset -lt 0 -or ($offset + $SectorSize) -gt $Bytes.Length)
    {
        throw "Sector T$Track/S$Sector is outside the image (offset $offset)."
    }

    # Two PowerShell hazards in one line. A range slice yields Object[], which
    # List[byte].AddRange refuses -- hence the cast. And `return` UNROLLS a
    # collection back into the pipeline, undoing that cast -- hence the leading
    # comma, which wraps the array so the single unroll step yields it intact.
    return ,[byte[]]$Bytes[$offset..($offset + $SectorSize - 1)]
}



function Get-FileTypeName
{
    param([int]$TypeFlags)

    # The high bit is the lock flag, not part of the type.
    $type = $TypeFlags -band 0x7F

    switch ($type)
    {
        0x00    { 'TEXT'    }
        0x01    { 'INTEGER' }
        0x02    { 'BASIC'   }
        0x04    { 'BINARY'  }
        0x08    { 'S'       }
        0x10    { 'RELOC'   }
        0x20    { 'A'       }
        0x40    { 'B'       }
        default { "?$('{0:X2}' -f $type)" }
    }
}



function Get-EntryName
{
    param([byte[]]$Bytes, [int]$Start)

    $chars = foreach ($i in 0..($FdeNameLength - 1))
    {
        # DOS 3.3 stores catalog names in high-bit ASCII.
        [char]($Bytes[$Start + $FdeName + $i] -band 0x7F)
    }

    return (-join $chars).TrimEnd()
}



function Get-Catalog
{
    param([byte[]]$Bytes)

    $entries = @()
    $vtoc    = Get-Sector -Bytes $Bytes -Track $VtocTrack -Sector $VtocSector
    $track   = $vtoc[$CatNextTrack]
    $sector  = $vtoc[$CatNextSector]
    $guard   = 0

    # A corrupt chain can loop; the catalog cannot exceed the sector count.
    $maxLinks = $SectorsPerTrack * $TrackCount

    while ($track -ne 0 -and $guard -lt $maxLinks)
    {
        $guard++
        $cat = Get-Sector -Bytes $Bytes -Track $track -Sector $sector

        foreach ($i in 0..($CatEntryCount - 1))
        {
            $start   = $CatFirstEntry + ($i * $CatEntrySize)
            $tsTrack = $cat[$start + $FdeTsTrack]

            if ($tsTrack -eq $FdeNeverUsed -or $tsTrack -eq $FdeDeleted)
            {
                continue
            }

            $flags = $cat[$start + $FdeTypeFlags]

            $entries += [pscustomobject]@{
                Name        = Get-EntryName -Bytes $cat -Start $start
                Type        = Get-FileTypeName -TypeFlags $flags
                Locked      = (($flags -band $LockedFlag) -ne 0)
                Sectors     = [int]$cat[$start + $FdeSectorCount] + ([int]$cat[$start + $FdeSectorCount + 1] * 256)
                TsListTrack = $tsTrack
                TsListSector= $cat[$start + $FdeTsSector]
                TypeFlags   = $flags
            }
        }

        $track  = $cat[$CatNextTrack]
        $sector = $cat[$CatNextSector]
    }

    return $entries
}



function Get-FileBytes
{
    param([byte[]]$Bytes, $Entry)

    $data   = [System.Collections.Generic.List[byte]]::new()
    $track  = $Entry.TsListTrack
    $sector = $Entry.TsListSector
    $guard  = 0

    $maxLinks = $SectorsPerTrack * $TrackCount

    while ($track -ne 0 -and $guard -lt $maxLinks)
    {
        $guard++
        $tsl = Get-Sector -Bytes $Bytes -Track $track -Sector $sector

        foreach ($i in 0..($TslPairCount - 1))
        {
            $pair    = $TslFirstPair + ($i * 2)
            $dTrack  = $tsl[$pair]
            $dSector = $tsl[$pair + 1]

            # A 0/0 pair is a hole in a random-access file, not an end marker --
            # but for the sequential files we capture it means no more data.
            if ($dTrack -eq 0 -and $dSector -eq 0)
            {
                continue
            }

            $data.AddRange((Get-Sector -Bytes $Bytes -Track $dTrack -Sector $dSector))
        }

        $track  = $tsl[$TslNextTrack]
        $sector = $tsl[$TslNextSector]
    }

    # Leading comma for the same reason as Get-Sector: keep it a byte[].
    return ,$data.ToArray()
}



$imagePath = (Resolve-Path -LiteralPath $Image).Path
$bytes     = [System.IO.File]::ReadAllBytes($imagePath)

if ($bytes.Length -ne $ExpectedSize)
{
    throw "Expected a $ExpectedSize-byte flat DOS-order image; '$imagePath' is $($bytes.Length) bytes. Nibble and ProDOS-order images are not supported -- that is what 020's disk get is for."
}

$catalog = Get-Catalog -Bytes $bytes

if (-not $Name)
{
    Write-Host "Catalog of $imagePath" -ForegroundColor Cyan
    $catalog | Format-Table -AutoSize @{ L = 'Name'; E = { $_.Name } },
                                      @{ L = 'Type'; E = { $_.Type } },
                                      @{ L = 'Lck';  E = { if ($_.Locked) { '*' } else { '' } } },
                                      @{ L = 'Sect'; E = { $_.Sectors } },
                                      @{ L = 'T/S';  E = { "$($_.TsListTrack)/$($_.TsListSector)" } }
    Write-Host "$($catalog.Count) file(s)." -ForegroundColor DarkGray
    return
}

$entry = $catalog | Where-Object { $_.Name -eq $Name }

if (-not $entry)
{
    $names = ($catalog | ForEach-Object { $_.Name }) -join ', '
    throw "No file named '$Name'. Catalog holds: $names"
}

$payload = Get-FileBytes -Bytes $bytes -Entry $entry
$loadAddress = $null
$declaredLen = $null

if (-not $Raw)
{
    if ($entry.Type -eq 'BINARY')
    {
        $loadAddress = [int]$payload[0] + ([int]$payload[1] * 256)
        $declaredLen = [int]$payload[2] + ([int]$payload[3] * 256)

        if (($BinHeaderSize + $declaredLen) -gt $payload.Length)
        {
            throw "BIN header declares $declaredLen bytes but only $($payload.Length - $BinHeaderSize) follow the header."
        }

        $payload = [byte[]]$payload[$BinHeaderSize..($BinHeaderSize + $declaredLen - 1)]
    }
    elseif ($entry.Type -eq 'TEXT')
    {
        # High-bit ASCII, NUL-terminated; trailing sector padding is not content.
        $end = [Array]::IndexOf($payload, [byte]0)

        if ($end -ge 0)
        {
            $payload = if ($end -eq 0) { [byte[]]@() } else { [byte[]]$payload[0..($end - 1)] }
        }

        $payload = [byte[]]($payload | ForEach-Object { $_ -band 0x7F })
    }
}

if (-not $OutFile)
{
    $OutFile = Join-Path (Split-Path -Parent $imagePath) ($Name -replace '[\\/:*?"<>|]', '_')
}

[System.IO.File]::WriteAllBytes($OutFile, [byte[]]$payload)

Write-Host "Extracted '$Name' ($($entry.Type))" -ForegroundColor Green

if ($null -ne $loadAddress)
{
    Write-Host ("  load address: `${0:X4}" -f $loadAddress) -ForegroundColor DarkGray
    Write-Host ("  declared length: {0} bytes" -f $declaredLen) -ForegroundColor DarkGray
}

Write-Host "  wrote $($payload.Length) bytes to $OutFile" -ForegroundColor DarkGray
