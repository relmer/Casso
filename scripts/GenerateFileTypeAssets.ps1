<#
.SYNOPSIS
    Writes the disk image file type's logos for the MSIX package, from the
    icon Casso.rc embeds as IDI_DISK_IMAGE.

.DESCRIPTION
    File Explorer draws a packaged file type's icon from the logo the
    manifest's uap:FileTypeAssociation gives it, requested by pixel count.
    Each target size is the icon's own frame at that size, copied as is,
    so a size drawn by hand stays the way it was drawn. The scale variants
    are resized from the smallest frame at least that large.

    THE OUTPUT IS CHECKED IN. Run this after editing the icon and commit
    what it writes.

.PARAMETER Source
    The .ico to read. Default: Resources/Icons/Casso-disk-image.ico.

.PARAMETER Destination
    Where to write the logos. Default: Installer/Assets.

.OUTPUTS
    One line per file written. Exit code 0 on success.
#>
param(
    [string]$Source      = "",
    [string]$Destination = ""
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$name     = 'DiskImageLogo'

if ([string]::IsNullOrEmpty($Source))
{
    $Source = Join-Path $repoRoot 'Resources/Icons/Casso-disk-image.ico'
}

if ([string]::IsNullOrEmpty($Destination))
{
    $Destination = Join-Path $repoRoot 'Installer/Assets'
}

Add-Type -AssemblyName System.Drawing

# The sizes File Explorer requests by pixel count, and the scales of the
# 44-pixel base it requests otherwise.
$targetSizes = @(16, 24, 32, 48, 256)
$scales      = @(100, 125, 150, 200, 400)

# The icon's frames, by width: an .ico of PNG frames, each one's bytes a
# complete PNG file.
$bytes  = [IO.File]::ReadAllBytes((Resolve-Path $Source).Path)
$count  = [BitConverter]::ToUInt16($bytes, 4)
$frames = @{}

for ($i = 0; $i -lt $count; $i++)
{
    $entry  = 6 + 16 * $i
    $width  = if ($bytes[$entry] -eq 0) { 256 } else { [int] $bytes[$entry] }
    $length = [BitConverter]::ToUInt32($bytes, $entry + 8)
    $offset = [BitConverter]::ToUInt32($bytes, $entry + 12)

    if ($bytes[$offset] -ne 0x89)
    {
        throw "The $width-pixel frame is not a PNG; this script reads PNG frames only."
    }

    $frames[$width] = $bytes[$offset..($offset + $length - 1)]
}

New-Item -ItemType Directory -Path $Destination -Force | Out-Null
Get-ChildItem $Destination -Filter "$name.*.png" -ErrorAction SilentlyContinue | Remove-Item -Force

$written = 0

foreach ($size in $targetSizes)
{
    if (-not $frames.ContainsKey($size))
    {
        throw "The icon has no $size-pixel frame."
    }

    foreach ($suffix in @('', '_altform-unplated'))
    {
        $path = Join-Path $Destination "$name.targetsize-$size$suffix.png"

        [IO.File]::WriteAllBytes($path, [byte[]] $frames[$size])
        Write-Host ("  {0,-46} {1}x{1}" -f (Split-Path $path -Leaf), $size)
        $written++
    }
}

foreach ($scale in $scales)
{
    $size   = [int] [Math]::Ceiling(44 * $scale / 100.0)
    $from   = ($frames.Keys | Where-Object { $_ -ge $size } | Sort-Object | Select-Object -First 1)
    $path   = Join-Path $Destination "$name.scale-$scale.png"
    $stream = New-Object IO.MemoryStream (, [byte[]] $frames[$from])
    $image  = [System.Drawing.Image]::FromStream($stream)
    $bitmap = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $canvas = [System.Drawing.Graphics]::FromImage($bitmap)

    try
    {
        $canvas.Clear([System.Drawing.Color]::Transparent)
        $canvas.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $canvas.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $canvas.DrawImage($image, 0, 0, $size, $size)
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $canvas.Dispose()
        $bitmap.Dispose()
        $image.Dispose()
        $stream.Dispose()
    }

    Write-Host ("  {0,-46} {1}x{1} from {2}" -f (Split-Path $path -Leaf), $size, $from)
    $written++
}

Write-Host ""
Write-Host "$written assets written to $Destination"
