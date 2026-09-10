<#
.SYNOPSIS
    Provisions the copyrighted machine ROMs Casso does not ship.

.DESCRIPTION
    Downloads each ROM from its upstream home and places it where Casso, or
    the unit suite, expects it. Nothing in this table is committed to the
    repository: the ROMs are Apple's, and the repository carries the
    emulator, the configuration that names each ROM, and this script.

    The catalog here mirrors s_kRomCatalog in CassoEmuCore/AssetBootstrap.cpp,
    which is what the running emulator uses to provision the same files on
    first launch. The two are kept in step by hand; a ROM added to one belongs
    in the other.

    Default layout is the product's, relative to the repo root:

      Machines/<MachineName>/<rom>     per-machine system + character ROMs
      Devices/DiskII/<rom>             shared device firmware

    -Fixtures writes the flat layout the unit suite reads instead:

      UnitTest/Fixtures/<rom>

    Every file is size-checked after download. A file already present at the
    right size is left alone unless -Force is given.

.PARAMETER Fixtures
    Write into UnitTest/Fixtures/ (flat) rather than the product layout.

.PARAMETER Verify
    Download nothing. Report which files are present at the right size and
    exit 1 if any is not. RunTests.ps1 runs this before launching vstest so
    a missing ROM fails the run with its name rather than letting the tests
    that need it go quietly untested.

.PARAMETER Force
    Re-download files that already exist.

.NOTES
    AppleWin's resource directory is pinned to a commit so a rename upstream
    cannot break a fresh clone or a CI run without someone choosing to take
    it. Bump the SHA deliberately.
#>

