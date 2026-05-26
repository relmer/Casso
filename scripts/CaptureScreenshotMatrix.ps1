<#
.SYNOPSIS
    Captures a curated Casso UI screenshot matrix for visual regression review.

.DESCRIPTION
    Launches Casso, drives the UI with window commands and keyboard input, and
    writes PNG captures to scripts\out\screenshots\<timestamp> by default.

    Matrix states:
      1. Boot screen, color monitor
      2. Boot screen, green monitor
      3. Boot screen, amber monitor
      4. Boot screen, white monitor
      5. Settings panel, Machine tab
      6. Settings panel, Display tab with amber monitor active
      7. Settings panel, Theme tab
      8. Settings panel, Hardware tab
      9. Help keymap dialog fallback

    TODO: Replace the Help keymap fallback with the drive-bar drag-over overlay
    when Casso exposes a scriptable way to engage its OLE IDropTarget state.

.PARAMETER OutDir
    Destination directory for PNG captures.
    Default: scripts\out\screenshots\<yyyyMMdd-HHmmss>

.PARAMETER Configuration
    Build configuration used for the default CassoPath.
    Default: Debug

.PARAMETER Platform
    Build platform used for the default CassoPath.
    Default: x64

.PARAMETER DelaySecs
    Seconds to wait after each UI action before capturing.
    Default: 2

.PARAMETER CassoPath
    Path to Casso.exe. Relative paths are resolved from the repository root.
    Default: <Platform>\<Configuration>\Casso.exe
#>
[CmdletBinding()]
param(
    [string]$OutDir,

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [ValidateRange(0, 60)]
    [int]$DelaySecs = 2,

    [string]$CassoPath
)

$ErrorActionPreference = 'Stop'

$script:RepoRoot = Split-Path $PSScriptRoot -Parent

if (-not $OutDir) {
    $stamp  = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutDir = Join-Path $PSScriptRoot "out\screenshots\$stamp"
}

if (-not $CassoPath) {
    $CassoPath = Join-Path -Path $script:RepoRoot -ChildPath "$Platform\$Configuration\Casso.exe"
}

Add-Type -AssemblyName System.Windows.Forms

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class NativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, UIntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
}
'@ -ReferencedAssemblies System.Drawing

$script:Commands = @{
    ViewColor    = 40030
    ViewGreen    = 40031
    ViewAmber    = 40032
    ViewWhite    = 40033
    ViewSettings = 40039
    HelpKeymap   = 40040
}

$script:WM_COMMAND            = 0x0111
$script:SW_RESTORE            = 9
$script:PW_RENDERFULLCONTENT  = 0x00000002
$script:BlackSampleStep       = 23
$script:BlackBrightnessCutoff = 24
$script:BlackMinimumRatio     = 0.02

function Resolve-LocalPath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path -Path $script:RepoRoot -ChildPath $Path
}

function Wait-CassoWindow {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [System.Diagnostics.Process]$Process,

        [int]$TimeoutSecs = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSecs)

    while ((Get-Date) -lt $deadline) {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "Casso exited before creating a window. Exit code: $($Process.ExitCode)"
        }

        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return $Process.MainWindowHandle
        }

        Start-Sleep -Milliseconds 250
    }

    throw "Timed out waiting for Casso to create its main window. Tiny GUI, big attitude."
}

function Set-CassoForeground {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Hwnd
    )

    if ($Hwnd -eq [IntPtr]::Zero) {
        throw 'Cannot foreground a null HWND.'
    }

    $null = [NativeMethods]::ShowWindow($Hwnd, $script:SW_RESTORE)
    $null = [NativeMethods]::SetForegroundWindow($Hwnd)
    Start-Sleep -Milliseconds 250

    if (-not [NativeMethods]::IsWindowVisible($Hwnd)) {
        throw "Window is not visible: $Hwnd"
    }
}

function Invoke-CassoCommand {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Hwnd,

        [Parameter(Mandatory = $true)]
        [int]$CommandId
    )

    $posted = [NativeMethods]::PostMessage($Hwnd, $script:WM_COMMAND, [UIntPtr]$CommandId, [IntPtr]::Zero)
    if (-not $posted) {
        throw "PostMessage failed for command $CommandId"
    }
}

function Send-CassoKeys {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Keys
    )

    [System.Windows.Forms.SendKeys]::SendWait($Keys)
}

function Test-BitmapMostlyBlack {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [System.Drawing.Bitmap]$Bitmap
    )

    $samples  = 0
    $nonBlack = 0

    for ($y = 0; $y -lt $Bitmap.Height; $y += $script:BlackSampleStep) {
        for ($x = 0; $x -lt $Bitmap.Width; $x += $script:BlackSampleStep) {
            $pixel      = $Bitmap.GetPixel($x, $y)
            $brightness = [int]$pixel.R + [int]$pixel.G + [int]$pixel.B
            $samples++

            if ($brightness -gt $script:BlackBrightnessCutoff) {
                $nonBlack++
            }
        }
    }

    if ($samples -eq 0) {
        return $true
    }

    return (($nonBlack / $samples) -lt $script:BlackMinimumRatio)
}

