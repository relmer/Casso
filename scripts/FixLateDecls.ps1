# Hoists local declarations that appear after the first executable line
# of their block to the top of that block (function decl run for
# depth-1, block top otherwise), per the decls-at-top rule.
#
#   - decl with initializer  -> decl (default-init) hoisted, assignment
#                               stays at the original site
#   - decl without init      -> whole line moves
#   - constexpr / static constexpr with init -> whole line moves
#   - auto / references / arrays-with-init / static / lock types
#                            -> skipped, logged for manual handling
#
# Byte-safe IO. Run FixDeclAlign afterward to realign grown decl runs.

param([switch] $Apply)

$wt = Split-Path $PSScriptRoot -Parent   # scripts/ -> repo root
Push-Location $wt
$files = git ls-files '*.cpp' | Where-Object { $_ -notmatch 'External/' }
Pop-Location

$cp1252    = [System.Text.Encoding]::GetEncoding(1252)
$declRegex = '^\s*(?:const\s+|constexpr\s+|static\s+)*[A-Za-z_][A-Za-z0-9_:<>,\s\*&\[\]]*\s+[\*&]?\s*[A-Za-z_]\w*(\s*\[[^\]]*\])?\s*(=[^;]*)?;\s*(//.*)?$'
$declExcl  = '\breturn\b|\bdelete\b|\bgoto\b|\bthrow\b|\+\+|--|^\s*(else|do)\b|\boperator\b|^\s*[A-Za-z_]\w*\s*(<<|>>)'

$hoisted = 0
$moved   = 0
$manual  = New-Object System.Collections.Generic.List[string]
$filesHit = 0

