<#
.SYNOPSIS
    Downloads the Merlin Pro macro assembler disk and manual for assembler
    dialect validation.

.DESCRIPTION
    Fetches the Merlin Pro distribution disk and its manual into `DevDisks/`
    at the repo root. Merlin is the reference assembler the Merlin dialect is
    validated against: its own sample sources are assembled under Casso and
    the resulting object code is compared byte-for-byte against the objects
    the vendor shipped in 1984.

    The disk carries roughly ten matched source/object pairs (LABELS.S with
    LABELS, KEYMAC.S with KEYMAC, the Apple PI project, and so on), each of
    which is an independent end-to-end oracle: assemble the source, compare
    against the shipped object. A mismatch points at a specific dialect
    construct.

    Files downloaded, all into DevDisks/:
      - Merlin-proDos2.23.dsk         (140 KB) the assembler, DOS 3.3 order
      - Merlin Pro Manual_djvu.txt    (244 KB) the manual, OCR text
      - MerlinProExtras_djvu.txt      ( 24 KB) supplementary documentation
      - Merlin-proDos2.23Catalog.txt  (  2 KB) catalog listing

    Source: https://archive.org/details/MerlinProMacroAssembler
    Author: Glen Bredon. Published by Roger Wagner Publishing, 1984.
    License: CC BY-NC-ND 3.0 -- http://creativecommons.org/licenses/by-nc-nd/3.0/

    The license permits verbatim non-commercial redistribution with
    attribution, so these files may be committed as test data. This script
    exists either way: it is how a clone that does not carry them gets them,
    and how one that does confirms the bytes are unmodified.

    The ProDOS variants on the same archive item (2.33-a / 2.33-b) are not
    fetched: Casso's extraction tooling reads DOS 3.3 order, and the DOS 3.3
    build carries the same assembler.

.PARAMETER Force
    If set, re-downloads files even if they already exist.

.NOTES
    Exit codes: 0 = success, 1 = failure
