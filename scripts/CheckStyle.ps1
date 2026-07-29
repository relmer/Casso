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
        # `programme` is spelled out as `programmes?` on purpose: a trailing
        # \w* would swallow "programmed" and "programmer", which are correct
        # American English (from "program", not "programme").
        Pattern = '(?i)\b(?:(?:colour|behaviour|centre|grey|initialise|optimise|cancelled|honour|favour|licence|modelled|labelled|signalled|catalogue)\w*|programmes?|analyse[drs]?)\b'
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
        # Producing S_FALSE overloads the return with a second, private
        # meaning -- "succeeded, but not the way you'd assume" -- which the
        # caller can only decode by reading the callee. Prefer an explicit
        # out-param or enum. TESTING for S_FALSE is not flagged: consuming an
        # external API that returns it leaves no choice.
        Id       = 'CS0009'
        Globs    = @('*.cpp', '*.h')
        Pattern  = '\breturn\s+S_FALSE\b|(?<![=!<>])=\s*S_FALSE\b|,\s*S_FALSE\s*\)'
        Message  = 'producing S_FALSE -- use an explicit status enum/out-param, or mark the line // EHM-ALLOW-SFALSE: <reason>'
        Exclude  = @('CassoCore/Ehm.h')
        Suppress = 'EHM-ALLOW-SFALSE'
    },
    @{
        # An -Ex variant exists to REPLACE the family's default hr. Passing the
        # default back in says nothing the base macro does not already say:
        # CBREx (x, E_FAIL) is CBR (x).
        #
        # CPR needs no rule here: its -Ex variants no longer exist, so the
        # compiler rejects them outright, which beats a style check.
        Id      = 'CS0010'
        Globs   = @('*.cpp', '*.h')
        Pattern = 'CB[RW]?A?F?Ex\s*\(.*,\s*E_FAIL\s*[,)]'
        Message = 'redundant -Ex: that is the family default, so use the base macro'
        Exclude = @('CassoCore/Ehm.h')
    },
    @{
        Id      = 'CS0007'
        Globs   = @('*.cpp', '*.h')
        Pattern = '\((?:int|unsigned|float|double|char|bool|size_t|Word|Byte|SByte|u?int(?:8|16|32|64)_t)\)[A-Za-z_(]'
        Message = 'missing space after C-style cast -- write (int) value not (int)value'
        Exclude = @()
    },
    @{
        # A macro that wraps the call hides what failed, and buries a side
        # effect inside something that may not evaluate its argument once.
        # Assign to a local first, then check the local. Deliberately does not
        # match a bare identifier -- CHR (hr) is the whole point.
        # Only flags the shape where the macro's argument IS the call, so the
        # thing that failed is the thing being hidden. A call appearing inside
        # a larger condition -- raw.size() == kFoo, Peek() == '"' -- is left
        # alone: the condition still reads as a condition, and hoisting it
        # would cost clarity rather than buy any.
        Id      = 'CS0011'
        Globs   = @('*.cpp', '*.h')
        Pattern = '\bCHR[AFN]*(?:Ex)?\s*\(\s*[A-Za-z_][A-Za-z0-9_:]*(?:(?:\.|->)[A-Za-z0-9_]+)?\s*\('
        Message = 'CHR wraps a call -- hoist it into hr, then CHR (hr)'
        Exclude = @('CassoCore/Ehm.h')
    },
    @{
        # Ehm.h reaches every translation unit through its project's Pch, so a
        # direct include is redundant and drifts out of sync. Ehm.cpp
        # implements it and the Pch files are where it belongs.
        Id      = 'CS0012'
        Globs   = @('*.cpp', '*.h')
        Pattern = '#include\s*"(?:\.\./)*(?:CassoCore/)?Ehm\.h"'
        Message = 'Ehm.h comes from Pch.h -- do not include it directly'
        Exclude = @('CassoCore/Ehm.cpp', 'Pch.h')
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

function Get-ApplicableChecks
{
    param([string]$Path)

    $applicable = @()

    foreach ($check in $checks)
    {
        if (-not (Test-GlobMatch -Path $Path -Globs $check.Globs)) { continue }
        if (Test-Excluded -Path $Path -Exclude $check.Exclude)     { continue }

        $applicable += $check
    }

    return $applicable
}


####################################################################
#
#  Invoke-FileChecks
#
#  Runs the checks that apply to one file over a set of lines.
#
#  Glob and exclusion matching depend only on the PATH, so they are
#  resolved once per file rather than once per line -- the tree carries
#  ~400k lines, and re-deriving the applicable check list for each of
#  them dominated the run time.
#
####################################################################

function Invoke-FileChecks
{
    param([string]$Path, [object[]]$Lines, [System.Collections.Generic.List[object]]$Sink)

    if ($Path -like '*External/*') { return }

    $applicable = Get-ApplicableChecks -Path $Path
    if ($applicable.Count -eq 0) { return }

    foreach ($entry in $Lines)
    {
        foreach ($check in $applicable)
        {
            # A per-line opt-out, for checks that gate a judgment call rather
            # than a defect. Requires the author to write the marker on the
            # line, which is the "explicit approval" the rule is asking for.
            if ($check.Suppress -and $entry.Text -match $check.Suppress)
            {
                continue
            }

            if ($entry.Text -match $check.Pattern)
            {
                $Sink.Add([pscustomobject]@{
                    Id   = $check.Id
                    Text = "$Path`:$($entry.Line) -- $($check.Message)"
                })
            }
        }
    }
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

$sink = [System.Collections.Generic.List[object]]::new()

if ($Mode -eq 'Diff')
{
    $added        = Get-AddedLines -Base $Against -Tip $Revision
    $touchedFiles = @($added | Select-Object -ExpandProperty File -Unique)
    $scopeLabel   = "lines added between $Against and $Revision"

    foreach ($group in ($added | Group-Object File))
    {
        Invoke-FileChecks -Path $group.Name -Lines $group.Group -Sink $sink
    }
}
else
{
    $touchedFiles = @(git -C $repoRoot ls-files -- '*.cpp' '*.h' '*.md' ':(exclude)*External/*' 2>$null)
    $scopeLabel   = 'every tracked source file'

    foreach ($rel in $touchedFiles)
    {
        $full = Join-Path $repoRoot $rel
        if (-not (Test-Path -LiteralPath $full)) { continue }

        $raw   = @(Get-Content -LiteralPath $full)
        $lines = [System.Collections.Generic.List[object]]::new()

        for ($i = 0; $i -lt $raw.Length; $i++)
        {
            $lines.Add([pscustomobject]@{ Line = $i + 1; Text = $raw[$i] })
        }

        Invoke-FileChecks -Path $rel -Lines $lines -Sink $sink
    }
}

foreach ($b in (Test-PchFirst -Files $touchedFiles))
{
    $sink.Add([pscustomobject]@{ Id = 'CS0005'; Text = $b })
}

if (-not $SkipCommitCheck -and $Mode -eq 'Diff')
{
    foreach ($b in (Test-CommitMessages -Base $Against -Tip $Revision))
    {
        $sink.Add([pscustomobject]@{ Id = 'CS0008'; Text = $b })
    }
}

$violations = $sink

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
