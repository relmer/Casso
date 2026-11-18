<#
.SYNOPSIS
    Draws the icon Windows shows for Casso's disk image files (.dsk .do .po
    .woz .nib .nb2) and packs every size into one .ico.

.DESCRIPTION
    A 5.25-inch floppy: a square near-black jacket, a row of two stickers
    across the top, the hub cutout with the spindle hole at the jacket's
    center, the index hole just below the hub's center line to its right,
    and the head access slot, a long narrow capsule, below the hub.

    The brand sticker at the left of the row is Casso Explorer's icon
    without its folder tab: the folder's body, a manila rounded rectangle
    shaded lighter at the top, Apple's six stripes in a band down its left
    edge with a soft shadow beside the band, and the black cassowary in its
    middle. The label at the right is white, with the same rounded corners
    and lines to write on.

    Every size is drawn at its own pixel size, never scaled down from 256.
    Up to SnapUpTo the geometry snaps to the pixel grid and is stacked from
    the top so the hub stays centered; up to PixelArtUpTo every shape is
    drawn binary, each pixel fully on or off, so no thin part washes out.

    THE OUTPUT IS CHECKED IN. Casso.rc and CassoExplorer.rc embed
    Resources/Icons/Casso-disk-image.ico as IDI_DISK_IMAGE; run this after
    changing the design and commit what it writes.

.PARAMETER Out
    The .ico to write. Default: Resources/Icons/Casso-disk-image.ico.

.PARAMETER Sheet
    Optional preview PNG: every size at 1:1 on a dark and a light background,
    then 16-48 px magnified 4x.

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
    $Out = Join-Path $iconDir 'Casso-disk-image.ico'
}

Add-Type -AssemblyName System.Drawing

$sizes = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)

# Positions and sizes are fractions of the icon's edge unless the comment
# says otherwise. Up to SnapUpTo they are rounded to the pixel grid.
$design = @{
    SnapUpTo      = 48        # icon size in px up to which geometry snaps to pixels
    PixelArtUpTo  = 40        # icon size in px up to which shapes are binary, no antialiasing
    JacketInset   = 0.02      # transparent margin around the jacket
    BorderWidth   = 1 / 128   # darker gray edge around the jacket, at least 1 px
    RowTop        = 0.07      # sticker row, top edge
    RowBottom     = 0.29      # sticker row, bottom edge, above SnapUpTo; below, one pixel row above the cutout
    RowInset      = 0.07      # sticker row, left and right edges
    BrandWidth    = 0.40      # brand sticker width, fraction of the row's width
    LabelStart    = 0.50      # label's left edge, fraction of the row's width
    StickerRadius = 0.095     # corner radius of both stickers, fraction of their height
    CornerPixels  = 40        # icon size in px from which pixel art drops the stickers' corner pixels
    BandWidth     = 0.11      # stripe band width, fraction of the sticker's height, at least 1 px ...
    BandWideFrom  = 32        # ... and at least 2 px from this icon size in px up
    ShadowDepth   = 0.85      # darkness of the band's shadow where it meets the band ...
    ShadowDepthPx = 0.25      # ... and in pixel art, where it is one column at about 0.88 of the manila
    ShadowWidth   = 0.0165    # distance over which the shadow fades by 1/e, fraction of the sticker's height ...
    ShadowMin     = 0.5       # ... but at least this many px, so the small sizes show it ...
    ShadowFrom    = 32        # ... from this icon size in px up; below it there is no room for it
    BirdTop       = 0.227     # silhouette top, fraction of the sticker's height
    BirdHeight    = 0.645     # silhouette height, fraction of the sticker's height
    BirdCenterX   = 0.542     # silhouette center, fraction of the sticker's width right of the band
    BirdGain      = 1.6       # coverage multiplier for the silhouette drawn from the mask ...
    BirdGainUpTo  = 96        # ... up to this icon size in px, so the tail and head keep a black pixel
    LineInset     = 0.08      # label lines' side margin, fraction of the label's width
    LineWidth     = 1         # label line thickness in px, at every size
    LinePitch     = 3         # least distance in px from one label line to the next
    HubCenterY    = 0.50      # hub center; it is centered horizontally
    HubCutout     = 0.30      # diameter of the jacket's cutout around the hub
    HubBinaryUpTo = 48        # icon size in px up to which the hub is binary, as in pixel art
    SpindleHole   = 0.75      # spindle hole diameter, fraction of the cutout's
    IndexHoleX    = 0.725     # index hole center
    IndexHoleDrop = 0.035     # index hole center below the hub's center
    IndexHole     = 0.05      # index hole diameter
    SlotWidth     = 0.092     # head access slot, a capsule centered horizontally
    SlotGap       = 0.035     # jacket between the cutout and the slot, at least 1 px
    SlotEnd       = 0.93      # slot's bottom end, but see SlotMargin
    SlotMargin    = 1         # least jacket in px between the slot and the border ...
    SlotMarginBig = 2         # ... and from SnapUpTo up
}

