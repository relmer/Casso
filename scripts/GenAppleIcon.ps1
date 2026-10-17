<#
.SYNOPSIS
    Draws Casso Explorer's tree icon for an Apple II machine,
    and packs every size into one .ico.

.DESCRIPTION
    The classic six-stripe apple of the 1980s: a body of two rounded lobes
    with a dip at the top and a smaller one at the bottom, a bite out of
    the right side, green, yellow, orange, red, purple and blue bands from
    top to bottom, and a green leaf above it. A dark outline sets the apple
    apart from a dark background.

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
    Outline   = [System.Drawing.Color]::FromArgb(255, 0x2A, 0x2A, 0x2A)
    Leaf      = [System.Drawing.Color]::FromArgb(255, 0x61, 0xBB, 0x46)
    LeafEdge  = [System.Drawing.Color]::FromArgb(255, 0x2B, 0x6B, 0x1F)
    Stripes   = @(
        [System.Drawing.Color]::FromArgb(255, 0x61, 0xBB, 0x46),
        [System.Drawing.Color]::FromArgb(255, 0xFD, 0xB8, 0x27),
        [System.Drawing.Color]::FromArgb(255, 0xF5, 0x82, 0x1F),
        [System.Drawing.Color]::FromArgb(255, 0xE0, 0x3A, 0x3E),
        [System.Drawing.Color]::FromArgb(255, 0x96, 0x3D, 0x97),
        [System.Drawing.Color]::FromArgb(255, 0x00, 0x9D, 0xDC))
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

        $outlineWidth = [single] [Math]::Max(1.0, $size / 44.0)

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

        # The body, with a bite out of its right side.
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

        $bite = New-Object System.Drawing.Drawing2D.GraphicsPath
        $bite.AddEllipse([single] (0.80 * $s), [single] (0.44 * $s), [single] (0.26 * $s), [single] (0.26 * $s))

        $shape = New-Object System.Drawing.Region($body)
        $shape.Exclude($bite)

        # The bands, drawn through the body less the bite.
        $top    = 0.26
        $bottom = 0.98
        $band   = ($bottom - $top) / $colors.Stripes.Count

        $graphics.SetClip($shape, [System.Drawing.Drawing2D.CombineMode]::Replace)

        for ($i = 0; $i -lt $colors.Stripes.Count; $i++)
        {
            $brush = New-Object System.Drawing.SolidBrush($colors.Stripes[$i])
            $graphics.FillRectangle($brush, [single] 0, [single] (($top + $i * $band) * $s), $s, [single] ($band * $s + 1))
            $brush.Dispose()
        }

        $graphics.ResetClip()

        # The outline of the body less the bite.
        $outline = New-Object System.Drawing.Pen($colors.Outline, $outlineWidth)
        $edge    = New-Object System.Drawing.Drawing2D.GraphicsPath
        $edge.AddPath($body, $false)
        $graphics.SetClip($bite, [System.Drawing.Drawing2D.CombineMode]::Exclude)
        $graphics.DrawPath($outline, $edge)
        $graphics.ResetClip()
        $graphics.SetClip($body, [System.Drawing.Drawing2D.CombineMode]::Replace)
        $graphics.DrawPath($outline, $bite)
        $graphics.ResetClip()

        $leaf.Dispose(); $turn.Dispose(); $leafBrush.Dispose(); $leafPen.Dispose()
        $body.Dispose(); $bite.Dispose(); $shape.Dispose(); $edge.Dispose(); $outline.Dispose()

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
