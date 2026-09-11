#Requires -Version 7.0
<#
.SYNOPSIS
    Measures progress of the spec 031 executable extraction.

.DESCRIPTION
    Reports the three numbers every slice of spec 031 has to record: how much
    code each project still holds, how many functions each executable project
    still defines, and how many executable sources are still compiled a second
    time into the test DLL.

    The function count is the check SC-002 turns on. It is expected to reach
    zero for both executable projects, and it is a check that can fail.

    The dual-compile count is the workaround the missing library boundary
    forced: UnitTest cannot link an Application, so exe sources were listed as
    ClCompile items and built twice. It is expected to reach zero as well.

.PARAMETER Markdown
    Emit a Markdown table suitable for pasting into measurements.md or a
    commit message, instead of PowerShell objects.
#>

param(
    [switch]$Markdown
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

#
# Directory names whose contents are never counted: build output, and vendored
# third-party source that is not ours to move.
#
$s_kSkippedDirectories = 'x64', 'ARM64', 'Debug', 'Release', 'External'

####################################################################################
#
#  Test-CountablePath
#
#  Reports whether a file sits outside every skipped directory. Splitting on the
#  separator avoids a regex, whose backslashes are the kind of thing that reads
#  correctly and matches nothing.
#
####################################################################################

function Test-CountablePath
{
    param([string]$Path)

    $segments = $Path.Split([System.IO.Path]::DirectorySeparatorChar)

    foreach ($segment in $segments)
    {
        if ($s_kSkippedDirectories -contains $segment)
        {
            return $false
        }
    }

    return $true
}

####################################################################################
#
#  Get-SourceLineCount
#
#  Totals the .cpp and .h lines under a project directory, skipping build
#  output and vendored third-party source. External/ is excluded because it is
#  not ours to move and counting it would make every slice's delta noisy.
#
####################################################################################

function Get-SourceLineCount
{
    param([string]$Directory)

    if (-not (Test-Path $Directory))
    {
        return [pscustomobject]@{ Files = 0; Lines = 0 }
    }

    $files = @(Get-ChildItem -Path $Directory -Recurse -File -Include *.cpp, *.h |
               Where-Object { Test-CountablePath -Path $_.FullName })

    $lines = 0
    foreach ($file in $files)
    {
        #
        # Count the array, not Measure-Object -Line, which reports zero for a
        # blank string and so silently drops every blank line in the tree.
        #
        $lines += @(Get-Content -LiteralPath $file.FullName).Count
    }

    return [pscustomobject]@{ Files = $files.Count; Lines = $lines }
}

####################################################################################
#
#  Get-FunctionCount
#
#  Counts function definitions in a project's own translation units.
#
#  A definition is a line that opens a parameter list at column zero or after a
#  return type and is followed by a brace, which is what this codebase's style
#  guarantees: a definition's opening brace sits on its own line. Declarations
#  end in a semicolon and are skipped, and so is anything inside a comment.
#
#  This is deliberately simple. It only ever runs against an executable project,
#  where the expected answer is zero and any non-zero result is read by a human.
#
####################################################################################

function Get-FunctionCount
{
    param([string]$Directory)

    if (-not (Test-Path $Directory))
    {
        return 0
    }

    $sources = @(Get-ChildItem -Path $Directory -File -Include *.cpp -Recurse |
                 Where-Object { Test-CountablePath -Path $_.FullName })

    $count = 0

    foreach ($source in $sources)
    {
        $text     = @(Get-Content -LiteralPath $source.FullName)
        $inBlock  = $false

        for ($i = 0; $i -lt $text.Count; $i++)
        {
            $line = $text[$i]

            if ($inBlock)
            {
                if ($line -match '\*/') { $inBlock = $false }
                continue
            }

            if ($line -match '^\s*/\*') { $inBlock = $true; continue }
            if ($line -match '^\s*//')  { continue }
            if ($line -match ';\s*$')   { continue }

            #
            # A definition looks like "<something> (<args>)" possibly with
            # trailing qualifiers, and its brace is on this line or the next.
            #
            if ($line -notmatch '\w\s*\([^;]*$' -and $line -notmatch '\w\s*\(.*\)\s*(const)?\s*(noexcept)?\s*\{?\s*$')
            {
                continue
            }

            #
            # A keyword that takes parens is not a function. Without this the
            # count reports every if and for in the project, which is how the
            # first run of this script claimed Casso.exe defined 4,124
            # functions.
            #
            if ($line -match '^\s*(if|else|for|while|switch|catch|return|sizeof|do)\b')
            {
                continue
            }

            $hasBrace = $line -match '\{\s*$'

            if (-not $hasBrace -and ($i + 1) -lt $text.Count)
            {
                $hasBrace = $text[$i + 1] -match '^\s*\{\s*$'
            }

            if ($hasBrace)
            {
                $count++
            }
        }
    }

    return $count
}

####################################################################################
#
#  Get-DualCompileCount
#
#  Counts exe sources still compiled a second time into UnitTest.dll.
#
####################################################################################

function Get-DualCompileCount
{
    $project = Join-Path $repoRoot 'UnitTest\UnitTest.vcxproj'

    if (-not (Test-Path $project))
    {
        return 0
    }

    $entries = @(Select-String -LiteralPath $project -Pattern 'ClCompile Include="\.\.\\Casso\\')

    return $entries.Count
}

$projects = 'Casso', 'CassoCli', 'CassoCore', 'CassoEmuCore', 'Dxui'
$rows     = @()

foreach ($project in $projects)
{
    $directory = Join-Path $repoRoot $project
    $counted   = Get-SourceLineCount -Directory $directory
    $functions = $null

    if ($project -eq 'Casso' -or $project -eq 'CassoCli')
    {
        $functions = Get-FunctionCount -Directory $directory
    }

    $rows += [pscustomobject]@{
        Project   = $project
        Files     = $counted.Files
        Lines     = $counted.Lines
        Functions = $functions
    }
}

$dualCompiled = Get-DualCompileCount

if ($Markdown)
{
    Write-Output '| Project | Files | Lines | Functions |'
    Write-Output '|---|---:|---:|---:|'

    foreach ($row in $rows)
    {
        $functions = if ($null -eq $row.Functions) { 'n/a' } else { $row.Functions }
        Write-Output ('| `{0}` | {1} | {2} | {3} |' -f $row.Project, $row.Files, $row.Lines, $functions)
    }

    Write-Output ''
    Write-Output ('Dual-compiled exe sources in `UnitTest.vcxproj`: **{0}**' -f $dualCompiled)
}
else
{
    $rows | Format-Table -AutoSize
    Write-Output ('Dual-compiled exe sources in UnitTest.vcxproj: {0}' -f $dualCompiled)
}
