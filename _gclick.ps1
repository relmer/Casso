Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Clik {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public struct RECT { public int left, top, right, bottom; }
}
"@
# args: pid  titleRegex  x  y  outPng
[Clik]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$tp=[uint32]$args[0]; $rx=[string]$args[1]; $x=[int]$args[2]; $y=[int]$args[3]; $out=[string]$args[4]
$script:t=[IntPtr]::Zero
$cb=[Clik+EnumProc]{ param($hh,$p)
  $pid2=0;[Clik]::GetWindowThreadProcessId($hh,[ref]$pid2)|Out-Null
  if($pid2 -eq $tp){ $sb=New-Object System.Text.StringBuilder 512;[Clik]::GetWindowTextW($hh,$sb,512)|Out-Null; if($sb.ToString() -match $rx){$script:t=$hh} }
  return $true }
[Clik]::EnumWindows($cb,[IntPtr]::Zero)|Out-Null
if($script:t -eq [IntPtr]::Zero){Write-Output "NOT FOUND: $rx";exit 2}
$lp=[IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
[Clik]::PostMessageW($script:t,0x0200,[IntPtr]0,$lp)|Out-Null
[Clik]::PostMessageW($script:t,0x0201,[IntPtr]1,$lp)|Out-Null
Start-Sleep -Milliseconds 120
[Clik]::PostMessageW($script:t,0x0202,[IntPtr]0,$lp)|Out-Null
Start-Sleep -Milliseconds 500
$r=New-Object Clik+RECT;[Clik]::GetClientRect($script:t,[ref]$r)|Out-Null
$w=$r.right-$r.left;$h=$r.bottom-$r.top
Add-Type -AssemblyName System.Drawing
$bmp=New-Object System.Drawing.Bitmap($w,$h);$g=[System.Drawing.Graphics]::FromImage($bmp);$hdc=$g.GetHdc()
[Clik]::PrintWindow($script:t,$hdc,2)|Out-Null;$g.ReleaseHdc($hdc);$g.Dispose()
$bmp.Save($out,[System.Drawing.Imaging.ImageFormat]::Png);$bmp.Dispose()
Write-Output ("clicked={0},{1} saved={2}" -f $x,$y,$out)
