<#
.SYNOPSIS
    Regenerates Installer/Assets -- the PNG logo set an MSIX package needs --
    from the application icon in Resources/Icons.

.DESCRIPTION
    Six logos at five scales each, plus the target sizes the shell requests
    by pixel count. Windows upscales rather than falling back, so a package
    with only scale-100 looks soft above 100%.

    THE OUTPUT IS CHECKED IN. Run this after editing the source icon and
    commit what it writes; CI does no image work.

    The source is the icon Explorer shows for Casso.exe, from its 1024x1024
    PNG rather than the .ico, which stops at 256. It is a finished badge, so
    it fills every asset edge to edge; padding it inside a second frame only
    draws Casso smaller than its neighbors.

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
    [string]$Destination = "",

    # Put in front of every file name written, so a second application's
    # tiles sit beside Casso's in the same folder without colliding. The
    # manifest asks for them by the same names with the same prefix.
    [string]$Prefix      = ""
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

# The only scales Windows requests; 175% takes the 200% asset.
$scales = @(100, 125, 150, 200, 400)

# Requested by pixel count, not scale. The _altform-unplated twin is what
# Windows 11 draws in Start, search and the taskbar; same bitmap either way.
$targetSizes = @(16, 24, 32, 48, 256)

# Base size at 100%. Fill is the fraction of the SHORT edge, which keeps the
# badge square on the 310x150 wide tile.
$logos = @(
    @{ Name = 'Square44x44Logo';   Width =  44; Height =  44; Fill = 1.00 },
    @{ Name = 'StoreLogo';         Width =  50; Height =  50; Fill = 1.00 },
    @{ Name = 'Square71x71Logo';   Width =  71; Height =  71; Fill = 1.00 },
    @{ Name = 'Square150x150Logo'; Width = 150; Height = 150; Fill = 1.00 },
    @{ Name = 'Square310x310Logo'; Width = 310; Height = 310; Fill = 1.00 },
    @{ Name = 'Wide310x150Logo';   Width = 310; Height = 150; Fill = 1.00 }
)

# Draws the badge centered on a transparent canvas of the requested size.
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

        # Half pixel offset: without it the badge shifts half a source pixel
        # up and left and the rounded corners fray at small sizes.
        $graphics.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality

        # TileFlipXY stops GDI+ sampling past the edge, which shows as a
        # pale halo around the badge.
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

# Start from empty; MakeAppx packs whatever is in the folder. Only this
# run's own names go, since the folder also has the other application's
# tiles: an empty prefix must not reach a prefixed name, so the match is
# anchored at the start.
foreach ($logo in $logos)
{
    Get-ChildItem $Destination -Filter "$Prefix$($logo.Name).*.png" -ErrorAction SilentlyContinue |
        Remove-Item -Force
}

$badge = [System.Drawing.Image]::FromFile((Resolve-Path $Source).Path)
$count = 0

try
{
    Write-Host "Source: $Source ($($badge.Width)x$($badge.Height))"

    foreach ($logo in $logos)
    {
        foreach ($scale in $scales)
        {
            # Ceiling, not rounding: 71 at 125% is 88.75, and the asset is 89.
            $width  = [int] [Math]::Ceiling($logo.Width  * $scale / 100.0)
            $height = [int] [Math]::Ceiling($logo.Height * $scale / 100.0)
            $path   = Join-Path $Destination "$Prefix$($logo.Name).scale-$scale.png"

            Write-Logo $badge $width $height $logo.Fill $path
            Write-Host ("  {0,-46} {1}x{2}" -f (Split-Path $path -Leaf), $width, $height)
            $count++
        }
    }

    foreach ($size in $targetSizes)
    {
        foreach ($suffix in @('', '_altform-unplated'))
        {
            $path = Join-Path $Destination "${Prefix}Square44x44Logo.targetsize-$size$suffix.png"

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
