<#
.SYNOPSIS
    Fetches CiderPress II's archive and disk test files for the scenario tests.

.DESCRIPTION
    Downloads the files below from CiderPress II's TestData folder, pinned to
    one commit, into UnitTest/Fixtures/CiderPress/, which git ignores. They
    are CiderPress II's, under its Apache 2.0 license, and aren't committed
    here. The scenario tests that read them are skipped when they are absent.

    Every file is size-checked after download. A file already present at the
    right size isn't fetched again unless -Force is given.

.PARAMETER Verify
    Download nothing. Report which files are present at the right size and
    exit 1 if any is not.

.PARAMETER Force
    Fetch files that are already present.

.PARAMETER Extract
    After fetching, have CiderPress II's cp2.exe extract test-files.sdk into
    UnitTest/Fixtures/CiderPress/extracted/test-files/ and each RDOS save disk
    and DOS 3.2 disk into a folder beside it, for the scenario tests that
    compare Casso's reads
    with them. Each fork's file name is in NAPS
    form, with the type and aux type after '#'. cp2.exe is looked for in
    %LOCALAPPDATA%\Programs\CiderPress2 unless its path is passed in -Cp2.

.PARAMETER Cp2
    The path of CiderPress II's cp2.exe.

.NOTES
    The commit is pinned so a change upstream can't change a test's input.
    Move it deliberately.
#>

