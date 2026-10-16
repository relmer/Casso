<#
.SYNOPSIS
    Draws Casso Explorer's tree icon for an Apple II machine,
    and packs every size into one .ico.

.DESCRIPTION
    A red apple, the fruit: a body of two rounded lobes with a dip at the
    top and a smaller one at the bottom, shaded darker toward the lower
    right, a brown stem and a green leaf above it, and a soft highlight on
    the upper left from 32 px up. A dark outline sets the apple apart from a dark
    background. It is not Apple's logo: no bite, no stripes.

    Every size is drawn at its own pixel size, never scaled down from 256.

    THE OUTPUT IS CHECKED IN. CassoExplorer.rc embeds
    Resources/Icons/Casso-apple-machine.ico as IDI_APPLE_MACHINE; run this
    after changing the design and commit what it writes.

.PARAMETER Out
    The .ico to write. Default: Resources/Icons/Casso-apple-machine.ico.

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
    $Out = Join-Path $iconDir 'Casso-apple-machine.ico'
}

Add-Type -AssemblyName System.Drawing

$sizes = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)





# The body's outline in fractions of the icon's edge, as cubic Bezier
# segments: each is two control points and an end point, starting from the
# dip at the top.
$bodyStart = @(0.50, 0.33)
$bodyCurves = @(
    @(0.40, 0.25,  0.10, 0.24,  0.08, 0.50),
    @(0.07, 0.74,  0.24, 0.98,  0.38, 0.96),
    @(0.44, 0.94,  0.56, 0.94,  0.62, 0.96),
    @(0.76, 0.98,  0.93, 0.74,  0.92, 0.50),
    @(0.90, 0.24,  0.60, 0.25,  0.50, 0.33)
)

$colors = @{
    BodyLight = [System.Drawing.Color]::FromArgb(255, 0xE8, 0x3A, 0x30)
    BodyDark  = [System.Drawing.Color]::FromArgb(255, 0x9E, 0x14, 0x14)
    Outline   = [System.Drawing.Color]::FromArgb(255, 0x5A, 0x0C, 0x0C)
    Stem      = [System.Drawing.Color]::FromArgb(255, 0x6B, 0x45, 0x23)
    Leaf      = [System.Drawing.Color]::FromArgb(255, 0x4C, 0xA8, 0x3A)
    LeafEdge  = [System.Drawing.Color]::FromArgb(255, 0x2B, 0x6B, 0x1F)
    Highlight = [System.Drawing.Color]::FromArgb(110, 0xFF, 0xFF, 0xFF)
}





# One size, as PNG bytes.
function New-AppleIcon
{
    param([int] $size)

    $bitmap   = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $s        = [single] $size

    try
    {
        $graphics.SmoothingMode   = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([System.Drawing.Color]::Transparent)

        $outlineWidth = [single] [Math]::Max(1.0, $size / 28.0)

        # The leaf, behind the stem: an ellipse turned up to the right.
        $leaf  = New-Object System.Drawing.Drawing2D.GraphicsPath
        $leaf.AddEllipse([single] (0.52 * $s), [single] (0.12 * $s), [single] (0.28 * $s), [single] (0.12 * $s))
        $turn  = New-Object System.Drawing.Drawing2D.Matrix
        $turn.RotateAt(-24, (New-Object System.Drawing.PointF([single] (0.54 * $s), [single] (0.18 * $s))))
        $leaf.Transform($turn)

        $leafBrush = New-Object System.Drawing.SolidBrush($colors.Leaf)
        $leafPen   = New-Object System.Drawing.Pen($colors.LeafEdge, [single] [Math]::Max(1.0, $size / 48.0))
        $graphics.FillPath($leafBrush, $leaf)
        $graphics.DrawPath($leafPen, $leaf)

        # The stem.
        $stemPen = New-Object System.Drawing.Pen($colors.Stem, [single] [Math]::Max(1.5, $size / 14.0))
        $stemPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $stemPen.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
        $graphics.DrawLine($stemPen, [single] (0.50 * $s), [single] (0.34 * $s), [single] (0.47 * $s), [single] (0.13 * $s))

        # The body, shaded from the upper left.
        $body = New-Object System.Drawing.Drawing2D.GraphicsPath
        $x    = $bodyStart[0] * $s
        $y    = $bodyStart[1] * $s

        foreach ($c in $bodyCurves)
        {
            $body.AddBezier([single] $x, [single] $y,
                            [single] ($c[0] * $s), [single] ($c[1] * $s),
                            [single] ($c[2] * $s), [single] ($c[3] * $s),
                            [single] ($c[4] * $s), [single] ($c[5] * $s))
            $x = $c[4] * $s
            $y = $c[5] * $s
        }

        $body.CloseFigure()

        $shade = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            (New-Object System.Drawing.PointF([single] (0.15 * $s), [single] (0.30 * $s))),
            (New-Object System.Drawing.PointF([single] (0.85 * $s), [single] (0.98 * $s))),
            $colors.BodyLight, $colors.BodyDark)
        $outline = New-Object System.Drawing.Pen($colors.Outline, $outlineWidth)

        $graphics.FillPath($shade, $body)
        $graphics.DrawPath($outline, $body)

        # The highlight, from 32 px up, where it reads as shine and not a smudge.
        if ($size -ge 32)
        {
            $shine = New-Object System.Drawing.SolidBrush($colors.Highlight)
            $graphics.FillEllipse($shine, [single] (0.20 * $s), [single] (0.38 * $s), [single] (0.13 * $s), [single] (0.22 * $s))
            $shine.Dispose()
        }

        $leaf.Dispose(); $turn.Dispose(); $leafBrush.Dispose(); $leafPen.Dispose()
        $stemPen.Dispose(); $body.Dispose(); $shade.Dispose(); $outline.Dispose()

        $stream = New-Object System.IO.MemoryStream
        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        return ,$stream.ToArray()
    }
    finally
    {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}





