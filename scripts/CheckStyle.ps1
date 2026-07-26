<#
.SYNOPSIS
    Fails if changed code violates the mechanically-checkable subset of the
    Casso coding standards in .github/copilot-instructions.md.

.DESCRIPTION
    The standards document is enforced by hand today, and it has not held:
    the space-before-empty-parens rule has been on the books since
    2026-05-13 and the violation count rose from 2,711 to 3,321 lines in
    the ten weeks after. This script is the mechanical half of the fix.

    It checks only rules that reduce to a regex with near-zero false-
    positive risk. Rules needing judgment -- column alignment, the exact
    5-blank-line / 3-blank-line spacing rules, "no magic numbers", EHM
    single-exit-point, declarations-at-top-of-function -- are out of scope
    and remain review's job. Roughly 8 of ~30 documented rules are covered,
    but they are the ones the measured drift is concentrated in.

    DEFAULT MODE IS `Diff`, and that is deliberate. The tree currently
    carries thousands of pre-existing violations, so a whole-tree gate
    would fail on the first push and be switched off the same day. Diff
    mode inspects only lines the push ADDS, which stops the bleeding
    without requiring a big-bang sweep first. Once a rule has been swept
    to zero tree-wide it can be promoted by running this script in `Tree`
    mode from CI.

.PARAMETER Mode
    `Diff`  (default) -- check only lines added between -Against and
                         -Revision.
    `Tree`  -- check every tracked file. Expect existing violations until
               the backlog sweep lands.

.PARAMETER Against
    Base ref for Diff mode. Defaults to origin/master, then master.

.PARAMETER Revision
    Tip ref for Diff mode. Default HEAD.

.PARAMETER SkipCommitCheck
    Skip the commit-message check (useful when running against a working
    tree with no new commits).

.OUTPUTS
    Exit code 0 when clean, 1 on any violation. Violations print as
    `file:line -- message` on stderr, prefixed with an `error CSnnnn:`
    code so MSBuild and CI surface them as clickable errors.

.EXAMPLE
    scripts/CheckStyle.ps1
    Checks lines added on the current branch versus origin/master.

.EXAMPLE
    scripts/CheckStyle.ps1 -Mode Tree
    Full-tree audit -- reports the whole existing backlog.
#>
param(
    [ValidateSet('Diff', 'Tree')]
    [string]$Mode = 'Diff',

    [string]$Against = '',

    [string]$Revision = 'HEAD',

    [switch]$SkipCommitCheck
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

# Every check: a regex over single lines, the file globs it applies to,
# and paths where the pattern is legitimate. Keep patterns conservative --
# a noisy check gets disabled, which is worse than no check.
$checks = @(
    @{
        Id      = 'CS0001'
        Globs   = @('*.cpp')
        Pattern = '\w \(\)'
        Message = 'space before empty parens -- write fn() not fn ()'
        Exclude = @()
    },
    @{
        Id      = 'CS0002'
        Globs   = @('*.cpp', '*.h')
        Pattern = '^\s*namespace\s*\{\s*$|^\s*namespace\s*$'
        Message = 'anonymous namespace -- use file-scope static constexpr or class statics'
        Exclude = @()
    },
    @{
        Id      = 'CS0003'
        Globs   = @('*.cpp', '*.h', '*.md')
        Pattern = '(?i)\b(colour|behaviour|centre|grey|initialise|optimise|analyse|cancelled|honour|favour|licence|modelled|labelled|signalled|catalogue|programme)\w*'
        Message = 'British spelling -- American spelling is required everywhere'
        # The standards document has to spell the forbidden words out to
        # forbid them, so it can never satisfy its own rule.
        Exclude = @('.github/copilot-instructions.md')
    },
    @{
        Id      = 'CS0004'
        Globs   = @('*.cpp', '*.h')
        Pattern = '^\s*#include\s*<'
        Message = 'angle-bracket include outside Pch.h -- system headers belong in Pch.h'
        Exclude = @('Pch.h', 'Dxui/Dxui.h')
    },
    @{
        Id      = 'CS0006'
        Globs   = @('*.cpp', '*.h')
        Pattern = '(?<!_)\bgoto\s+Error'
        Message = 'bare goto Error -- use an EHM macro (CHR / CBR / CWRA / ...)'
        Exclude = @('CassoCore/Ehm.h', 'CassoCore/Ehm.cpp', 'UnitTest/EhmTestHelper.h', 'UnitTest/EhmTestHelper.cpp')
    },
    @{
        Id      = 'CS0007'
        Globs   = @('*.cpp', '*.h')
        Pattern = '\((?:int|unsigned|float|double|char|bool|size_t|Word|Byte|SByte|u?int(?:8|16|32|64)_t)\)[A-Za-z_(]'
        Message = 'missing space after C-style cast -- write (int) value not (int)value'
        Exclude = @()
    }
)

$violations = @()


####################################################################
#
#  Test-Excluded -- is this path exempt from a given check?
#
####################################################################

function Test-Excluded
{
    param([string]$Path, [string[]]$Exclude)

    $normalized = $Path -replace '\\', '/'

    foreach ($e in $Exclude)
    {
        if ($normalized -like "*$e") { return $true }
    }

    return $false
}


####################################################################
#
#  Test-GlobMatch -- does this path match any of the check's globs?
#
####################################################################

function Test-GlobMatch
{
    param([string]$Path, [string[]]$Globs)

    foreach ($g in $Globs)
    {
        if ($Path -like $g) { return $true }
    }

    return $false
}


####################################################################
#
#  Resolve-BaseRef -- pick the diff base when none was supplied.
#
####################################################################

function Resolve-BaseRef
{
    foreach ($candidate in @('origin/master', 'master'))
    {
        git -C $repoRoot rev-parse --verify --quiet $candidate 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) { return $candidate }
    }

    return ''
}


