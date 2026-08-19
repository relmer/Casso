<#
.SYNOPSIS
    Reproduces every shipped Merlin Pro object from its vendor source, through
    CassoCli.exe.

.DESCRIPTION
    The unit tests assemble the same six oracles in-process, against an
    AssemblerOptions struct built in C++. This script assembles them the way a
    developer does: an argv, a source file read off the filesystem, and a file
    written to disk. Three links exist only on this path -- the grammar handing
    parsed options to the assembler, the reader taking source off the host
    filesystem, and the output writer -- and none of them are linked into the
    test project.

    The comparison is against the WHOLE shipped file, 4-byte DOS 3.3 header
    included, because --dos-bin reproduces the header too. That makes it a
    stricter check than the corpus tests make: those compare the payload, so a
    wrong origin reaching the header would pass there and fail here.

    This became possible when the vendor sources were committed as ordinary
    Windows text. Stored as Apple II text they were unreadable to the tool under
    test, and this end-to-end path could not be exercised at all.

.PARAMETER Configuration
    Which CassoCli build to use. Default: whichever is found, Debug first.

.NOTES
    Exit codes: 0 = every oracle reproduced, 1 = a mismatch or a failure
#>
[CmdletBinding()]
param (
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot  = Split-Path -Parent $scriptDir
$fixtures  = Join-Path $repoRoot 'UnitTest\Fixtures\Merlin'

#
#  The six oracles, with the answers each source asks for.
#
#  These match UnitTest/MerlinCorpusTests.cpp's table deliberately: the same
#  entries assembled two ways is the point, and an entry that reproduces its
#  object in one place and not the other is a finding about the path, not about
#  the entry.
#
#  CLOCK is one source and two objects, which is why the answers live with the
#  ENTRY rather than with the source.
#
$oracles = @(
    @{ Name = 'LABELS';     Source = 'LABELS.S';     Object = 'LABELS';     Answers = @() },
    @{ Name = 'MAKE DUMP';  Source = 'MAKE DUMP.S';  Object = 'MAKE DUMP';  Answers = @() },
    @{ Name = 'KEYMAC';     Source = 'KEYMAC.S';     Object = 'KEYMAC';     Answers = @('SAVOBJ=0') },
    @{ Name = 'CLOCK.24';   Source = 'CLOCK.S';      Object = 'CLOCK.24';   Answers = @('SAVOBJ=0', 'VERSION=24') },
    @{ Name = 'CLOCK.12';   Source = 'CLOCK.S';      Object = 'CLOCK.12';   Answers = @('SAVOBJ=0', 'VERSION=12') },
    @{ Name = 'PRINTFILER'; Source = 'PRINTFILER.S'; Object = 'PRINTFILER'; Answers = @('FORMAT=1', 'MONITOR=0') }
)

if ($Configuration) {
    $exeCandidates = @((Join-Path $repoRoot "x64\$Configuration\CassoCli.exe"))
}
else {
    $exeCandidates = @(
        (Join-Path $repoRoot 'x64\Debug\CassoCli.exe'),
        (Join-Path $repoRoot 'x64\Release\CassoCli.exe')
    )
}

$cassoCli = $null

foreach ($candidate in $exeCandidates) {
    if (Test-Path $candidate) {
        $cassoCli = $candidate
        break
    }
}

if (-not $cassoCli) {
    Write-Host "CassoCli.exe not found. Run scripts/Build.ps1 first." -ForegroundColor Red
    exit 1
}

Write-Host "Using: $cassoCli"
Write-Host "Fixtures: $fixtures"
Write-Host ""

$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "merlin-oracles-$PID"
$passed  = 0
$failed  = 0

try {
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    foreach ($oracle in $oracles) {
        $sourcePath = Join-Path $fixtures $oracle.Source
        $objectPath = Join-Path $fixtures $oracle.Object
        $outputPath = Join-Path $tempDir  $oracle.Name

        if (-not (Test-Path -LiteralPath $sourcePath)) {
            Write-Host ("  FAIL  {0} -- source not found: {1}" -f $oracle.Name, $sourcePath) -ForegroundColor Red
            $failed++
            continue
        }

        $arguments = @('merlin', $sourcePath, '--dos-bin', '-o', $outputPath)

        foreach ($answer in $oracle.Answers) {
            $arguments += @('-d', $answer)
        }

        & $cassoCli @arguments 2>&1 | Out-Null

        if ($LASTEXITCODE -ne 0) {
            Write-Host ("  FAIL  {0} -- CassoCli exited {1}" -f $oracle.Name, $LASTEXITCODE) -ForegroundColor Red
            $failed++
            continue
        }

        $produced = [System.IO.File]::ReadAllBytes($outputPath)
        $shipped  = [System.IO.File]::ReadAllBytes($objectPath)

        if ($produced.Length -ne $shipped.Length) {
            Write-Host ("  FAIL  {0} -- {1} bytes produced, {2} shipped" -f $oracle.Name, $produced.Length, $shipped.Length) -ForegroundColor Red
            $failed++
            continue
        }

        # The first differing offset, not a count. A count says how bad it is;
        # an offset says where to look, and the header lives in the first four
        # bytes so an origin fault and a code fault land in different places.
        $firstDifference = -1

        for ($i = 0; $i -lt $shipped.Length; $i++) {
            if ($produced[$i] -ne $shipped[$i]) {
                $firstDifference = $i
                break
            }
        }

        if ($firstDifference -ge 0) {
            Write-Host ("  FAIL  {0} -- differs at offset {1}: produced \${2:X2}, shipped \${3:X2}" -f `
                $oracle.Name, $firstDifference, $produced[$firstDifference], $shipped[$firstDifference]) -ForegroundColor Red
            $failed++
            continue
        }

        Write-Host ("  OK    {0} -- {1} bytes, byte for byte" -f $oracle.Name, $produced.Length) -ForegroundColor Green
        $passed++
    }
}
finally {
    if (Test-Path $tempDir) {
        Remove-Item -Recurse -Force $tempDir
    }
}

Write-Host ""
Write-Host ("Reproduced: {0}  Failed: {1}" -f $passed, $failed)

if ($failed -gt 0) {
    Write-Host "Merlin oracle check FAILED." -ForegroundColor Red
    exit 1
}

Write-Host "Every shipped object reproduced from its vendor source." -ForegroundColor Green
exit 0
