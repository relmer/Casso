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
    bool                 ok      = false;



    outMajor = 0;
    outBuild = 0;

    // A ladder, not three independent guards: each step needs the one before
    // it. Any failure leaves the outputs zeroed, which every caller reads as
    // "older than the build I asked about".
    hNtDll = GetModuleHandleW (L"ntdll.dll");

    if (hNtDll != nullptr)
    {
        pfn = (PFN_RtlGetVersion) GetProcAddress (hNtDll, "RtlGetVersion");
    }

    if (pfn != nullptr)
    {
        ovi.dwOSVersionInfoSize = sizeof (ovi);
        status = pfn (&ovi);
        ok     = (status == 0);
    }

    if (ok)
    {
        outMajor = ovi.dwMajorVersion;
        outBuild = ovi.dwBuildNumber;
    }

    return ok;
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



    // Win11 reports major == 10, build >= 22000. A failed lookup leaves both
    // zero, which fails the comparison -- no separate guard needed.
    (void) GetOsBuild (major, build);

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



    // Same shape as IsWindows11OrGreater: a failed lookup zeroes both outputs
    // and therefore reports "older".
    (void) GetOsBuild (major, build);

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
    HRESULT  hr        = S_OK;
    HRESULT  hrAttrib  = S_OK;
    DWORD    pref      = round ? kDwmwcpRound : kDwmwcpDoNotRound;
    bool     supported = false;



    supported = IsWindows11OrGreater();
    BAIL_OUT_IF (hwnd == nullptr || !supported, S_OK);

    // Best-effort: failure means the OS does not recognize the attribute,
    // which is exactly the case the version gate above already filters for.
    // Logging would be noise. IGNORE_RETURN_VALUE rather than a (void) cast
    // so the decision is greppable instead of only being readable in prose.
    hrAttrib = DwmSetWindowAttribute (hwnd,
                                      kDwmwaWindowCornerPreference,
                                      &pref,
                                      sizeof (pref));
    IGNORE_RETURN_VALUE (hrAttrib, S_OK);

Error:
    return;
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
    HRESULT  hr        = S_OK;
    HRESULT  hrAttrib  = S_OK;
    DWORD    type      = mica ? kDwmsbtMainWindow : kDwmsbtNone;
    bool     supported = false;



    supported = IsWindows11OrGreater();
    BAIL_OUT_IF (hwnd == nullptr || !supported, S_OK);

    // Best-effort, same as ApplyRoundedCorners.
    hrAttrib = DwmSetWindowAttribute (hwnd,
                                      kDwmwaSystemBackdropType,
                                      &type,
                                      sizeof (type));
    IGNORE_RETURN_VALUE (hrAttrib, S_OK);

Error:
    return;
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
    HRESULT  hr        = S_OK;
    HRESULT  hrAttrib  = S_OK;
    BOOL     flag      = dark ? TRUE : FALSE;
    bool     supported = false;



    supported = IsWindows10_1809OrGreater();
    BAIL_OUT_IF (hwnd == nullptr || !supported, S_OK);

    // Best-effort, same as ApplyRoundedCorners.
    hrAttrib = DwmSetWindowAttribute (hwnd,
                                      kDwmwaUseImmersiveDarkMode,
                                      &flag,
                                      sizeof (flag));
    IGNORE_RETURN_VALUE (hrAttrib, S_OK);

Error:
    return;
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
    HRESULT  hr       = S_OK;
    HRESULT  hrExtend = S_OK;
    MARGINS  m        = { inset, inset, inset, inset };



    BAIL_OUT_IF (hwnd == nullptr, S_OK);

    // Best-effort: a compositor that refuses the extension costs a drop
    // shadow, not correctness, and there is no fallback worth taking.
    hrExtend = DwmExtendFrameIntoClientArea (hwnd, &m);
    IGNORE_RETURN_VALUE (hrExtend, S_OK);

Error:
    return;
}
