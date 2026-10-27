<#
.SYNOPSIS
    Downloads the Apple II network software disks the Uthernet II scenario
    cases boot, if this checkout does not already have them.

.DESCRIPTION
    `ScenarioTests/UthernetSoftwareTests.cpp` boots IP65's disk (WGET65,
    Telnet65) and Contiki's third disk (its web server) on an enhanced //e
    with the Uthernet II. Neither is committed: they are third-party releases,
    and the repository redistributes nothing it does not own. Without them
    those cases FAIL, so `RunTests.ps1 -Scenario` runs this first, after
    FetchStockDisks.ps1.

    LOOKS BEFORE IT FETCHES: a verified copy under `Disks/Network/` in the
    repo means no network access. A missing or wrong copy is replaced by the
    disk from the release archive, which is downloaded to a temporary folder,
    unpacked, and deleted. Both names are `.gitignore`d.

    Sources: the GitHub releases the projects publish. The SHA1s below are
    those of the disks in the pinned releases.

.PARAMETER Force
    Re-download even when a verified copy is already in the repo.

.PARAMETER Quiet
    Print only failures. Used by RunTests.ps1, which has its own banner.

.NOTES
    Exit codes: 0 = every disk is available, 1 = at least one is missing.
#>
[CmdletBinding()]
param (
    [switch]$Force,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir
$destDir   = Join-Path $repoRoot 'Disks/Network'

#
#  RepoName is what the tests look for under Disks/Network; ZipEntry is the
#  disk's name inside the release archive.
#
$assets = @(
    @{ RepoName = 'ip65.dsk'
       ZipUrl   = 'https://github.com/cc65/ip65/releases/download/2025-01-19/ip65-apple2.zip'
       ZipEntry = 'ip65.dsk'
       Size     = 143360
       Sha1     = 'BCFDC6CAD6AB718FC81D47451F76771C2CDC08FC'
       Desc     = 'IP65 2025-01-19 (WGET65, Telnet65)' },

    @{ RepoName = 'contiki-apple2-3.dsk'
       ZipUrl   = 'https://github.com/oliverschmidt/contiki/releases/download/2019-05-08/contiki-apple2.zip'
       ZipEntry = 'contiki-apple2-3.dsk'
       Size     = 143360
       Sha1     = 'F43421A7CCDC57B8E7D1BD20BE2FBAD043274653'
       Desc     = 'Contiki 2019-05-08, disk 3 (web server)' }
)

#
#  Why the file on disk is not the disk it claims to be, or $null if it is.
#
function Get-AssetProblem {
    param([string]$Path, [hashtable]$Asset)

    $size = (Get-Item -LiteralPath $Path).Length

    if ($size -ne $Asset.Size) {
        return "wrong size ($size bytes, expected $($Asset.Size))"
    }

    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA1).Hash

    if ($hash -ne $Asset.Sha1) {
        return "hash mismatch (got $hash)"
    }

    return $null
}

function Write-Status {
    param([string]$Text, [string]$Color = 'Gray', [switch]$NoNewline)

    if ($Quiet) {
        return
    }

    if ($NoNewline) {
        Write-Host $Text -ForegroundColor $Color -NoNewline
    }
    else {
        Write-Host $Text -ForegroundColor $Color
    }
}

$present = 0
$fetched = 0
$missing = 0

foreach ($asset in $assets) {
    $destPath = Join-Path $destDir $asset.RepoName

    if ((Test-Path -LiteralPath $destPath) -and -not $Force) {
        $problem = Get-AssetProblem -Path $destPath -Asset $asset

        if (-not $problem) {
            Write-Status "  HAVE  $($asset.RepoName) -- in the repo, verified" 'DarkGray'
            $present++
            continue
        }

        Write-Status "  BAD   $($asset.RepoName) -- $problem, re-downloading" 'Yellow'
    }

    if (-not (Test-Path -LiteralPath $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    $work = Join-Path ([IO.Path]::GetTempPath()) ("CassoNetworkDisk-" + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work -Force | Out-Null

    Write-Status "  GET   $($asset.RepoName) ($($asset.Desc))..." 'Gray' -NoNewline

    try {
        $zipPath = Join-Path $work 'release.zip'

        Invoke-WebRequest -Uri $asset.ZipUrl -OutFile $zipPath -MaximumRedirection 10 -UseBasicParsing
        Expand-Archive -LiteralPath $zipPath -DestinationPath (Join-Path $work 'unpacked') -Force

        $entry = Get-ChildItem -LiteralPath (Join-Path $work 'unpacked') -Recurse -File |
                 Where-Object { $_.Name -eq $asset.ZipEntry } |
                 Select-Object -First 1

        if (-not $entry) {
            Write-Status " NOT FOUND -- $($asset.ZipEntry) is not in the archive" 'Red'
            $missing++
            continue
        }

        $problem = Get-AssetProblem -Path $entry.FullName -Asset $asset

        if ($problem) {
            Write-Status " DISCARDED -- $problem" 'Red'
            $missing++
            continue
        }

        Copy-Item -LiteralPath $entry.FullName -Destination $destPath -Force
        Write-Status " OK ($($asset.Size) bytes, hash verified)" 'Green'
        $fetched++
    }
    catch {
        Write-Status " FAILED: $_" 'Red'
        $missing++
    }
    finally {
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($missing -gt 0) {
    Write-Host ''
    Write-Host "Network disks: $missing of $($assets.Count) could not be fetched." -ForegroundColor Yellow
    Write-Host '  The Uthernet II scenario cases boot these, so they fail without them.' -ForegroundColor DarkGray
    Write-Host '  Dropping a verified copy at Disks/Network/ under the repo root also works.' -ForegroundColor DarkGray
    Write-Host ''
    exit 1
}

if (-not $Quiet -and ($fetched -gt 0 -or $present -gt 0)) {
    Write-Host "Network disks: $present already here, $fetched downloaded." -ForegroundColor DarkGray
}

exit 0
