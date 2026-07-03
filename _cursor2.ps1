Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Cur2 {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern IntPtr GetCursor();
  [DllImport("user32.dll")] public static extern IntPtr LoadCursorW(IntPtr hInst, IntPtr name);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public struct POINT { public int x, y; }
}
"@
[Cur2]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$tp=[uint32]$args[0]
$script:t=[IntPtr]::Zero
$cb=[Cur2+EnumProc]{ param($hh,$p)
  $pid2=0;[Cur2]::GetWindowThreadProcessId($hh,[ref]$pid2)|Out-Null
  if($pid2 -eq $tp){ $sb=New-Object System.Text.StringBuilder 512;[Cur2]::GetWindowTextW($hh,$sb,512)|Out-Null; if($sb.ToString() -match 'Disk'){$script:t=$hh} }
  return $true }
[Cur2]::EnumWindows($cb,[IntPtr]::Zero)|Out-Null
if($script:t -eq [IntPtr]::Zero){Write-Output "NO DISK2";exit 2}
$sizewe = [Cur2]::LoadCursorW([IntPtr]::Zero, [IntPtr]32644)
$WM_SETCURSOR=0x0020; $HTCLIENT=1; $WM_MOUSEMOVE=0x0200
$lp = [IntPtr](($WM_MOUSEMOVE -shl 16) -bor $HTCLIENT)
$hits=@()
foreach ($y in 520,530,540,550) {
  foreach ($x in 150..950) {
    $sp = New-Object Cur2+POINT; $sp.x=$x; $sp.y=$y
    [Cur2]::ClientToScreen($script:t, [ref]$sp) | Out-Null
    [Cur2]::SetCursorPos($sp.x, $sp.y) | Out-Null
    [Cur2]::SendMessageW($script:t, $WM_SETCURSOR, $script:t, $lp) | Out-Null
    if ([Cur2]::GetCursor() -eq $sizewe) { $hits += ("x=$x y=$y") }
  }
}
Write-Output ("SIZEWE hits: {0}" -f $hits.Count)
$hits | Select-Object -First 20 | ForEach-Object { Write-Output $_ }
