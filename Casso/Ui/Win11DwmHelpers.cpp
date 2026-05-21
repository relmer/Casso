#include "Pch.h"

#include "Win11DwmHelpers.h"

#pragma comment(lib, "dwmapi.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Local OSVERSIONINFOW + RtlGetVersion typedef
//
//  Avoids pulling in <winternl.h> and its avalanche of NT-internal
//  declarations. The struct layout has been stable since Win2000.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    struct WinDwmOsVersion
    {
        DWORD    dwOSVersionInfoSize;
        DWORD    dwMajorVersion;
        DWORD    dwMinorVersion;
        DWORD    dwBuildNumber;
        DWORD    dwPlatformId;
        WCHAR    szCSDVersion[128];
    };


    typedef LONG (WINAPI * PFN_RtlGetVersion) (WinDwmOsVersion *);


    bool GetOsBuild (DWORD & outMajor, DWORD & outBuild)
    {
        HMODULE              hNtDll  = nullptr;
        PFN_RtlGetVersion    pfn     = nullptr;
        WinDwmOsVersion      ovi     = {};
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


    // DWMWA_* values that are not always declared in older SDK headers.
    constexpr DWORD kDwmwaUseImmersiveDarkMode       = 20;
    constexpr DWORD kDwmwaWindowCornerPreference     = 33;
    constexpr DWORD kDwmwaSystemBackdropType         = 38;

    constexpr DWORD kDwmwcpDefault                   = 0;
    constexpr DWORD kDwmwcpDoNotRound                = 1;
    constexpr DWORD kDwmwcpRound                     = 2;

    constexpr DWORD kDwmsbtAuto                      = 0;
    constexpr DWORD kDwmsbtNone                      = 1;
    constexpr DWORD kDwmsbtMainWindow                = 2;   // Mica.
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsWindows11OrGreater
//
////////////////////////////////////////////////////////////////////////////////

bool Win11DwmHelpers::IsWindows11OrGreater()
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
////////////////////////////////////////////////////////////////////////////////

bool Win11DwmHelpers::IsWindows10_1809OrGreater()
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
////////////////////////////////////////////////////////////////////////////////

void Win11DwmHelpers::ApplyRoundedCorners (HWND hwnd, bool round)
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
////////////////////////////////////////////////////////////////////////////////

void Win11DwmHelpers::ApplyMicaBackdrop (HWND hwnd, bool mica)
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
////////////////////////////////////////////////////////////////////////////////

void Win11DwmHelpers::ApplyImmersiveDarkMode (HWND hwnd, bool dark)
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
////////////////////////////////////////////////////////////////////////////////

void Win11DwmHelpers::ExtendFrameIntoClientArea (HWND hwnd, int inset)
{
    MARGINS  m = { inset, inset, inset, inset };


    if (hwnd == nullptr)
    {
        return;
    }

    (void) DwmExtendFrameIntoClientArea (hwnd, &m);
}
