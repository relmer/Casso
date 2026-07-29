#include "Pch.h"

#include "DxuiDwm.h"

#pragma comment(lib, "dwmapi.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Local OSVERSIONINFOW + RtlGetVersion typedef
//
//  Avoids pulling in <winternl.h> and its avalanche of NT-internal
//  declarations. The struct layout has been stable since Win2000.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDwm::GetOsBuild (DWORD & outMajor, DWORD & outBuild)
{
    HMODULE              hNtDll  = nullptr;
    PFN_RtlGetVersion    pfn     = nullptr;
    OsVersion            ovi     = {};
    LONG                 status  = 0;


    outMajor = 0;
    outBuild = 0;

    hNtDll = GetModuleHandleW (L"ntdll.dll");

    if (hNtDll == nullptr)
    {
        return false;
    }

    pfn = (PFN_RtlGetVersion) GetProcAddress (hNtDll, "RtlGetVersion");

    if (pfn == nullptr)
    {
        return false;
    }

    ovi.dwOSVersionInfoSize = sizeof (ovi);
    status = pfn (&ovi);

    if (status != 0)
    {
        return false;
    }

    outMajor = ovi.dwMajorVersion;
    outBuild = ovi.dwBuildNumber;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsWindows11OrGreater
//
//  True iff the running OS is Windows 11 (build >= 22000) or newer.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDwm::IsWindows11OrGreater()
{
    DWORD  major = 0;
    DWORD  build = 0;



    if (!GetOsBuild (major, build))
    {
        return false;
    }

    // Win11 reports major == 10, build >= 22000.
    return major >= 10 && build >= 22000;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsWindows10_1809OrGreater
//
//  True iff the running OS is Windows 10 1809 (build >= 17763) or
//  newer. DWMWA_USE_IMMERSIVE_DARK_MODE works from this build up.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDwm::IsWindows10_1809OrGreater()
{
    DWORD  major = 0;
    DWORD  build = 0;



    if (!GetOsBuild (major, build))
    {
        return false;
    }

    return major >= 10 && build >= 17763;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyRoundedCorners
//
//  Sets DWMWA_WINDOW_CORNER_PREFERENCE. No-op on pre-Win11.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDwm::ApplyRoundedCorners (HWND hwnd, bool round)
{
    DWORD  pref = round ? kDwmwcpRound : kDwmwcpDoNotRound;



    if (hwnd == nullptr || !IsWindows11OrGreater())
    {
        return;
    }

    // Best-effort: ignore HRESULT — failure means the OS doesn't
    // recognize the attribute, which is exactly the case the version
    // gate above already filters for. Logging would be noise.
    (void) DwmSetWindowAttribute (hwnd,
                                  kDwmwaWindowCornerPreference,
                                  &pref,
                                  sizeof (pref));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyMicaBackdrop
//
//  Sets DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_MAINWINDOW (Mica). No-op
//  on pre-Win11. Caller is responsible for extending the frame into
//  the client area first (DwmExtendFrameIntoClientArea) — without
//  that, the backdrop is invisible.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDwm::ApplyMicaBackdrop (HWND hwnd, bool mica)
{
    DWORD  type = mica ? kDwmsbtMainWindow : kDwmsbtNone;



    if (hwnd == nullptr || !IsWindows11OrGreater())
    {
        return;
    }

    (void) DwmSetWindowAttribute (hwnd,
                                  kDwmwaSystemBackdropType,
                                  &type,
                                  sizeof (type));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyImmersiveDarkMode
//
//  Sets DWMWA_USE_IMMERSIVE_DARK_MODE. No-op on builds older than
//  1809.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDwm::ApplyImmersiveDarkMode (HWND hwnd, bool dark)
{
    BOOL  flag = dark ? TRUE : FALSE;



    if (hwnd == nullptr || !IsWindows10_1809OrGreater())
    {
        return;
    }

    (void) DwmSetWindowAttribute (hwnd,
                                  kDwmwaUseImmersiveDarkMode,
                                  &flag,
                                  sizeof (flag));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtendFrameIntoClientArea
//
//  Extends the DWM frame into the client area by `inset` pixels on
//  every edge. Required to keep the OS drop-shadow visible on a
//  borderless window. Safe on every Win10+ build.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDwm::ExtendFrameIntoClientArea (HWND hwnd, int inset)
{
    MARGINS  m = { inset, inset, inset, inset };



    if (hwnd == nullptr)
    {
        return;
    }

    (void) DwmExtendFrameIntoClientArea (hwnd, &m);
}