param (
    [switch]$Verify,
    [switch]$Force,
    [switch]$Extract,
    [string]$Cp2 = (Join-Path $env:LOCALAPPDATA 'Programs\CiderPress2\cp2.exe')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir
$destRoot  = Join-Path $repoRoot 'UnitTest/Fixtures/CiderPress'

#  CiderPress II main as of 2026-11-02.
$ref     = '49b67b405a7f82a5b8be369351e381ca72b36796'
$baseUrl = "https://raw.githubusercontent.com/fadden/CiderPress2/$ref/TestData"

$files = @(
    @{ Path = 'fileconv/test-files.sdk';     Size = 445987  },
    @{ Path = 'fileconv/dos-files.do.gz';    Size = 28898   },
    @{ Path = 'dos32/3.2-blank.d13';         Size = 116480  },
    @{ Path = 'dos32/3.2.1-blank.nib';       Size = 232960  },
    @{ Path = 'dos32/3.2.1-master.d13';      Size = 116480  },
    @{ Path = 'nib/dos321-master.nib';       Size = 232960  },
    @{ Path = 'nufx/DIcEd.BSE';              Size = 71552   },
    @{ Path = 'nufx/GSHK11.SEA';             Size = 118055  },
    @{ Path = 'nufx/PatchHFS.shk';           Size = 13083   },
    @{ Path = 'nufx/SIMPLE.DOS.SDK';         Size = 1272    },
    @{ Path = 'nufx/Samples.BXY';            Size = 4480    },
    @{ Path = 'nufx/gshk-empty-forks.shk';   Size = 1036    },
    @{ Path = 'nufx/mislabeled_bny.shk';     Size = 2304    },
    @{ Path = 'nufx/old-archive.shk';        Size = 18462   },
    @{ Path = 'bny/SAMPLE.BQY';              Size = 37120   },
    @{ Path = 'gzip/hello__.as.gz';          Size = 117     },
    @{ Path = 'gzip/multi-member.gz';        Size = 90      },
    @{ Path = 'gzip/no-filename.gz';         Size = 52      },
    @{ Path = 'rdos/rdos3-rss-save.woz';     Size = 234745  },
    @{ Path = 'rdos/rdos32-shiloh-save.woz'; Size = 234731  },
    @{ Path = 'rdos/rdos33-rw2000-save.woz'; Size = 234727  },
    @{ Path = 'turducken/MultiPart.hdv';     Size = 1640448 },
    @{ Path = 'turducken/Shrunk.zip.gz';     Size = 21596   },
    @{ Path = 'turducken/WrappedSDK.zip';    Size = 475     },
    @{ Path = 'turducken/subdirs.zip';       Size = 667     },
    @{ Path = 'turducken/zip4.zip';          Size = 936     },
    @{ Path = 'woz/dos32master_2.woz';       Size = 234496  },
    @{ Path = 'zip/GSHK11.zip';              Size = 76046   },
    @{ Path = 'zip/hello-as.zip';            Size = 262     },
    @{ Path = 'zip/new-init-po.zip';         Size = 7489    },
    @{ Path = 'zip/unix-streamed.zip';       Size = 471     }
)

$downloaded = 0
$present    = 0
$failed     = 0

foreach ($file in $files) {
    $destPath = Join-Path $destRoot $file.Path
    $destDir  = Split-Path -Parent $destPath
    $label    = "UnitTest/Fixtures/CiderPress/$($file.Path)"

    if ((Test-Path -LiteralPath $destPath) -and -not $Force) {
        $fileSize = (Get-Item -LiteralPath $destPath).Length

        if ($fileSize -eq $file.Size) {
            $present++
            continue
        }

        Write-Host "  SIZE  $label -- $fileSize bytes, should be $($file.Size)" -ForegroundColor Yellow
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
        Invoke-WebRequest -Uri "$baseUrl/$($file.Path)" -OutFile $destPath -UseBasicParsing

        $fileSize = (Get-Item -LiteralPath $destPath).Length

        if ($fileSize -ne $file.Size) {
            Write-Host " SIZE MISMATCH ($fileSize bytes, should be $($file.Size))" -ForegroundColor Red
            Remove-Item -LiteralPath $destPath -Force
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

if ($Verify) {
    if ($failed -gt 0) {
        Write-Host "$failed CiderPress II test file(s) missing. Run: scripts/FetchCiderPressTestFiles.ps1" -ForegroundColor Red
        exit 1
    }

    Write-Host "All $present CiderPress II test files present."
    exit 0
}

Write-Host "Downloaded: $downloaded  Present: $present  Failed: $failed"

if ($failed -gt 0) {
    exit 1
}

if ($Extract) {
    $extracted = Join-Path $destRoot 'extracted/test-files'

    if (-not (Test-Path -LiteralPath $Cp2)) {
        Write-Host "cp2.exe is not at $Cp2. Install CiderPress II or pass -Cp2." -ForegroundColor Red
        exit 1
    }

    if (Test-Path -LiteralPath $extracted) {
        Remove-Item -LiteralPath $extracted -Recurse -Force
    }

    New-Item -ItemType Directory -Path $extracted -Force | Out-Null
    & $Cp2 extract --preserve=naps "--exdir=$extracted" (Join-Path $destRoot 'fileconv/test-files.sdk') | Out-Null

    if ($LASTEXITCODE -ne 0) {
        Write-Host "cp2.exe failed to extract test-files.sdk" -ForegroundColor Red
        exit 1
    }

    Write-Host "Extracted test-files.sdk to UnitTest/Fixtures/CiderPress/extracted/test-files"

    #  The RDOS save disks. Track 0 of the RDOS 3 and RDOS 3.2 disks has
    #  no sector CiderPress II can read, so the first file of each is left out.
    #  A DOS file is extracted without --raw, so that its bytes are the
    #  program or the text and not the sectors.
    $disks = @(
        @{ Image = 'rdos/rdos33-rw2000-save.woz'; Raw = $true;  Files = @() },
        @{ Image = 'rdos/rdos3-rss-save.woz';     Raw = $true;  Files = @('FIGHTMENU', 'DISKCHECK') },
        @{ Image = 'rdos/rdos32-shiloh-save.woz'; Raw = $true;  Files = @('MYSAVE') },
        @{ Image = 'dos32/3.2.1-master.d13';      Raw = $false; Files = @() },
        @{ Image = 'nib/dos321-master.nib';       Raw = $false; Files = @() },
        @{ Image = 'woz/dos32master_2.woz';       Raw = $false; Files = @() }
    )

    foreach ($disk in $disks) {
        $stem      = [IO.Path]::GetFileNameWithoutExtension($disk.Image)
        $extracted = Join-Path $destRoot "extracted/$stem"

        if (Test-Path -LiteralPath $extracted) {
            Remove-Item -LiteralPath $extracted -Recurse -Force
        }

        New-Item -ItemType Directory -Path $extracted -Force | Out-Null
        $cp2Args = @('extract', '--preserve=naps', "--exdir=$extracted")

        if ($disk.Raw) {
            $cp2Args += '--raw'
        }

        & $Cp2 @cp2Args (Join-Path $destRoot $disk.Image) @($disk.Files) | Out-Null

        if ($LASTEXITCODE -ne 0) {
            Write-Host "cp2.exe failed to extract $($disk.Image)" -ForegroundColor Red
            exit 1
        }

        Write-Host "Extracted $($disk.Image) to UnitTest/Fixtures/CiderPress/extracted/$stem"
    }}