####################################################################
#
#  Get-AddedLines
#
#  Parses `git diff --unified=0` into {File, Line, Text} for every line
#  the range ADDS. Hunk headers give the starting line in the new file;
#  each following `+` line advances it. Removed lines are ignored -- a
#  push cannot be blamed for text it deletes.
#
####################################################################

function Get-AddedLines
{
    param([string]$Base, [string]$Tip)

    $results = @()
    $file    = ''
    $lineNo  = 0

    $diff = git -C $repoRoot diff --unified=0 --no-color --diff-filter=d "$Base...$Tip" 2>$null

    foreach ($raw in $diff)
    {
        if ($raw -match '^\+\+\+ b/(.*)$')
        {
            $file = $Matches[1]
            continue
        }

        if ($raw -match '^@@ -\d+(?:,\d+)? \+(\d+)')
        {
            $lineNo = [int]$Matches[1]
            continue
        }

        if ($raw.StartsWith('+') -and -not $raw.StartsWith('+++'))
        {
            if ($file -ne '')
            {
                $results += [pscustomobject]@{
                    File = $file
                    Line = $lineNo
                    Text = $raw.Substring(1)
                }
            }
            $lineNo++
        }
    }

    return $results
}


####################################################################
#
#  Get-TreeLines -- every line of every tracked file, for Tree mode.
#
####################################################################

function Get-TreeLines
{
    $results = @()
    $tracked = git -C $repoRoot ls-files -- '*.cpp' '*.h' '*.md' ':(exclude)*External/*' 2>$null

    foreach ($rel in $tracked)
    {
        $full = Join-Path $repoRoot $rel
        if (-not (Test-Path -LiteralPath $full)) { continue }

        $lines = Get-Content -LiteralPath $full
        for ($i = 0; $i -lt $lines.Length; $i++)
        {
            $results += [pscustomobject]@{
                File = $rel
                Line = $i + 1
                Text = $lines[$i]
            }
        }
    }

    return $results
}


####################################################################
#
#  Test-PchFirst
#
#  Structural rule that no single-line regex can express: every .cpp
#  must include "Pch.h" as its FIRST include. Checked per touched file
#  rather than per line.
#
####################################################################

function Test-PchFirst
{
    param([string[]]$Files)

    $bad = @()

    foreach ($rel in $Files)
    {
        if ($rel -notlike '*.cpp')      { continue }
        if ($rel -like '*External/*')   { continue }

        $full = Join-Path $repoRoot $rel
        if (-not (Test-Path -LiteralPath $full)) { continue }

        $lines = Get-Content -LiteralPath $full
        for ($i = 0; $i -lt $lines.Length; $i++)
        {
            if ($lines[$i] -notmatch '^\s*#include') { continue }

            if ($lines[$i] -notmatch '^\s*#include\s+"[^"]*Pch\.h"')
            {
                $bad += "${rel}:$($i + 1) -- Pch.h must be the first #include in every .cpp"
            }
            break
        }
    }

    return $bad
}