function Get-DefaultInit
{
    param([string] $Type, [string] $Ptr)

    if ($Ptr -eq '*')                                      { return 'nullptr' }
    $t = $Type -replace '^(const|constexpr|static)\s+', '' -replace '\s+(const)$', ''

    switch -Regex ($t)
    {
        '^HRESULT$'                                        { return 'S_OK' }
        '^(bool|BOOL)$'                                    { return $(if ($t -ceq 'BOOL') { 'FALSE' } else { 'false' }) }
        '^float$'                                          { return '0.0f' }
        '^double$'                                         { return '0.0' }
        '^(int|long|short|size_t|ptrdiff_t|Byte|Word|BYTE|WORD|DWORD|UINT|ULONG|LONG|USHORT|WPARAM|LPARAM|LRESULT|UINT_PTR|INT_PTR|DWORD_PTR|u?int\d+_t|unsigned(\s+\w+)*|signed(\s+\w+)*|char|wchar_t|LONGLONG|ULONGLONG|uint64_t|int64_t)$' { return '0' }
        '^(RECT|SIZE|POINT|POINTL|MSG|GUID|FILETIME|SYSTEMTIME|D3D11_\w+|DXGI_\w+|WNDCLASS\w*|MONITORINFO\w*|WINDOWPLACEMENT|PAINTSTRUCT|TRACKMOUSEEVENT|NONCLIENTMETRICSW?|LARGE_INTEGER|ULARGE_INTEGER)$' { return '{}' }
        '^(HINSTANCE|HWND|HFONT|HMENU|HICON|HCURSOR|HBRUSH|HBITMAP|HDC|HGDIOBJ|HMODULE|HKEY|LPWSTR|PWSTR)$' { return 'nullptr' }
        '^(UINT32|UINT64|INT32|INT64|(std::)?streamsize|(std::)?streamoff|(std::)?streampos|std::u?int\d+_t)$' { return '0' }
        '^(IrqSourceId|DxuiHitTestKind|JsonType|Directive|ThemeBootstrapAction|DWRITE_TEXT_ALIGNMENT|DWRITE_PARAGRAPH_ALIGNMENT|DWRITE_LINE_METRICS|DWRITE_FONT_METRICS|D2D1_RECT_F|D2D1_POINT_2F|DxuiMouseEvent|Disk2Event|OpcodeEntry|Cpu6502Registers|CrtParams|LedIndicatorLayout|DebugSelectionResult|SettingsMemoryRegion|FileReadResult|WindowPlacementProfile::Bounds|InputDeviceSelector::Segment|DxuiScrollbar::Metrics|DriveWidgetState::Door|ParsedLine|(TestCpu::)?StopReason|AutoMountResolver::Decision)$' { return '{}' }
        default                                            { return $null }   # class type: no initializer
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

    # Collect (siteIndex, blockOpenIndex, isFunctionTop) for every late decl.
    $sites = New-Object System.Collections.Generic.List[object]

    $seenSites = New-Object 'System.Collections.Generic.HashSet[int]'

    for ($i = 1; $i -lt $lines.Count; $i++)
    {
        if ($lines[$i].TrimEnd("`r").Trim() -ne '{') { continue }
        $sig = $lines[$i - 1].TrimEnd("`r").Trim()

        # A function body's opening brace: the previous line must read as a
        # signature tail (identifier..., ends with ')' or a qualifier).
        # TEST_CLASS opens a CLASS body -- its members are not locals and
        # must never be hoisted -- so it is excluded; TEST_METHOD is a real
        # function and stays.
        if ($sig -notmatch '^[A-Za-z_~][^;{}=]*\)\s*(const\s*|noexcept\s*|override\s*|final\s*)*$') { continue }
        if ($sig -match '^(if|for|while|switch|else|do|catch|struct|class|enum|namespace|union)\b')  { continue }
        if ($sig -match '^TEST_CLASS\b')                                                            { continue }
        if ($sig -match '->|\[')                                                                    { continue }

        $depth = 1
        # stack entries: @{ Open = line idx of '{'; Seen = bool }
        $stack = New-Object System.Collections.Generic.List[object]
        $stack.Add(@{ Open = $i; Seen = $false })
        $j = $i + 1
        # Depth stack of nested type definitions being skipped: their
        # members are not locals.
        $typeSkip = New-Object System.Collections.Generic.Stack[int]

        while ($j -lt $lines.Count -and $depth -gt 0)
        {
            $raw = $lines[$j].TrimEnd("`r")

            # Blank out char literals ('{'), string bodies, and trailing
            # comments so braces inside them never skew the depth walk --
            # JsonParser-style tokenizers are full of brace literals.
            $ln = $raw -replace "'(\\.|[^'\\])'", "' '"
            $ln = $ln -replace '"(\\.|[^"\\])*"', '""'
            $ln = $ln -replace '//.*$', ''
            $ln = $ln.TrimEnd()
            $t  = $ln.Trim()

            $opens  = ([regex]::Matches($ln, '\{')).Count
            $closes = ([regex]::Matches($ln, '\}')).Count

            # A multi-line nested type definition: skip its whole body.
            if ($typeSkip.Count -eq 0 -and
                $t -match '^(struct|class|enum|union)\b' -and $t -notmatch ';\s*$')
            {
                $typeSkip.Push($depth)
            }

            if ($typeSkip.Count -gt 0)
            {
                $depth += $opens - $closes
                if ($depth -le $typeSkip.Peek()) { $typeSkip.Pop() | Out-Null }
                for ($o = 0; $o -lt $opens; $o++)  { $stack.Add(@{ Open = $j; Seen = $false }) }
                for ($o = 0; $o -lt $closes; $o++) { if ($stack.Count -gt 0) { $stack.RemoveAt($stack.Count - 1) } }
                $j++
                continue
            }

            $isNeutral = ($t -eq '' -or $t -eq '{' -or $t -eq '}' -or
                          $t -match '^/' -or $t -match '^\*' -or
                          $t -match '^(public|private|protected)\s*:' -or
                          $t -match '^[A-Za-z_]\w*:$' -or $t -match '^#' -or
                          $t -match '^(case\b|default\s*:)' -or
                          $t -match '^(struct|class|enum|union|using)\b' -or
                          $t -match '^\{.*\},?\s*$' -or $t -match '^\};?\s*$' -or
                          ($t -match '^(?:static\s+|const\s+|constexpr\s+)*[A-Za-z_][\w:<>,\s\*&\[\]]*=\s*\{?\s*$'))

            if ($isNeutral)
            {
            }
            elseif ($ln -match $declRegex -and $ln -notmatch $declExcl -and $ln -notmatch '"')
            {
                $topEntry = $stack[$stack.Count - 1]
                if ($topEntry.Seen -and $seenSites.Add($j))
                {
                    $sites.Add(@{ Site = $j; Open = $topEntry.Open; FuncTop = ($stack.Count -eq 1) })
                }
            }
            else
            {
                $stack[$stack.Count - 1].Seen = $true
            }

            for ($o = 0; $o -lt $opens; $o++)  { $depth++; $stack.Add(@{ Open = $j; Seen = $false }) }
            for ($o = 0; $o -lt $closes; $o++) { $depth--; if ($stack.Count -gt 0) { $stack.RemoveAt($stack.Count - 1) } }

            $j++
        }
    }

    if ($sites.Count -eq 0) { continue }

    # Classify sites and build the edit plan. Work on a copy of lines;
    # inserts shift indices, so process sites in DESCENDING site order for
    # the replacement, but inserts go above -- handle by tracking offsets:
    # simplest correct scheme: process sites descending; each edit touches
    # (a) the site line (replace or remove) and (b) an insert at a line
    # ABOVE the site. Descending order means earlier (higher) sites'
    # inserts shift later-processed (lower-index) sites only if the insert
    # line is below them -- it never is (insert point is above its own
    # site but could be below a lower site? No: a lower site's line index
    # is smaller; an insert at open+K with open < site only shifts lines
    # >= insert point. Lower sites processed later have smaller indices;
    # if the insert point is <= their index they'd shift... so instead of
    # descending order, apply ALL edits at the end against original
    # indices using a rebuild.
    $replacements = @{}   # siteIndex -> replacement line (or $null to delete)
    $insertions   = @{}   # insertAfterIndex -> List[string]

    foreach ($s in $sites)
    {
        $siteIdx = $s.Site
        $ln      = $lines[$siteIdx].TrimEnd("`r")
        $cr      = if ($lines[$siteIdx].EndsWith("`r")) { "`r" } else { '' }

        # Parse: comment, indent, lhs, init
        $comment = ''
        $m = [regex]::Match($ln, '\s*(//.*)$')
        $body = $ln
        if ($m.Success) { $comment = ' ' + $m.Groups[1].Value.Trim(); $body = $ln.Substring(0, $m.Index) }
        $body = $body.TrimEnd()
        if (-not $body.EndsWith(';')) { $manual.Add("${rel}:$($siteIdx+1): no-semicolon"); continue }
        $body = $body.Substring(0, $body.Length - 1)

        $indent = ($body -replace '^(\s*).*$', '$1')
        $core   = $body.Trim()

        $init  = $null
        $lhs   = $core
        $eqPos = $core.IndexOf(' = ')
        if ($eqPos -ge 0)
        {
            $lhs  = $core.Substring(0, $eqPos).TrimEnd()
            $init = $core.Substring($eqPos + 3).Trim()
        }
        elseif ($core -match '=')
        {
            $manual.Add("${rel}:$($siteIdx+1): glued =")
            continue
        }

        if ($lhs -match ',(?![^<]*>)(?![^\[]*\])') { $manual.Add("${rel}:$($siteIdx+1): comma decl"); continue }

        $tokens = @($lhs -split '\s+' | Where-Object { $_ -ne '' })
        if ($tokens.Count -lt 2) { $manual.Add("${rel}:$($siteIdx+1): unparseable"); continue }

        $decl = $tokens[-1]
        $ptr  = ''
        $tEnd = $tokens.Count - 2
        if ($tEnd -ge 0 -and ($tokens[$tEnd] -eq '*' -or $tokens[$tEnd] -eq '&')) { $ptr = $tokens[$tEnd]; $tEnd-- }
        if ($tEnd -lt 0 -or $decl -match '^[\*&]') { $manual.Add("${rel}:$($siteIdx+1): unparseable"); continue }
        $type = ($tokens[0..$tEnd] -join ' ')

        $isConstexpr = $type -match '\bconstexpr\b'
        $isStatic    = $type -match '\bstatic\b'
        $isAuto      = $type -match '^(const\s+)?auto\b'
        $isRef       = $ptr -eq '&'
        $isArray     = $decl -match '\['
        $isLock      = $type -match 'lock_guard|unique_lock|scoped_lock'

        # auto decls: resolve the type from a table of recognized RHS
        # shapes so the split can name it; unrecognized shapes stay manual.
        if ($isAuto -and $null -ne $init -and $type -notmatch '^const')
        {
            $resolved = $null
            if     ($init -match 'dynamic_cast<\s*([^>]+?)\s*\*\s*>') { $resolved = $Matches[1].TrimEnd(); $ptr = '*' }
            elseif ($init -match 'std::make_unique<\s*([^>]+?)\s*>')  { $resolved = 'std::unique_ptr<' + $Matches[1] + '>' }
            elseif ($init -match '^RomDevice::CreateFromFile')        { $resolved = 'std::unique_ptr<MemoryDevice>' }
            elseif ($init -match '^DiskMru::FromUtf8')                { $resolved = 'DiskMru' }
            elseif ($init -match '^DiskMru::DistinctFolders')         { $resolved = 'std::vector<std::filesystem::path>' }
            elseif ($init -match '\.GetRegisteredTypes\s*\(\)')         { $resolved = 'std::vector<std::string>' }
            elseif ($init -match '\.tellg\s*\(\)')                      { $resolved = 'std::streampos' }
            elseif ($init -match '^AutoMountResolver::Resolve')          { $resolved = 'AutoMountResolver::Decision' }
            elseif ($init -match '\.m_registry\.Create\s*\(')            { $resolved = 'std::unique_ptr<MemoryDevice>' }
            elseif ($init -match '^streamsize\s*\{')                     { $resolved = 'std::streamsize' }
            elseif ($init -match '\.RunUntil\s*\(')                     { $resolved = 'TestCpu::StopReason' }
            elseif ($init -match '\.Assemble\s*\(')                     { $resolved = 'AssemblyResult' }
            elseif ($init -match '^MakeStream\s*\(')                    { $resolved = 'std::istringstream' }
            elseif ($rel -like '*DiskMruTests*' -and $init -match '\.(Snapshot\s*\(\)|Prune\s*\()') { $resolved = 'std::vector<DiskMru::Entry>' }
            elseif ($rel -like '*MachineConfigUpgradeTests*' -and $init -match '^MakePriors\s*\(\)') { $resolved = 'vector<MachineConfigPriorHash>' }

            if ($null -ne $resolved) { $type = $resolved; $isAuto = $false }
        }

        if ($isAuto)                    { $manual.Add("${rel}:$($siteIdx+1): auto"); continue }
        if ($isRef)                     { $manual.Add("${rel}:$($siteIdx+1): reference"); continue }
        if ($isLock)                    { $manual.Add("${rel}:$($siteIdx+1): lock"); continue }
        if ($isStatic -and -not $isConstexpr) { $manual.Add("${rel}:$($siteIdx+1): static"); continue }
        # An empty-brace array init has no dependencies: the whole line can
        # move. Arrays with real initializers stay manual (not assignable).
        $isArrayEmpty = $isArray -and $null -ne $init -and $init -match '^\{\s*\}$'
        if ($isArray -and $null -ne $init -and -not $isArrayEmpty) { $manual.Add("${rel}:$($siteIdx+1): array with init"); continue }

        # A top-level comma in the init means this is really a comma
        # declaration (`size_t a = 0, b = 0;`) -- never split it.
        if ($null -ne $init)
        {
            $probe = $init
            for ($r = 0; $r -lt 6; $r++) { $probe = $probe -replace '\{[^{}]*\}', '' -replace '\([^()]*\)', '' -replace '\[[^\[\]]*\]', '' -replace '<[^<>]*>', '' }
            if ($probe -match ',') { $manual.Add("${rel}:$($siteIdx+1): comma decl"); continue }
        }

        # A site separated from its block top by a preprocessor line must
        # not be hoisted across it -- the decl may only exist under that
        # condition (e.g. HANDLE inside #ifdef _WINDOWS_).
        # ...same for local type definitions: a decl must not be hoisted
        # above the struct/enum that defines its type.
        $open       = $s.Open
        $crossesPre = $false
        for ($k = $open + 1; $k -lt $siteIdx; $k++)
        {
            if ($lines[$k].TrimEnd("`r") -match '^\s*#' -or
                $lines[$k].TrimEnd("`r") -match '^\s*(struct|class|enum|union)\b')
            {
                $crossesPre = $true; break
            }
        }
        if ($crossesPre) { $manual.Add("${rel}:$($siteIdx+1): crosses preprocessor or local type"); continue }

        # Find the insert point: end of the leading decl run of the block.
        $ins  = $open          # insert AFTER this index
        $k    = $open + 1
        while ($k -lt $siteIdx)
        {
            $bl = $lines[$k].TrimEnd("`r")
            if ($bl -match $declRegex -and $bl -notmatch $declExcl -and $bl -notmatch '"') { $ins = $k; $k++; continue }
            if ($bl.Trim() -eq '' -and $ins -eq $open) { $k++; continue }   # tolerate blanks before any decl? no: stop
            break
        }

        # Determine the block's indent for the hoisted decl: match the
        # site's own indent (block-consistent in this codebase).
        # A const with a literal-only initializer (no calls) moves whole so
        # it stays const; a const with a runtime init is split and loses
        # the qualifier, matching the EHM-locals idiom.
        # Const-literal move is only safe when the init cannot reference a
        # runtime local: every identifier in it must be a named constant
        # (kFoo / s_kFoo / ALL_CAPS) and there must be no calls.
        $isConstLit = ($type -match '^const\b') -and ($null -ne $init) -and ($init -notmatch '[()?]')
        if ($isConstLit)
        {
            foreach ($id in [regex]::Matches($init, '[A-Za-z_]\w*'))
            {
                if ($id.Value -cnotmatch '^(s_k\w+|k[A-Z]\w*|[A-Z][A-Z0-9_]+|nullptr|true|false|sizeof)$')
                {
                    $isConstLit = $false; break
                }
            }
        }
        # An init that IS the type's default (S_OK, 0, false, nullptr, {})
        # has no dependencies -- move the whole line rather than leaving a
        # no-op reassignment behind.
        $defProbe   = Get-DefaultInit $type $ptr
        $isDefault  = ($null -ne $init) -and ($init -eq $defProbe)
        $moveWhole  = ($null -eq $init) -or $isConstexpr -or $isConstLit -or $isDefault -or $isArrayEmpty

        if ($moveWhole)
        {
            $hoistLine = $indent + $core + ';' + $comment + $cr
            $replacements[$siteIdx] = $null
            $moved++
        }
        else
        {
            $def = Get-DefaultInit $type $ptr

            # Splitting a CLASS type needs a default ctor plus assignment.
            # Only std-family value types are known-safe; any other class
            # type (Assembler, ClipboardManager, ...) goes to manual.
            if ($null -eq $def -and $ptr -eq '' -and
                $type -notmatch '^(const\s+)?(std::)?(w?string|wstring_view|string_view|i?stringstream$|vector\s*<|array\s*<|span\s*<|pair\s*<|optional\s*<|map\s*<|unordered_map\s*<|set\s*<|deque\s*<|unique_ptr\s*<|filesystem::path|fs::path|JsonValue$|AssemblerOptions$|AssemblyResult$|ExprResult$|DialogDefinition$|DiskMru$)')
            {
                $manual.Add("${rel}:$($siteIdx+1): class type $type"); continue
            }

            $declPart = $indent + $type + $(if ($ptr -ne '') { ' ' + $ptr + ' ' } else { '  ' }) + $decl
            if ($null -ne $def) { $declPart += ' = ' + $def }
            $hoistLine = $declPart + ';' + $cr

            # const dropped on split (the assignment needs a mutable local)
            # -- but only for non-pointer decls: on `const X * p` the
            # leading const binds to the POINTEE and must stay.
            if ($ptr -eq '')
            {
                $hoistLine = $hoistLine -replace '^(\s*)const\s+', '$1'
            }

            $replacements[$siteIdx] = $indent + $decl.Split('[')[0] + ' = ' + $init + ';' + $comment + $cr
            $hoisted++
        }

        if (-not $insertions.ContainsKey($ins)) { $insertions[$ins] = New-Object System.Collections.Generic.List[string] }
        $insertions[$ins].Add($hoistLine)

        # A brand-new decl run (nothing to append to) needs a separator
        # before the following statement: 3 blanks at function top
        # (CS0016), 1 blank inside a nested block. Recorded as a marker
        # appended once after the group; resolved at rebuild time.
        if ($ins -eq $open -and $lines[$open + 1].TrimEnd("`r").Trim() -ne '')
        {
            $marker = if ($s.FuncTop) { '<<<BLANKS3>>>' } else { '<<<BLANKS1>>>' }
            if (-not $insertions[$ins].Contains($marker)) { $insertions[$ins].Add($marker) }
        }
    }

    if ($replacements.Count -eq 0 -and $insertions.Count -eq 0) { continue }

    # Rebuild the file: walk original indices, apply replacement/deletion,
    # and append insertions after their anchor line.
    $out = [System.Collections.Generic.List[string]]::new()
    for ($i = 0; $i -lt $lines.Count; $i++)
    {
        $emit = $true
        $val  = $lines[$i]

        if ($replacements.ContainsKey($i))
        {
            if ($null -eq $replacements[$i]) { $emit = $false } else { $val = $replacements[$i] }
        }

        if ($emit) { $out.Add($val) }

        if ($insertions.ContainsKey($i))
        {
            $group  = $insertions[$i]
            $blanks = 0
            foreach ($h in $group)
            {
                if     ($h -eq '<<<BLANKS3>>>') { $blanks = 3 }
                elseif ($h -eq '<<<BLANKS1>>>') { $blanks = 1 }
                else                            { $out.Add($h) }
            }

            if ($blanks -gt 0)
            {
                $cr2 = if ($val.EndsWith("`r")) { "`r" } else { '' }
                for ($b = 0; $b -lt $blanks; $b++) { $out.Add($cr2) }
            }
        }
    }

    if ($Apply)
    {
        [IO.File]::WriteAllBytes($full, $enc.GetBytes(($out -join "`n")))
    }
    $filesHit++
}

$mode = if ($Apply) { 'APPLIED' } else { 'WOULD apply' }
"$mode split-hoist=$hoisted whole-move=$moved across $filesHit file(s); manual=$($manual.Count)"
foreach ($s in $manual) { Write-Warning "manual: $s" }
