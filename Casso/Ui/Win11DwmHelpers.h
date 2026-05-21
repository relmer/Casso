#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win11DwmHelpers
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

class Win11DwmHelpers
{
public:
    // True iff the running OS is Windows 11 (build >= 22000) or newer.
    static bool IsWindows11OrGreater();

    // True iff the running OS is Windows 10 1809 (build >= 17763) or
    // newer. DWMWA_USE_IMMERSIVE_DARK_MODE works from this build up.
    static bool IsWindows10_1809OrGreater();

    // Sets DWMWA_WINDOW_CORNER_PREFERENCE. No-op on pre-Win11.
    static void ApplyRoundedCorners (HWND hwnd, bool round);

    // Sets DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_MAINWINDOW (Mica). No-op
    // on pre-Win11. Caller is responsible for extending the frame into
    // the client area first (DwmExtendFrameIntoClientArea) — without
    // that, the backdrop is invisible.
    static void ApplyMicaBackdrop (HWND hwnd, bool mica);

    // Sets DWMWA_USE_IMMERSIVE_DARK_MODE. No-op on builds older than
    // 1809.
    static void ApplyImmersiveDarkMode (HWND hwnd, bool dark);

    // Extends the DWM frame into the client area by `inset` pixels on
    // every edge. Required to keep the OS drop-shadow visible on a
    // borderless window. Safe on every Win10+ build.
    static void ExtendFrameIntoClientArea (HWND hwnd, int inset);
};
