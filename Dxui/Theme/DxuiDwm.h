#pragma once

////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDwm
//
//  Runtime-gated wrappers around DWM attribute APIs that only exist
//  (or only have meaningful behavior) on Windows 11 / Windows 10 1809+.
//
//  IsWindows11OrGreater() resolves NTDLL!RtlGetVersion at runtime so we
//  get a real OS version unaffected by the absence of a Win11-aware
//  application manifest. (VersionHelpers.h's IsWindows10OrGreater
//  family lies on systems that lack the GUID in the manifest.)
//
//  All helpers are no-ops on unsupported OS versions — callers can
//  invoke them unconditionally during window creation.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDwm
{
public:
    static bool IsWindows11OrGreater      ();
    static bool IsWindows10_1809OrGreater();
    static void ApplyRoundedCorners       (HWND hwnd, bool round);
    static void ApplyMicaBackdrop         (HWND hwnd, bool mica);
    static void ApplyImmersiveDarkMode    (HWND hwnd, bool dark);
    static void ExtendFrameIntoClientArea (HWND hwnd, int inset);
};