####################################################################
#
#  Test-CommitMessages
#
#  The repo forbids Claude / Claude Code attribution in commit
#  messages and PR bodies. Checked over the commits being pushed.
#
####################################################################

function Test-CommitMessages
{
    param([string]$Base, [string]$Tip)

    $bad     = @()
    $shas    = git -C $repoRoot rev-list "$Base..$Tip" 2>$null

    foreach ($sha in $shas)
    {
        $body = git -C $repoRoot log -1 --format=%B $sha 2>$null | Out-String

        if ($body -imatch 'Co-Authored-By:\s*Claude|Generated with \[Claude Code\]|noreply@anthropic\.com')
        {
            $short = $sha.Substring(0, 8)
            $bad  += "commit ${short} -- Claude attribution is not permitted in commit messages"
        }
    }

    return $bad
}



####################################################################
#
#  Main
#
####################################################################

if ($Mode -eq 'Diff')
{
    # An unresolvable base makes `git diff` fail, which would otherwise
    # yield zero candidate lines and report a cheerful OK -- a gate that
    # passes when its own diff broke is worse than no gate. Resolve it
    # explicitly and say SKIPPED, loudly, if there is nothing to compare.
    if (-not [string]::IsNullOrEmpty($Against))
    {
        git -C $repoRoot rev-parse --verify --quiet "$Against^{commit}" 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0)
        {
            [Console]::Error.WriteLine("CheckStyle: base ref '$Against' does not resolve; falling back.")
            $Against = ''
        }
    }

    if ([string]::IsNullOrEmpty($Against))
    {
        $Against = Resolve-BaseRef
    }

    if ([string]::IsNullOrEmpty($Against))
    {
        [Console]::Error.WriteLine("CheckStyle: SKIPPED -- no resolvable base ref (origin/master or master) to diff against.")
        exit 0
    }
}

if ($Mode -eq 'Diff')
{
    $candidates    = Get-AddedLines -Base $Against -Tip $Revision
    $touchedFiles  = $candidates | Select-Object -ExpandProperty File -Unique
    $scopeLabel    = "lines added between $Against and $Revision"
}
else
{
    $candidates    = Get-TreeLines
    $touchedFiles  = $candidates | Select-Object -ExpandProperty File -Unique
    $scopeLabel    = 'every tracked source file'
}

foreach ($c in $candidates)
{
    if ($c.File -like '*External/*') { continue }

    foreach ($check in $checks)
    {
        if (-not (Test-GlobMatch -Path $c.File -Globs $check.Globs))    { continue }
        if (Test-Excluded -Path $c.File -Exclude $check.Exclude)        { continue }

        if ($c.Text -match $check.Pattern)
        {
            $violations += [pscustomobject]@{
                Id      = $check.Id
                Text    = "$($c.File):$($c.Line) -- $($check.Message)"
            }
        }
    }
}

foreach ($b in (Test-PchFirst -Files $touchedFiles))
{
    $violations += [pscustomobject]@{ Id = 'CS0005'; Text = $b }
}

if (-not $SkipCommitCheck -and $Mode -eq 'Diff')
{
    foreach ($b in (Test-CommitMessages -Base $Against -Tip $Revision))
    {
        $violations += [pscustomobject]@{ Id = 'CS0008'; Text = $b }
    }
}

if ($violations.Count -gt 0)
{
    foreach ($v in $violations)
    {
        [Console]::Error.WriteLine("error $($v.Id): $($v.Text)")
    }

    Write-Host ""
    Write-Host "CheckStyle: FAILED -- $($violations.Count) violation(s) in $scopeLabel." -ForegroundColor Red
    Write-Host "Rules: .github/copilot-instructions.md. To audit the whole backlog: scripts/CheckStyle.ps1 -Mode Tree" -ForegroundColor Red
    exit 1
}

Write-Host "CheckStyle: $($touchedFiles.Count) file(s) checked over $scopeLabel -- OK."
exit 0
