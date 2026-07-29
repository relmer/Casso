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

private:
    // RTL_OSVERSIONINFOW by another name -- declared here so we do not
    // depend on which SDK's ntddk headers happen to be reachable.
    // Nested rather than file-scope so the type has internal linkage.
    struct OsVersion
    {
        DWORD    dwOSVersionInfoSize;
        DWORD    dwMajorVersion;
        DWORD    dwMinorVersion;
        DWORD    dwBuildNumber;
        DWORD    dwPlatformId;
        WCHAR    szCSDVersion[128];
    };

    typedef LONG (WINAPI * PFN_RtlGetVersion) (OsVersion *);

    static bool GetOsBuild (DWORD & outMajor, DWORD & outBuild);

    // DWMWA_* values that are not always declared in older SDK headers.
    static constexpr DWORD kDwmwaUseImmersiveDarkMode       = 20;
    static constexpr DWORD kDwmwaWindowCornerPreference     = 33;
    static constexpr DWORD kDwmwaSystemBackdropType         = 38;

    static constexpr DWORD kDwmwcpDefault                   = 0;
    static constexpr DWORD kDwmwcpDoNotRound                = 1;
    static constexpr DWORD kDwmwcpRound                     = 2;

    static constexpr DWORD kDwmsbtAuto                      = 0;
    static constexpr DWORD kDwmsbtNone                      = 1;
    static constexpr DWORD kDwmsbtMainWindow                = 2;   // Mica.
};