function New-WindowBitmap {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Hwnd
    )

    $rect = New-Object NativeMethods+RECT
    $ok   = [NativeMethods]::GetWindowRect($Hwnd, [ref]$rect)
    if (-not $ok) {
        throw "GetWindowRect failed for HWND $Hwnd"
    }

    $width  = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "Window has an invalid capture size: ${width}x${height}"
    }

    $bitmap   = New-Object System.Drawing.Bitmap $width, $height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    try {
        $hdc     = $graphics.GetHdc()
        $printed = $false

        try {
            $printed = [NativeMethods]::PrintWindow($Hwnd, $hdc, $script:PW_RENDERFULLCONTENT)
        }
        finally {
            $graphics.ReleaseHdc($hdc)
        }
    }
    finally {
        $graphics.Dispose()
    }

    if ($printed -and -not (Test-BitmapMostlyBlack -Bitmap $bitmap)) {
        return $bitmap
    }

    $bitmap.Dispose()

    $bitmap   = New-Object System.Drawing.Bitmap $width, $height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    try {
        $size = New-Object System.Drawing.Size $width, $height
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $size, [System.Drawing.CopyPixelOperation]::SourceCopy)
    }
    finally {
        $graphics.Dispose()
    }

    return $bitmap
}

function Save-CassoScreenshot {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Hwnd,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $bitmap = New-WindowBitmap -Hwnd $Hwnd

    try {
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

function Invoke-ScreenshotState {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$State,

        [Parameter(Mandatory = $true)]
        [IntPtr]$MainHwnd,

        [Parameter(Mandatory = $true)]
        [string]$OutputDirectory,

        [Parameter(Mandatory = $true)]
        [int]$DelaySeconds
    )

    $captureHwnd = $MainHwnd
    $fileName    = '{0:00}_{1}.png' -f $State.Index, $State.Name
    $path        = Join-Path -Path $OutputDirectory -ChildPath $fileName

    Set-CassoForeground -Hwnd $MainHwnd
    & $State.Action $MainHwnd
    Start-Sleep -Seconds $DelaySeconds

    if ($State.Target -eq 'Foreground') {
        $captureHwnd = [NativeMethods]::GetForegroundWindow()
    }

    Set-CassoForeground -Hwnd $captureHwnd
    Save-CassoScreenshot -Hwnd $captureHwnd -Path $path
}

function New-ScreenshotMatrix {
    [CmdletBinding()]
    param()

    return @(
        [pscustomobject]@{ Index = 1; Name = 'boot_color';        Target = 'Main';       Action = { param($hwnd) Invoke-CassoCommand -Hwnd $hwnd -CommandId $script:Commands.ViewColor } }
        [pscustomobject]@{ Index = 2; Name = 'boot_green';        Target = 'Main';       Action = { param($hwnd) Invoke-CassoCommand -Hwnd $hwnd -CommandId $script:Commands.ViewGreen } }
        [pscustomobject]@{ Index = 3; Name = 'boot_amber';        Target = 'Main';       Action = { param($hwnd) Invoke-CassoCommand -Hwnd $hwnd -CommandId $script:Commands.ViewAmber } }
        [pscustomobject]@{ Index = 4; Name = 'boot_white';        Target = 'Main';       Action = { param($hwnd) Invoke-CassoCommand -Hwnd $hwnd -CommandId $script:Commands.ViewWhite } }
        [pscustomobject]@{ Index = 5; Name = 'settings_machine';  Target = 'Main';       Action = { param($hwnd) Invoke-CassoCommand -Hwnd $hwnd -CommandId $script:Commands.ViewAmber; Invoke-CassoCommand -Hwnd $hwnd -CommandId $script:Commands.ViewSettings } }
        [pscustomobject]@{ Index = 6; Name = 'settings_display';  Target = 'Main';       Action = { param($hwnd) Send-CassoKeys -Keys '^{TAB}'; Send-CassoKeys -Keys '^{TAB}'; Send-CassoKeys -Keys '^{TAB}' } }
        [pscustomobject]@{ Index = 7; Name = 'settings_theme';    Target = 'Main';       Action = { param($hwnd) Send-CassoKeys -Keys '^+{TAB}' } }
        [pscustomobject]@{ Index = 8; Name = 'settings_hardware'; Target = 'Main';       Action = { param($hwnd) Send-CassoKeys -Keys '^+{TAB}' } }
        [pscustomobject]@{ Index = 9; Name = 'help_keymap';       Target = 'Foreground'; Action = { param($hwnd) Invoke-CassoCommand -Hwnd $hwnd -CommandId $script:Commands.HelpKeymap } }
    )
}

$cassoExe   = Resolve-LocalPath -Path $CassoPath
$outputPath = Resolve-LocalPath -Path $OutDir
$process    = $null

if (-not (Test-Path -Path $cassoExe -PathType Leaf)) {
    throw "Casso.exe not found at '$cassoExe'. Build it first with '.\scripts\Build.ps1 -Configuration $Configuration -Platform $Platform'."
}

$null = New-Item -Path $outputPath -ItemType Directory -Force

try {
    Write-Host "Starting Casso: $cassoExe"
    $process = Start-Process -FilePath $cassoExe -WorkingDirectory (Split-Path $cassoExe -Parent) -PassThru

    try {
        $null = $process.WaitForInputIdle(10000)
    }
    catch {
    }

    $mainHwnd = Wait-CassoWindow -Process $process
    $matrix   = New-ScreenshotMatrix

    Write-Host "Capturing $($matrix.Count) states to $outputPath"
    foreach ($state in $matrix) {
        Invoke-ScreenshotState -State $state -MainHwnd $mainHwnd -OutputDirectory $outputPath -DelaySeconds $DelaySecs
    }

    Write-Host "Done: $outputPath"
}
finally {
    if ($process -and -not $process.HasExited) {
        $processId = $process.Id
        Stop-Process -Id $processId -Force
        $process.WaitForExit()
    }
}
