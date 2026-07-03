Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Sw {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public struct RECT { public int left, top, right, bottom; }
}
"@
[Sw]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$tp = [uint32]$args[0]
$script:target = [IntPtr]::Zero
$cb = [Sw+EnumProc]{ param($hh,$p)
  $pid2=0; [Sw]::GetWindowThreadProcessId($hh,[ref]$pid2)|Out-Null
  if($pid2 -eq $tp){
    $sb=New-Object System.Text.StringBuilder 512
    [Sw]::GetWindowTextW($hh,$sb,512)|Out-Null
    $t=$sb.ToString()
    Write-Output ("hwnd={0} title='{1}'" -f $hh,$t)
    if($t -match 'Disk' -and $t -match 'debug'){ $script:target=$hh }
  }
  return $true
}
[Sw]::EnumWindows($cb,[IntPtr]::Zero) | Out-Null
if($script:target -eq [IntPtr]::Zero){ Write-Output "NO DISK2"; exit 2 }
Write-Output ("target={0}" -f $script:target)
[Sw]::ShowWindow($script:target, 5) | Out-Null
[Sw]::SetForegroundWindow($script:target) | Out-Null
Start-Sleep -Milliseconds 600
$r = New-Object Sw+RECT
[Sw]::GetClientRect($script:target, [ref]$r) | Out-Null
$w=$r.right-$r.left; $h=$r.bottom-$r.top
Write-Output ("client={0}x{1}" -f $w,$h)
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap($w,$h)
$g=[System.Drawing.Graphics]::FromImage($bmp)
$hdc=$g.GetHdc()
[Sw]::PrintWindow($script:target,$hdc,2)|Out-Null
$g.ReleaseHdc($hdc); $g.Dispose()
$out="C:\Users\relmer\source\repos\relmer\Casso\_disk2_shot.png"
$bmp.Save($out,[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Write-Output ("saved={0}" -f $out)
