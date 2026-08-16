<#
.SYNOPSIS
    Captures one Merlin corpus entry: source in, Merlin's bytes out.

.DESCRIPTION
    Offline capture tooling for the Merlin dialect corpus. See
    UnitTest/MerlinCorpus/README.md for the full procedure and for why the disk
    image is never committed.

    SKELETON. The disk-side steps are not implemented yet -- driving Merlin under
    Casso needs the emulator harness, and the source-in half is a manual editor
    step by design. What works today is -Verify, which is the half that must not
    be skipped: it round-trips source that is already on the disk and reports
    whether it survived intact.

    The read-back half delegates to ExtractDos33File.ps1, which is throwaway
    tooling that works only because the Merlin disk is a flat DOS-order image. It
    does not stand in for 020's disk get.

.PARAMETER Entry
    Corpus entry name. Becomes the identifier in CorpusEntries.h.

.PARAMETER MerlinImage
    Path to the Merlin 8 disk image. Defaults to DevDisks/Merlin8-v2.47.do
    relative to the repository root.

.PARAMETER SourceName
    Name of the source file on the Merlin disk.

.PARAMETER ObjectName
    Name of the assembled object file on the Merlin disk.

.PARAMETER Expected
    Path to the source as it was INTENDED, for -Verify to compare against.

.PARAMETER Verify
    Round-trip check only: extract SourceName and compare it against Expected.
    Run this before trusting any captured bytes -- issue #110 reports the guest
    paste path garbling input, and a garbled paste captured as an expectation is
    worse than no entry at all.

.EXAMPLE
    ./scripts/CaptureMerlinCorpus.ps1 -Entry strings -SourceName STRINGS.S -Expected corpus/strings.s -Verify
    Confirms the pasted source survived before anything is captured from it.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Entry,

    [string]$MerlinImage = '',

    [string]$SourceName = '',

    [string]$ObjectName = '',

    [string]$Expected = '',

    [switch]$Verify
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot  = Split-Path -Parent $PSScriptRoot
$extractor = Join-Path $PSScriptRoot 'ExtractDos33File.ps1'

$searched = @()

if (-not $MerlinImage)
{
    $candidates = @(Join-Path $repoRoot 'DevDisks\Merlin8-v2.47.do')

    # A linked worktree has its own root, but the disk lives once beside the
    # PRIMARY working tree -- worktrees share a filesystem, not a directory. The
    # common git dir's parent is that primary tree.
    $commonDir = (git rev-parse --path-format=absolute --git-common-dir 2>$null)

    if ($LASTEXITCODE -eq 0 -and $commonDir)
    {
        $primaryRoot = Split-Path -Parent $commonDir
        $candidates += Join-Path $primaryRoot 'DevDisks\Merlin8-v2.47.do'
    }

    foreach ($candidate in $candidates)
    {
        $searched += $candidate

        if (Test-Path -LiteralPath $candidate)
        {
            $MerlinImage = $candidate
            break
        }
    }
}

if (-not $MerlinImage -or -not (Test-Path -LiteralPath $MerlinImage))
{
    $where = if ($searched) { "Looked in:`n  " + ($searched -join "`n  ") } else { "Looked for '$MerlinImage'." }
    throw "No Merlin 8 disk image found. $where`n`nIt is commercial software and is never committed -- supply your own copy, the way machine ROMs work. See UnitTest/MerlinCorpus/README.md."
}

if ($Verify)
{
    if (-not $SourceName -or -not $Expected)
    {
        throw '-Verify needs both -SourceName (the file on the Merlin disk) and -Expected (the source as intended).'
    }

    $roundTripped = Join-Path ([System.IO.Path]::GetTempPath()) "merlin-roundtrip-$Entry.txt"

    & $extractor -Image $MerlinImage -Name $SourceName -OutFile $roundTripped | Out-Null

    $actualBytes   = [System.IO.File]::ReadAllBytes($roundTripped)
    $expectedBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Expected).Path)

    # Merlin stores source with CR line endings; normalize both sides so the
    # comparison reports real corruption rather than a line-ending difference.
    $actualText   = ([System.Text.Encoding]::ASCII.GetString($actualBytes)   -replace "`r`n", "`n") -replace "`r", "`n"
    $expectedText = ([System.Text.Encoding]::ASCII.GetString($expectedBytes) -replace "`r`n", "`n") -replace "`r", "`n"

    if ($actualText -ceq $expectedText)
    {
        Write-Host "Round trip CLEAN for '$Entry' -- the paste survived intact." -ForegroundColor Green
        Write-Host '  Bytes captured from this source can be trusted.' -ForegroundColor DarkGray
        return
    }

    Write-Host "Round trip MISMATCH for '$Entry' -- do NOT capture from this source." -ForegroundColor Red

    $actualLines   = $actualText   -split "`n"
    $expectedLines = $expectedText -split "`n"
    $lineCount     = [Math]::Max($actualLines.Count, $expectedLines.Count)

    foreach ($i in 0..($lineCount - 1))
    {
        $a = if ($i -lt $actualLines.Count)   { $actualLines[$i]   } else { '<missing>' }
        $e = if ($i -lt $expectedLines.Count) { $expectedLines[$i] } else { '<missing>' }

        if ($a -cne $e)
        {
            Write-Host ("  line {0}:" -f ($i + 1)) -ForegroundColor Yellow
            Write-Host ("    intended: {0}" -f $e) -ForegroundColor DarkGray
            Write-Host ("    on disk : {0}" -f $a) -ForegroundColor DarkGray
        }
    }

    throw "Source round trip failed for '$Entry'. Re-enter the source and verify again before capturing (see issue #110)."
}

throw @"
Capture is not implemented yet -- only -Verify is.

The remaining steps are manual, and deliberately so while the harness is being
built. See UnitTest/MerlinCorpus/README.md:

  1. Enter the source into Merlin's editor and save it to the disk.
  2. Re-run this script with -Verify to prove the paste survived.
  3. Assemble under Merlin with the listing on and save the object.
  4. Extract it:
       ./scripts/ExtractDos33File.ps1 -Image '$MerlinImage' -Name $ObjectName
  5. Record source, bytes, and the exact Merlin version in CorpusEntries.h.
"@
