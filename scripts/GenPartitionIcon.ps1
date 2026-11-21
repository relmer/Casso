<#
.SYNOPSIS
    Draws the icon for a partition in Casso's trees and lists, one .ico for
    dark surfaces and one for light ones.

.DESCRIPTION
    A disk drawn as a pie with its top-right quarter pulled out along the
    diagonal: the part of a disk that a partition is. The body and the slice
    each have a gradient from a lighter color at the top left to a darker one
    at the bottom right, and a dark outline.

    For dark surfaces the body is silver and the slice blue; for light ones
    the body is blue and the slice amber, so each contrasts with its
    background and both share the blue.

    Every size is drawn at its pixel size, never scaled down from 256. Up
    to SnapUpTo the pull and the outline are whole pixels, so the gap between
    the body and the slice stays open at 16 px.

    THE OUTPUT IS CHECKED IN. Casso.rc and CassoExplorer.rc embed
    Resources/Icons/Casso-partition-dark.ico as IDI_PARTITION_DARK and
    Casso-partition-light.ico as IDI_PARTITION_LIGHT; run this after
    changing the design and commit what it writes.

.PARAMETER Sheet
    Optional preview PNG: every size of both icons at 1:1 on a dark and a
    light background, then 16-48 px magnified 4x.

.OUTPUTS
    One line per file written. Exit code 0 on success.
#>
param(
    [string] $Sheet = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$iconDir  = Join-Path $repoRoot 'Resources/Icons'

Add-Type -AssemblyName System.Drawing

$sizes = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)

$design = @{
    SnapUpTo = 48       # icon size in px up to which the pull and outline are whole pixels
    Pull     = 0.085    # how far the slice is pulled out, as a fraction of the edge
    Edge     = 1 / 24   # the outline's width, as a fraction of the edge
    Angle    = 60       # the gradients' direction, in degrees
}

$variants = @(
    @{
        Name = 'Casso-partition-dark.ico'
        Body = @('#F8F9FA', '#A8ADB3', '#4A4F55')
        Cut  = @('#7CC4FF', '#0F6CBD', '#0A3D6B')
    },
    @{
        Name = 'Casso-partition-light.ico'
        Body = @('#7CC4FF', '#0F6CBD', '#0A3D6B')
        Cut  = @('#FFD27A', '#E8A317', '#7A5200')
    }
)





# Returns a color from "#RRGGBB".
function Get-Color
{
    param([string] $hex)

    return [System.Drawing.ColorTranslator]::FromHtml($hex)
}





# Draws one size of a variant and returns it as PNG bytes. The colors are
# light, dark and outline, for the body and for the slice.
function New-PartitionIcon
{
    param(
        [int]      $size,
        [string[]] $body,
        [string[]] $cut
    )

    $isSnapped = $size -le $design.SnapUpTo
    $pull      = [math]::Max(1.0, $size * $design.Pull)
    $edge      = [math]::Max(1.0, $size * $design.Edge)

    if ($isSnapped)
    {
        $pull = [math]::Round($pull)
        $edge = [math]::Round($edge)
    }

    $margin = $pull + $edge
    $side   = $size - 2 * $margin
    $step   = $pull * [math]::Sqrt(0.5)
    $bodyAt = New-Object System.Drawing.RectangleF(($margin - $step / 2), ($margin + $step / 2), $side, $side)
    $cutAt  = New-Object System.Drawing.RectangleF(($bodyAt.X + $step), ($bodyAt.Y - $step), $side, $side)

    $bitmap   = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $stream   = New-Object System.IO.MemoryStream

    try
    {
        $graphics.SmoothingMode   = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $bodyFill = New-Object System.Drawing.Drawing2D.LinearGradientBrush($bodyAt, (Get-Color $body[0]), (Get-Color $body[1]), $design.Angle)
        $cutFill  = New-Object System.Drawing.Drawing2D.LinearGradientBrush($cutAt,  (Get-Color $cut[0]),  (Get-Color $cut[1]),  $design.Angle)
        $bodyPen  = New-Object System.Drawing.Pen((Get-Color $body[2]), $edge)
        $cutPen   = New-Object System.Drawing.Pen((Get-Color $cut[2]),  $edge)

        $graphics.FillPie($bodyFill, $bodyAt.X, $bodyAt.Y, $bodyAt.Width, $bodyAt.Height, 0, 270)
        $graphics.DrawPie($bodyPen,  $bodyAt.X, $bodyAt.Y, $bodyAt.Width, $bodyAt.Height, 0, 270)
        $graphics.FillPie($cutFill,  $cutAt.X,  $cutAt.Y,  $cutAt.Width,  $cutAt.Height,  270, 90)
        $graphics.DrawPie($cutPen,   $cutAt.X,  $cutAt.Y,  $cutAt.Width,  $cutAt.Height,  270, 90)

        $bodyFill.Dispose()
        $cutFill.Dispose()
        $bodyPen.Dispose()
        $cutPen.Dispose()

        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)

        return ,$stream.ToArray()
    }
    finally
    {
        $graphics.Dispose()
        $bitmap.Dispose()
        $stream.Dispose()
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

        foreach ($png in $pngs)
        {
            $writer.Write($png)
        }
    }
    finally
    {
        $writer.Close()
    }
}