# The silhouette at these sizes, a pixel at a time ('#' is the bird): a
# shape read off the mask at a few pixels high loses the head or the tail,
# so these are drawn by hand from it. Each keeps the head at the top, with
# its beak to the left from 48 up, the tail out to the right below the
# middle and, where there is room, the notch under the tail. The box is
# centered where the larger sizes center the mask's box, with at least one
# row of manila above and below it.
$birdPixels = @{
    16 = @('#')
    20 = @('#.',
           '##')
    24 = @('##..',
           '.##.',
           '.###')
    32 = @('##..',
           '.##.',
           '.##.',
           '####',
           '##..')
    40 = @('.##..',
           '###..',
           '.##..',
           '.###.',
           '####.',
           '#####',
           '##...')
    48 = @('.##...',
           '###...',
           '.###..',
           '.###..',
           '.####.',
           '#####.',
           '######',
           '###...',
           '###...')
    64 = @('.##....',
           '###....',
           '.###...',
           '.###...',
           '.####..',
           '#####..',
           '######.',
           '#######',
           '###....',
           '###....')
}

# Whole pixels to move a table's bird right of where the box centering puts
# it, so it sits where Casso Explorer's does, a little right of the middle
# of the manila beside the band's shadow.
$birdNudge = @{
    32 = 1
}

# Which of the six stripes get a row when the band has fewer than six rows:
# green and blue always, as at the folder's top and bottom corners, then
# orange, red and purple. Yellow goes first, since against the manila it
# reads as part of the sticker.
$bandPicks = @{
    1 = @(0)
    2 = @(0, 5)
    3 = @(0, 2, 5)
    4 = @(0, 2, 3, 5)
    5 = @(0, 2, 3, 4, 5)
}

# Label lines by the label's height in pixels: the first entry whose
# threshold the label reaches gives the number of lines. They are spaced a
# whole number of pixels apart, never less than LinePitch, and centered on
# the label.
$lineCounts = @(
    @{ MinHeight = 10; Count = 3 },
    @{ MinHeight = 6;  Count = 2 }
)

$colors = @{
    Jacket  = @(38, 38, 42)
    Border  = @(70, 70, 78)
    Media   = @(120, 108, 92)    # what shows through the cutout, slot and index hole
    Spindle = @(18, 18, 20)
    Label   = @(251, 250, 246)
    Line    = @(168, 180, 198)
    Bird    = @(22, 20, 26)      # this and the two below were measured on CassoExplorer.png
    Manila  = @(254, 216, 102)   # the folder body at its top edge ...
    Manila2 = @(245, 185, 33)    # ... and at its bottom edge
}

# Apple's six stripes, top to bottom, as CassoExplorer.png draws its band.
$appleStripes = @(
    @(97, 189, 79),
    @(253, 184, 39),
    @(245, 130, 32),
    @(224, 58, 62),
    @(150, 61, 151),
    @(0, 159, 223)
)





# Supersampled rasterizer. Pixel edges on integer coordinates come out exact,
# so grid-snapped rectangles stay crisp while circles and capsules are
# antialiased by coverage. With Binary set, a pixel is drawn fully when the
# shape covers at least half of it and not at all otherwise.
Add-Type -TypeDefinition @'
using System;

public sealed class DiskIconMask
{
    public readonly int     Width;
    public readonly int     Height;
    public readonly float[] Values;
    public int              Left;
    public int              Top;
    public int              Right;
    public int              Bottom;

    // Coverage from brightness: dark is 0, light is 1, scaled by alpha.
    public DiskIconMask (byte[] bgra, int width, int height, double dark, double light)
    {
        Width  = width;
        Height = height;
        Values = new float[width * height];

        for (int i = 0; i < width * height; i++)
        {
            double level = (bgra[i * 4] + bgra[i * 4 + 1] + bgra[i * 4 + 2]) / 3.0;
            double cover = (level - dark) / (light - dark);

            cover     = Math.Max (0.0, Math.Min (1.0, cover));
            Values[i] = (float) (cover * bgra[i * 4 + 3] / 255.0);
        }

        Left   = width;
        Top    = height;
        Right  = 0;
        Bottom = 0;

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (Values[y * width + x] < 0.5f)
                {
                    continue;
                }

                Left   = Math.Min (Left, x);
                Top    = Math.Min (Top, y);
                Right  = Math.Max (Right, x + 1);
                Bottom = Math.Max (Bottom, y + 1);
            }
        }
    }

    public double Sample (double u, double v)
    {
        double fx = u - 0.5;
        double fy = v - 0.5;
        int    x0 = (int) Math.Floor (fx);
        int    y0 = (int) Math.Floor (fy);
        double tx = fx - x0;
        double ty = fy - y0;

        return (Get (x0, y0) * (1 - tx) + Get (x0 + 1, y0) * tx) * (1 - ty)
             + (Get (x0, y0 + 1) * (1 - tx) + Get (x0 + 1, y0 + 1) * tx) * ty;
    }

    private double Get (int x, int y)
    {
        if (x < 0 || y < 0 || x >= Width || y >= Height)
        {
            return 0;
        }

        return Values[y * Width + x];
    }
}

