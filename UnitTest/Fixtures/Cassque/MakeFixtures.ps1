<#
.SYNOPSIS
    Regenerates every fixture in this directory from repo-original content.

.DESCRIPTION
    Builds one DOS 3.3 image and one ProDOS image through CassoCli's disk
    commands, then records the command-line outputs the Cassque tests compare
    against. Everything here is authored for this repository; nothing is
    copied from a vendor disk.

    The ProDOS subdirectory is written by hand because the volume layer has no
    directory-creation call. The layout follows the ProDOS Technical Reference
    Manual: a subdirectory entry in the volume directory (storage type $D), one
    key block holding the subdirectory header (storage type $E), and the block
    marked used in the volume bitmap.

    Modification dates are stamped by hand for the same reason: the writer
    zeroes the date fields, and the tests need entries that carry one.

.PARAMETER CassoCli
    Path to CassoCli.exe. Defaults to the x64 Debug build.
#>
param(
    [string]$CassoCli = (Join-Path $PSScriptRoot '..\..\..\x64\Debug\CassoCli.exe')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$here    = $PSScriptRoot
$dos     = Join-Path $here 'dos33.dsk'
$pro     = Join-Path $here 'prodos.po'
$hostDir = Join-Path $here 'Host'

if (-not (Test-Path $CassoCli)) { throw "CassoCli not found at $CassoCli" }

New-Item -ItemType Directory -Force $hostDir | Out-Null

function Invoke-Cli {
    param([string[]]$Arguments)
    $output = & $CassoCli @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "CassoCli $($Arguments -join ' ') failed ($LASTEXITCODE):`n$output" }
    return $output
}

# ---------------------------------------------------------------------------
# Host-side fixtures (also the sources of the on-disk files).
# ---------------------------------------------------------------------------

$applesoft = @(
    '10 PRINT "HELLO, CASSQUE"',
    '20 FOR I = 1 TO 3',
    '30 PRINT I * 2',
    '40 NEXT I',
    '50 REM  A REMARK WITH   SPACES',
    '60 END'
) -join "`r`n"
$applesoft += "`r`n"

$implicitLet = @(
    '10 A = 5',
    '20 B$ = "TEXT"',
    '30 C = A * 2'
) -join "`r`n"
$implicitLet += "`r`n"

$numberedReadme = @(
    '1 Introduction',
    '2 Installing the program',
    '3 Running it'
) -join "`r`n"
$numberedReadme += "`r`n"

# The Integer BASIC listing carries DSP, which Applesoft never had, so the
# content rule cannot mistake it for an Applesoft program.
$integerListing = @(
    '10 PRINT "HELLO"',
    '15 DSP I',
    '20 FOR I=1 TO 3',
    '30 PRINT I',
    '40 NEXT I',
    '50 END'
) -join "`r`n"
$integerListing += "`r`n"

$text = @(
    'CASSQUE TEST NOTES',
    '',
    'This file is ordinary printable text.',
    "It has a tab`there and ends with a newline."
) -join "`r`n"
$text += "`r`n"

[System.IO.File]::WriteAllText((Join-Path $hostDir 'applesoft.txt'),       $applesoft,      [System.Text.Encoding]::ASCII)
[System.IO.File]::WriteAllText((Join-Path $hostDir 'implicit-let.txt'),    $implicitLet,    [System.Text.Encoding]::ASCII)
[System.IO.File]::WriteAllText((Join-Path $hostDir 'numbered-readme.txt'), $numberedReadme, [System.Text.Encoding]::ASCII)
[System.IO.File]::WriteAllText((Join-Path $hostDir 'integer-listing.txt'), $integerListing, [System.Text.Encoding]::ASCII)
[System.IO.File]::WriteAllText((Join-Path $hostDir 'notes.txt'),           $text,           [System.Text.Encoding]::ASCII)

