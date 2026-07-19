#include "Pch.h"

#include "BuildInfo.h"
#include "Version.h"




#if   defined (_M_ARM64)
    #define BI_ARCH  "ARM64"
#elif defined (_M_X64)
    #define BI_ARCH  "x64"
#elif defined (_M_IX86)
    #define BI_ARCH  "x86"
#else
    #define BI_ARCH  "unknown-arch"
#endif

#if defined (_DEBUG)
    #define BI_FLAVOR  "Debug"
#else
    #define BI_FLAVOR  "Release"
#endif




////////////////////////////////////////////////////////////////////////////////
//
//  BuildTimeFromExe
//
//  The build time = the running .exe's own last-write (link) time, read at
//  runtime. This is always accurate regardless of which translation units an
//  incremental build recompiled -- unlike __DATE__/__TIME__, which only refresh
//  when THIS file recompiles and so goes stale on an incremental relink. Local
//  time, "YYYY-MM-DD HH:MM:SS"; empty on any failure.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring  BuildTimeFromExe ()
{
    wchar_t                       exePath[MAX_PATH] = {};
    WIN32_FILE_ATTRIBUTE_DATA     fad               = {};
    SYSTEMTIME                    utc               = {};
    SYSTEMTIME                    local             = {};

    if (GetModuleFileNameW (nullptr, exePath, MAX_PATH) == 0)
    {
        return L"";
    }
    if (!GetFileAttributesExW (exePath, GetFileExInfoStandard, &fad))
    {
        return L"";
    }
    if (!FileTimeToSystemTime (&fad.ftLastWriteTime, &utc) ||
        !SystemTimeToTzSpecificLocalTime (nullptr, &utc, &local))
    {
        return L"";
    }

    return std::format (L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
                        local.wYear, local.wMonth, local.wDay,
                        local.wHour, local.wMinute, local.wSecond);
}




////////////////////////////////////////////////////////////////////////////////
//
//  CassoBuildInfo
//
//  Version + arch + flavor (from compile-time macros -- each is exact for this
//  configuration's binary) + the exe's link time (read at runtime, so it always
//  names the actual build the user is running).
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t *  CassoBuildInfo ()
{
    static const std::wstring   s = std::format (
        L"v{}.{}.{} {} {} ({})",
        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
        std::wstring (BI_ARCH, BI_ARCH + sizeof (BI_ARCH) - 1),
        std::wstring (BI_FLAVOR, BI_FLAVOR + sizeof (BI_FLAVOR) - 1),
        BuildTimeFromExe ());

    return s.c_str ();
}
