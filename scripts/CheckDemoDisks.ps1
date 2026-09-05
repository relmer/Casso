<#
.SYNOPSIS
    Checks that the committed demo disks are what their sources build.

.DESCRIPTION
    Every folder under Apple2\Demos that carries a Build.ps1 is rebuilt from
    scratch, and the result is compared against what is committed. A difference
    means someone edited a source and did not rebuild, so the disk in the tree
    is stale.

    THE CASE THIS EXISTS FOR is the speech demo, whose data tables are emitted
    by gen-speech-demo-data.py rather than written by hand. Editing a line of
    speech changes nothing until the generator runs, and nothing about the edit
    says so: the .a65 still assembles, the disk still boots, and it still says
    the old line. Nobody was asking that question.

    casso-rocks is covered here as well, since it gained a Build.ps1 when its
    sources moved into a folder of their own. scripts/BuildDemoDisk.ps1 -Verify
    still runs in CI beside this, and the overlap is worth its few seconds: it
    verifies the image in memory and never writes, so it holds even if the
    capture-and-restore below were ever to go wrong.

    NOTHING IS LEFT BEHIND. Verifying has to build, and building writes into the
    tree, so every tracked file under Apple2\Demos is captured first and put
    back afterwards whatever the outcome. A checker that reports drift by
    leaving drift behind is a checker that fails the working-tree gate for its
    own reasons.

.PARAMETER Configuration
    Which CassoCli build the demo scripts should use. Default: Release.

.EXAMPLE
    scripts/CheckDemoDisks.ps1 -Configuration Debug
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$demoDir  = Join-Path $repoRoot 'Apple2\Demos'

Push-Location $repoRoot

try
{
    $builders = Get-ChildItem $demoDir -Recurse -Filter 'Build.ps1' -File | Sort-Object FullName

    if (-not $builders)
    {
        Write-Host 'CheckDemoDisks: no demo folder carries a Build.ps1 -- nothing to check.'
        exit 0
    }

    #  Byte-for-byte, before anything is built. Tracked files only: an
    #  untracked .bin left beside a source is an intermediate, not an artifact.
    $tracked  = @(git ls-files -- Apple2/Demos)
    $snapshot = @{}

    foreach ($rel in $tracked)
    {
        $full = Join-Path $repoRoot $rel

        if (Test-Path $full)
        {
            $snapshot[$rel] = [IO.File]::ReadAllBytes($full)
        }
    }

    Write-Host "CheckDemoDisks: rebuilding $($builders.Count) demo folder(s) over $($snapshot.Count) tracked file(s)."

    #  A builder's chatter is only interesting when it failed. CassoCli exits
    #  non-zero on a mere unused-label warning, so the failure signal is the
    #  throw, not the exit code -- which is also why this script ends on an
    #  explicit exit rather than letting the last tool's code leak out.
    foreach ($b in $builders)
    {
        $log = $null

        try
        {
            $log = & $b.FullName -Force -Configuration $Configuration 2>&1
        }
        catch
        {
            #  Restored by the finally below, which is why this may simply
            #  leave. Exiting from here without one left every disk built
            #  before the failing one sitting modified in the tree, and CI's
            #  working-tree check then failed for that instead of for the
            #  build, which buried the error that actually mattered.
            Write-Host "CheckDemoDisks: $($b.FullName) failed." -ForegroundColor Red
            $log | ForEach-Object { Write-Host "  $_" }
            Write-Host "  $_" -ForegroundColor Red
            exit 1
        }
    }

    #  Compare here, restore in the finally. Every way out of this script --
    #  a stale disk, a failed build, Ctrl+C -- passes through there.
    $stale = @()

    foreach ($rel in $snapshot.Keys)
    {
        $full = Join-Path $repoRoot $rel

        if (-not (Test-Path $full))
        {
            $stale += "$rel (the rebuild did not produce it)"
            continue
        }

        $now = [IO.File]::ReadAllBytes($full)

        if ($now.Length -ne $snapshot[$rel].Length -or
            [Convert]::ToBase64String($now) -ne [Convert]::ToBase64String($snapshot[$rel]))
        {
            $stale += $rel
        }
    }

    if ($stale)
    {
        Write-Host ''
        Write-Host 'These committed files are not what their sources build:' -ForegroundColor Red

        foreach ($rel in ($stale | Sort-Object))
        {
            Write-Host "  $rel" -ForegroundColor Yellow
        }

        Write-Host ''
        Write-Host 'Run the Build.ps1 in that demo folder and commit what it changes.' -ForegroundColor Red
        Write-Host 'The working tree was left exactly as it was found.'
        exit 1
    }

    Write-Host "CheckDemoDisks: every committed demo disk matches its sources -- OK." -ForegroundColor Green
    exit 0
}
finally
{
    #  Whatever happened, the tree goes back exactly as it was found. $snapshot
    #  is empty until it has been filled, so an early exit before that restores
    #  nothing, which is correct: nothing had been built yet either.
    if ($snapshot)
    {
        foreach ($rel in $snapshot.Keys)
        {
            [IO.File]::WriteAllBytes((Join-Path $repoRoot $rel), $snapshot[$rel])
        }
    }

    Pop-Location
}