# Binary blobs. Deterministic patterns with the high bit set often enough that
# no printable-text rule can claim them.
function New-Pattern {
    param([int]$Length, [int]$Seed)
    $bytes = New-Object byte[] $Length
    $state = $Seed
    for ($i = 0; $i -lt $Length; $i++) {
        $state = ($state * 1103515245 + 12345) -band 0x7FFFFFFF
        $bytes[$i] = [byte](($state -shr 16) -band 0xFF)
    }
    return $bytes
}

$hires  = New-Pattern -Length 8192  -Seed 1
$lores  = New-Pattern -Length 1024  -Seed 2
$dhires = New-Pattern -Length 16384 -Seed 3
$odd    = New-Pattern -Length 777   -Seed 4

[System.IO.File]::WriteAllBytes((Join-Path $hostDir 'hires.bin'),  $hires)
[System.IO.File]::WriteAllBytes((Join-Path $hostDir 'lores.bin'),  $lores)
[System.IO.File]::WriteAllBytes((Join-Path $hostDir 'dhires.bin'), $dhires)
[System.IO.File]::WriteAllBytes((Join-Path $hostDir 'odd.bin'),    $odd)

# The Integer BASIC program, tokenized by hand: each line is a length byte
# (counting itself), a little-endian line number, tokens, and $01. Keywords
# are single bytes below $80; letters and digits carry the high bit; an
# integer constant is its first digit (high bit set) followed by the
# little-endian value; a string is $28, high-bit characters, $29.
function Hi { param([string]$s) return [byte[]]($s.ToCharArray() | ForEach-Object { [byte]([int]$_ -bor 0x80) }) }
function IntConst { param([int]$v) return [byte[]]@(([byte]([int][char]("$v"[0]) -bor 0x80)), [byte]($v -band 0xFF), [byte]($v -shr 8)) }
function IntLine {
    param([int]$Number, [byte[]]$Body)
    $len = 3 + $Body.Length + 1
    return [byte[]](@([byte]$len, [byte]($Number -band 0xFF), [byte]($Number -shr 8)) + $Body + @([byte]0x01))
}

$integerProgram = [byte[]]@()
$integerProgram += IntLine 10 ([byte[]](@(0x62, 0x28) + (Hi 'HELLO') + @(0x29)))          # PRINT "HELLO"
$integerProgram += IntLine 15 ([byte[]](@(0x7B) + (Hi 'I')))                                # DSP I
$integerProgram += IntLine 20 ([byte[]](@(0x55) + (Hi 'I') + @(0x56) + (IntConst 1) + @(0x57) + (IntConst 3)))   # FOR I=1 TO 3
$integerProgram += IntLine 30 ([byte[]](@(0x61) + (Hi 'I')))                                # PRINT I
$integerProgram += IntLine 40 ([byte[]](@(0x59) + (Hi 'I')))                                # NEXT I
$integerProgram += IntLine 50 ([byte[]]@(0x51))                                             # END

[System.IO.File]::WriteAllBytes((Join-Path $hostDir 'integer.tok'), $integerProgram)

# ---------------------------------------------------------------------------
# The two images.
# ---------------------------------------------------------------------------

Remove-Item -Force -ErrorAction SilentlyContinue $dos, $pro

Invoke-Cli @('disk', 'create', $dos, '--format', 'dos33', '--volume', '77') | Out-Null
Invoke-Cli @('disk', 'create', $pro, '--format', 'prodos', '--volume', 'CASSQUE') | Out-Null

foreach ($image in @($dos, $pro)) {
    Invoke-Cli @('disk', 'put', $image, (Join-Path $hostDir 'applesoft.txt'), '--as', 'HELLO',   '--basic') | Out-Null
    Invoke-Cli @('disk', 'put', $image, (Join-Path $hostDir 'notes.txt'),     '--as', 'NOTES',   '--text')  | Out-Null
    Invoke-Cli @('disk', 'put', $image, (Join-Path $hostDir 'hires.bin'),     '--as', 'PICTURE', '--type', 'B', '--load', '$2000') | Out-Null
    Invoke-Cli @('disk', 'put', $image, (Join-Path $hostDir 'lores.bin'),     '--as', 'LORES',   '--type', 'B', '--load', '$400')  | Out-Null
    Invoke-Cli @('disk', 'put', $image, (Join-Path $hostDir 'dhires.bin'),    '--as', 'DHIRES',  '--type', 'B', '--load', '$2000') | Out-Null
    Invoke-Cli @('disk', 'put', $image, (Join-Path $hostDir 'odd.bin'),       '--as', 'ODD',     '--type', 'B', '--load', '$803')  | Out-Null
}

