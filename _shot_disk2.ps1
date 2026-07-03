Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Nn {
  [DllImport("user32.dll")] public static extern IntPtr PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern IntPtr FindWindowW(string c, string n);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public struct RECT { public int left, top, right, bottom; }
}
"@
[Nn]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null

$targetPid = [uint32]$args[0]
$main = [IntPtr][int64]$args[1]
Write-Output "main=$main"

# IDM_VIEW_DISK2_DEBUG = 40038
[Nn]::PostMessageW($main, 0x0111, [IntPtr]40038, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1200

# find the disk2 panel window owned by our target pid
$found = [IntPtr]::Zero
$cb = [Nn+EnumProc]{
  param($h,$p)
  $sb = New-Object System.Text.StringBuilder 256
  [Nn]::GetWindowTextW($h, $sb, 256) | Out-Null
  $t = $sb.ToString()
  if ($t -like "*Disk*debug*") {
    $wpid = 0
    [Nn]::GetWindowThreadProcessId($h, [ref]$wpid) | Out-Null
    if ($wpid -eq $script:targetPid) { $script:found = $h; return $false }
  }
  return $true
}
[Nn]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
Write-Output "disk2hwnd=$found"
if ($found -eq [IntPtr]::Zero) { Write-Output "PANEL NOT FOUND"; exit 2 }

$r = New-Object Nn+RECT
[Nn]::GetClientRect($found, [ref]$r) | Out-Null
$w = $r.right - $r.left; $h = $r.bottom - $r.top
Write-Output "client=${w}x${h}"
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[Nn]::PrintWindow($found, $hdc, 2) | Out-Null
$g.ReleaseHdc($hdc); $g.Dispose()
$out = "C:\Users\relmer\source\repos\relmer\Casso\_disk2_shot.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved=$out"
