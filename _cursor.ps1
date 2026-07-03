Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Cur {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern IntPtr GetCursor();
  [DllImport("user32.dll")] public static extern IntPtr LoadCursorW(IntPtr hInst, IntPtr name);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public struct RECT { public int left, top, right, bottom; }
  public struct POINT { public int x, y; }
}
"@
[Cur]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$tp=[uint32]$args[0]
$script:t=[IntPtr]::Zero
$cb=[Cur+EnumProc]{ param($hh,$p)
  $pid2=0;[Cur]::GetWindowThreadProcessId($hh,[ref]$pid2)|Out-Null
  if($pid2 -eq $tp){ $sb=New-Object System.Text.StringBuilder 512;[Cur]::GetWindowTextW($hh,$sb,512)|Out-Null; if($sb.ToString() -match 'Disk'){$script:t=$hh} }
  return $true }
[Cur]::EnumWindows($cb,[IntPtr]::Zero)|Out-Null
if($script:t -eq [IntPtr]::Zero){Write-Output "NO DISK2";exit 2}
[Cur]::SetForegroundWindow($script:t)|Out-Null
Start-Sleep -Milliseconds 400
$sizewe = [Cur]::LoadCursorW([IntPtr]::Zero, [IntPtr]32644)  # IDC_SIZEWE
$arrow  = [Cur]::LoadCursorW([IntPtr]::Zero, [IntPtr]32512)  # IDC_ARROW
Write-Output ("SIZEWE={0} ARROW={1}" -f $sizewe,$arrow)
# Header row is just below the filter area. Sweep x across likely dividers at several y in the header strip.
$hits = @()
foreach ($y in 515,525,535,545,555) {
  foreach ($x in 150..950) {
    $sp = New-Object Cur+POINT; $sp.x=$x; $sp.y=$y
    [Cur]::ClientToScreen($script:t, [ref]$sp) | Out-Null
    [Cur]::SetCursorPos($sp.x, $sp.y) | Out-Null
    Start-Sleep -Milliseconds 2
    $c = [Cur]::GetCursor()
    if ($c -eq $sizewe) { $hits += ("x=$x y=$y") }
  }
}
Write-Output ("SIZEWE hits: {0}" -f $hits.Count)
$hits | Select-Object -First 12 | ForEach-Object { Write-Output $_ }
