<#
.SYNOPSIS
    Fetches sample disks from the Asimov Apple II archive for the scenario tests.

.DESCRIPTION
    Downloads the disks below from www.apple.asimov.net into
    UnitTest/Fixtures/Asimov/, which git ignores. They aren't committed here.
    The scenario tests that read them are skipped when they are absent.

    The archive has no versions to pin to, so every file is checked against
    its SHA-256 after download, and a file whose hash differs is removed. A
    file already present with the right hash isn't fetched again unless
    -Force is given.

.PARAMETER Verify
    Download nothing. Report which files are present with the right hash and
    exit 1 if any is not.

.PARAMETER Force
    Fetch files that are already present.
#>

param (
    [switch]$Verify,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir
$destRoot  = Join-Path $repoRoot 'UnitTest/Fixtures/Asimov'
$baseUrl   = 'https://www.apple.asimov.net/images'

$files = @(
    @{ Path = 'productivity/spreadsheet/Visicalc 1.27.dsk';                             Sha256 = 'D00E0606C4ACBB0D0A93AB15503529978F201AE2C366A13F23C9A1A250DB8794' },
    @{ Path = 'productivity/spreadsheet/VisiCalc 1.37 - 1979.dsk';                      Sha256 = '5A4C6AC221B8C69D50204776A8D211F3554D70AD42BB7109F2B39A7979ABDBFC' },
    @{ Path = 'productivity/spreadsheet/Visicalc Home and Office Companion disk 1.dsk'; Sha256 = 'BF9CCF8044618A3701BAE4399C9AD05E961E132A5B7A52971D2F31031C001F97' },
    @{ Path = 'productivity/spreadsheet/Visicalc Home and Office Companion disk 2.dsk'; Sha256 = '70F826B189525AFB600F617B3187AEA0EDF08CEEC48902182E260273A75E14EF' }
)

$downloaded = 0
$present    = 0
$failed     = 0

foreach ($file in $files) {
    $destPath = Join-Path $destRoot $file.Path
    $destDir  = Split-Path -Parent $destPath
    $label    = "UnitTest/Fixtures/Asimov/$($file.Path)"

    if ((Test-Path -LiteralPath $destPath) -and -not $Force) {
        $hash = (Get-FileHash -LiteralPath $destPath -Algorithm SHA256).Hash

        if ($hash -eq $file.Sha256) {
            $present++
            continue
        }

        Write-Host "  HASH  $label -- $hash, should be $($file.Sha256)" -ForegroundColor Yellow
    }
    elseif ($Verify) {
        Write-Host "  MISSING  $label" -ForegroundColor Red
        $failed++
        continue
    }

    if ($Verify) {
        $failed++
        continue
    }

    if (-not (Test-Path -LiteralPath $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    Write-Host "  GET   $label..." -NoNewline

    try {
        $url = $baseUrl + '/' + ((($file.Path -split '/') | ForEach-Object { [uri]::EscapeDataString($_) }) -join '/')

        Invoke-WebRequest -Uri $url -OutFile $destPath -UseBasicParsing

        $hash = (Get-FileHash -LiteralPath $destPath -Algorithm SHA256).Hash

        if ($hash -ne $file.Sha256) {
            Write-Host " HASH MISMATCH ($hash)" -ForegroundColor Red
            Remove-Item -LiteralPath $destPath -Force
            $failed++
        }
        else {
            Write-Host " OK" -ForegroundColor Green
            $downloaded++
        }
    }
    catch {
        Write-Host " FAILED: $_" -ForegroundColor Red
        $failed++
    }
}

if ($Verify) {
    if ($failed -gt 0) {
        Write-Host "$failed Asimov sample file(s) missing. Run: scripts/FetchAsimovSamples.ps1" -ForegroundColor Red
        exit 1
    }

    Write-Host "All $present Asimov sample files present."
    exit 0
}

Write-Host "Downloaded: $downloaded  Present: $present  Failed: $failed"

if ($failed -gt 0) {
    exit 1
}
