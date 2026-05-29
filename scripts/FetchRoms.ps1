<#
.SYNOPSIS
    Downloads Apple II ROM images from the AppleWin project for use with Casso.

.DESCRIPTION
    Downloads ROM files from AppleWin's GitHub repository and places them in
    the per-machine `Machines/<MachineName>/` (and shared
    `Devices/DiskII/`) subdirectories.

    Files downloaded (per spec 005-disk-ii-audio Phase 12; some
    upstream files land in more than one machine folder so each
    machine's assets are self-contained):
      - Apple II ROM         (12 KB) → Machines/Apple2/Apple2.rom
      - Apple II+ ROM        (12 KB) → Machines/Apple2Plus/Apple2Plus.rom
      - Apple IIe ROM        (16 KB) → Machines/Apple2e/Apple2e.rom
      - Apple IIe Video ROM  (4 KB)  → Machines/Apple2e/Apple2e_Video.rom
      - Apple II Video ROM   (2 KB)  → Machines/Apple2/Apple2_Video.rom + Machines/Apple2Plus/Apple2_Video.rom
      - Disk II Boot ROM     (256 B) → Devices/DiskII/Disk2.rom
      - Disk II 13-sector    (256 B) → Devices/DiskII/Disk2_13Sector.rom

    Source: https://github.com/AppleWin/AppleWin/tree/master/resource

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

$baseUrl = 'https://raw.githubusercontent.com/AppleWin/AppleWin/master/resource'

# Map: AppleWin filename → (Casso filename, target subdir relative to
# the repo root). The Apple II/II+ character generator and the //e
# character generator are shared upstream files but get duplicated
# into each owning machine's folder so a single machine's assets are
# self-contained (per spec 005-disk-ii-audio Q1).
$romFiles = @(
    @{ Source = 'Apple2.rom';                  Dest = 'Apple2.rom';              Subdir = 'Machines/Apple2';           Size = 12288;  Desc = 'Apple II ROM (Integer BASIC)' },
    @{ Source = 'Apple2_Plus.rom';             Dest = 'Apple2Plus.rom';          Subdir = 'Machines/Apple2Plus';       Size = 12288;  Desc = 'Apple II+ ROM (Applesoft BASIC)' },
    @{ Source = 'Apple2e.rom';                 Dest = 'Apple2e.rom';             Subdir = 'Machines/Apple2e';          Size = 16384;  Desc = 'Apple IIe ROM' },
    @{ Source = 'Apple2e_Enhanced.rom';        Dest = 'Apple2eEnhanced.rom';     Subdir = 'Machines/Apple2eEnhanced';  Size = 16384;  Desc = 'Apple IIe Enhanced ROM (65C02)' },
    @{ Source = 'Apple2_Video.rom';            Dest = 'Apple2_Video.rom';        Subdir = 'Machines/Apple2';           Size = 2048;   Desc = 'Apple II/II+ Character Generator ROM (][)' },
    @{ Source = 'Apple2_Video.rom';            Dest = 'Apple2_Video.rom';        Subdir = 'Machines/Apple2Plus';       Size = 2048;   Desc = 'Apple II/II+ Character Generator ROM (][+)' },
    @{ Source = 'Apple2e_Enhanced_Video.rom';  Dest = 'Apple2e_Video.rom';       Subdir = 'Machines/Apple2e';          Size = 4096;   Desc = 'Apple IIe Character Generator ROM (//e)' },
    @{ Source = 'Apple2e_Enhanced_Video.rom';  Dest = 'Apple2e_Video.rom';       Subdir = 'Machines/Apple2eEnhanced';  Size = 4096;   Desc = 'Apple IIe Character Generator ROM (//e Enhanced)' },
    @{ Source = 'DISK2.rom';                   Dest = 'Disk2.rom';               Subdir = 'Devices/DiskII';            Size = 256;    Desc = 'Disk II Boot ROM (slot 6)' },
    @{ Source = 'DISK2-13sector.rom';          Dest = 'Disk2_13Sector.rom';      Subdir = 'Devices/DiskII';            Size = 256;    Desc = 'Disk II Boot ROM (13-sector original)' }
)

$downloaded = 0
$skipped    = 0
$failed     = 0

foreach ($rom in $romFiles) {
    $destDir  = Join-Path $repoRoot $rom.Subdir
    $destPath = Join-Path $destDir  $rom.Dest
    $url      = "$baseUrl/$($rom.Source)"

    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    if ((Test-Path $destPath) -and -not $Force) {
        $fileSize = (Get-Item $destPath).Length

        if ($fileSize -eq $rom.Size) {
            Write-Host "  SKIP  $($rom.Subdir)/$($rom.Dest) ($($rom.Desc)) — already exists" -ForegroundColor DarkGray
            $skipped++
            continue
        }

        Write-Host "  SIZE  $($rom.Subdir)/$($rom.Dest) — wrong size ($fileSize, expected $($rom.Size)), re-downloading" -ForegroundColor Yellow
    }

    Write-Host "  GET   $($rom.Subdir)/$($rom.Dest) ($($rom.Desc))..." -NoNewline

    try {
        Invoke-WebRequest -Uri $url -OutFile $destPath -UseBasicParsing

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
Write-Host "Downloaded: $downloaded  Skipped: $skipped  Failed: $failed"

if ($failed -gt 0) {
    Write-Host "Some ROM downloads failed. The emulator may not work without all ROM files." -ForegroundColor Red
    exit 1
}

Write-Host "ROM files placed under: $repoRoot\Machines and $repoRoot\Devices" -ForegroundColor Green
Write-Host "NOTE: ROM images are Apple copyrighted material sourced from AppleWin." -ForegroundColor DarkYellow