# Integer BASIC is a DOS 3.3 type; ProDOS has no command-line spelling for it.
Invoke-Cli @('disk', 'put', $dos, (Join-Path $hostDir 'integer.tok'), '--as', 'INTPROG', '--type', 'I') | Out-Null

# ---------------------------------------------------------------------------
# ProDOS: one subdirectory and modification dates, by hand.
# ---------------------------------------------------------------------------

$bytes = [System.IO.File]::ReadAllBytes($pro)

function Block-Offset { param([int]$Block) return $Block * 512 }

$entryLength     = 0x27
$entriesPerBlock = 0x0D
$dirKeyBlock     = 2
$bitmapBlock     = 6

# ProDOS date word: year in bits 15-9, month in 8-5, day in 4-0; time word:
# minute in the low byte, hour in the high byte. 1984-08-17 12:34.
$dateWord = (84 -shl 9) -bor (8 -shl 5) -bor 17
$timeWord = (12 -shl 8) -bor 34
$stamp    = [byte[]]@(($dateWord -band 0xFF), ($dateWord -shr 8), ($timeWord -band 0xFF), ($timeWord -shr 8))

# Stamp creation and modification on every active file entry.
for ($block = 2; $block -le 5; $block++) {
    $base  = Block-Offset $block
    $first = if ($block -eq $dirKeyBlock) { 1 } else { 0 }
    for ($n = $first; $n -lt $entriesPerBlock; $n++) {
        $at = $base + 4 + $n * $entryLength
        if (($bytes[$at] -band 0xF0) -eq 0) { continue }
        [Array]::Copy($stamp, 0, $bytes, $at + 0x18, 4)
        [Array]::Copy($stamp, 0, $bytes, $at + 0x21, 4)
    }
}

# Find the first inactive entry slot for the subdirectory.
$slotBlock = -1
$slotIndex = -1
for ($block = 2; $block -le 5 -and $slotBlock -lt 0; $block++) {
    $base  = Block-Offset $block
    $first = if ($block -eq $dirKeyBlock) { 1 } else { 0 }
    for ($n = $first; $n -lt $entriesPerBlock; $n++) {
        $at = $base + 4 + $n * $entryLength
        if (($bytes[$at] -band 0xF0) -eq 0) { $slotBlock = $block; $slotIndex = $n; break }
    }
}
if ($slotBlock -lt 0) { throw 'volume directory is full' }

# Find the first free block in the bitmap (a SET bit is free).
$keyBlock = -1
for ($b = 7; $b -lt 280 -and $keyBlock -lt 0; $b++) {
    $byteAt = (Block-Offset $bitmapBlock) + [int][Math]::Floor($b / 8)
    $mask   = 0x80 -shr ($b % 8)
    if (($bytes[$byteAt] -band $mask) -ne 0) { $keyBlock = $b }
}
if ($keyBlock -lt 0) { throw 'no free block for the subdirectory' }

