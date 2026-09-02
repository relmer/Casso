<#
.SYNOPSIS
    Downloads the stock Apple operating-system masters the scenario suite
    boots, if this machine does not already have them.

.DESCRIPTION
    `ScenarioTests.dll` boots real guests from the DOS 3.3 System Master and
    the ProDOS Users Disk. Neither is committed: they are Apple's, and the
    repository redistributes nothing it does not own. Without them every
    guest-visible case FAILS, which is the intended signal but a poor
    welcome for a fresh clone, so `RunTests.ps1 -Scenario` runs this first.

    LOOKS BEFORE IT FETCHES, in the two places the tests themselves read, so
    a machine that already has an image never touches the network:

      1. `Disks/Apple/<name>` under the repo root, which is where this script
         puts what it downloads and where the tests look first.
      2. `%LOCALAPPDATA%\Casso\Disks\<name>`, the emulator's own download
         cache. Picking the DOS 3.3 or ProDOS row in Casso's Boot Disk or
         Insert Disk picker fills it, and that copy counts.

    NOTHING IS WRITTEN TO THE EMULATOR'S CACHE. A test run must not alter
    what the application installed for the user, so a missing image is
    fetched into the repository instead. Both names are `.gitignore`d, so a
    fetched image never becomes a commit and never trips CI's clean-tree
    check.

    The files land beside the repo rather than in a temp directory so the
    download survives between runs. The archive is a volunteer mirror; asking
    it for 280 KB once per checkout is courteous, once per test run is not.

    Source: the Asimov archive, the same host and paths the emulator's own
    downloader uses (`Casso/AssetBootstrap.cpp`, `s_kDos33Disk` and
    `s_kProDOSDisk`). The SHA1s below are that file's, recorded there against
    the copies this project has always tested with.

.PARAMETER Force
    Re-download even when a verified copy is already in the repo. Ignores a
    copy in the emulator's cache as well, so this always refreshes what the
    repository holds.

.PARAMETER Quiet
    Print only failures. Used by RunTests.ps1, which has its own banner.

.NOTES
    Exit codes: 0 = every image is available, 1 = at least one is missing.

    RunTests.ps1 treats a non-zero exit as fatal, and the asymmetry above is
    why that is safe: a machine holding an image never reaches the network,
    so being offline exits 0. What is left is a run that cannot boot the
    guests it exists to boot.
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

$baseUrl = 'https://www.apple.asimov.net'
$destDir = Join-Path $repoRoot 'Disks/Apple'

#
#  Hashes are pinned, and they are the whole of the check.
#
#  FetchMerlin.ps1 also probes each image's volume structure, because those
#  disks are an oracle for byte-comparison and a second check there is worth
#  its cost. An exact hash already establishes identity, so a structural probe
#  after it could only fail on bytes the hash has already accepted.
#
#  RepoName is what the tests look for under Disks/Apple; CacheName is what
#  the emulator saves the same image as. The two differ, and both differ from
#  the name on the archive, so all three appear here.
#
$assets = @(
    @{ RepoName  = 'dos33-master.dsk'
       CacheName = 'DOS 3.3 System Master.dsk'
       UrlPath   = '/images/masters/DOS%203.3%20System%20Master%20-%20680-0210-A%20%281982%29.dsk'
       Size      = 143360
       Sha1      = '27EA2EE7114EBFA91DA0A16B7B8EBFF24EB8EE88'
       Desc      = 'DOS 3.3 System Master (680-0210-A, 1982)' },

    @{ RepoName  = 'prodos-users.dsk'
       CacheName = 'ProDOS Users Disk.dsk'
       UrlPath   = '/images/masters/prodos/ProDOS%20Users%20Disk%20-%20680-0224-C.dsk'
       Size      = 143360
       Sha1      = '40DC1A16E3F234857A29B49CA0B996E1B14D38B9'
       Desc      = 'ProDOS Users Disk (680-0224-C)' }
)

#
#  Why the file on disk is not the image it claims to be, or $null if it is.
#  Shared by the already-present and the just-downloaded cases so the two
#  cannot drift apart.
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

#
#  The emulator's cache path for an image, or $null when this machine has no
#  LOCALAPPDATA to speak of. Read-only: nothing here creates that directory.
#
function Get-CachedImagePath {
    param([hashtable]$Asset)

    if (-not $env:LOCALAPPDATA) {
        return $null
    }

    return Join-Path $env:LOCALAPPDATA "Casso/Disks/$($Asset.CacheName)"
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

    #  Already in the repo, and still the right bytes.
    if ((Test-Path -LiteralPath $destPath) -and -not $Force) {
        $problem = Get-AssetProblem -Path $destPath -Asset $asset

        if (-not $problem) {
            Write-Status "  HAVE  $($asset.RepoName) -- in the repo, verified" 'DarkGray'
            $present++
            continue
        }

        Write-Status "  BAD   $($asset.RepoName) -- $problem, re-downloading" 'Yellow'
    }

    #  Already installed by the emulator. Left where it is: a test run has no
    #  business rewriting the application's own cache, and the tests read it.
    if (-not $Force) {
        $cachePath = Get-CachedImagePath -Asset $asset

        if ($cachePath -and (Test-Path -LiteralPath $cachePath)) {
            $problem = Get-AssetProblem -Path $cachePath -Asset $asset

            if (-not $problem) {
                Write-Status "  HAVE  $($asset.RepoName) -- in the emulator's cache, verified" 'DarkGray'
                $present++
                continue
            }

            Write-Status "  BAD   $($asset.CacheName) in the cache -- $problem, downloading a fresh copy" 'Yellow'
        }
    }

    if (-not (Test-Path -LiteralPath $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    Write-Status "  GET   $($asset.RepoName) ($($asset.Desc))..." 'Gray' -NoNewline

    try {
        Invoke-WebRequest -Uri "$baseUrl$($asset.UrlPath)" -OutFile $destPath `
                          -MaximumRedirection 10 -UseBasicParsing

        $problem = Get-AssetProblem -Path $destPath -Asset $asset

        if ($problem) {
            #
            #  Delete rather than leave it. A wrong image that sits on disk
            #  looking downloaded is worse than none: the next run accepts it
            #  and every guest booted from it proves nothing.
            #
            Write-Status " REJECTED -- $problem" 'Red'
            Remove-Item -LiteralPath $destPath -Force
            $missing++
            continue
        }

        Write-Status " OK ($($asset.Size) bytes, hash verified)" 'Green'
        $fetched++
    }
    catch {
        #  Partial transfers leave a file behind, and it would be accepted on
        #  the next run only if it happened to hash correctly -- but a stray
        #  file in Disks/Apple is worth removing regardless.
        if (Test-Path -LiteralPath $destPath) {
            Remove-Item -LiteralPath $destPath -Force
        }

        Write-Status " FAILED: $_" 'Red'
        $missing++
    }
}

if ($missing -gt 0) {
    Write-Host ''
    Write-Host "Stock disks: $missing of $($assets.Count) could not be fetched." -ForegroundColor Yellow
    Write-Host '  The scenario suite boots these, so it cannot run without them.' -ForegroundColor DarkGray
    Write-Host '  Picking DOS 3.3 or ProDOS in the emulator disk picker also installs one,' -ForegroundColor DarkGray
    Write-Host '  as does dropping a copy at Disks/Apple/ under the repo root.' -ForegroundColor DarkGray
    Write-Host ''
    exit 1
}

if (-not $Quiet -and ($fetched -gt 0 -or $present -gt 0)) {
    Write-Host "Stock disks: $present already here, $fetched downloaded." -ForegroundColor DarkGray
}

exit 0
