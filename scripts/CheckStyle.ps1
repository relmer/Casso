<#
.SYNOPSIS
    Fails if changed code violates the mechanically-checkable subset of the
    Casso coding standards in .github/copilot-instructions.md.

.DESCRIPTION
    The standards document is enforced by hand today, and it has not held:
    the space-before-empty-parens rule has been on the books since
    2026-05-13 and the violation count rose from 2,711 to 3,321 lines in
    the ten weeks after. This script is the mechanical half of the fix.

    It checks only rules that reduce to a mechanical test with near-zero
    false-positive risk. Rules needing judgment -- "no magic numbers", EHM
    single-exit-point, declarations-at-top-of-function -- are out of scope
    and remain review's job. Declaration-run column alignment (CS0019) IS
    gated: it flags only runs scripts/FixDeclAlign.ps1 can mechanically
    repair, and skips every shape that parser does not understand.

    DEFAULT MODE IS `Diff`, which suits the pre-push hook: it inspects
    only lines the push ADDS, so a push is judged on its own contribution.
    Every rule has now been swept to zero tree-wide, and CI runs `Tree`
    mode as the backstop -- the hook catches violations before they leave
    the machine, CI catches anything that slips past it (a --no-verify
    push, a clone that never enabled the hook).

.PARAMETER Mode
    `Diff`   (default) -- check only lines added between -Against and
                         -Revision.
    `Staged` -- check only lines the INDEX adds over HEAD. Use before
                committing, and especially when adding a NEW file: Diff
                mode compares commits, so a file that has never been
                committed contributes no added lines and is invisible to
                it. Its first violation therefore surfaces only after the
                commit exists, making the fix an amend. Staged mode sees
                it while it is still staged.
    `Tree`   -- check every tracked file. Expect existing violations until
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
    scripts/CheckStyle.ps1 -Mode Staged
    Checks what is staged, before committing it. The only mode that sees a
    brand-new file.

.EXAMPLE
    scripts/CheckStyle.ps1 -Mode Tree
    Full-tree audit -- reports the whole existing backlog.
#>
param(
    [ValidateSet('Diff', 'Staged', 'Tree')]
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
    [switch]$NoStructural,

    [switch]$NormalPriority,

    [switch]$LowPriority
)

#
#  OFF THE FOREGROUND'S BACK. This saturates every core and the disk with
#  it, and nothing about it is latency-sensitive -- nobody watches a build.
#  Lowered here rather than around the tool because Windows hands a child
#  its parent's priority class, so this reaches MSBuild, every cl.exe it
#  fans out, and vstest and its hosts. See scripts/HostLoad.ps1.
#
. (Join-Path $PSScriptRoot 'HostLoad.ps1')

$priorityWas = $null

if (-not $NormalPriority) {
    $priorityWas = Set-CassoHostLoad -Priority ($LowPriority ? 'Idle' : 'BelowNormal')
}

#  Put it back on the way out however this ends -- these are run from an
#  interactive shell as often as from a fresh one, and a session left at
#  BelowNormal for the rest of the day is a slow shell nobody can explain.
trap { Restore-CassoHostLoad -Priority $priorityWas; break }

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
        # CS0021: a lookup table in an executable. A switch arm returning a
        # string literal is a mapping -- format to name, filling to caption --
        # and a mapping is a decision, which Principle VI says lives in a core
        # library where the UnitTest project can reach it.
        #
        # THIS IS NOT PEDANTRY ABOUT WHERE FILES GO. Both instances that
        # prompted the rule ended in a default arm answering with another
        # entry's name, so a value added without an arm was silently rendered
        # as something else rather than refused -- and being in the exe, which
        # the test assembly does not link, nothing could reach them to notice.
        # One shipped a create dialog that would have named a nibble image
        # ".woz".
        #
        # Narrow on purpose: only `case X::Y: return "..."`. A switch that
        # dispatches, computes, or returns non-literals is untouched, so the
        # check stays quiet enough to survive.
        Id      = 'CS0021'
        Globs   = @('*.cpp')
        Include = @('Casso/', 'CassoCli/')
        Pattern = 'case [A-Za-z_][A-Za-z0-9_]*::[A-Za-z0-9_]+: *return +L?"'
        Message = 'lookup table in an executable -- move the mapping into a core library where a test can reach it'
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
    },
    @{
        # IGNORE_RETURN_VALUE (result, replacement) overwrites an already-
        # captured result with a neutral value. A call as the second argument
        # just expands to `result = call();` -- an assignment dressed up as an
        # ignore, with a function call buried in a macro argument. Capture
        # first, then neutralize:  hr = Foo (...);  IGNORE_RETURN_VALUE (hr, S_OK);
        #
        # The second alternation catches the WRAPPED form -- a line ending
        # right after the first argument. A neutral constant never needs a
        # continuation line, so a wrapped second argument is always a call.
        # The macro itself also rejects non-constants (constexpr local ->
        # C2131), so this rule is the pre-build twin of a compile error.
        Id      = 'CS0018'
        Globs   = @('*.cpp', '*.h')
        Pattern = 'IGNORE_RETURN_VALUE\s*\(\s*\w+\s*,\s*([^)]*\(|$)'
        Message = 'call inside IGNORE_RETURN_VALUE -- capture the result first, then IGNORE_RETURN_VALUE (result, S_OK)'
        Exclude = @('CassoCore/Ehm.h')
    }
)

