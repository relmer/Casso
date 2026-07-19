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
//  CassoBuildInfo
//
//  Version + arch + flavor + compile timestamp, built once. The narrow compile
//  macros (__DATE__, __TIME__, arch, flavor) are widened to match the wide
//  caption; the version numbers come straight from Version.h.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t *  CassoBuildInfo ()
{
    static const std::wstring   s = std::format (
        L"v{}.{}.{} {} {} ({})",
        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
        std::wstring (BI_ARCH, BI_ARCH + sizeof (BI_ARCH) - 1),
        std::wstring (BI_FLAVOR, BI_FLAVOR + sizeof (BI_FLAVOR) - 1),
        std::wstring (VERSION_BUILD_TIMESTAMP,
                      VERSION_BUILD_TIMESTAMP + sizeof (VERSION_BUILD_TIMESTAMP) - 1));

    return s.c_str ();
}
