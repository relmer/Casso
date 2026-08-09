# Realigns misaligned local-declaration blocks: name columns equal, ptr
# column reserved when any line has one, `=` columns equal. Only rewrites
# runs that are actually misaligned; already-aligned runs keep whatever
# gap convention they use. Runs containing a line the parser can't handle
# are logged and left alone.
#
# Byte-safe: ASCII passthrough / CP1252, exact newline preservation.

param([switch] $Apply)

$wt = Split-Path $PSScriptRoot -Parent   # scripts/ -> repo root
Push-Location $wt
$files = git ls-files '*.cpp' '*.h' | Where-Object { $_ -notmatch 'External/' }
Pop-Location

$cp1252    = [System.Text.Encoding]::GetEncoding(1252)
$declRegex = '^\s*(?:const\s+|constexpr\s+|static\s+)*[A-Za-z_][A-Za-z0-9_:<>,\s\*&\[\]]*\s+[\*&]?\s*[A-Za-z_]\w*(\s*\[[^\]]*\])?\s*(=[^;]*)?;\s*(//.*)?$'
$declExcl  = '\breturn\b|\bdelete\b|\bgoto\b|\bthrow\b|\+\+|--|^\s*(else|do)\b|\boperator\b|^\s*[A-Za-z_]\w*\s*(<<|>>)'

$rewritten = 0
$skipped   = New-Object System.Collections.Generic.List[string]
$filesHit  = 0

function Parse-Decl
{
    param([string] $Line)

    # Strip trailing comment, then the semicolon.
    $comment = ''
    $m = [regex]::Match($Line, '\s*(//.*)$')
    $body = $Line
    if ($m.Success) { $comment = $m.Groups[1].Value; $body = $Line.Substring(0, $m.Index) }

    $body = $body.TrimEnd()
    if (-not $body.EndsWith(';')) { return $null }
    $body = $body.Substring(0, $body.Length - 1)

    $indent = ($body -replace '^(\s*).*$', '$1')
    $body   = $body.Trim()

    # Split off the initializer at the first top-level ' = '.
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

    # A declarator that starts with * or & glued on is nonstandard here.
    if ($decl -match '^[\*&]') { return $null }
    if ($tEnd -lt 0)           { return $null }
    if ($decl -notmatch '^[A-Za-z_]\w*(\[[^\]]*\])?$') { return $null }

    $type = ($tokens[0..$tEnd] -join ' ')

    return [pscustomobject]@{
        Indent  = $indent
        Type    = $type
        Ptr     = $ptr
        Decl    = $decl
        Init    = $init
        Comment = $comment
    }
}

foreach ($rel in $files)
{
    $full = Join-Path $wt $rel
    if (-not (Test-Path $full)) { continue }

    $bytes   = [IO.File]::ReadAllBytes($full)
    $isAscii = $true
    foreach ($b in $bytes) { if ($b -gt 0x7F) { $isAscii = $false; break } }
    $enc     = if ($isAscii) { [System.Text.Encoding]::ASCII } else { $cp1252 }
    $text    = $enc.GetString($bytes)

    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($piece in $text.Split("`n")) { $lines.Add($piece) }

    $changed = $false
    $run     = New-Object System.Collections.Generic.List[int]

    for ($i = 0; $i -le $lines.Count; $i++)
    {
        $isDecl = $false
        if ($i -lt $lines.Count)
        {
            $ln = $lines[$i].TrimEnd("`r")
            $isDecl = ($ln -match $declRegex -and $ln -notmatch $declExcl -and $ln -match '^\s+\S' -and $ln -notmatch '"')
        }

        if ($isDecl) { $run.Add($i); continue }

        if ($run.Count -ge 2)
        {
            $parsed = @()
            $ok     = $true
            foreach ($r in $run)
            {
                $p = Parse-Decl ($lines[$r].TrimEnd("`r"))
                if ($null -eq $p) { $ok = $false; break }
                $parsed += $p
            }

            # All lines must share one indent to be a single block.
            if ($ok)
            {
                $indents = $parsed | ForEach-Object { $_.Indent } | Sort-Object -Unique
                if (@($indents).Count -ne 1) { $ok = $false }
            }

            if ($ok)
            {
                # Current alignment check: name start columns and = columns.
                $nameCols = @()
                $eqCols   = @()
                foreach ($r in $run)
                {
                    $ln = $lines[$r].TrimEnd("`r")
                    $mm = [regex]::Match($ln, '^\s*(?:const\s+|constexpr\s+|static\s+)*.*?([A-Za-z_]\w*(\[[^\]]*\])?)\s*(=|;)')
                    $nameCols += $mm.Groups[1].Index
                    $pe = $ln.IndexOf(' = ')
                    if ($pe -ge 0) { $eqCols += $pe + 1 }
                }

                $misName = (@($nameCols | Sort-Object -Unique)).Count -gt 1
                $misEq   = (@($eqCols   | Sort-Object -Unique)).Count -gt 1

                if ($misName -or $misEq)
                {
                    if (-not $Apply) { Write-Host ("HIT {0}:{1}" -f $rel, ($run[0] + 1)) }
                    $maxType = ($parsed | ForEach-Object { $_.Type.Length } | Measure-Object -Maximum).Maximum
                    $maxDecl = ($parsed | ForEach-Object { $_.Decl.Length } | Measure-Object -Maximum).Maximum
                    $hasPtr  = ($parsed | Where-Object { $_.Ptr -ne '' }).Count -gt 0

                    for ($k = 0; $k -lt $run.Count; $k++)
                    {
                        $p  = $parsed[$k]
                        $cr = if ($lines[$run[$k]].EndsWith("`r")) { "`r" } else { '' }

                        $s = $p.Indent + $p.Type.PadRight($maxType) + '  '
                        if ($hasPtr) { $s += $(if ($p.Ptr -ne '') { $p.Ptr } else { ' ' }) + ' ' }

                        if ($null -ne $p.Init)
                        {
                            $s += $p.Decl.PadRight($maxDecl) + ' = ' + $p.Init + ';'
                        }
                        else
                        {
                            $s += $p.Decl + ';'
                        }

                        if ($p.Comment -ne '') { $s += '   ' + $p.Comment }

                        $lines[$run[$k]] = $s + $cr
                    }

                    $rewritten++
                    $changed = $true
                }
            }
            else
            {
                $skipped.Add(("{0}:{1}" -f $rel, ($run[0] + 1)))
            }
        }

        $run.Clear()
    }

    if ($changed -and $Apply)
    {
        [IO.File]::WriteAllBytes($full, $enc.GetBytes(($lines -join "`n")))
        $filesHit++
    }
    elseif ($changed)
    {
        $filesHit++
    }
}

$mode = if ($Apply) { 'REWROTE' } else { 'WOULD rewrite' }
"$mode $rewritten run(s) across $filesHit file(s); $($skipped.Count) run(s) skipped (unparseable)."
foreach ($s in $skipped) { Write-Warning "unparseable run: $s" }
