Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Gs {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public struct RECT { public int left, top, right, bottom; }
}
"@
# args: pid  mainHwnd  menuCmd  titleRegex  outPng
[Gs]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$tp=[uint32]$args[0]; $main=[IntPtr][int64]$args[1]; $cmd=[int]$args[2]; $rx=[string]$args[3]; $out=[string]$args[4]
[Gs]::SendMessageW($main, 0x0111, [IntPtr]$cmd, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 900
$script:t=[IntPtr]::Zero
$cb=[Gs+EnumProc]{ param($hh,$p)
  $pid2=0;[Gs]::GetWindowThreadProcessId($hh,[ref]$pid2)|Out-Null
  if($pid2 -eq $tp){ $sb=New-Object System.Text.StringBuilder 512;[Gs]::GetWindowTextW($hh,$sb,512)|Out-Null; if($sb.ToString() -match $rx){$script:t=$hh} }
  return $true }
[Gs]::EnumWindows($cb,[IntPtr]::Zero)|Out-Null
if($script:t -eq [IntPtr]::Zero){Write-Output "NOT FOUND: $rx";exit 2}
[Gs]::ShowWindow($script:t,5)|Out-Null; [Gs]::SetForegroundWindow($script:t)|Out-Null
Start-Sleep -Milliseconds 500
$r=New-Object Gs+RECT;[Gs]::GetClientRect($script:t,[ref]$r)|Out-Null
$w=$r.right-$r.left;$h=$r.bottom-$r.top
Add-Type -AssemblyName System.Drawing
$bmp=New-Object System.Drawing.Bitmap($w,$h);$g=[System.Drawing.Graphics]::FromImage($bmp);$hdc=$g.GetHdc()
[Gs]::PrintWindow($script:t,$hdc,2)|Out-Null;$g.ReleaseHdc($hdc);$g.Dispose()
$bmp.Save($out,[System.Drawing.Imaging.ImageFormat]::Png);$bmp.Dispose()
Write-Output ("hwnd={0} client={1}x{2} saved={3}" -f $script:t,$w,$h,$out)
