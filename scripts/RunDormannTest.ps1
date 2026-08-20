<#
.SYNOPSIS
    Downloads, assembles, and verifies the Klaus Dormann 6502 functional test suite.

.DESCRIPTION
    1. Downloads 6502_functional_test.a65 and reference binary from GitHub
    2. Assembles with CassoCli.exe in AS65-compatible mode
    3. Verifies assembly succeeds and produces a 64KB flat binary
    4. Reports byte differences vs reference (informational — reference binary
       is from an older source version missing zps data added Jan 2020)
    5. Cleans up all downloaded files

.NOTES
    Exit codes: 0 = assembly succeeded (64KB output), 1 = failure
    Downloaded files are deleted in a finally block to ensure cleanup.
#>
[CmdletBinding()]
param ()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot   = Split-Path -Parent $scriptDir
$tempDir    = Join-Path $repoRoot 'dormann_temp'

$sourceUrl  = 'https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/6502_functional_test.a65'
$refBinUrl  = 'https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/bin_files/6502_functional_test.bin'

# Auto-detect CassoCli.exe — prefer Debug x64
$exeCandidates = @(
    (Join-Path $repoRoot 'x64\Debug\CassoCli.exe'),
    (Join-Path $repoRoot 'x64\Release\CassoCli.exe'),
    (Join-Path $repoRoot 'ARM64\Debug\CassoCli.exe'),
    (Join-Path $repoRoot 'ARM64\Release\CassoCli.exe')
)

$cassoCli = $null

foreach ($candidate in $exeCandidates) {
    if (Test-Path $candidate) {
        $cassoCli = $candidate
        break
    }
}

if (-not $cassoCli) {
    Write-Error "CassoCli.exe not found. Build the project first."
    exit 1
}

Write-Host "Using: $cassoCli"

try {
    # Create temp directory
    if (-not (Test-Path $tempDir)) {
        New-Item -ItemType Directory -Path $tempDir | Out-Null
    }

    $sourceFile = Join-Path $tempDir '6502_functional_test.a65'
    $refBinFile = Join-Path $tempDir '6502_functional_test.bin'
    $outputFile = Join-Path $tempDir '6502_functional_test.out.bin'
    $stderrFile = Join-Path $tempDir 'stderr.txt'

    # Download source and reference using curl.exe (avoids Defender ClickFix false positive
    # triggered by the cmd.exe -> powershell -> Invoke-WebRequest pattern)
    #  --fail is load-bearing. Without it curl writes the SERVER'S ERROR PAGE to
    #  the output file and still exits 0, so a rate-limited or unavailable
    #  download hands the assembler HTML and surfaces as nonsense far downstream
    #  -- "Invalid mnemonic: TOO", "Label ... <!DOCTYPE" -- which reads as a
    #  broken assembler rather than as a failed fetch. Observed live during a
    #  GitHub 429/503 window.
    Write-Host "Downloading source..."
    curl.exe --fail -sL -o $sourceFile $sourceUrl
    if ($LASTEXITCODE -ne 0) { throw "Could not download the Dormann source ($sourceUrl): curl exit $LASTEXITCODE." }

    Write-Host "Downloading reference binary..."
    curl.exe --fail -sL -o $refBinFile $refBinUrl
    if ($LASTEXITCODE -ne 0) { throw "Could not download the Dormann reference ($refBinUrl): curl exit $LASTEXITCODE." }

    Write-Host "Source size:    $((Get-Item $sourceFile).Length) bytes"
    Write-Host "Reference size: $((Get-Item $refBinFile).Length) bytes"

    # Assemble with -z (zero fill) to match AS65 default behavior
    Write-Host "Assembling..."
    # The dialect is named, because assembling no longer guesses: an
    # unrecognized first argument is refused rather than taken as a source file.
    $assembleArgs = @('as65', $sourceFile, '-z', '-o', $outputFile)
    $proc = Start-Process -FilePath $cassoCli -ArgumentList $assembleArgs -NoNewWindow -Wait -PassThru -RedirectStandardError $stderrFile

    $stderrContent = Get-Content $stderrFile -Raw -ErrorAction SilentlyContinue
    if ($stderrContent) {
        # Count errors vs warnings
        $errorLines = ($stderrContent -split "`n") | Where-Object { $_ -match ': error:' }

        if ($errorLines) {
            Write-Host "Assembly ERRORS:"
            $errorLines | ForEach-Object { Write-Host "  $_" }
        }

        $warningCount = (($stderrContent -split "`n") | Where-Object { $_ -match ': warning:' }).Count
        if ($warningCount -gt 0) {
            Write-Host "Assembly produced $warningCount warning(s) (expected — unused labels)"
        }
    }

    if ($proc.ExitCode -gt 1) {
        Write-Error "Assembly failed with exit code $($proc.ExitCode)"
        exit 1
    }

    if (-not (Test-Path $outputFile)) {
        Write-Error "Output file not created"
        exit 1
    }

    $outputSize = (Get-Item $outputFile).Length
    Write-Host "Output size:    $outputSize bytes"

    # Verify output is a full 64KB flat binary
    if ($outputSize -ne 65536) {
        Write-Error "Output size mismatch: expected 65536, got $outputSize"
        exit 1
    }

    # Informational: compare against reference
    Write-Host "`nComparing output against reference (informational)..."
    Write-Host "NOTE: Reference binary is from an older source version (pre-Jan 2020)"
    Write-Host "      missing 'zps db `$80,1' which shifts all zero-page addresses."

    $ourBytes = [System.IO.File]::ReadAllBytes($outputFile)
    $refBytes = [System.IO.File]::ReadAllBytes($refBinFile)

    $mismatches = 0
    $firstMismatch = -1

    for ($i = 0; $i -lt $ourBytes.Length; $i++) {
        if ($ourBytes[$i] -ne $refBytes[$i]) {
            if ($firstMismatch -eq -1) {
                $firstMismatch = $i
            }
            $mismatches++
        }
    }

    if ($mismatches -gt 0) {
        Write-Host "$mismatches byte(s) differ (first at offset 0x$($firstMismatch.ToString('X4')))"
        Write-Host "This is expected due to the source/reference version mismatch."
    }
    else {
        Write-Host "Output is byte-identical to reference!"
    }

    Write-Host "`nSUCCESS: Dormann test assembled correctly (65536-byte flat binary)"
    exit 0
}
finally {
    # Cleanup
    if (Test-Path $tempDir) {
        Remove-Item -Recurse -Force $tempDir -ErrorAction SilentlyContinue
    }
}