$name  = 'SUBDIR'
$entry = (Block-Offset $slotBlock) + 4 + $slotIndex * $entryLength
for ($i = 0; $i -lt $entryLength; $i++) { $bytes[$entry + $i] = 0 }
$bytes[$entry] = [byte](0xD0 -bor $name.Length)
[Array]::Copy([System.Text.Encoding]::ASCII.GetBytes($name), 0, $bytes, $entry + 1, $name.Length)
$bytes[$entry + 0x10] = 0x0F                                        # file type: directory
$bytes[$entry + 0x11] = [byte]($keyBlock -band 0xFF)
$bytes[$entry + 0x12] = [byte]($keyBlock -shr 8)
$bytes[$entry + 0x13] = 1                                           # blocks used
$bytes[$entry + 0x15] = 0x00; $bytes[$entry + 0x16] = 0x02          # eof 512
[Array]::Copy($stamp, 0, $bytes, $entry + 0x18, 4)                  # created
$bytes[$entry + 0x1E] = 0xC3                                        # access
[Array]::Copy($stamp, 0, $bytes, $entry + 0x21, 4)                  # modified
$bytes[$entry + 0x25] = $dirKeyBlock                                # header pointer

$key = Block-Offset $keyBlock
for ($i = 0; $i -lt 512; $i++) { $bytes[$key + $i] = 0 }
$hdr = $key + 4
$bytes[$hdr] = [byte](0xE0 -bor $name.Length)
[Array]::Copy([System.Text.Encoding]::ASCII.GetBytes($name), 0, $bytes, $hdr + 1, $name.Length)
$bytes[$hdr + 0x10] = 0x75                                          # reserved, as ProDOS writes it
[Array]::Copy($stamp, 0, $bytes, $hdr + 0x18, 4)                    # created
$bytes[$hdr + 0x1E] = 0xC3                                          # access
$bytes[$hdr + 0x1F] = $entryLength
$bytes[$hdr + 0x20] = $entriesPerBlock
$bytes[$hdr + 0x21] = 0; $bytes[$hdr + 0x22] = 0                    # file count
$bytes[$hdr + 0x23] = $dirKeyBlock; $bytes[$hdr + 0x24] = 0         # parent pointer
$bytes[$hdr + 0x25] = [byte]$slotIndex                              # parent entry number
$bytes[$hdr + 0x26] = $entryLength                                  # parent entry length

# Mark the key block used and count the new entry.
$bitAt = (Block-Offset $bitmapBlock) + [int][Math]::Floor($keyBlock / 8)
$bytes[$bitAt] = [byte]($bytes[$bitAt] -band (-bnot (0x80 -shr ($keyBlock % 8))) -band 0xFF)
$countAt = (Block-Offset $dirKeyBlock) + 4 + 0x21
$count   = $bytes[$countAt] -bor ($bytes[$countAt + 1] -shl 8)
$count++
$bytes[$countAt]     = [byte]($count -band 0xFF)
$bytes[$countAt + 1] = [byte]($count -shr 8)

[System.IO.File]::WriteAllBytes($pro, $bytes)

# ---------------------------------------------------------------------------
# Expected command-line outputs.
# ---------------------------------------------------------------------------

$expected = Join-Path $here 'Expected'
New-Item -ItemType Directory -Force $expected | Out-Null

Invoke-Cli @('disk', 'get', $dos, 'HELLO', '--basic', '--out', (Join-Path $expected 'dos33-HELLO-basic.txt')) | Out-Null
Invoke-Cli @('disk', 'get', $dos, 'NOTES', '--text',  '--out', (Join-Path $expected 'dos33-NOTES-text.txt'))  | Out-Null
Invoke-Cli @('disk', 'get', $pro, 'HELLO', '--basic', '--out', (Join-Path $expected 'prodos-HELLO-basic.txt')) | Out-Null
Invoke-Cli @('disk', 'get', $pro, 'NOTES', '--text',  '--out', (Join-Path $expected 'prodos-NOTES-text.txt'))  | Out-Null

$dosList = ((Invoke-Cli @('disk', 'list', $dos)) -join "`n") + "`n"
$proList = ((Invoke-Cli @('disk', 'list', $pro)) -join "`n") + "`n"
[System.IO.File]::WriteAllText((Join-Path $expected 'dos33-list.txt'),  $dosList, [System.Text.Encoding]::ASCII)
[System.IO.File]::WriteAllText((Join-Path $expected 'prodos-list.txt'), $proList, [System.Text.Encoding]::ASCII)

Write-Host "Fixtures regenerated under $here"