param (
    [switch]$Fixtures,
    [switch]$Verify,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir

#  AppleWin master as of 2026-09-09.
$appleWinRef = '3e8054b4627624398e4589f7f27b3d40a6b9718e'
$appleWinUrl = "https://raw.githubusercontent.com/AppleWin/AppleWin/$appleWinRef/resource"

#  AppleWin does not emulate the //c, so its 32K ROM 4 (memory-expansion //c,
#  chip 341-0445-B) comes from the Apple II Documentation Project mirror.
$apple2cRomUrl = 'https://mirrors.apple2.org.za/Apple%20II%20Documentation%20Project/Computers/Apple%20II/Apple%20IIc/ROM%20Images/Apple%20IIc%20ROM%2004%20-%20341-0445-B.bin'

#  One row per file Casso wants on disk. The same upstream file can appear
#  more than once under different destinations: the ][ and ][+ share a
#  character generator, the //e, Enhanced //e and //c share the MouseText one,
#  and each machine's folder is kept self-contained.
$romFiles = @(
    @{ Url = "$appleWinUrl/Apple2.rom";                 Dest = 'Apple2.rom';           Subdir = 'Machines/Apple2';          Size = 12288; Desc = 'Apple ][ ROM (Integer BASIC)' },
    @{ Url = "$appleWinUrl/Apple2_Video.rom";           Dest = 'Apple2_Video.rom';     Subdir = 'Machines/Apple2';          Size = 2048;  Desc = 'Apple ][/][+ Character Generator' },
    @{ Url = "$appleWinUrl/Apple2_Plus.rom";            Dest = 'Apple2Plus.rom';       Subdir = 'Machines/Apple2Plus';      Size = 12288; Desc = 'Apple ][+ ROM (Applesoft BASIC)' },
    @{ Url = "$appleWinUrl/Apple2_Video.rom";           Dest = 'Apple2_Video.rom';     Subdir = 'Machines/Apple2Plus';      Size = 2048;  Desc = 'Apple ][/][+ Character Generator' },
    @{ Url = "$appleWinUrl/Apple2e.rom";                Dest = 'Apple2e.rom';          Subdir = 'Machines/Apple2e';         Size = 16384; Desc = 'Apple //e ROM' },
    @{ Url = "$appleWinUrl/Apple2e_Enhanced_Video.rom"; Dest = 'Apple2e_Video.rom';    Subdir = 'Machines/Apple2e';         Size = 4096;  Desc = 'Apple //e Character Generator + MouseText' },
    @{ Url = "$appleWinUrl/Apple2e_Enhanced.rom";       Dest = 'Apple2eEnhanced.rom';  Subdir = 'Machines/Apple2eEnhanced'; Size = 16384; Desc = 'Apple //e Enhanced ROM (65C02)' },
    @{ Url = "$appleWinUrl/Apple2e_Enhanced_Video.rom"; Dest = 'Apple2e_Video.rom';    Subdir = 'Machines/Apple2eEnhanced'; Size = 4096;  Desc = 'Apple //e Character Generator + MouseText' },
    @{ Url = $apple2cRomUrl;                            Dest = 'Apple2c.rom';          Subdir = 'Machines/Apple2c';         Size = 32768; Desc = 'Apple //c ROM 4 (341-0445-B, memory expansion)' },
    @{ Url = "$appleWinUrl/Apple2e_Enhanced_Video.rom"; Dest = 'Apple2c_Video.rom';    Subdir = 'Machines/Apple2c';         Size = 4096;  Desc = 'Apple //c Character Generator + MouseText' },
    @{ Url = "$appleWinUrl/DISK2.rom";                  Dest = 'Disk2.rom';            Subdir = 'Devices/DiskII';           Size = 256;   Desc = 'Disk ][ Boot ROM (slot 6)' },
    @{ Url = "$appleWinUrl/DISK2-13sector.rom";         Dest = 'Disk2_13Sector.rom';   Subdir = 'Devices/DiskII';           Size = 256;   Desc = 'Disk ][ Boot ROM (13-sector)' }
)

#  The flat fixture layout wants each file once. Two rows that differ only in
#  Subdir collapse to one here.
if ($Fixtures) {
    $seen     = @{}
    $romFiles = @($romFiles | ForEach-Object {
        if (-not $seen.ContainsKey($_.Dest)) {
            $seen[$_.Dest] = $true
            $_
        }
    })
}

$downloaded = 0
$present    = 0
$failed     = 0

foreach ($rom in $romFiles) {
    $destDir  = if ($Fixtures) { Join-Path $repoRoot 'UnitTest/Fixtures' } else { Join-Path $repoRoot $rom.Subdir }
    $destPath = Join-Path $destDir $rom.Dest
    $label    = if ($Fixtures) { "UnitTest/Fixtures/$($rom.Dest)" } else { "$($rom.Subdir)/$($rom.Dest)" }

    if (-not $Verify -and -not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    if ((Test-Path $destPath) -and -not $Force) {
        $fileSize = (Get-Item $destPath).Length

        if ($fileSize -eq $rom.Size) {
            if (-not $Verify) {
                Write-Host "  OK    $label ($($rom.Desc)) -- already present" -ForegroundColor DarkGray
            }
            $present++
            continue
        }

        if ($Verify) {
            Write-Host "  SIZE  $label -- $fileSize bytes, expected $($rom.Size)" -ForegroundColor Red
            $failed++
            continue
        }

        Write-Host "  SIZE  $label -- wrong size ($fileSize, expected $($rom.Size)), re-downloading" -ForegroundColor Yellow
    }
    elseif ($Verify) {
        Write-Host "  MISSING  $label ($($rom.Desc))" -ForegroundColor Red
        $failed++
        continue
    }

    Write-Host "  GET   $label ($($rom.Desc))..." -NoNewline

    try {
        Invoke-WebRequest -Uri $rom.Url -OutFile $destPath -UseBasicParsing

        $fileSize = (Get-Item $destPath).Length

        if ($fileSize -ne $rom.Size) {
            Write-Host " SIZE MISMATCH ($fileSize bytes, expected $($rom.Size))" -ForegroundColor Red
            Remove-Item $destPath -Force
            $failed++
        }
        else {
            Write-Host " OK ($fileSize bytes)" -ForegroundColor Green
            $downloaded++
        }
    }
    catch {
        Write-Host " FAILED: $_" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""

if ($Verify) {
    if ($failed -gt 0) {
        Write-Host "$failed ROM file(s) missing from UnitTest/Fixtures. Run: scripts/FetchRoms.ps1 -Fixtures" -ForegroundColor Red
        exit 1
    }
    Write-Host "All $present fixture ROMs present."
    exit 0
}

Write-Host "Downloaded: $downloaded  Present: $present  Failed: $failed"

if ($failed -gt 0) {
    exit 1
}