public sealed class DiskIconRaster
{
    private delegate void Sampler (double x, double y, double[] rgba);

    private readonly int      m_size;
    private readonly int      m_samples;
    private readonly double[] m_pixels;     // premultiplied RGBA, 0..1

    public bool Binary;

    public DiskIconRaster (int size, int samples)
    {
        m_size    = size;
        m_samples = samples;
        m_pixels  = new double[size * size * 4];
    }

    public void FillRect (double left, double top, double right, double bottom, int[] rgb)
    {
        Paint (left, top, right, bottom, (x, y, rgba) =>
        {
            bool inside = x >= left && x < right && y >= top && y < bottom;

            Solid (inside ? 1.0 : 0.0, rgb, rgba);
        });
    }

    public void FillEllipse (double left, double top, double right, double bottom, int[] rgb)
    {
        double cx = (left + right) / 2;
        double cy = (top + bottom) / 2;
        double rx = (right - left) / 2;
        double ry = (bottom - top) / 2;

        Paint (left, top, right, bottom, (x, y, rgba) =>
        {
            double dx = (x - cx) / rx;
            double dy = (y - cy) / ry;

            Solid (dx * dx + dy * dy <= 1.0 ? 1.0 : 0.0, rgb, rgba);
        });
    }

    // A rectangle with fully rounded ends on its short sides.
    public void FillCapsule (double left, double top, double right, double bottom, int[] rgb)
    {
        double width  = right - left;
        double height = bottom - top;
        double radius = Math.Min (width, height) / 2;
        double cx     = (left + right) / 2;
        double cy     = (top + bottom) / 2;
        double halfX  = Math.Max (0.0, width  / 2 - radius);
        double halfY  = Math.Max (0.0, height / 2 - radius);

        Paint (left, top, right, bottom, (x, y, rgba) =>
        {
            double dx = Math.Max (0.0, Math.Abs (x - cx) - halfX);
            double dy = Math.Max (0.0, Math.Abs (y - cy) - halfY);

            Solid (dx * dx + dy * dy <= radius * radius ? 1.0 : 0.0, rgb, rgba);
        });
    }

    // A rectangle with rounded corners, in one color.
    public void FillRoundRect (double left, double top, double right, double bottom, double radius, int[] rgb)
    {
        Paint (left, top, right, bottom, (x, y, rgba) =>
        {
            double dx = Math.Max (0.0, Math.Max (left + radius - x, x - (right - radius)));
            double dy = Math.Max (0.0, Math.Max (top + radius - y, y - (bottom - radius)));
            bool   inside = x >= left && x < right && y >= top && y < bottom && dx * dx + dy * dy <= radius * radius;

            Solid (inside ? 1.0 : 0.0, rgb, rgba);
        });
    }

    // The brand sticker: a rectangle with rounded corners, shaded from
    // topRgb at its top edge to bottomRgb at its bottom edge, with the
    // stripe band down its left edge, one color per pixel row, and the
    // band's shadow on the shading to the band's right, darkest against the
    // band and fading over shadowWidth.
    public void FillSticker (int left, int top, int right, int bottom, double radius, int bandRight,
                             int[][] bandRows, int[] topRgb, int[] bottomRgb, double shadowDepth, double shadowWidth)
    {
        double height = bottom - top;

        Paint (left, top, right, bottom, (x, y, rgba) =>
        {
            double dx = Math.Max (0.0, Math.Max (left + radius - x, x - (right - radius)));
            double dy = Math.Max (0.0, Math.Max (top + radius - y, y - (bottom - radius)));

            if (dx * dx + dy * dy > radius * radius)
            {
                Array.Clear (rgba, 0, 4);
                return;
            }

            if (x < bandRight)
            {
                int row = Math.Max (0, Math.Min (bandRows.Length - 1, (int) Math.Floor (y - top)));

                Solid (1.0, bandRows[row], rgba);
                return;
            }

            double t     = (y - top) / height;
            double d     = (x - bandRight) / shadowWidth;
            double light = 1.0 - shadowDepth * Math.Exp (-d * d);

            for (int c = 0; c < 3; c++)
            {
                rgba[c] = (topRgb[c] + (bottomRgb[c] - topRgb[c]) * t) / 255.0 * light;
            }

            rgba[3] = 1.0;
        });
    }

