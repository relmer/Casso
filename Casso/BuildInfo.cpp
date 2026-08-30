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

// No flavor tag: the caption only carries this stamp on a _DEBUG build, so its
// mere presence already says Debug. Spelling it out was redundant.





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

static std::wstring  BuildTimeFromExe()
{
    wchar_t                       exePath[MAX_PATH] = {};
    WIN32_FILE_ATTRIBUTE_DATA     fad               = {};
    SYSTEMTIME                    utc               = {};
    SYSTEMTIME                    local             = {};
    std::wstring                  stamp;
    bool                          ok                = false;



    // A ladder -- each step needs the one before it. Any failure yields the
    // empty string, which the caller renders as an omitted build time rather
    // than as an error.
    ok = (GetModuleFileNameW (nullptr, exePath, MAX_PATH) != 0);

    if (ok)
    {
        ok = (GetFileAttributesExW (exePath, GetFileExInfoStandard, &fad) != FALSE);
    }

    if (ok)
    {
        ok = FileTimeToSystemTime (&fad.ftLastWriteTime, &utc)
             && SystemTimeToTzSpecificLocalTime (nullptr, &utc, &local);
    }

    if (ok)
    {
        stamp = std::format (L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
                             local.wYear, local.wMonth, local.wDay,
                             local.wHour, local.wMinute, local.wSecond);
    }

    return stamp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCassoBuildInfo
//
//  Version + arch (from compile-time macros -- each is exact for this
//  configuration's binary) + the exe's link time (read at runtime, so it always
//  names the actual build the user is running).
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t *  GetCassoBuildInfo()
{
    static const std::wstring   s = std::format (
        L"v{}.{}.{} {} ({})",
        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
        std::wstring (BI_ARCH, BI_ARCH + sizeof (BI_ARCH) - 1),
        BuildTimeFromExe());



    return s.c_str();
}
