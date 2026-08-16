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
    Path to the Merlin Pro disk image. Defaults to DevDisks/Merlin-proDos2.23.dsk
    relative to the repository root.

.PARAMETER SourceName
    Name of the source file on the Merlin disk.

.PARAMETER ObjectName
    Name of the assembled object file on the Merlin disk.

.PARAMETER Expected
    Path to the source as it was INTENDED, for -Verify to compare against.

.PARAMETER Verify
    Reads the source back off the disk and compares it against Expected.

    Its PURPOSE is to obtain the canonical source, not to prove the paste was
    byte-exact. What Merlin assembled is the copy on the disk, so that is the
    only text guaranteed to correspond to the captured bytes, and that is the
    copy that gets committed -- written to -Canonical.

    A mismatch is therefore INFORMATION, not a failed capture. Merlin's editor is
    column-oriented over a high-bit, CR-terminated format, and normalizing
    whitespace or column positions on save would be entirely ordinary. Reported
    loudly and left for a human to judge; committing the disk copy makes the
    entry self-consistent either way.

    Issue #110 reports the guest paste path garbling input, so a mismatch may
    also be a genuine garble. The difference matters to what the entry TESTS, not
    to whether it is consistent -- see the procedure's note on the residual gap.

.PARAMETER Canonical
    Where to write the source as read back off the disk. This is the copy to
    commit alongside the captured bytes.

.PARAMETER ConfirmAbsent
    Assert that ObjectName is NOT on the disk, which is the precondition for
    capture. Delete the target from within DOS before every assembly and confirm
    it here: absence afterwards then proves the assembly produced nothing, and
    presence proves THIS assembly wrote it.

    DOS 3.3 catalogs carry no timestamps, so this is the only freshness check
    available. Without it, an assembly that errors before saving leaves the
    PREVIOUS entry's object file in place, and capturing it records one entry's
    bytes as another's expectation -- self-consistent, plausible, wrong, and it
    will never fail, because the assembler faithfully reproduces the first
    entry's bytes from the first entry's constructs.

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

    [string]$Canonical = '',

    [switch]$Verify,

    [switch]$ConfirmAbsent
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot  = Split-Path -Parent $PSScriptRoot
$extractor = Join-Path $PSScriptRoot 'ExtractDos33File.ps1'

$searched = @()

if (-not $MerlinImage)
{
    $candidates = @(Join-Path $repoRoot 'DevDisks\Merlin-proDos2.23.dsk')

    # A linked worktree has its own root, but the disk lives once beside the
    # PRIMARY working tree -- worktrees share a filesystem, not a directory. The
    # common git dir's parent is that primary tree.
    $commonDir = (git rev-parse --path-format=absolute --git-common-dir 2>$null)

    if ($LASTEXITCODE -eq 0 -and $commonDir)
    {
        $primaryRoot = Split-Path -Parent $commonDir
        $candidates += Join-Path $primaryRoot 'DevDisks\Merlin-proDos2.23.dsk'
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
    throw "No Merlin Pro disk image found. $where`n`nIt is commercial software and is never committed -- supply your own copy, the way machine ROMs work. See UnitTest/MerlinCorpus/README.md."
}

if ($ConfirmAbsent)
{
    if (-not $ObjectName)
    {
        throw '-ConfirmAbsent needs -ObjectName (the target you deleted from within DOS).'
    }

    # Attempt an extraction rather than scanning the printed catalog. Two
    # reasons: the listing goes to the host and cannot be captured cleanly, and
    # substring matching would be WRONG -- looking for "PI.ADD" in catalog text
    # also matches "PI.ADD.S", so a check meant to prove absence would report a
    # file present that is not there. Extraction matches the name exactly.
    $probe   = Join-Path ([System.IO.Path]::GetTempPath()) "merlin-absent-probe-$Entry.bin"
    $present = $true

    try
    {
        & $extractor -Image $MerlinImage -Name $ObjectName -OutFile $probe -Raw *> $null
    }
    catch
    {
        $present = $false
    }

    if (Test-Path -LiteralPath $probe)
    {
        Remove-Item -LiteralPath $probe -Force
    }

    if ($present)
    {
        throw "'$ObjectName' is still on the disk. Delete it from within DOS before assembling -- otherwise a failed assembly leaves the previous object in place and you capture the wrong entry's bytes."
    }

    Write-Host "'$ObjectName' is absent -- safe to assemble." -ForegroundColor Green
    Write-Host '  Anything present afterwards was written by THIS assembly.' -ForegroundColor DarkGray
    return
}

if ($Verify)
{
    if (-not $SourceName -or -not $Expected)
    {
        throw '-Verify needs both -SourceName (the file on the Merlin disk) and -Expected (the source as intended).'
    }

    # The disk copy is the ARTIFACT, not a check result: it is what Merlin
    # actually assembled, so it is the only text guaranteed to correspond to the
    # bytes captured from it. Written where the developer can commit it.
    $roundTripped = if ($Canonical) { $Canonical } else { Join-Path ([System.IO.Path]::GetTempPath()) "merlin-canonical-$Entry.txt" }

    & $extractor -Image $MerlinImage -Name $SourceName -OutFile $roundTripped | Out-Null

    $actualBytes   = [System.IO.File]::ReadAllBytes($roundTripped)
    $expectedBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Expected).Path)

    # Merlin stores source with CR line endings; normalize both sides so the
    # comparison reports real corruption rather than a line-ending difference.
    $actualText   = ([System.Text.Encoding]::ASCII.GetString($actualBytes)   -replace "`r`n", "`n") -replace "`r", "`n"
    $expectedText = ([System.Text.Encoding]::ASCII.GetString($expectedBytes) -replace "`r`n", "`n") -replace "`r", "`n"

    if ($actualText -ceq $expectedText)
    {
        Write-Host "Round trip CLEAN for '$Entry' -- the disk copy matches what you pasted." -ForegroundColor Green
        Write-Host "  Canonical source: $roundTripped" -ForegroundColor DarkGray
        return
    }

    Write-Host "DIFFERS for '$Entry' -- REVIEW REQUIRED, but not necessarily a failed capture." -ForegroundColor Yellow
    Write-Host '  Commit the DISK copy either way: it is what Merlin assembled, so it' -ForegroundColor DarkGray
    Write-Host '  matches the captured bytes by construction.' -ForegroundColor DarkGray
    Write-Host '  Judge which of these it is:' -ForegroundColor DarkGray
    Write-Host '    * editor normalization (whitespace or column positions) -- expected, benign' -ForegroundColor DarkGray
    Write-Host '    * a garbled paste (issue #110) -- the entry is still self-consistent,' -ForegroundColor DarkGray
    Write-Host '      but it tests a construct you did not intend' -ForegroundColor DarkGray

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

    # Loud, but NOT fatal. Making routine editor normalization fail the capture
    # invites loosening the comparison, which removes the guard entirely -- and
    # the guard is worth keeping for the case it can still catch.
    Write-Host ''
    Write-Host "  Canonical source written to: $roundTripped" -ForegroundColor Cyan
    Write-Host '  Commit THAT file, not the text you pasted.' -ForegroundColor Cyan
    return
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