    // The mask's bounding box mapped onto the rectangle, in one color. Gain
    // multiplies each pixel's coverage, so thin parts a pixel or less across
    // still reach full color.
    public void FillMask (DiskIconMask mask, double left, double top, double right, double bottom, double gain, int[] rgb)
    {
        double scaleX = (mask.Right - mask.Left) / (right - left);
        double scaleY = (mask.Bottom - mask.Top) / (bottom - top);

        Paint (left, top, right, bottom, (x, y, rgba) =>
        {
            Solid (mask.Sample (mask.Left + (x - left) * scaleX, mask.Top + (y - top) * scaleY), rgb, rgba);
        }, gain);
    }

    // Straight-alpha BGRA, top row first.
    public byte[] ToBgra ()
    {
        byte[] bgra = new byte[m_size * m_size * 4];

        for (int i = 0; i < m_size * m_size; i++)
        {
            double a = m_pixels[i * 4 + 3];

            if (a <= 0)
            {
                continue;
            }

            bgra[i * 4]     = ToByte (m_pixels[i * 4 + 2] / a);
            bgra[i * 4 + 1] = ToByte (m_pixels[i * 4 + 1] / a);
            bgra[i * 4 + 2] = ToByte (m_pixels[i * 4]     / a);
            bgra[i * 4 + 3] = ToByte (a);
        }

        return bgra;
    }

    private static byte ToByte (double v)
    {
        return (byte) Math.Max (0, Math.Min (255, (int) Math.Round (v * 255)));
    }

    private static void Solid (double cover, int[] rgb, double[] rgba)
    {
        rgba[0] = rgb[0] / 255.0 * cover;
        rgba[1] = rgb[1] / 255.0 * cover;
        rgba[2] = rgb[2] / 255.0 * cover;
        rgba[3] = cover;
    }

    // Averages the sampler over a grid of points in each pixel the bounds
    // touch, then composites the result over the canvas. The average's
    // coverage is multiplied by gain, up to full. Binary drawing makes the
    // average opaque when it covers at least half the pixel and drops it
    // otherwise.
    private void Paint (double left, double top, double right, double bottom, Sampler sampler, double gain = 1.0)
    {
        int      x0     = Math.Max (0, (int) Math.Floor (left));
        int      y0     = Math.Max (0, (int) Math.Floor (top));
        int      x1     = Math.Min (m_size, (int) Math.Ceiling (right));
        int      y1     = Math.Min (m_size, (int) Math.Ceiling (bottom));
        double   step   = 1.0 / m_samples;
        double   norm   = 1.0 / (m_samples * m_samples);
        double[] sample = new double[4];
        double[] sum    = new double[4];

        for (int py = y0; py < y1; py++)
        {
            for (int px = x0; px < x1; px++)
            {
                Array.Clear (sum, 0, 4);

                for (int j = 0; j < m_samples; j++)
                {
                    for (int i = 0; i < m_samples; i++)
                    {
                        sampler (px + (i + 0.5) * step, py + (j + 0.5) * step, sample);

                        for (int c = 0; c < 4; c++)
                        {
                            sum[c] += sample[c];
                        }
                    }
                }

                int    k     = (py * m_size + px) * 4;
                double alpha = sum[3] * norm;
                double scale = norm;

                if (gain != 1.0 && sum[3] > 0)
                {
                    alpha = Math.Min (1.0, alpha * gain);
                    scale = alpha / sum[3];
                }

                if (Binary)
                {
                    if (alpha < 0.5)
                    {
                        continue;
                    }

                    scale = 1.0 / sum[3];
                    alpha = 1.0;
                }

                for (int c = 0; c < 4; c++)
                {
                    m_pixels[k + c] = sum[c] * scale + m_pixels[k + c] * (1 - alpha);
                }
            }
        }
    }
}
'@





# Rounds half away from zero; [Math]::Round rounds half to even.
function Get-Rounded
{
    param([double] $value)

    return [int] [Math]::Floor($value + 0.5)
}





# Nearest even integer, so a shape of that width centered on a pixel
# boundary starts and ends on pixel boundaries.
function Get-EvenRounded
{
    param([double] $value)

    return 2 * (Get-Rounded ($value / 2))
}





