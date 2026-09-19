<#
.SYNOPSIS
    Regenerates Installer/Assets -- the PNG logo set an MSIX package needs --
    from the application icon in Resources/Icons.

.DESCRIPTION
    An MSIX manifest names five logos, and Windows picks among a scale
    variant of each for every display it draws on. A package that ships only
    the scale-100 of each looks soft on a 150% laptop and badly so on a 200%
    tablet, because the shell upscales rather than falling back to the
    largest available. So the set is the whole grid: five logos times five
    scales, plus the target-size variants the taskbar and the Start list ask
    for by pixel count rather than by scale.

    THE OUTPUT IS CHECKED IN, and this script is how it is refreshed, not a
    build step. Three reasons. The release job runs on a hosted image whose
    imaging stack is not ours to depend on; a logo set that changes only when
    the icon changes has no business being recomputed on every push; and a
    reviewer can see what ships. Run this after editing the source icon and
    commit what it writes.

    The source is the icon Explorer already shows for Casso.exe -- the lowest
    numbered ICON in Casso/Casso.rc -- taken from its 1024x1024 PNG original
    rather than from the .ico, which tops out at 256. That icon is a complete
    badge, a cassowary on a dark rounded square, not a bare glyph on nothing.
    That distinction sets the two rules below.

    FILLED, NOT PADDED, at 44x44 and at every target size. Those assets are
    the app list, the taskbar and Alt+Tab, where the badge IS the icon and
    Windows adds any plate it wants around it. Padding here would render
    Casso a size smaller than every neighboring icon.

    PADDED ON THE TILES. A 150x150 Start tile drawn edge to edge reads as a
    cropped photograph rather than as an icon, so the badge takes two thirds
    of the short edge and sits centered on transparency. Transparent, not
    filled, because the badge carries its own dark plate: a background color
    behind it would draw a second square around the first.

.PARAMETER Source
    The PNG to generate from. Default: the Casso-0-silhouette.png that
    Casso.rc embeds as IDI_CASSO_SILHOUETTE.

.PARAMETER Destination
    Where to write the set. Default: Installer/Assets.

.OUTPUTS
    One line per file written. Exit code 0 on success.
#>
param(
    [string]$Source      = "",
    [string]$Destination = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

if ([string]::IsNullOrEmpty($Source))
{
    $Source = Join-Path $repoRoot 'Resources/Icons/Casso-0-silhouette.png'
}

if ([string]::IsNullOrEmpty($Destination))
{
    $Destination = Join-Path $repoRoot 'Installer/Assets'
}

if (-not (Test-Path $Source))
{
    throw "Source icon not found: $Source"
}

Add-Type -AssemblyName System.Drawing

# The scales Windows ships assets for. A display at 175% asks for 200% and
# gets it; one at 350% asks for 400%. There is no scale between these.
$scales = @(100, 125, 150, 200, 400)

# The pixel sizes the shell requests by count rather than by scale: the
# taskbar, the Start list, Alt+Tab, and Explorer's largest view. Each also
# gets an _altform-unplated twin, which is what Windows draws where it does
# NOT paint its own colored square behind the icon -- the taskbar being the
# one everybody sees. Same bitmap; the suffix is the whole difference.
$targetSizes = @(16, 24, 32, 48, 256)

# logo name -> base size at 100%, and how much of the short edge the badge
# takes. See the two rules in .DESCRIPTION for why these differ.
$logos = @(
    @{ Name = 'Square44x44Logo';   Width =  44; Height =  44; Fill = 1.00 },
    @{ Name = 'StoreLogo';         Width =  50; Height =  50; Fill = 1.00 },
    @{ Name = 'Square71x71Logo';   Width =  71; Height =  71; Fill = 0.66 },
    @{ Name = 'Square150x150Logo'; Width = 150; Height = 150; Fill = 0.66 },
    @{ Name = 'Square310x310Logo'; Width = 310; Height = 310; Fill = 0.66 },
    @{ Name = 'Wide310x150Logo';   Width = 310; Height = 150; Fill = 0.66 }
)

# Draws the source badge centered on a transparent canvas of the requested
# size, at $fill of the canvas's SHORT edge. Short edge, so the wide tile
# keeps the badge square instead of stretching it to the tile's aspect.
function Write-Logo
{
    param(
        [System.Drawing.Image] $image,
        [int]                  $width,
        [int]                  $height,
        [double]               $fill,
        [string]               $path
    )

    $side = [int] [Math]::Round([Math]::Min($width, $height) * $fill)
    $x    = [int] [Math]::Round(($width  - $side) / 2.0)
    $y    = [int] [Math]::Round(($height - $side) / 2.0)

    $bitmap   = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try
    {
        $graphics.Clear([System.Drawing.Color]::Transparent)

        # HighQualityBicubic plus a Half pixel offset: without the offset the
        # resampler reads from pixel corners rather than centers, which shifts
        # the badge half a source pixel up and left and frays the rounded
        # corners at the small sizes where it shows most.
        $graphics.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality

        # The destination rectangle alone is not enough. GDI+ samples beyond
        # the source rectangle's edge by default and picks up the adjacent
        # texels, which on a transparent-bordered badge means a pale halo. A
        # wrap mode of TileFlipXY makes the edge mirror itself instead.
        $attributes = New-Object System.Drawing.Imaging.ImageAttributes
        try
        {
            $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
            $destination = New-Object System.Drawing.Rectangle($x, $y, $side, $side)
            $graphics.DrawImage($image, $destination, 0, 0, $image.Width, $image.Height,
                                [System.Drawing.GraphicsUnit]::Pixel, $attributes)
        }
        finally
        {
            $attributes.Dispose()
        }

        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

New-Item -ItemType Directory -Path $Destination -Force | Out-Null

# Start from empty. A logo renamed or a scale dropped would otherwise leave
# its old file behind, and MakeAppx packs whatever is in the folder.
Get-ChildItem $Destination -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force

$badge = [System.Drawing.Image]::FromFile((Resolve-Path $Source).Path)
$count = 0

try
{
    Write-Host "Source: $Source ($($badge.Width)x$($badge.Height))"

    foreach ($logo in $logos)
    {
        foreach ($scale in $scales)
        {
            # Ceiling, not rounding: 71 at 125% is 88.75, and the name
            # Windows looks for is Square71x71Logo.scale-125 at 89 pixels.
            $width  = [int] [Math]::Ceiling($logo.Width  * $scale / 100.0)
            $height = [int] [Math]::Ceiling($logo.Height * $scale / 100.0)
            $path   = Join-Path $Destination "$($logo.Name).scale-$scale.png"

            Write-Logo $badge $width $height $logo.Fill $path
            Write-Host ("  {0,-46} {1}x{2}" -f (Split-Path $path -Leaf), $width, $height)
            $count++
        }
    }

    foreach ($size in $targetSizes)
    {
        foreach ($suffix in @('', '_altform-unplated'))
        {
            $path = Join-Path $Destination "Square44x44Logo.targetsize-$size$suffix.png"

            Write-Logo $badge $size $size 1.00 $path
            Write-Host ("  {0,-46} {1}x{1}" -f (Split-Path $path -Leaf), $size)
            $count++
        }
    }
}
finally
{
    $badge.Dispose()
}

Write-Host ""
Write-Host "$count assets written to $Destination"
