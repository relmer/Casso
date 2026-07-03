Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Dg {
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
}
"@
[Dg]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$tp = [uint32]$args[0]
$main = [IntPtr][int64]$args[1]
[Dg]::SendMessageW($main, 0x0111, [IntPtr]40038, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1000
$lines = New-Object System.Collections.ArrayList
$cb = [Dg+EnumProc]{ param($hh,$p)
  $pid2=0; [Dg]::GetWindowThreadProcessId($hh,[ref]$pid2)|Out-Null
  if($pid2 -eq $tp){
    $sb=New-Object System.Text.StringBuilder 256
    [Dg]::GetWindowTextW($hh,$sb,256)|Out-Null
    $vis=[Dg]::IsWindowVisible($hh)
    [void]$lines.Add(("hwnd={0} vis={1} title='{2}'" -f $hh,$vis,$sb.ToString()))
  }
  return $true
}
[Dg]::EnumWindows($cb,[IntPtr]::Zero) | Out-Null
$lines | ForEach-Object { Write-Output $_ }
Write-Output ("count={0}" -f $lines.Count)
