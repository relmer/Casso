#include "Pch.h"

//
// EHM Bridge — provides the Windows (_WINDOWS_) versions of EHM functions
// for Casso65Emu. The Casso65Core static library compiles Ehm.cpp without
// <windows.h>, producing the portable (const char *) variants. Since
// Casso65Emu's Pch.h includes <windows.h>, the _WINDOWS_ branch of Ehm.h
// declares LPCWSTR variants. This file satisfies those symbols.
//

#include <strsafe.h>





EHM_BREAKPOINT_FUNC g_pfnEmuBreakpoint = nullptr;

// Avoid duplicate definition of g_pfnBreakpoint — the linker already has
// the one from Casso65Core.lib.  If the linker complains about
// g_pfnBreakpoint, the Casso65Core lib should not be linked; but we need
// EmuCpu, so we must link it. The solution: reuse the same global.
// Ehm.h declares it extern.  The Casso65Core portable path defines it.
// We just need the Windows-path functions.




////////////////////////////////////////////////////////////////////////////////
//
//  DEBUGMSG  (Windows / LPCWSTR path)
//
////////////////////////////////////////////////////////////////////////////////

void DEBUGMSG (LPCWSTR pszFormat, ...)
{
#ifdef _DEBUG
    va_list vlArgs;
    WCHAR   szMsg[1024];

    va_start (vlArgs, pszFormat);
    HRESULT hr = StringCchVPrintf (szMsg, ARRAYSIZE (szMsg), pszFormat, vlArgs);

    if (SUCCEEDED (hr))
    {
        OutputDebugString (szMsg);
    }

    va_end (vlArgs);
#else
    UNREFERENCED_PARAMETER (pszFormat);
#endif
}





////////////////////////////////////////////////////////////////////////////////
//
//  RELEASEMSG  (Windows / LPCWSTR path)
//
////////////////////////////////////////////////////////////////////////////////

void RELEASEMSG (LPCWSTR pszFormat, ...)
{
    va_list vlArgs;
    WCHAR   szMsg[1024];

    va_start (vlArgs, pszFormat);
    HRESULT hr = StringCchVPrintf (szMsg, ARRAYSIZE (szMsg), pszFormat, vlArgs);

    if (SUCCEEDED (hr))
    {
        OutputDebugString (szMsg);
    }

    va_end (vlArgs);
}
