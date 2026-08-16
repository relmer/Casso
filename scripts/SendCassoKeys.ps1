<#
.SYNOPSIS
    Types into a running Casso instance without stealing keyboard focus.

.DESCRIPTION
    Capture tooling for the Merlin corpus: driving Merlin's editor and menus
    means sending a lot of keystrokes, and doing that with SendInput requires
    the emulator window to be foreground -- which takes the keyboard away from
    whoever is using the machine.

    PostMessage does not. Casso's main window handles keyboard itself (it has no
    child windows), so posted key messages are delivered to it whether or not it
    is focused, and the developer can keep typing elsewhere.

    Verified against Merlin 8 v2.47: posting "C" from a background window
    produced Merlin's catalog listing, and the listing matched
    ExtractDos33File.ps1's own catalog walk file for file.

.PARAMETER ProcessId
    The Casso process to type into. Kill only instances you launched -- other
    worktrees run their own.

.PARAMETER Text
    Characters to send, in order.

.PARAMETER Return
    Send RETURN after the text.

.PARAMETER Escape
    Send ESC instead of text. For backing out of a prompt.

.PARAMETER DelayMs
    Pause between keystrokes. The emulated keyboard has a single-byte latch, so
    typing faster than the guest polls it drops characters.

.EXAMPLE
    ./scripts/SendCassoKeys.ps1 -ProcessId 1234 -Text "C"
    Catalog, without taking focus.

.EXAMPLE
    ./scripts/SendCassoKeys.ps1 -ProcessId 1234 -Text " LDA #`$41" -Return
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [int]$ProcessId,

    [string]$Text = '',

    [switch]$Return,

    [switch]$Escape,

    [int]$DelayMs = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CassoKeys {
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
    [DllImport("user32.dll")] public static extern short VkKeyScan(char c);

    const uint WM_KEYDOWN = 0x0100;
    const uint WM_KEYUP   = 0x0101;
    const uint WM_CHAR    = 0x0102;

    // Posts the full down/char/up sequence. WM_CHAR carries the character the
    // guest actually latches; the key messages bracket it so anything watching
    // for key state sees a consistent press.
    public static void Send(IntPtr h, ushort vk, char ch) {
        uint sc = MapVirtualKey(vk, 0);
        IntPtr down = (IntPtr)(1 | (sc << 16));
        IntPtr up   = (IntPtr)(1 | (sc << 16) | (1u << 30) | (1u << 31));
        PostMessage(h, WM_KEYDOWN, (IntPtr)vk, down);
        PostMessage(h, WM_CHAR,    (IntPtr)ch, down);
        PostMessage(h, WM_KEYUP,   (IntPtr)vk, up);
    }

    public static ushort VkFor(char c) {
        short r = VkKeyScan(c);
        return (ushort)(r & 0xFF);
    }
}
'@

$proc = Get-Process -Id $ProcessId -ErrorAction Stop
$hwnd = $proc.MainWindowHandle

if ($hwnd -eq [IntPtr]::Zero)
{
    throw "PID $ProcessId has no main window -- is Casso still starting up?"
}

if ($Escape)
{
    [CassoKeys]::Send($hwnd, 0x1B, [char]27)
    Start-Sleep -Milliseconds $DelayMs
}

foreach ($ch in $Text.ToCharArray())
{
    $vk = [CassoKeys]::VkFor($ch)
    [CassoKeys]::Send($hwnd, $vk, $ch)
    Start-Sleep -Milliseconds $DelayMs
}

if ($Return)
{
    [CassoKeys]::Send($hwnd, 0x0D, [char]13)
    Start-Sleep -Milliseconds $DelayMs
}