# Preview: both icons at every size on a dark and a light background, then
# the sizes up to 48 px at 4x with nearest-neighbor scaling.
function Write-Sheet
{
    param(
        [int[]]  $sizes,
        [object] $sets,
        [string] $path
    )

    $gap    = 12
    $zoom   = 4
    $rowH   = 256 + 2 * $gap
    $zoomH  = 48 * $zoom + 2 * $gap
    $width  = 0

    foreach ($size in $sizes)
    {
        $width += $size + $gap
    }

    $width  = [math]::Max($width + $gap, (16 + 20 + 24 + 32 + 40 + 48) * $zoom + 7 * $gap)
    $height = $sets.Count * 2 * ($rowH + $zoomH)
    $sheet  = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $draw   = [System.Drawing.Graphics]::FromImage($sheet)

    try
    {
        $draw.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $draw.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::Half

        $top = 0

        foreach ($set in $sets)
        {
            foreach ($background in '#202020', '#F3F3F3')
            {
                $brush = New-Object System.Drawing.SolidBrush((Get-Color $background))
                $draw.FillRectangle($brush, 0, $top, $width, $rowH + $zoomH)
                $brush.Dispose()

                $x = $gap

                for ($i = 0; $i -lt $sizes.Count; $i++)
                {
                    $stream = New-Object System.IO.MemoryStream(, $set[$i])
                    $image  = [System.Drawing.Image]::FromStream($stream)
                    $draw.DrawImage($image, $x, $top + $gap, $sizes[$i], $sizes[$i])

                    if ($sizes[$i] -le 48)
                    {
                        $zx = $gap + ($sizes | Where-Object { $_ -lt $sizes[$i] } | ForEach-Object { $_ * $zoom + $gap } | Measure-Object -Sum).Sum
                        $draw.DrawImage($image, [int] $zx, $top + $rowH, $sizes[$i] * $zoom, $sizes[$i] * $zoom)
                    }

                    $image.Dispose()
                    $stream.Dispose()
                    $x += $sizes[$i] + $gap
                }

                $top += $rowH + $zoomH
            }
        }

        $sheet.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $draw.Dispose()
        $sheet.Dispose()
    }
}





$sets = New-Object System.Collections.Generic.List[object]

foreach ($variant in $variants)
{
    $pngs = New-Object System.Collections.Generic.List[byte[]]

    foreach ($size in $sizes)
    {
        $pngs.Add((New-PartitionIcon $size $variant.Body $variant.Cut))
    }

    $out = Join-Path $iconDir $variant.Name
    Write-Ico $sizes $pngs $out
    Write-Host "Wrote $out ($($sizes.Count) sizes)"
    $sets.Add($pngs)
}

if (-not [string]::IsNullOrEmpty($Sheet))
{
    Write-Sheet $sizes $sets $Sheet
    Write-Host "Wrote $Sheet"
}
