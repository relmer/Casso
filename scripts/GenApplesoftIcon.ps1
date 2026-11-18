<#
.SYNOPSIS
    Draws the icon Casso Explorer shows for an Applesoft BASIC file inside a
    disk image and packs every size into one .ico.

.DESCRIPTION
    An Apple II monitor: a beige case around a dark green phosphor screen.
    From FullFrom px up the screen holds three numbered BASIC lines, then
    the ] prompt with the checkerboard cursor beside it, square and no
    taller than the prompt. Below that size text cannot be read, so the
    lines become bright bars and the prompt a short bar with a block cursor.

    THE OUTPUT IS CHECKED IN. CassoExplorer.rc embeds
    Resources/Icons/Casso-applesoft.ico as IDI_APPLESOFT_FILE; run this
    after changing the design and commit what it writes.

.PARAMETER Out
    The .ico to write. Default: Resources/Icons/Casso-applesoft.ico.

.PARAMETER Sheet
    Optional preview PNG: every size at 1:1 on a dark and a light background.

.OUTPUTS
    One line per file written. Exit code 0 on success.
#>
param(
    [string] $Out   = "",
    [string] $Sheet = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$iconDir  = Join-Path $repoRoot 'Resources/Icons'

if ([string]::IsNullOrEmpty($Out))
{
    $Out = Join-Path $iconDir 'Casso-applesoft.ico'
}

Add-Type -AssemblyName System.Drawing

$sizes    = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
$fullFrom = 64   # icon size in px from which the screen shows real BASIC lines

$colors = @{
    Case      = [System.Drawing.Color]::FromArgb(255, 0xC9, 0xC1, 0xA6)
    CaseEdge  = [System.Drawing.Color]::FromArgb(255, 0x8C, 0x84, 0x6C)
    ScreenMid = [System.Drawing.Color]::FromArgb(255, 0x0E, 0x2A, 0x12)
    ScreenRim = [System.Drawing.Color]::FromArgb(255, 0x03, 0x0A, 0x04)
    Phosphor  = [System.Drawing.Color]::FromArgb(255, 0x6C, 0xF2, 0x6C)
    Cursor    = [System.Drawing.Color]::FromArgb(255, 0x7C, 0xFF, 0x7C)
}

$lines = @('10 HOME', '20 PRINT', '30 GOTO 20')





# A rounded rectangle as a path, in pixels.
function New-RoundedPath
{
    param([float] $x, [float] $y, [float] $w, [float] $h, [float] $r)

    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d    = [Math]::Max(0.1, $r * 2)

    $path.AddArc($x,          $y,          $d, $d, 180, 90)
    $path.AddArc($x + $w - $d, $y,          $d, $d, 270, 90)
    $path.AddArc($x + $w - $d, $y + $h - $d, $d, $d,   0, 90)
    $path.AddArc($x,          $y + $h - $d, $d, $d,  90, 90)
    $path.CloseFigure()

    return ,$path
}





# The case, the screen and the stand, shared by both drawings. Positions are
# in the design's own units, `unit` pixels each.
function Add-Monitor
{
    param($g, [float] $unit, [float] $caseX, [float] $caseY, [float] $caseW, [float] $caseH, [float] $caseR,
          [float] $scrX, [float] $scrY, [float] $scrW, [float] $scrH, [float] $scrR,
          [float] $standX, [float] $standY, [float] $standW, [float] $standH)

    $case   = New-RoundedPath ($caseX * $unit) ($caseY * $unit) ($caseW * $unit) ($caseH * $unit) ($caseR * $unit)
    $screen = New-RoundedPath ($scrX * $unit) ($scrY * $unit) ($scrW * $unit) ($scrH * $unit) ($scrR * $unit)
    $glow   = New-Object System.Drawing.Drawing2D.PathGradientBrush($screen)

    $glow.CenterColor    = $colors.ScreenMid
    $glow.SurroundColors = @($colors.ScreenRim)

    $g.FillPath((New-Object System.Drawing.SolidBrush($colors.Case)), $case)
    $g.DrawPath((New-Object System.Drawing.Pen($colors.CaseEdge, [Math]::Max(1.0, 1.2 * $unit))), $case)
    $g.FillPath($glow, $screen)
    $g.FillRectangle((New-Object System.Drawing.SolidBrush($colors.CaseEdge)), $standX * $unit, $standY * $unit, $standW * $unit, $standH * $unit)
}





# The checkerboard cursor: a square of cells, lit and dark alternating.
function Add-Cursor
{
    param($g, [float] $x, [float] $y, [float] $side, [int] $cells)

    $lit  = New-Object System.Drawing.SolidBrush($colors.Cursor)
    $cell = $side / $cells

    for ($row = 0; $row -lt $cells; $row++)
    {
        for ($col = 0; $col -lt $cells; $col++)
        {
            if ((($row + $col) % 2) -eq 0)
            {
                $g.FillRectangle($lit, $x + $col * $cell, $y + $row * $cell, $cell, $cell)
            }
        }
    }
}





# The full drawing, on a 64-unit grid: the lines as text.
function Add-FullScreen
{
    param($g, [int] $size)

    $unit     = $size / 64.0
    $font     = New-Object System.Drawing.Font('Consolas', [float] (7.2 * $unit), [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $ink      = New-Object System.Drawing.SolidBrush($colors.Phosphor)
    $format   = [System.Drawing.StringFormat]::GenericTypographic
    $ascent   = $font.FontFamily.GetCellAscent($font.Style) * $font.Size / $font.FontFamily.GetEmHeight($font.Style)
    $baseline = @(21.0, 30.0, 39.0)

    Add-Monitor $g $unit 3 6 58 50 7  8 11 48 38 4  26 57 12 3

    for ($i = 0; $i -lt $lines.Count; $i++)
    {
        $g.DrawString($lines[$i], $font, $ink, [float] (11 * $unit), [float] ($baseline[$i] * $unit - $ascent), $format)
    }

    #  The prompt, and the cursor beside it as tall as its top allows.
    $g.DrawString(']', $font, $ink, [float] (11 * $unit), [float] (47.5 * $unit - $ascent), $format)
    Add-Cursor $g (15.5 * $unit) (43.5 * $unit) (4 * $unit) 4
}





# The simplified drawing, on a 16-unit grid: bars for lines.
function Add-SmallScreen
{
    param($g, [int] $size)

    $unit = $size / 16.0
    $ink  = New-Object System.Drawing.SolidBrush($colors.Phosphor)

    Add-Monitor $g $unit 0.5 1.5 15 12.5 2  2 3 12 9.5 1  6.5 14.4 3 1

    $g.FillRectangle($ink, 3 * $unit, 4.2 * $unit, 7 * $unit, 1.2 * $unit)
    $g.FillRectangle($ink, 3 * $unit, 6.4 * $unit, 9 * $unit, 1.2 * $unit)
    $g.FillRectangle($ink, 3 * $unit, 8.6 * $unit, 1 * $unit, 2.2 * $unit)
    $g.FillRectangle((New-Object System.Drawing.SolidBrush($colors.Cursor)), 4.6 * $unit, 8.6 * $unit, 1.6 * $unit, 2.2 * $unit)
}





function New-ApplesoftIcon
{
    param([int] $size)

    $bitmap = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g      = [System.Drawing.Graphics]::FromImage($bitmap)

    try
    {
        $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        $g.Clear([System.Drawing.Color]::Transparent)

        if ($size -ge $fullFrom)
        {
            Add-FullScreen $g $size
        }
        else
        {
            Add-SmallScreen $g $size
        }

        $stream = New-Object System.IO.MemoryStream
        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        return ,$stream.ToArray()
    }
    finally
    {
        $g.Dispose()
        $bitmap.Dispose()
    }
}





function Write-Ico
{
    param([int[]] $sizes, [object] $pngs, [string] $path)

    $stream = [System.IO.File]::Create($path)
    $writer = New-Object System.IO.BinaryWriter($stream)

    try
    {
        # ICONDIR: reserved, type 1 (icon), entry count.
        $writer.Write([uint16] 0)
        $writer.Write([uint16] 1)
        $writer.Write([uint16] $sizes.Count)

        $offset = 6 + 16 * $sizes.Count

        # ICONDIRENTRY per size; 256 is written as 0.
        for ($i = 0; $i -lt $sizes.Count; $i++)
        {
            $writer.Write([byte] ($sizes[$i] % 256))
            $writer.Write([byte] ($sizes[$i] % 256))
            $writer.Write([byte] 0)
            $writer.Write([byte] 0)
            $writer.Write([uint16] 1)
            $writer.Write([uint16] 32)
            $writer.Write([uint32] $pngs[$i].Length)
            $writer.Write([uint32] $offset)

            $offset += $pngs[$i].Length
        }

        foreach ($d in $pngs)
        {
            $writer.Write($d)
        }
    }
    finally
    {
        $writer.Close()
    }
}





function Write-Sheet
{
    param([int[]] $sizes, [object] $pngs, [string] $path)

    $width  = [int] (40 + ($sizes | Measure-Object -Sum).Sum + 24 * $sizes.Count)
    $sheet  = New-Object System.Drawing.Bitmap($width, 600)
    $g      = [System.Drawing.Graphics]::FromImage($sheet)

    try
    {
        $g.FillRectangle((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 25, 25, 25))), 0, 0, $width, 300)
        $g.FillRectangle([System.Drawing.Brushes]::White, 0, 300, $width, 300)

        foreach ($band in @(0, 300))
        {
            $x = 20

            for ($i = 0; $i -lt $sizes.Count; $i++)
            {
                $image = [System.Drawing.Image]::FromStream((New-Object System.IO.MemoryStream(, $pngs[$i])))
                $g.DrawImageUnscaled($image, $x, $band + 280 - $sizes[$i])
                $x += $sizes[$i] + 24
            }
        }

        $sheet.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $g.Dispose()
        $sheet.Dispose()
    }
}





$pngs = New-Object System.Collections.Generic.List[byte[]]

foreach ($size in $sizes)
{
    $pngs.Add((New-ApplesoftIcon $size))
}

Write-Ico $sizes $pngs $Out
Write-Host "Wrote $Out ($($sizes.Count) sizes)"

if (-not [string]::IsNullOrEmpty($Sheet))
{
    Write-Sheet $sizes $pngs $Sheet
    Write-Host "Wrote $Sheet"
}
