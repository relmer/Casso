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

    [switch]$SkipCommitCheck,

    # The structural rules (CS0014/15/16) -- the ones a per-line pattern cannot
    # see, because the evidence is a run of blank lines or the absence of a
    # banner rather than anything written on one line.
    #
    # Armed. They shipped off by default while their backlog stood at 1,882
    # tree-wide, since a gate that fails on pre-existing violations is one
    # people switch off. That backlog is now zero over all tracked files, so
    # the switch inverts: `-NoStructural` opts out.
    [switch]$NoStructural
)

$Structural = -not $NoStructural

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
        # `cancelled` is deliberately absent: the doubled L is standard American
        # usage as well, just less common than `canceled`. Flagging it also meant
        # fighting the Win32 ERROR_CANCELLED family, which cannot be renamed.
        #
        # No leading \b -- it made the rule blind to anything behind a prefix or
        # inside PascalCase, which hid 11 real hits (unmodelled, truecolour,
        # recentre, CompositeColoursFollowOverprint, ...) while the gate read
        # green. The lookbehind keeps SCREAMING_SNAKE API names out of scope,
        # which is the only thing the anchor was really protecting.
        Pattern = '(?i)(?<!_)(?:(?:colour|behaviour|centre|grey|initialise|optimise|honour|favour|licence|modelled|labelled|signalled|catalogue)\w*|programmes?|analyse[drs]?)\b'
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
        # Section headers decorated with dashes -- // ---- foo ---------- or
        # // foo ------------- -- are not a sanctioned comment style. The one
        # block-comment form is the 80-slash banner; anything a banner is too
        # heavy for is a plain // comment.
        #
        # Deliberately matches only a dash-DECORATED LABEL, never a bare run of
        # dashes. A line that is nothing but dashes is usually prose inside a
        # banner -- a heading underline, or the rule under an ASCII table
        # header (DxuiTreeView.h) -- and banning those would be wrong.
        Id      = 'CS0013'
        Globs   = @('*.cpp', '*.h')
        # THREE dashes minimum, never two. `--` is this codebase's em-dash and
        # opens many a wrapped comment line ("-- the caller treats absent
        # ..."), and `--trace` / `--disk2` name real command-line flags. A
        # two-dash rule mangles all of those.
        #
        # The text class excludes dashes on purpose: with a bare \S the
        # [-=]{3,} run backtracks by one and the final dash matches as "text",
        # so every heading underline gets flagged.
        Pattern = '^\s*//\s*[-=]{3,}\s*[^-=\s].*$|^\s*//\s*[^-=\s].*\s[-=]{4,}\s*$'
        Message = 'dash-decorated section comment -- use a plain // comment, or an 80-slash banner'
        Exclude = @()
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
#  Test-EhmConditionCalls  (CS0011)
#
#  An EHM macro's CONDITION may not contain a call. The macro hides
#  what failed, and buries a side effect inside something that may
#  not evaluate its argument once -- so the guard tests a named local
#  saying what is being checked, and the bail names the condition.
#
#  The ban is absolute, including .empty() / .size() / .good(). That
#  is deliberate: the judgment version ("only calls that do work")
#  cannot be expressed as a check, sat unenforced in the standards
#  text, and had drifted to 88 sites before anyone counted.
#
#  Only the condition is checked. An ACTION argument is normally a
#  call and stays exempt -- CBRF (isComma, SetError ("...")).
#
#  Not a regex rule: the condition ends at the first TOP-LEVEL comma,
#  which means tracking paren depth and skipping string/char literals.
#  CBRF (Peek() == ',', SetError (...)) has a comma inside a literal
#  and a line pattern gets it wrong.
#
####################################################################

# Function-like macros that expand to a comparison, not a call.
$script:EhmAllowedMacros = @('BCRYPT_SUCCESS', 'NT_SUCCESS', 'HRESULT_FROM_WIN32', 'IID_PPV_ARGS')

# Keywords and casts that take parens but are not calls.
$script:EhmNotCalls = @('sizeof', 'return', 'if', 'while', 'for', 'switch', 'defined',
                        'static_cast', 'reinterpret_cast', 'const_cast', 'dynamic_cast')

####################################################################
#
#  Hide-Comments
#
#  Blanks comment bodies with spaces, preserving length and newlines
#  so offsets and line numbers still line up.
#
#  Needed because the scanner reads whole-file text: without this it
#  flagged the doc comment on JsonValue::HasString, which quotes the
#  very pattern it exists to replace. A checker that fails a file for
#  describing the rule is one people switch off.
#
####################################################################