# Packs PNG entries into one .ico.
function Write-Ico
{
    param(
        [int[]]  $sizes,
        [object] $pngs,
        [string] $path
    )

    $stream = [System.IO.File]::Create($path)
    $writer = New-Object System.IO.BinaryWriter($stream)
    try
    {
        $writer.Write([uint16] 0)
        $writer.Write([uint16] 1)
        $writer.Write([uint16] $sizes.Count)

        $offset = 6 + 16 * $sizes.Count

        for ($i = 0; $i -lt $sizes.Count; $i++)
        {
            $s = $sizes[$i]
            $d = $pngs[$i]

            $writer.Write([byte] ($s % 256))
            $writer.Write([byte] ($s % 256))
            $writer.Write([byte] 0)
            $writer.Write([byte] 0)
            $writer.Write([uint16] 1)
            $writer.Write([uint16] 32)
            $writer.Write([uint32] $d.Length)
            $writer.Write([uint32] $offset)

            $offset += $d.Length
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





# Preview: every size at 1:1 on a dark band and a light band.
function Write-Sheet
{
    param(
        [int[]]  $sizes,
        [object] $pngs,
        [string] $path
    )

    $gap    = 16
    $width  = [int] (($sizes | Measure-Object -Sum).Sum + $gap * ($sizes.Count + 1))
    $band   = [int] (($sizes | Measure-Object -Maximum).Maximum + 2 * $gap)
    $sheet  = New-Object System.Drawing.Bitmap($width, [int] (2 * $band), [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $canvas = [System.Drawing.Graphics]::FromImage($sheet)

    try
    {
        $canvas.Clear([System.Drawing.Color]::FromArgb(255, 0x1C, 0x1C, 0x1C))
        $canvas.FillRectangle((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0xF3, 0xF3, 0xF3))), 0, $band, $width, $band)

        for ($row = 0; $row -lt 2; $row++)
        {
            $x = $gap

            for ($i = 0; $i -lt $sizes.Count; $i++)
            {
                $ms    = New-Object System.IO.MemoryStream(, $pngs[$i])
                $image = New-Object System.Drawing.Bitmap($ms)
                $canvas.DrawImageUnscaled($image, $x, $row * $band + $gap)
                $image.Dispose()
                $ms.Dispose()
                $x += $sizes[$i] + $gap
            }
        }

        $sheet.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $canvas.Dispose()
        $sheet.Dispose()
    }
}





$pngs = New-Object System.Collections.Generic.List[byte[]]

foreach ($size in $sizes)
{
    $pngs.Add((New-AppleIcon $size))
}

Write-Ico $sizes $pngs $Out
Write-Host "Wrote $Out ($($sizes.Count) sizes)"

if (-not [string]::IsNullOrEmpty($Sheet))
{
    Write-Sheet $sizes $pngs $Sheet
    Write-Host "Wrote $Sheet"
}