# One color per pixel row for six stripes over that many rows. Under six
# rows, bandPicks gives the stripes that get one. Otherwise stripe b covers
# rows round(b * rows / 6) up to round((b + 1) * rows / 6), so every row is
# one stripe.
function Get-RowStripes
{
    param(
        [int]     $rows,
        [int[][]] $stripes
    )

    $result = New-Object 'int[][]' $rows
    $count  = $stripes.Count

    if ($bandPicks.ContainsKey($rows))
    {
        for ($r = 0; $r -lt $rows; $r++)
        {
            $result[$r] = $stripes[$bandPicks[$rows][$r]]
        }

        return ,$result
    }

    for ($b = 0; $b -lt $count; $b++)
    {
        $from = Get-Rounded ($b * $rows / $count)
        $to   = Get-Rounded (($b + 1) * $rows / $count)

        for ($r = $from; $r -lt $to; $r++)
        {
            $result[$r] = $stripes[$b]
        }
    }

    return ,$result
}





# Reads an image file as straight-alpha BGRA bytes.
function Read-Bgra
{
    param([string] $path)

    if (-not (Test-Path $path))
    {
        throw "Image not found: $path"
    }

    $bitmap = New-Object System.Drawing.Bitmap($path)
    try
    {
        $rect = New-Object System.Drawing.Rectangle(0, 0, $bitmap.Width, $bitmap.Height)
        $data = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                                 [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try
        {
            $bytes = New-Object byte[] ($bitmap.Width * $bitmap.Height * 4)
            [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
        }
        finally
        {
            $bitmap.UnlockBits($data)
        }

        return [pscustomobject] @{ Width = $bitmap.Width; Height = $bitmap.Height; Bytes = $bytes }
    }
    finally
    {
        $bitmap.Dispose()
    }
}





# Encodes straight-alpha BGRA bytes as a PNG.
function ConvertTo-Png
{
    param(
        [byte[]] $bgra,
        [int]    $size
    )

    $bitmap = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try
    {
        $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
        $data = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
                                 [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try
        {
            [System.Runtime.InteropServices.Marshal]::Copy($bgra, 0, $data.Scan0, $bgra.Length)
        }
        finally
        {
            $bitmap.UnlockBits($data)
        }

        $stream = New-Object System.IO.MemoryStream
        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        return ,$stream.ToArray()
    }
    finally
    {
        $bitmap.Dispose()
    }
}





# Pixel geometry for one size. Above SnapUpTo it is the design fractions
# times the size. Up to SnapUpTo everything lands on the pixel grid and is
# stacked from the top: the sticker row, one row of jacket, the hub cutout
# centered on the icon, the slot gap, then the slot down to SlotEnd or its
# margin above the border.
function Get-Layout
{
    param([int] $size)

    $d    = $design
    $snap = $size -le $d.SnapUpTo

    $inset  = Get-Rounded ($size * $d.JacketInset)
    $border = [Math]::Max(1, (Get-Rounded ($size * $d.BorderWidth)))
    $edge   = $inset + $border

    $rowTop   = [Math]::Max($edge + 1, (Get-Rounded ($size * $d.RowTop)))
    $rowLeft  = [Math]::Max($edge + 1, (Get-Rounded ($size * $d.RowInset)))
    $rowRight = $size - $rowLeft
    $rowWidth = $rowRight - $rowLeft

    $hubX    = $size / 2.0
    $slotW   = [double] [Math]::Max(2, (Get-EvenRounded ($size * $d.SlotWidth)))
    $margin  = if ($size -ge $d.SnapUpTo) { $d.SlotMarginBig } else { $d.SlotMargin }
    $slotMax = $size - $edge - $margin

    if ($snap)
    {
        # The sticker row takes every pixel row above the cutout but one.
        $cutout    = [double] (Get-EvenRounded ($size * $d.HubCutout))
        $hubY      = [double] (Get-Rounded ($size * $d.HubCenterY))
        $rowBottom = [int] ($hubY - $cutout / 2 - 1)
        $ring      = [Math]::Max(1, (Get-Rounded ($size * $d.HubCutout * (1 - $d.SpindleHole) / 2)))
        $spindle   = [double] ($cutout - 2 * $ring)

        $indexD    = [double] [Math]::Max(1, (Get-Rounded ($size * $d.IndexHole)))
        $indexLeft = [double] (Get-Rounded ($size * $d.IndexHoleX - $indexD / 2))
        $indexTop  = [double] (Get-Rounded ($hubY + $size * $d.IndexHoleDrop - $indexD / 2))

        $slotTop    = $hubY + $cutout / 2 + [Math]::Max(1, (Get-Rounded ($size * $d.SlotGap)))
        $slotBottom = [double] [Math]::Min($slotMax, (Get-Rounded ($size * $d.SlotEnd)))
    }
    else
    {
        $rowBottom = Get-Rounded ($size * $d.RowBottom)
        $cutout    = $size * $d.HubCutout
        $spindle   = $cutout * $d.SpindleHole
        $hubY      = $size * $d.HubCenterY

        $indexD    = $size * $d.IndexHole
        $indexLeft = $size * $d.IndexHoleX - $indexD / 2
        $indexTop  = $hubY + $size * $d.IndexHoleDrop - $indexD / 2

        $slotTop    = $hubY + $cutout / 2 + $size * $d.SlotGap
        $slotBottom = [Math]::Min([double] $slotMax, $size * $d.SlotEnd)
    }

    # Label lines: a whole number of pixels apart, the group centered on the
    # label (an odd leftover pixel goes above it).
    $labelLeft = $rowLeft + (Get-Rounded ($rowWidth * $d.LabelStart))
    $labelH    = $rowBottom - $rowTop
    $lineW     = $d.LineWidth
    $lineInset = Get-Rounded (($rowRight - $labelLeft) * $d.LineInset)
    $lines     = @()

    foreach ($set in $lineCounts)
    {
        if ($labelH -ge $set.MinHeight)
        {
            $pitch = [Math]::Max($d.LinePitch, (Get-Rounded ($labelH / ($set.Count + 1))))
            $group = ($set.Count - 1) * $pitch + $lineW
            $first = $rowTop + (Get-Rounded (($labelH - $group) / 2.0))

            for ($i = 0; $i -lt $set.Count; $i++)
            {
                $lines += $first + $i * $pitch
            }

            break
        }
    }

    $brandH   = $rowBottom - $rowTop
    $bandMin  = if ($size -ge $d.BandWideFrom) { 2 } else { 1 }
    $pixelArt = $size -le $d.PixelArtUpTo

    return [pscustomobject] @{
        Size       = $size
        Snap       = $snap
        PixelArt   = $pixelArt
        CutCorners = $pixelArt -and $size -ge $d.CornerPixels
        Inset      = $inset
        Border     = $border
        RowTop     = $rowTop
        RowBottom  = $rowBottom
        RowLeft    = $rowLeft
        RowRight   = $rowRight
        BrandRight = $rowLeft + (Get-Rounded ($rowWidth * $d.BrandWidth))
        BandRight  = $rowLeft + [Math]::Max($bandMin, (Get-Rounded ($brandH * $d.BandWidth)))
        Radius     = $brandH * $d.StickerRadius
        LabelLeft  = $labelLeft
        LineLeft   = $labelLeft + $lineInset
        LineRight  = $rowRight - $lineInset
        LineWidth  = $lineW
        Lines      = $lines
        HubX       = $hubX
        HubY       = $hubY
        Cutout     = $cutout
        Spindle    = $spindle
        IndexLeft  = $indexLeft
        IndexTop   = $indexTop
        IndexD     = $indexD
        SlotLeft   = $hubX - $slotW / 2
        SlotRight  = $hubX + $slotW / 2
        SlotTop    = $slotTop
        SlotBottom = $slotBottom
    }
}





# In pixel art the stickers' corner radius is under a pixel, so the binary
# threshold keeps their corner pixels; from CornerPixels up this drops them
# to the jacket, so the stickers still read as rounded.
function Clear-StickerCorners
{
    param(
        [object] $raster,
        [int]    $left,
        [int]    $top,
        [int]    $right,
        [int]    $bottom
    )

    foreach ($x in @($left, ($right - 1)))
    {
        foreach ($y in @($top, ($bottom - 1)))
        {
            $raster.FillRect($x, $y, $x + 1, $y + 1, [int[]] $colors.Jacket)
        }
    }
}





# Draws the brand sticker: Casso Explorer's folder body, with the stripe
# band, the band's shadow and the black silhouette in its middle.
function Add-BrandSticker
{
    param(
        [object] $raster,
        [object] $l,
        [object] $mask
    )

    $stickerH = $l.RowBottom - $l.RowTop
    $bandRows = Get-RowStripes $stickerH ([int[][]] $appleStripes)
    $shadow   = 0.0

    if ($l.Size -ge $design.ShadowFrom)
    {
        $shadow = if ($l.PixelArt) { $design.ShadowDepthPx } else { $design.ShadowDepth }
    }

    $raster.FillSticker($l.RowLeft, $l.RowTop, $l.BrandRight, $l.RowBottom, $l.Radius, $l.BandRight,
                        $bandRows, [int[]] $colors.Manila, [int[]] $colors.Manila2,
                        $shadow, [Math]::Max($design.ShadowMin, $stickerH * $design.ShadowWidth))

    if ($l.CutCorners)
    {
        Clear-StickerCorners $raster $l.RowLeft $l.RowTop $l.BrandRight $l.RowBottom
    }

    # The silhouette, its bounding box placed as on Casso Explorer's icon.
    $centerX = $l.BandRight + ($l.BrandRight - $l.BandRight) * $design.BirdCenterX
    $centerY = $l.RowTop + $stickerH * ($design.BirdTop + $design.BirdHeight / 2)

    if ($birdPixels.ContainsKey($l.Size))
    {
        $rows = $birdPixels[$l.Size]
        $left = Get-Rounded ($centerX - $rows[0].Length / 2.0)

        if ($birdNudge.ContainsKey($l.Size))
        {
            $left += $birdNudge[$l.Size]
        }

        $top  = Get-Rounded ($centerY - $rows.Count / 2.0)
        $top  = [Math]::Max($l.RowTop + 1, [Math]::Min($l.RowBottom - 1 - $rows.Count, $top))

        for ($r = 0; $r -lt $rows.Count; $r++)
        {
            for ($c = 0; $c -lt $rows[$r].Length; $c++)
            {
                if ($rows[$r][$c] -eq '#')
                {
                    $raster.FillRect($left + $c, $top + $r, $left + $c + 1, $top + $r + 1, [int[]] $colors.Bird)
                }
            }
        }
    }
    else
    {
        # The feet are a flat edge: it lands on a pixel boundary, so no
        # half-tone row runs under them.
        $birdH  = $stickerH * $design.BirdHeight
        $birdW  = $birdH * ($mask.Right - $mask.Left) / ($mask.Bottom - $mask.Top)
        $bottom = [double] (Get-Rounded ($centerY + $birdH / 2))
        $gain   = if ($l.Size -le $design.BirdGainUpTo) { $design.BirdGain } else { 1.0 }

        $raster.FillMask($mask, $centerX - $birdW / 2, $bottom - $birdH, $centerX + $birdW / 2, $bottom,
                         $gain, [int[]] $colors.Bird)
    }
}





# Draws one size and returns it as PNG bytes.
function New-DiskIcon
{
    param(
        [int]    $size,
        [object] $mask
    )

    $l       = Get-Layout $size
    $samples = if ($size -le 64) { 16 } else { 8 }
    $raster  = New-Object DiskIconRaster($size, $samples)
    $far     = $size - $l.Inset

    $raster.Binary = $l.PixelArt

    # Jacket: the border color first, then the jacket inside it.
    $raster.FillRect($l.Inset, $l.Inset, $far, $far, [int[]] $colors.Border)
    $raster.FillRect($l.Inset + $l.Border, $l.Inset + $l.Border, $far - $l.Border, $far - $l.Border, [int[]] $colors.Jacket)

    Add-BrandSticker $raster $l $mask

    # Label, with lines to write on. Its corners are rounded as the brand
    # sticker's are.
    $raster.FillRoundRect($l.LabelLeft, $l.RowTop, $l.RowRight, $l.RowBottom, $l.Radius, [int[]] $colors.Label)

    if ($l.CutCorners)
    {
        Clear-StickerCorners $raster $l.LabelLeft $l.RowTop $l.RowRight $l.RowBottom
    }

    foreach ($y in $l.Lines)
    {
        $raster.FillRect($l.LineLeft, $y, $l.LineRight, $y + $l.LineWidth, [int[]] $colors.Line)
    }

    # Hub: the media shows through the jacket's cutout, around the spindle
    # hole. A circle 4 px across has no binary form that reads as round, so
    # that small the hub is drawn a pixel at a time: the media on the
    # cutout's edges, half blended into the jacket at its corners, around a
    # sharp hole.
    $c0 = $l.HubX - $l.Cutout / 2
    $c1 = $l.HubX + $l.Cutout / 2
    $s0 = $l.HubX - $l.Spindle / 2
    $s1 = $l.HubX + $l.Spindle / 2
    $v0 = $l.HubY - $l.Cutout / 2
    $v1 = $l.HubY + $l.Cutout / 2
    $w0 = $l.HubY - $l.Spindle / 2
    $w1 = $l.HubY + $l.Spindle / 2

    if ($l.Snap -and $l.Cutout -le 4)
    {
        $corner = [int[]] @(0..2 | ForEach-Object { Get-Rounded (($colors.Media[$_] + $colors.Jacket[$_]) / 2.0) })

        $raster.FillRect($c0, $v0, $c1, $v1, $corner)
        $raster.FillRect($s0, $v0, $s1, $v1, [int[]] $colors.Media)
        $raster.FillRect($c0, $w0, $c1, $w1, [int[]] $colors.Media)
        $raster.FillRect($s0, $w0, $s1, $w1, [int[]] $colors.Spindle)
    }
    else
    {
        # Up to HubBinaryUpTo the hub is binary too, so its thin ring has
        # no gray halo against the jacket next to the pixel-art stickers.
        $raster.Binary = $l.PixelArt -or $l.Size -le $design.HubBinaryUpTo

        $raster.FillEllipse($c0, $v0, $c1, $v1, [int[]] $colors.Media)
        $raster.FillEllipse($s0, $w0, $s1, $w1, [int[]] $colors.Spindle)

        $raster.Binary = $l.PixelArt
    }

    # Index hole. Two pixels or less is a square; a circle that small only blurs.
    $indexRight  = $l.IndexLeft + $l.IndexD
    $indexBottom = $l.IndexTop + $l.IndexD

    if ($l.IndexD -le 2)
    {
        $raster.FillRect($l.IndexLeft, $l.IndexTop, $indexRight, $indexBottom, [int[]] $colors.Media)
    }
    else
    {
        $raster.FillEllipse($l.IndexLeft, $l.IndexTop, $indexRight, $indexBottom, [int[]] $colors.Media)
    }

    # Head access slot.
    $raster.FillCapsule($l.SlotLeft, $l.SlotTop, $l.SlotRight, $l.SlotBottom, [int[]] $colors.Media)

    return ,(ConvertTo-Png $raster.ToBgra() $size)
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





# Preview: every size at 1:1 on a dark and a light background, then the
# sizes up to 48 px at 4x with nearest-neighbor scaling.
function Write-Sheet
{
    param(
        [int[]]  $sizes,
        [object] $pngs,
        [string] $path
    )

    $gap     = 16
    $zoom    = 4
    $small   = @($sizes | Where-Object { $_ -le 48 })
    $fullW   = ($sizes | Measure-Object -Sum).Sum + $gap * ($sizes.Count + 1)
    $zoomW   = ($small | Measure-Object -Sum).Sum * $zoom + $gap * ($small.Count + 1)
    $maxSize = ($sizes | Measure-Object -Maximum).Maximum
    $zoomMax = ($small | Measure-Object -Maximum).Maximum * $zoom
    $fullH   = $maxSize + 2 * $gap
    $zoomH   = $zoomMax + 2 * $gap
    $bandH   = @($fullH, $fullH, $zoomH, $zoomH)
    $width   = [Math]::Max($fullW, $zoomW)
    $height  = ($bandH | Measure-Object -Sum).Sum
    $dark    = [System.Drawing.Color]::FromArgb(255, 0x1C, 0x1C, 0x1C)
    $light   = [System.Drawing.Color]::FromArgb(255, 0xF3, 0xF3, 0xF3)
    $images  = @()
    $streams = @()

    foreach ($d in $pngs)
    {
        $ms       = New-Object System.IO.MemoryStream(, $d)
        $streams += $ms
        $images  += New-Object System.Drawing.Bitmap($ms)
    }

    $sheet    = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($sheet)
    try
    {
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $graphics.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::Half

        $top = 0

        for ($band = 0; $band -lt 4; $band++)
        {
            $background = if ($band % 2 -eq 0) { $dark } else { $light }
            $brush      = New-Object System.Drawing.SolidBrush($background)
            $graphics.FillRectangle($brush, 0, $top, $width, $bandH[$band])
            $brush.Dispose()

            $scale = if ($band -lt 2) { 1 } else { $zoom }
            $x     = $gap

            for ($i = 0; $i -lt $sizes.Count; $i++)
            {
                if ($band -ge 2 -and $sizes[$i] -gt 48)
                {
                    continue
                }

                $side = $sizes[$i] * $scale
                $rect = New-Object System.Drawing.Rectangle($x, ($top + $gap), $side, $side)
                $graphics.DrawImage($images[$i], $rect)
                $x += $side + $gap
            }

            $top += $bandH[$band]
        }

        $sheet.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $graphics.Dispose()
        $sheet.Dispose()

        foreach ($image in $images)
        {
            $image.Dispose()
        }

        foreach ($ms in $streams)
        {
            $ms.Dispose()
        }
    }
}





# The cream silhouette of the dark-background logo, as coverage. It is the
# same shape, cropped the same way, as the black one on Casso Explorer's icon.
$logo = Read-Bgra (Join-Path $iconDir 'Casso-0-silhouette.png')
$mask = New-Object DiskIconMask($logo.Bytes, $logo.Width, $logo.Height, 20.0, 230.0)

if ($mask.Right -le $mask.Left -or $mask.Bottom -le $mask.Top)
{
    throw "No silhouette found in Casso-0-silhouette.png"
}

$pngs = New-Object System.Collections.Generic.List[byte[]]

foreach ($size in $sizes)
{
    $pngs.Add((New-DiskIcon $size $mask))
}

Write-Ico $sizes $pngs $Out
Write-Host "Wrote $Out ($($sizes.Count) sizes)"

if (-not [string]::IsNullOrEmpty($Sheet))
{
    Write-Sheet $sizes $pngs $Sheet
    Write-Host "Wrote $Sheet"
}
