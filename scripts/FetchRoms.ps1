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

#  Two parts AppleWin does not ship, from the Apple II Documentation Project
#  mirror: the //c's 32K ROM 4 (AppleWin does not emulate the //c at all), and
#  the UNENHANCED //e's character generator -- AppleWin has only the enhanced
#  one and uses it for both of its //e models, which puts MouseText on a
#  machine that never had it.
$mirrorRoot    = 'https://mirrors.apple2.org.za/Apple%20II%20Documentation%20Project/Computers/Apple%20II'
$apple2cRomUrl = "$mirrorRoot/Apple%20IIc/ROM%20Images/Apple%20IIc%20ROM%2004%20-%20341-0445-B.bin"
$apple2eVidUrl = "$mirrorRoot/Apple%20IIe/ROM%20Images/Apple%20IIe%20Video%20ROM%20-%20342-0133-A%20-%20US%201982.bin"

#  Card and drive firmware, and the //c Plus, from the same mirror. Each row
#  has a SHA-1 checked after download: MAME's for the SCSI card, the 3.5
#  controller card and the //c Plus; the SHA-1 of the files whose SHA-256 the
#  a2-liron disassembly's Makefile lists for the Liron card and the UniDisk 3.5;
#  and the SHA-1 pinned at the first fetch for the two ProFile ROMs.
$docRoot     = 'https://mirrors.apple2.org.za/Apple%20II%20Documentation%20Project'
$cardRoot    = "$docRoot/Interface%20Cards"
$scsiRomUrl  = "$cardRoot/SCSI%20Controllers/Apple%20II%20SCSI%20Card/ROM%20Images/Apple%20II%20SCSI%20Card%20ROM%20Rev.%20C%20-%20341-0437-A.bin"
$a35RomUrl   = "$cardRoot/Disk%20Drive%20Controllers/Apple%20II%203.5%20Disk%20Controller%20Card/ROM%20Images/Apple%20II%203.5%20Disk%20Controller%20Card%20ROM%20-%20341-0438-A.bin"
$lironUrl    = "$cardRoot/Disk%20Drive%20Controllers/Apple%203.5%20Floppy%20Disk%20Drive%20Interface%20Card/ROM%20Images/Apple%203.5%20Floppy%20Disk%20Drive%20Interface%20Card%20ROM.bin"
$uniDiskUrl  = "$docRoot/Peripherals/Disk%20Drives/Apple%20UniDisk%203.5/ROM%20Images/Apple%20UniDisk%203.5%20ROM.bin"
$proFileRoot = "$cardRoot/Hard%20Disk%20Drive%20Controllers/Apple%20II%20ProFile%20Interface%20Card/ROM%20Images"
$proFile5Url = "$proFileRoot/Apple%20II%20Interface%20341-0271-A%20ROM%202716%20for%205MB%20Profile.bin"
$proFile10Url = "$proFileRoot/Apple%20II%20Interface%20341-0299-B%20ROM%202716%20for%2010MB%20Profile.bin"
$cPlusRoot   = "$mirrorRoot/Apple%20IIc%20plus/ROM%20Images"
$cPlusRomUrl = "$cPlusRoot/Apple%20IIc%20plus%20ROM%2005%20-%20342-0625-A%20-%201988.bin"
$cPlusVidUrl = "$cPlusRoot/Apple%20IIc%20plus%20Video%20ROM%20-%20342-0265-A.bin"

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
    @{ Url = $apple2eVidUrl;                            Dest = 'Apple2e_Video.rom';    Subdir = 'Machines/Apple2e';         Size = 4096;  Desc = 'Apple //e Character Generator (342-0133-A)' },
    @{ Url = "$appleWinUrl/Apple2e_Enhanced.rom";       Dest = 'Apple2eEnhanced.rom';  Subdir = 'Machines/Apple2eEnhanced'; Size = 16384; Desc = 'Apple //e Enhanced ROM (65C02)' },
    @{ Url = "$appleWinUrl/Apple2e_Enhanced_Video.rom"; Dest = 'Apple2eEnhanced_Video.rom'; Subdir = 'Machines/Apple2eEnhanced'; Size = 4096; Desc = 'Apple //e Enhanced Character Generator + MouseText (342-0265-A)' },
    @{ Url = $apple2cRomUrl;                            Dest = 'Apple2c.rom';          Subdir = 'Machines/Apple2c';         Size = 32768; Desc = 'Apple //c ROM 4 (341-0445-B, memory expansion)' },
    @{ Url = "$appleWinUrl/Apple2e_Enhanced_Video.rom"; Dest = 'Apple2c_Video.rom';    Subdir = 'Machines/Apple2c';         Size = 4096;  Desc = 'Apple //c Character Generator + MouseText' },
    @{ Url = "$appleWinUrl/DISK2.rom";                  Dest = 'Disk2.rom';            Subdir = 'Devices/DiskII';           Size = 256;   Desc = 'Disk ][ Boot ROM (slot 6)' },
    @{ Url = "$appleWinUrl/DISK2-13sector.rom";         Dest = 'Disk2_13Sector.rom';   Subdir = 'Devices/DiskII';           Size = 256;   Desc = 'Disk ][ Boot ROM (13-sector)' },
    @{ Url = $scsiRomUrl;   Dest = '341-0437-A.rom';        Subdir = 'Devices/AppleScsiCard'; Size = 16384; Sha1 = '451c85c46b92e6ad2ad930f055ccf0fe3049936d'; Desc = 'Apple II SCSI Card ROM (341-0437-A)' },
    @{ Url = $a35RomUrl;    Dest = '341-0438-A.rom';        Subdir = 'Devices/Apple35Card';   Size = 32768; Sha1 = '440c3f84176c7b9f542da0b6ddf4fb13ec326c46'; Desc = 'Apple II 3.5 Disk Controller Card ROM (341-0438-A)' },
    @{ Url = $lironUrl;     Dest = 'Liron.rom';             Subdir = 'Devices/LironCard';     Size = 4096;  Sha1 = 'fa94ecc2aa23f336f7da9e18bc8f4a6cfbb4ea60'; Desc = 'UniDisk 3.5 interface card ROM' },
    @{ Url = $uniDiskUrl;   Dest = 'UniDisk35.rom';         Subdir = 'Devices/UniDisk35';     Size = 8192;  Sha1 = '69f3dee914027eb2fa6eda512f8f5c649ad4c227'; Desc = 'UniDisk 3.5 drive controller ROM' },
    @{ Url = $proFile5Url;  Dest = '341-0271-A.rom';        Subdir = 'Devices/ProFileCard';   Size = 2048;  Sha1 = 'd9d4a7a2c14232c3872f9bbf55aff1b31f9a1cb5'; Desc = 'ProFile interface card ROM, 5 MB (341-0271-A)' },
    @{ Url = $proFile10Url; Dest = '341-0299-B.rom';        Subdir = 'Devices/ProFileCard';   Size = 2048;  Sha1 = '81750f023f964c2f3781d687cee5b1ea5b49d4ea'; Desc = 'ProFile interface card ROM, 10 MB (341-0299-B)' },
    @{ Url = $cPlusRomUrl;  Dest = 'Apple2cPlus.rom';       Subdir = 'Machines/Apple2cPlus';  Size = 32768; Sha1 = '1a27ae26966bbafd825d08ad1a24742d3e33557c'; Desc = 'Apple //c Plus ROM 05 (342-0625-A)' },
    @{ Url = $cPlusVidUrl;  Dest = 'Apple2cPlus_Video.rom'; Subdir = 'Machines/Apple2cPlus';  Size = 4096;  Sha1 = 'b2b5d87f52693817fc747df087a4aa1ddcdb1f10'; Desc = 'Apple //c Plus Character Generator + MouseText (342-0265-A)' }
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
        elseif ($rom.ContainsKey('Sha1') -and (Get-FileHash $destPath -Algorithm SHA1).Hash.ToLower() -ne $rom.Sha1) {
            Write-Host " CHECKSUM MISMATCH (expected SHA-1 $($rom.Sha1))" -ForegroundColor Red
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