function Hide-Comments
{
    param([string] $Text)

    $sb    = New-Object System.Text.StringBuilder $Text.Length
    $i     = 0
    $inStr = $false
    $inChr = $false

    while ($i -lt $Text.Length)
    {
        $c    = $Text[$i]
        $next = if ($i + 1 -lt $Text.Length) { $Text[$i + 1] } else { [char]0 }
        $prev = if ($i -gt 0) { $Text[$i - 1] } else { [char]0 }
        $esc  = ($prev -eq '\')

        if ($inStr)     { [void] $sb.Append($c); if ($c -eq '"' -and -not $esc) { $inStr = $false }; $i++; continue }
        if ($inChr)     { [void] $sb.Append($c); if ($c -eq "'" -and -not $esc) { $inChr = $false }; $i++; continue }
        if ($c -eq '"') { $inStr = $true; [void] $sb.Append($c); $i++; continue }
        if ($c -eq "'") { $inChr = $true; [void] $sb.Append($c); $i++; continue }

        if ($c -eq '/' -and $next -eq '/')
        {
            while ($i -lt $Text.Length -and $Text[$i] -ne "`n") { [void] $sb.Append(' '); $i++ }
            continue
        }

        if ($c -eq '/' -and $next -eq '*')
        {
            while ($i -lt $Text.Length)
            {
                if ($Text[$i] -eq "`n") { [void] $sb.Append("`n") }
                else                    { [void] $sb.Append(' ')  }

                if ($Text[$i] -eq '*' -and $i + 1 -lt $Text.Length -and $Text[$i + 1] -eq '/')
                {
                    [void] $sb.Append(' '); $i += 2; break
                }
                $i++
            }
            continue
        }

        [void] $sb.Append($c)
        $i++
    }

    return $sb.ToString()
}


function Get-EhmConditionSpan
{
    param([string] $Text, [int] $OpenParenIndex)

    $depth = 0
    $i     = $OpenParenIndex
    $start = $OpenParenIndex + 1
    $inStr = $false
    $inChr = $false

    while ($i -lt $Text.Length)
    {
        $c    = $Text[$i]
        $prev = if ($i -gt 0) { $Text[$i - 1] } else { [char]0 }
        $esc  = ($prev -eq '\')

        if     ($inStr)     { if ($c -eq '"' -and -not $esc) { $inStr = $false } }
        elseif ($inChr)     { if ($c -eq "'" -and -not $esc) { $inChr = $false } }
        elseif ($c -eq '"') { $inStr = $true }
        elseif ($c -eq "'") { $inChr = $true }
        elseif ($c -eq '(') { $depth++ }
        elseif ($c -eq ')')
        {
            $depth--
            if ($depth -eq 0) { return $Text.Substring($start, $i - $start) }
        }
        elseif ($c -eq ',' -and $depth -eq 1)
        {
            return $Text.Substring($start, $i - $start)
        }

        $i++
    }

    return ''
}

function Test-EhmConditionCalls
{
    param([string[]]$Files)

    $bad = @()

    foreach ($rel in $Files)
    {
        if ($rel -notlike '*.cpp' -and $rel -notlike '*.h') { continue }
        if ($rel -like '*External/*')                       { continue }
        if ($rel -like '*CassoCore/Ehm.h')                  { continue }

        $full = Join-Path $repoRoot $rel
        if (-not (Test-Path -LiteralPath $full)) { continue }

        $text = Hide-Comments -Text ([IO.File]::ReadAllText($full))

        # SUCCEEDED / FAILED / IGNORE_RETURN_VALUE join the EHM families here.
        # They are result-testing macros too, so wrapping a call in one hides
        # exactly what the EHM rule exists to keep visible -- and with an out
        # param it hides where the result landed as well.
        #
        # Unit tests are NOT exempt, though the first cut of this rule excused
        # them. `Assert::IsTrue (SUCCEEDED (Foo()))` looked like the sanctioned
        # test idiom until the fix turned out to be better than the exemption:
        # AssertSucceeded (Foo()) reads the same and prints the HRESULT, which
        # Assert::IsTrue could not -- it only ever saw a bool.
        foreach ($m in [regex]::Matches($text, '\b(?:C[BWPH]R[AFN]*(?:Ex)?|SUCCEEDED|FAILED|IGNORE_RETURN_VALUE)\s*\('))
        {
            $open = $text.IndexOf('(', $m.Index)
            if ($open -lt 0) { continue }

            $cond = Get-EhmConditionSpan -Text $text -OpenParenIndex $open
            if ([string]::IsNullOrWhiteSpace($cond)) { continue }

            foreach ($call in [regex]::Matches($cond, '\b([A-Za-z_][A-Za-z0-9_]*(?:(?:::|\.|->)[A-Za-z0-9_]+)*)\s*\('))
            {
                $name = $call.Groups[1].Value
                $leaf = ($name -split '::|\.|->')[-1]

                if ($script:EhmNotCalls      -contains $leaf) { continue }
                if ($script:EhmAllowedMacros -contains $leaf) { continue }

                $line   = ($text.Substring(0, $m.Index) -split "`n").Count
                $macro  = $m.Value.TrimEnd(" ", "(")
                $bad   += "${rel}:${line} -- $macro condition calls $name -- hoist it into a named local, then test the local"
                break
            }
        }
    }

    return $bad
}


####################################################################
#
#  Test-Structure  (CS0014 / CS0015 / CS0016)
#
#  The three formatting rules that a per-line regex cannot see:
#
#    CS0014  a function definition in a .cpp has an 80-slash banner
#    CS0015  a top-level banner is preceded by EXACTLY 5 blank lines
#    CS0016  a declaration block is followed by EXACTLY 3 blank lines
#
#  These need whole-file context, which is why they sat un-gated: a
#  file-scoped check fails a push for violations the diff never touched,
#  and that is what made the first cut of CS0011 unusable.
#
#  The fix is to keep the ANALYSIS whole-file and scope the REPORTING.
#  Every finding carries the line range it is about; in Diff mode a
#  finding is reported only when the diff added a line inside that
#  range. Editing the body of an old function stays silent; adding a
#  function, or moving a banner, does not.
#
#  Ranges rather than single lines because the evidence spans lines: a
#  wrong banner gap is as much about the blank run as the banner, and a
#  reader who adds blanks above an existing banner has changed the thing
#  the rule is about without touching the banner itself.
#
####################################################################

function Test-Structure
{
    param([string[]] $Files, $AddedIndex)

    $bad = @()

    foreach ($rel in $Files)
    {
        if ($rel -notlike '*.cpp' -and $rel -notlike '*.h') { continue }
        if ($rel -like '*External/*')                       { continue }

        $full = Join-Path $repoRoot $rel
        if (-not (Test-Path -LiteralPath $full)) { continue }

        $lines    = [IO.File]::ReadAllLines($full)
        $findings = @()

        # ---- CS0015: blank run before an OPENING banner line -----------
        # Only the opening line: a banner is two rows of slashes and the
        # closing one always has zero blanks before it.
        $run = 0
        for ($i = 0; $i -lt $lines.Length; $i++)
        {
            if ($lines[$i].Trim() -eq '') { $run++; continue }

            $isBanner = ($lines[$i] -match '^/{60,}')
            $isOpener = $isBanner -and ($i + 1 -lt $lines.Length) -and ($lines[$i + 1] -match '^\s*//')

            if ($isOpener -and $i -gt 0 -and $run -ne 5)
            {
                $findings += [pscustomobject]@{
                    Id    = 'CS0015'
                    First = [Math]::Max(1, $i + 1 - $run)
                    Last  = $i + 1
                    Text  = "$($run) blank line(s) before a top-level banner -- exactly 5 required"
                }
            }
            $run = 0
        }

        # ---- CS0014 / CS0016: per function definition -------------------
        if ($rel -like '*.cpp')
        {
            for ($i = 1; $i -lt $lines.Length; $i++)
            {
                if ($lines[$i] -ne '{') { continue }

                $sig = $lines[$i - 1]
                if ($sig -notmatch '^[A-Za-z_~][A-Za-z0-9_:<>,&*\s]*\(')                                  { continue }
                if ($sig -match '^\s*(if|for|while|switch|else|do|struct|class|enum|namespace|union)\b')   { continue }

                # CS0014 -- banner anywhere in the 12 lines above
                $hasBanner = $false
                for ($k = [Math]::Max(0, $i - 12); $k -lt $i; $k++)
                {
                    if ($lines[$k] -match '^/{60,}') { $hasBanner = $true; break }
                }

                if (-not $hasBanner)
                {
                    $findings += [pscustomobject]@{
                        Id    = 'CS0014'
                        First = $i
                        Last  = $i + 1
                        Text  = 'function definition has no //// comment banner'
                    }
                }

                # CS0016 -- declaration block then exactly 3 blank lines
                $d       = $i + 1
                $sawDecl = $false
                while ($d -lt $lines.Length -and
                       $lines[$d] -match '^\s{4}[A-Za-z_][A-Za-z0-9_:<>,\s\*&\[\]]*\s+\w+\s*(=[^;]*)?;\s*$' -and
                       $lines[$d] -notmatch '\breturn\b|\bdelete\b|\+\+|--')
                {
                    $sawDecl = $true; $d++
                }

                if ($sawDecl)
                {
                    $blanks = 0
                    while ($d + $blanks -lt $lines.Length -and $lines[$d + $blanks].Trim() -eq '') { $blanks++ }

                    # 0 blanks is the "declarations only, no body" shape.
                    if ($blanks -ne 3 -and $blanks -ne 0)
                    {
                        $findings += [pscustomobject]@{
                            Id    = 'CS0016'
                            First = $d
                            Last  = $d + $blanks + 1
                            Text  = "$blanks blank line(s) after the declaration block -- exactly 3 required"
                        }
                    }
                }
            }
        }

        # ---- report, scoped ---------------------------------------------
        foreach ($f in $findings)
        {
            if ($null -ne $AddedIndex)
            {
                $touched = $false
                for ($n = $f.First; $n -le $f.Last; $n++)
                {
                    if ($AddedIndex.Contains("$rel|$n")) { $touched = $true; break }
                }
                if (-not $touched) { continue }
            }

            $bad += [pscustomobject]@{ Id = $f.Id; Text = "${rel}:$($f.Last) -- $($f.Text)" }
        }
    }

    return $bad
}


####################################################################
#
#  Test-CommitMessages
#
#  The repo forbids Claude / Claude Code attribution in commit
#  messages and PR bodies, and requires American spelling everywhere --
#  which includes the message, not just the files it carries.
#
####################################################################

function Test-CommitMessages
{
    param([string]$Base, [string]$Tip)

    $bad = @()

    # The CS0003 word list, reused rather than restated: a commit message is
    # prose the project ships. The file rules only ever looked at *.cpp / *.h /
    # *.md, which is how a "Behaviour" reached origin in 7c18ea45.
    $britishRule = $checks | Where-Object { $_.Id -eq 'CS0003' } | Select-Object -First 1

    # A silent miss would be worse than no check: looking the rule up by id
    # means a rename turns the gate off without failing anything.
    if ($null -eq $britishRule)
    {
        return @('CheckStyle internal error -- CS0003 rule not found for the commit-message scan')
    }

    # Messages are gated over UNPUSHED commits only, unlike the file rules which
    # span the branch. A message is effectively immutable once pushed -- fixing
    # one means rewriting history -- so branch-wide scoping would leave the gate
    # permanently red on anything already out there, which is exactly the
    # baseline problem that gets a check switched off. The remote tracking ref
    # is the honest base, falling back to $Base on a never-pushed branch.
    $upstream = git -C $repoRoot rev-parse --abbrev-ref '@{upstream}' 2>$null

    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($upstream))
    {
        $Base = $upstream.Trim()
    }

    $shas = git -C $repoRoot rev-list "$Base..$Tip" 2>$null

    # Grandfathered messages: already published, so unfixable without rewriting
    # pushed history, and the opt-out below did not exist when they landed.
    #
    #   b9bbb6e3  the British-spelling sweep itself. Its message quotes the
    #             words it replaced (Colour->Color, COLOUR->COLOR, ...) --
    #             precisely the "a rule has to be able to describe itself" case
    #             that STYLE-ALLOW-BRITISH was added for, only it landed first.
    #
    # Note this list is only reachable because the upstream scoping above
    # misses cross-branch publication: b9bbb6e3 was pushed on its feature
    # branch, never on master, so master's first push re-scans it as "unpushed".
    $grandfathered = @('b9bbb6e3')

    foreach ($sha in $shas)
    {
        $body  = git -C $repoRoot log -1 --format=%B $sha 2>$null | Out-String
        $short = $sha.Substring(0, 8)

        if ($body -imatch 'Co-Authored-By:\s*Claude|Generated with \[Claude Code\]|noreply@anthropic\.com')
        {
            $bad += "commit ${short} -- Claude attribution is not permitted in commit messages"
        }

        # STYLE-ALLOW-BRITISH opts a message out, following the per-line
        # Suppress precedent above. A spelling-sweep commit has to name the
        # words it is replacing, exactly as copilot-instructions.md does, and a
        # rule that cannot describe itself is one people route around.
        if ($body -match $britishRule.Pattern -and
            $body -notmatch 'STYLE-ALLOW-BRITISH' -and
            $short -notin $grandfathered)
        {
            $bad += "commit ${short} -- $($britishRule.Message) (found '$($Matches[0])')"
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

foreach ($b in (Test-EhmConditionCalls -Files $touchedFiles))
{
    $sink.Add([pscustomobject]@{ Id = 'CS0011'; Text = $b })
}

# Structural rules analyse the whole file but report only where the diff
# reached, so a pre-existing violation in a file you merely touched cannot
# block a push. Tree mode passes $null and reports everything.
$addedIndex = $null

if ($Mode -eq 'Diff')
{
    $addedIndex = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($a in $added) { [void] $addedIndex.Add("$($a.File)|$($a.Line)") }
}

if ($Structural)
{
    foreach ($b in (Test-Structure -Files $touchedFiles -AddedIndex $addedIndex))
    {
        $sink.Add([pscustomobject]@{ Id = $b.Id; Text = $b.Text })
    }
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