$violations = @()


####################################################################
#
#  Test-Included -- does this path fall under a check's own scope?
#
#  PREFIX matching, where Test-Excluded matches a suffix. The exclusions name
#  whole files, so a suffix is right for them; an inclusion names a project
#  directory, and `Casso/` is never the end of a path. Prefix matching also
#  keeps `Casso/` off `CassoCore/` and off the tests in `UnitTest/Casso/`.
#
####################################################################

function Test-Included
{
    param([string]$Path, [string[]]$Include)

    $normalized = $Path -replace '\\', '/'

    foreach ($i in $Include)
    {
        if ($normalized -like "$i*") { return $true }
    }

    return $false
}


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
#  -Cached diffs the index against HEAD instead of comparing two commits.
#  That is what lets a never-committed file be checked: against a commit
#  range it has no "added" lines at all, because neither side of the range
#  contains it, so every rule silently passes over it.
#
####################################################################

function Get-AddedLines
{
    param([string]$Base, [string]$Tip, [switch]$Cached)

    #  A List, not an array. `$results += ...` reallocates and copies the whole
    #  array for every added line, so the cost is quadratic in the size of the
    #  diff. A push whose range added 1.5M lines spent 34 minutes in here.
    $results = [System.Collections.Generic.List[object]]::new()
    $file    = ''
    $lineNo  = 0

    #  Ask git for the extensions the rules actually read. Every check's Globs
    #  are a subset of these three, so this changes no verdict -- it stops a
    #  generated model (one mesh carries 1.3M added lines) from being parsed
    #  into an object per line and then dropped for not matching a glob.
    $pathspec = @('*.cpp', '*.h', '*.md')

    if ($Cached)
    {
        $diff = git -C $repoRoot diff --cached --unified=0 --no-color --diff-filter=d -- $pathspec 2>$null
    }
    else
    {
        $diff = git -C $repoRoot diff --unified=0 --no-color --diff-filter=d "$Base...$Tip" -- $pathspec 2>$null
    }

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
                $results.Add([pscustomobject]@{
                    File = $file
                    Line = $lineNo
                    Text = $raw.Substring(1)
                })
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

        # Include is the inverse of Exclude: when present, the check applies
        # ONLY under those paths. A rule about where code lives needs it.
        #
        # NOT Test-Excluded WITH THE ARGUMENT FLIPPED. That matches a suffix,
        # which is right for the full paths the exclusions name and never true
        # of a directory prefix -- reusing it made this rule skip every file
        # and report a clean tree while checking nothing.
        if ($check.ContainsKey('Include') -and $check.Include.Count -gt 0 -and
            -not (Test-Included -Path $Path -Include $check.Include))
        {
            continue
        }

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
#  ConvertTo-DeclParts
#
#  Parses one local-declaration line into indent / type / ptr / name /
#  init / comment, or $null when the line is not a shape the alignment
#  rule understands. MUST stay identical to Parse-Decl in
#  scripts/FixDeclAlign.ps1: CS0019 skips any run this cannot parse, so
#  the gate only ever fires on runs the fixer can repair.
#
####################################################################

function ConvertTo-DeclParts
{
    param([string] $Line)

    $comment = ''
    $m = [regex]::Match($Line, '\s*(//.*)$')
    $body = $Line
    if ($m.Success) { $comment = $m.Groups[1].Value; $body = $Line.Substring(0, $m.Index) }

    $body = $body.TrimEnd()
    if (-not $body.EndsWith(';')) { return $null }
    $body = $body.Substring(0, $body.Length - 1)

    $indent = ($body -replace '^(\s*).*$', '$1')
    $body   = $body.Trim()

    $init  = $null
    $lhs   = $body
    $eqPos = $body.IndexOf(' = ')
    if ($eqPos -ge 0)
    {
        $lhs  = $body.Substring(0, $eqPos).TrimEnd()
        $init = $body.Substring($eqPos + 3).Trim()
    }

    if ($lhs -match ',(?![^<]*>)(?![^\[]*\])') { return $null }   # comma decl (or too odd)

    $tokens = @($lhs -split '\s+' | Where-Object { $_ -ne '' })
    if ($tokens.Count -lt 2) { return $null }

    $decl = $tokens[-1]
    $ptr  = ''
    $tEnd = $tokens.Count - 2

    if ($tEnd -ge 0 -and ($tokens[$tEnd] -eq '*' -or $tokens[$tEnd] -eq '&'))
    {
        $ptr  = $tokens[$tEnd]
        $tEnd--
    }

    if ($decl -match '^[\*&]') { return $null }
    if ($tEnd -lt 0)           { return $null }
    if ($decl -notmatch '^[A-Za-z_]\w*(\[[^\]]*\])?$') { return $null }

    return [pscustomobject]@{
        Indent = $indent
        Ptr    = $ptr
        Decl   = $decl
        Init   = $init
    }
}

# Same line classifiers FixDeclAlign.ps1 uses to spot a declaration run.
$script:DeclRegex = '^\s*(?:const\s+|constexpr\s+|static\s+)*[A-Za-z_][A-Za-z0-9_:<>,\s\*&\[\]]*\s+[\*&]?\s*[A-Za-z_]\w*(\s*\[[^\]]*\])?\s*(=[^;]*)?;\s*(//.*)?$'
$script:DeclExcl  = '\breturn\b|\bdelete\b|\bgoto\b|\bthrow\b|\+\+|--|^\s*(else|do)\b|\boperator\b|^\s*[A-Za-z_]\w*\s*(<<|>>)'


####################################################################
#
#  Test-Structure  (CS0014 / CS0015 / CS0016 / CS0017 / CS0019)
#
#  The formatting rules that a per-line regex cannot see:
#
#    CS0014  a function definition in a .cpp has an 80-slash banner
#    CS0015  a top-level banner is preceded by EXACTLY 5 blank lines
#    CS0016  a declaration block is followed by EXACTLY 3 blank lines
#    CS0017  a bare closing brace is followed by a blank line
#    CS0019  a declaration run aligns its name and `=` columns
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

        # ---- CS0017: blank line after a bare closing brace --------------
        # The documented exceptions are do-while (same-line `} while`), a
        # following `else`, and a following closing brace; `catch` joins
        # them because it is the same shape as `else`. Beyond the document,
        # three contexts are excluded as NOT-statements rather than
        # exceptions: preprocessor lines, access specifiers, and
        # continuation lines (`)`, `,`, `;`) that belong to an enclosing
        # expression. Guard clauses, `case` labels, and `Error:` labels are
        # deliberately NOT excluded -- the document says so for the first
        # two, and a label introduces the next statement like any other.
        for ($i = 0; $i -lt $lines.Length - 1; $i++)
        {
            if ($lines[$i] -notmatch '^\s*\}\s*$') { continue }

            $next = $lines[$i + 1]

            if ($next.Trim() -eq '')                              { continue }
            if ($next -match '^\s*\}')                            { continue }
            if ($next -match '^\s*(else|while|catch)\b')          { continue }
            if ($next -match '^\s*#')                             { continue }
            if ($next -match '^\s*(public|private|protected)\s*:') { continue }
            if ($next -match '^\s*[\),;]')                        { continue }

            $findings += [pscustomobject]@{
                Id    = 'CS0017'
                First = $i + 1
                Last  = $i + 2
                Text  = 'closing brace must be followed by a blank line'
            }
        }

        # ---- CS0014 / CS0016: per function definition -------------------
        if ($rel -like '*.cpp')
        {
            for ($i = 1; $i -lt $lines.Length; $i++)
            {
                if ($lines[$i] -ne '{') { continue }

                # The signature may wrap: a parameter list broken one per line
                # leaves an indented `... & last)` directly above the brace.
                # Walk up over the indented continuation to the line that
                # opens it, which is the one that names the function. Without
                # this every multi-line signature was skipped by CS0014 and
                # CS0016 both, and nothing said so.
                $s = $i - 1
                while ($s -gt 0 -and $lines[$s] -match '^\s+\S') { $s-- }

                $sig = $lines[$s]
                if ($sig -notmatch '^[A-Za-z_~][A-Za-z0-9_:<>,&*\s]*\(')                                  { continue }
                if ($sig -match '^\s*(if|for|while|switch|else|do|struct|class|enum|namespace|union|TEST_CLASS)\b')   { continue }

                # CS0014 -- banner anywhere in the 12 lines above the SIGNATURE.
                #
                # The window is measured from the line that names the function,
                # not from the brace: a parameter list broken one per line puts
                # arbitrarily many lines between the two, and they would eat the
                # window. ProDosVolume::WriteDirectoryEntry takes nine, so its
                # banner sat 14 lines above the brace and was reported missing
                # while being right there.
                #
                # BOTH BRANCHES FOUND THIS AND FIXED IT DIFFERENTLY -- one ended
                # the walk at the brace, one at the signature. The signature is
                # kept, because that is the variant the tree-wide sweep was run
                # against and the whole tree now satisfies.
                $hasBanner = $false
                for ($k = [Math]::Max(0, $s - 12); $k -lt $s; $k++)
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
                #
                # A declaration is `Type name;`, `Type name = init;`, or the
                # constructor form `Type name (args);`. The third was missing,
                # so `std::ifstream file (path);` ended the block early and a
                # statement reading it on the next line -- `isOpen =
                # file.is_open();` -- sat where the three blanks belong,
                # unreported.
                #
                # A declaration may carry a trailing comment, may be preceded
                # by a comment line of its own, and may run on past its first
                # line when the initializer wraps. Each of those is part of
                # the block, not the end of it. An array declarator carries
                # its bounds between the name and the terminator (char hex[8];),
                # and a type may carry a parenthesized signature of its own
                # (std::function<void()> f;). A line opening with a control
                # keyword or a local type definition is never a declaration,
                # whatever follows it, and neither is a stream insertion: a <<`r
                # ahead of any = marks a statement, while one inside an
                # initializer is a shift.
                $declStart = '^\s{4}(?!(if|for|while|switch|do|else|case|default|goto|throw|return|delete|struct|class|enum|union)\b)(?![^=]*<<)' +
                             '[A-Za-z_](?:[A-Za-z0-9_:<>,\s\*&\[\]]|\([A-Za-z0-9_:<>,\s\*&]*\))*\s+\w+\s*(\[[^\]]*\]\s*)*(=|\(|\{|;)'
                $declEnd   = ';\s*(//.*)?$'
                $d         = $i + 1
                $sawDecl   = $false
                while ($d -lt $lines.Length)
                {
                    $ln = $lines[$d]

                    if ($ln -match '^\s{4}//') { $d++; continue }

                    # A type alias is not a variable definition, and a local struct
                    # that follows one is not the first statement. Neither is
                    # modeled here: the alias is stepped over, and the struct ends
                    # the walk without a finding.
                    if ($ln -match '^\s{4}(using|typedef)\b') { $d++; continue }

                    # The statement test looks at code, not at string literals:
                    # a "--" in an initializer is not a decrement. It also stops
                    # at the "=": a lambda initializer has a "return" of its own,
                    # and that no more makes the line a statement than "i++" in
                    # an initializer does.
                    $code = $ln -replace '"([^"\\]|\\.)*"', '""'
                    $head = ($code -split '=', 2)[0]

                    if ($ln -notmatch $declStart -or $head -match '\breturn\b|\bdelete\b|\+\+|--') { break }

                    # Run past a wrapped initializer to the line that closes it:
                    # the first line ending in `;` at brace depth zero. Depth
                    # matters because a lambda's body has statements of its own
                    # that end in `;` before the `};` that ends the declaration.
                    # A table initializer may group its rows with blank lines, so
                    # a blank does not end the run; the function's own closing
                    # brace does, as the backstop for a line that only looked
                    # like a declaration.
                    $depth = 0
                    while ($d -lt $lines.Length -and $lines[$d] -notmatch '^\}')
                    {
                        $body   = $lines[$d] -replace '"([^"\\]|\\.)*"', '""' -replace "'([^'\\]|\\.)*'", "''" -replace '//.*$', ''
                        $depth += ([regex]::Matches($body, '\{')).Count - ([regex]::Matches($body, '\}')).Count

                        if ($depth -le 0 -and $lines[$d] -match $declEnd) { break }

                        $d++
                    }

                    $sawDecl = $true; $d++
                }

                if ($sawDecl)
                {
                    $blanks = 0
                    while ($d + $blanks -lt $lines.Length -and $lines[$d + $blanks].Trim() -eq '') { $blanks++ }

                    # 0 blanks is allowed ONLY for the "declarations and then
                    # the closing brace" shape. A statement directly under the
                    # block is the violation the rule exists for, and it used to
                    # pass here because nothing asked what the next line was.
                    $closesBody = ($d -lt $lines.Length) -and ($lines[$d] -match '^\s*\}')

                    if ($blanks -ne 3 -and -not ($blanks -eq 0 -and $closesBody))
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

        # ---- CS0019: declaration-run column alignment -------------------
        # A run of 2+ consecutive indented declaration lines at one indent
        # must align its name columns and its `=` columns. Detection
        # mirrors scripts/FixDeclAlign.ps1 exactly -- a run any line of
        # which that parser cannot handle is skipped, so every hit this
        # reports is one the fixer repairs mechanically.
        $declRun = [System.Collections.Generic.List[int]]::new()
        for ($i = 0; $i -le $lines.Length; $i++)
        {
            $isDecl = $false
            if ($i -lt $lines.Length)
            {
                $ln = $lines[$i]
                $isDecl = ($ln -match $script:DeclRegex -and $ln -notmatch $script:DeclExcl -and
                           $ln -match '^\s+\S' -and $ln -notmatch '"')
            }

            if ($isDecl) { $declRun.Add($i); continue }

            if ($declRun.Count -ge 2)
            {
                $parsed = @()
                $ok     = $true
                foreach ($r in $declRun)
                {
                    $p = ConvertTo-DeclParts $lines[$r]
                    if ($null -eq $p) { $ok = $false; break }
                    $parsed += $p
                }

                if ($ok)
                {
                    $indents = $parsed | ForEach-Object { $_.Indent } | Sort-Object -Unique
                    if (@($indents).Count -ne 1) { $ok = $false }
                }

                if ($ok)
                {
                    $nameCols = @()
                    $eqCols   = @()
                    foreach ($r in $declRun)
                    {
                        $ln = $lines[$r]
                        $mm = [regex]::Match($ln, '^\s*(?:const\s+|constexpr\s+|static\s+)*.*?([A-Za-z_]\w*(\[[^\]]*\])?)\s*(=|;)')
                        $nameCols += $mm.Groups[1].Index
                        $pe = $ln.IndexOf(' = ')
                        if ($pe -ge 0) { $eqCols += $pe + 1 }
                    }

                    if ((@($nameCols | Sort-Object -Unique)).Count -gt 1 -or
                        (@($eqCols   | Sort-Object -Unique)).Count -gt 1)
                    {
                        $findings += [pscustomobject]@{
                            Id    = 'CS0019'
                            First = $declRun[0] + 1
                            Last  = $declRun[$declRun.Count - 1] + 1
                            Text  = 'misaligned declaration run -- align name and = columns (scripts/FixDeclAlign.ps1 -Apply repairs it)'
                        }
                    }
                }
            }

            $declRun.Clear()
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
    # baseline problem that gets a check switched off.
    #
    # "Unpushed" means reachable from the tip but from NO origin ref -- not
    # merely absent from the upstream tracking ref. The distinction bit for
    # real: merging master into a feature branch imports every historical
    # master commit into upstream..HEAD, and the gate re-judged years of
    # published (immutable) messages and blocked the push. --not --remotes
    # excludes anything origin already has, whichever ref carries it.
    $originRefs = git -C $repoRoot for-each-ref 'refs/remotes/origin' --format '%(refname)' 2>$null

    if (@($originRefs).Count -gt 0)
    {
        $shas = git -C $repoRoot rev-list $Tip --not --remotes=origin 2>$null
    }
    else
    {
        # No remote-tracking refs (fresh clone with no fetch, offline mirror):
        # fall back to the diff base so the gate still judges something rather
        # than the whole history.
        $shas = git -C $repoRoot rev-list "$Base..$Tip" 2>$null
    }

    # There is no opt-out. A message is prose we write, so unlike source -- which
    # may be stuck with a name like ERROR_CANCELLED -- it can always be phrased
    # without the words it is about: say the file and the count, not the word.
    #
    # There WAS one, a STYLE-ALLOW-BRITISH token anywhere in the body. It was a
    # bad mechanism twice over: it disabled the whole check rather than one
    # finding, irrevocably once pushed; and being a bare substring test, a
    # message that merely DISCUSSED the token opted itself out, so the rule
    # could not be written about without being switched off.
    #
    # This list is what it left behind -- messages published under it that
    # would now fail, and cannot be fixed without rewriting pushed history.
    # It is closed: the mechanism is gone, so nothing can be added to it.
    #
    #   b9bbb6e3  the sweep itself; quotes the words it replaced
    #   52565ecb  introduced the message check, naming what it looks for
    #   d58f1ac9  added this list, quoting b9bbb6e3 to justify the entry
    #   909deb6b  narrowed and widened the rule, listing every hit it fixed
    #
    # Reachable at all only because the upstream scoping above misses
    # cross-branch publication: these were pushed on a feature branch, never on
    # master, so master's first push re-scans them as though they were new.
    $grandfathered = @('b9bbb6e3', '52565ecb', 'd58f1ac9', '909deb6b')

    foreach ($sha in $shas)
    {
        $body    = git -C $repoRoot log -1 --format=%B $sha 2>$null | Out-String
        $subject = git -C $repoRoot log -1 --format=%s $sha 2>$null
        $short   = $sha.Substring(0, 8)

        if ($body -imatch 'Co-Authored-By:\s*Claude|Generated with \[Claude Code\]|noreply@anthropic\.com')
        {
            $bad += [pscustomobject]@{ Id = 'CS0008'
                Text = "commit ${short} -- Claude attribution is not permitted in commit messages" }
        }

        if ($body -match $britishRule.Pattern -and $short -notin $grandfathered)
        {
            $bad += [pscustomobject]@{ Id = 'CS0008'
                Text = "commit ${short} -- $($britishRule.Message) (found '$($Matches[0])')" }
        }

        # CS0020: subjects follow `action(scope): description` -- the scope is
        # NOT optional (a bare `style:` is the drift this exists to stop).
        # Merge and revert subjects keep git's own conventions. Scoped to
        # unpushed commits like everything here, so historical scopeless
        # subjects on published branches never trip it.
        if ($subject -notmatch '^(Merge|Revert)\b' -and
            $subject -notmatch '^[a-z][a-z0-9]*\([^)]+\): .+')
        {
            $bad += [pscustomobject]@{ Id = 'CS0020'
                Text = "commit ${short} -- subject must be 'action(scope): description' (found '$subject')" }
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

if ($Mode -eq 'Diff' -or $Mode -eq 'Staged')
{
    if ($Mode -eq 'Staged')
    {
        $added      = Get-AddedLines -Cached
        $scopeLabel = 'lines added by the staged changes'

        # Nothing staged means nothing was checked, and this script's own
        # rule is that a gate reporting OK when it inspected nothing is
        # worse than no gate. Same reasoning as the unresolvable-base
        # guard above: say SKIPPED out loud rather than pass cheerfully.
        if ($added.Count -eq 0)
        {
            [Console]::Error.WriteLine("CheckStyle: SKIPPED -- nothing is staged. Stage your changes, then re-run.")
            exit 0
        }
    }
    else
    {
        $added      = Get-AddedLines -Base $Against -Tip $Revision
        $scopeLabel = "lines added between $Against and $Revision"
    }

    $touchedFiles = @($added | Select-Object -ExpandProperty File -Unique)

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
#
#  Note for Staged mode: the structural rules read whole files from the
#  working tree, not from the index. With everything staged those are the
#  same bytes; with a partial stage they are not, so a structural finding
#  can point at an unstaged line. Reporting it is still the right call --
#  a real violation sitting in the file is worth knowing about -- but the
#  line number belongs to the working tree.
#
$addedIndex = $null

if ($Mode -eq 'Diff' -or $Mode -eq 'Staged')
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

# Diff only, deliberately: in Staged mode the commit being checked does not
# exist yet, so there is no message to inspect. The pre-push hook still runs
# Diff mode and catches a bad subject before it leaves the machine.
if (-not $SkipCommitCheck -and $Mode -eq 'Diff')
{
    foreach ($b in (Test-CommitMessages -Base $Against -Tip $Revision))
    {
        $sink.Add([pscustomobject]@{ Id = $b.Id; Text = $b.Text })
    }
}

$violations = $sink

Restore-CassoHostLoad -Priority $priorityWas

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