#>
[CmdletBinding()]
param (
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir

$baseUrl = 'https://archive.org/download/MerlinProMacroAssembler'
$destDir = Join-Path $repoRoot 'DevDisks'

#
#  Hashes are pinned, not merely sizes.
#
#  These files are an oracle: object code assembled today is declared correct
#  because it matches bytes the vendor shipped in 1984. That argument only
#  holds if the bytes really are the vendor's. A size check catches a
#  truncated transfer and nothing else, and "the reference changed out from
#  under us" is the one failure that would read as a dialect bug and send
#  someone hunting through the assembler.
#
$assets = @(
    @{ Name = 'Merlin-proDos2.23.dsk';        Size = 143360; Dos33 = $true;  Desc = 'Merlin Pro 2.23 disk (DOS 3.3)';
       Sha256 = 'CB7FD9522A3B90792ACBB00D6C811323DC046DC2920FC05A640858BFE611F0E6' },
    @{ Name = 'Merlin Pro Manual_djvu.txt';   Size = 250249; Dos33 = $false; Desc = 'Merlin Pro manual (text)';
       Sha256 = 'B0D87F506CA477C76B2CCAC272CD2E199AE0605FD7A50F8CDA68845B4EC8AF8D' },
    @{ Name = 'MerlinProExtras_djvu.txt';     Size = 24960;  Dos33 = $false; Desc = 'Merlin Pro extras (text)';
       Sha256 = '82A12747D0AE6B152F4423CC779F5E11716DBC0BD1A4483D3B75DAA4B11449EA' },
    @{ Name = 'Merlin-proDos2.23Catalog.txt'; Size = 2445;   Dos33 = $false; Desc = 'Disk catalog listing';
       Sha256 = '2BE9A70FB401052D831AEEFDFC68C475B923744D41BDCB0F7EC50F5E3927AAD1' }
)

#
#  Confirms a downloaded image really is a DOS 3.3 volume by reading the VTOC
#  at track 17 sector 0.
#
#  Size alone does not establish this -- any 140 KB file passes that test --
#  and a disk that downloads cleanly but is not what it claims would surface
#  much later as a confusing extraction failure. Five fields is enough:
#  the catalog pointer, the DOS release, and the geometry.
#
function Test-Dos33Volume {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $vtoc  = (17 * 16) * 256

    if ($bytes.Length -lt ($vtoc + 0x40)) { return $false }

    return ($bytes[$vtoc + 0x01] -eq 17) -and    # catalog track
           ($bytes[$vtoc + 0x02] -eq 15) -and    # catalog sector
           ($bytes[$vtoc + 0x03] -eq 3)  -and    # DOS release
           ($bytes[$vtoc + 0x34] -eq 35) -and    # tracks
           ($bytes[$vtoc + 0x35] -eq 16)         # sectors per track
}

#
#  Returns a human-readable reason the file on disk is not the expected asset,
#  or $null if it checks out. Used for both the already-present case and the
#  just-downloaded case so the two cannot drift apart.
#
function Get-AssetProblem {
    param([string]$Path, [hashtable]$Asset)

    $size = (Get-Item $Path).Length
    if ($size -ne $Asset.Size) {
        return "wrong size ($size bytes, expected $($Asset.Size))"
    }

    $hash = (Get-FileHash $Path -Algorithm SHA256).Hash
    if ($hash -ne $Asset.Sha256) {
        return "hash mismatch (got $hash)"
    }

    if ($Asset.Dos33 -and -not (Test-Dos33Volume -Path $Path)) {
        return 'not a DOS 3.3 volume (VTOC check failed)'
    }

    return $null
}

if (-not (Test-Path $destDir)) {
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}

$downloaded = 0
$skipped    = 0
$failed     = 0

foreach ($asset in $assets) {
    $destPath = Join-Path $destDir $asset.Name
    $url      = "$baseUrl/" + [uri]::EscapeDataString($asset.Name)

    if ((Test-Path $destPath) -and -not $Force) {
        $problem = Get-AssetProblem -Path $destPath -Asset $asset

        if (-not $problem) {
            Write-Host "  SKIP  $($asset.Name) ($($asset.Desc)) -- present and verified" -ForegroundColor DarkGray
            $skipped++
            continue
        }

        Write-Host "  BAD   $($asset.Name) -- $problem, re-downloading" -ForegroundColor Yellow
    }

    Write-Host "  GET   $($asset.Name) ($($asset.Desc))..." -NoNewline

    try {
        Invoke-WebRequest -Uri $url -OutFile $destPath -MaximumRedirection 10 -UseBasicParsing

        $problem = Get-AssetProblem -Path $destPath -Asset $asset

        if ($problem) {
            #
            #  Delete rather than leave it. A half-right reference file that
            #  sits on disk looking downloaded is worse than none: the next
            #  run skips it, and every comparison made against it is quietly
            #  meaningless.
            #
            Write-Host " REJECTED -- $problem" -ForegroundColor Red
            Remove-Item $destPath -Force
            $failed++
            continue
        }

        Write-Host " OK ($($asset.Size) bytes, hash verified)" -ForegroundColor Green
        $downloaded++
    }
    catch {
        Write-Host " FAILED: $_" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "Downloaded: $downloaded  Skipped: $skipped  Failed: $failed"

if ($failed -gt 0) {
    Write-Host "Some downloads failed. Dialect validation needs the disk present." -ForegroundColor Red
    exit 1
}

Write-Host "Placed under: $destDir" -ForegroundColor Green
Write-Host "Merlin Pro (c) 1984 Glen Bredon / Roger Wagner Publishing. CC BY-NC-ND 3.0." -ForegroundColor DarkYellow
Write-Host "Work on a COPY: the disk is the oracle, and capture writes to it." -ForegroundColor DarkYellow
