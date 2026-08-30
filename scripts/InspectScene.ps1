<#
.SYNOPSIS
    Poses the 3D desk scene at a chosen angle and captures it, for judging
    model changes against the render rather than against the CAD source.

.DESCRIPTION
    Model work on the desk scene is verified by looking at it: a generator
    change is only correct if the thing on screen is right, and the angle
    that shows a defect is usually not the one Casso opens at. Doing that by
    hand means launching, rotating, capturing and cropping every time, which
    is slow enough that it tempts you into skipping it -- and a model defect
    that is not looked at from the reporting angle is a defect that survives
    the fix.

    The scene is rotated by SYNTHESIZED SHIFT+WHEEL messages rather than by
    dragging. A posted drag needs the real cursor parked on the scene (the
    capture that follows a SetCapture drag sees whatever the pointer is
    actually over), while a wheel message carries its own position and
    lands wherever it is aimed. Shift+wheel is the scene's orbit gesture, so
    this drives the same path a user does.

    Horizontal notches yaw the scene, vertical notches pitch it. Roughly 52
    horizontal notches at delta 120 brings the rear into view from the
    default pose.

.PARAMETER ProcId
    Attach to an already-running Casso. Omit to launch one.

.PARAMETER Machine
    Machine to launch with when not attaching. Default: Apple2e

.PARAMETER Yaw
    Horizontal wheel notches. Positive turns the scene's left side toward
    the viewer; about 52 reaches the rear.

.PARAMETER Pitch
    Vertical wheel notches. Positive tips the top away.

.PARAMETER Out
    PNG path for the capture.

.PARAMETER Crop
    Optional "left,top,right,bottom" in captured pixels, written beside Out
    with a "-crop" suffix. Needs Python with Pillow.

.PARAMETER Scale
    Scale applied to the crop. Above 1 to inspect a seam; default 1.

.PARAMETER KeepOpen
    Leave a launched Casso running. By default a launched one is closed
    again, so a verification run does not leave processes behind.

.EXAMPLE
    scripts\InspectScene.ps1 -Yaw 52 -Out out\rear.png
    Launches, turns the scene to the rear, captures, closes.

.EXAMPLE
    scripts\InspectScene.ps1 -ProcId 1234 -Yaw 52 -Out out\rear.png `
        -Crop "800,600,2200,1300" -Scale 2.5
    Poses a running instance and writes both the capture and a zoomed crop.
#>

[CmdletBinding()]
param(
    [int]     $ProcId   = 0,
    [string]  $Machine  = 'Apple2e',
    [int]     $Yaw      = 0,
    [int]     $Pitch    = 0,
    [Parameter(Mandatory = $true)][string]  $Out,
    [string]  $Crop     = '',
    [double]  $Scale    = 1.0,
    [switch]  $KeepOpen
)

$ErrorActionPreference = 'Stop'

$sig = @'
using System;
using System.Runtime.InteropServices;
public class SceneProbe {
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [StructLayout(LayoutKind.Sequential)] public struct RECT  { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
'@

if (-not ([System.Management.Automation.PSTypeName]'SceneProbe').Type)
{
    Add-Type -TypeDefinition $sig
}

Add-Type -AssemblyName System.Drawing
[SceneProbe]::SetProcessDPIAware() | Out-Null

$launched = $null

if ($ProcId -eq 0)
{
    $exe = Join-Path $PSScriptRoot '..\x64\Debug\Casso.exe' | Resolve-Path
    $launched = Start-Process $exe -ArgumentList '--machine', $Machine -PassThru
    $ProcId = $launched.Id

    # The window is not up the moment the process is: poll for a handle
    # rather than sleeping a guessed interval, which is what made every
    # hand-run of this sequence capture a zero-by-zero bitmap now and then.
    $deadline = (Get-Date).AddSeconds(60)

    do
    {
        Start-Sleep -Milliseconds 500
        $handle = (Get-Process -Id $ProcId).MainWindowHandle
    }
    while ($handle -eq 0 -and (Get-Date) -lt $deadline)

    if ($handle -eq 0) { throw "Casso window never appeared (pid $ProcId)." }

    # The scene needs a few frames before it is worth looking at.
    Start-Sleep -Seconds 6
}

$hwnd = (Get-Process -Id $ProcId).MainWindowHandle
if ($hwnd -eq 0) { throw "Process $ProcId has no window." }

#
#  Rotate. The wheel's lParam is in SCREEN coordinates, unlike the button
#  messages -- aiming it in client space turns the scene from a point
#  outside the window, which the handler declines.
#
function Send-Wheel([int] $count, [uint32] $msg)
{
    if ($count -eq 0) { return }

    [SceneProbe+RECT]$cr = New-Object SceneProbe+RECT
    [SceneProbe]::GetClientRect($hwnd, [ref]$cr) | Out-Null

    [SceneProbe+POINT]$pt = New-Object SceneProbe+POINT
    $pt.X = [int]($cr.R / 2)
    $pt.Y = [int]($cr.B / 2)
    [SceneProbe]::ClientToScreen($hwnd, [ref]$pt) | Out-Null

    $lp = [IntPtr](($pt.Y -shl 16) -bor ($pt.X -band 0xFFFF))
    $step = if ($count -ge 0) { 120 } else { -120 }
    $wp = [IntPtr]((($step -band 0xFFFF) -shl 16) -bor 0x0004)   # MK_SHIFT

    for ($i = 0; $i -lt [Math]::Abs($count); $i++)
    {
        [SceneProbe]::PostMessage($hwnd, $msg, $wp, $lp) | Out-Null
        Start-Sleep -Milliseconds 60
    }
}

Send-Wheel $Yaw   0x020E      # WM_MOUSEHWHEEL
Send-Wheel $Pitch 0x020A      # WM_MOUSEWHEEL

Start-Sleep -Milliseconds 900

#
#  Capture. PrintWindow with PW_RENDERFULLCONTENT (2), which is what gets a
#  composited D3D window; a plain BitBlt of the screen picks up whatever is
#  in front of it.
#
[SceneProbe+RECT]$wr = New-Object SceneProbe+RECT
[SceneProbe]::GetWindowRect($hwnd, [ref]$wr) | Out-Null

$width  = $wr.R - $wr.L
$height = $wr.B - $wr.T

if ($width -le 0 -or $height -le 0) { throw "Window rect is empty." }

$bmp = New-Object System.Drawing.Bitmap($width, $height)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $gfx.GetHdc()
[SceneProbe]::PrintWindow($hwnd, $hdc, 2) | Out-Null
$gfx.ReleaseHdc($hdc)
$gfx.Dispose()

$outDir = Split-Path -Parent $Out
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force $outDir | Out-Null }

$bmp.Save($Out)
$bmp.Dispose()

Write-Output "captured $width x $height -> $Out"

if ($Crop)
{
    $box = $Crop -split ','
    if ($box.Count -ne 4) { throw "Crop wants 'left,top,right,bottom'." }

    $cropOut = [IO.Path]::ChangeExtension($Out, $null).TrimEnd('.') + '-crop.png'
    $py = @"
from PIL import Image
im = Image.open(r'$Out').crop(($($box[0]), $($box[1]), $($box[2]), $($box[3])))
if $Scale != 1.0:
    im = im.resize((int(im.width * $Scale), int(im.height * $Scale)), Image.LANCZOS)
im.save(r'$cropOut')
print(f'crop {im.width} x {im.height} -> $cropOut')
"@
    $py | python -
}

if ($launched -and -not $KeepOpen)
{
    Stop-Process -Id $ProcId -Force -Confirm:$false -ErrorAction SilentlyContinue
}
