#include "Pch.h"

#include "Ehm.h"





EHM_BREAKPOINT_FUNC g_pfnBreakpoint = nullptr;
EHM_NOTIFY_FUNC     g_pfnNotify     = nullptr;





////////////////////////////////////////////////////////////////////////////////
//
//  SetBreakpointFunction
//
////////////////////////////////////////////////////////////////////////////////

void SetBreakpointFunction (EHM_BREAKPOINT_FUNC func)
{
    g_pfnBreakpoint = func;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EhmBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

void EhmBreakpoint (void)
{
#ifdef _WINDOWS_
    g_pfnBreakpoint ? g_pfnBreakpoint() : __debugbreak();
#elif defined(_MSC_VER)
    g_pfnBreakpoint ? g_pfnBreakpoint() : __debugbreak();
#else
    if (g_pfnBreakpoint)
    {
        g_pfnBreakpoint();
    }
#endif
}



////////////////////////////////////////////////////////////////////////////////
//
//  SetNotifyFunction
//
////////////////////////////////////////////////////////////////////////////////

void SetNotifyFunction (EHM_NOTIFY_FUNC func)
{
    g_pfnNotify = func;
}





#ifdef UNICODE

//
// Unicode path — wide string diagnostics and notification
//
////////////////////////////////////////////////////////////////////////////////

//
//  DEBUGMSG
//
////////////////////////////////////////////////////////////////////////////////

void DEBUGMSG (const wchar_t * pszFormat, ...)
{
#ifdef _DEBUG
    va_list vlArgs;
    va_start (vlArgs, pszFormat);

#ifdef _WINDOWS_
    wchar_t szMsg[1024];
    HRESULT hr = StringCchVPrintfW (szMsg, 1024, pszFormat, vlArgs);
    if (SUCCEEDED (hr))
    {
        OutputDebugStringW (szMsg);
    }
#else
    std::vfwprintf (stderr, pszFormat, vlArgs);
#endif

    va_end (vlArgs);
#else
    (void) pszFormat;
#endif
}





////////////////////////////////////////////////////////////////////////////////
//
//  RELEASEMSG
//
////////////////////////////////////////////////////////////////////////////////

void RELEASEMSG (const wchar_t * pszFormat, ...)
{
    va_list vlArgs;
    va_start (vlArgs, pszFormat);

#ifdef _WINDOWS_
    wchar_t szMsg[1024];
    HRESULT hr = StringCchVPrintfW (szMsg, 1024, pszFormat, vlArgs);
    if (SUCCEEDED (hr))
    {
        OutputDebugStringW (szMsg);
    }
#else
    std::vfwprintf (stderr, pszFormat, vlArgs);
#endif

    va_end (vlArgs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EhmNotifyUser
//
////////////////////////////////////////////////////////////////////////////////

void EhmNotifyUser (const wchar_t * message)
{
    if (g_pfnNotify != nullptr)
    {
        g_pfnNotify (message);
        return;
    }

#ifdef _WINDOWS_
    HANDLE hConsole = GetStdHandle (STD_ERROR_HANDLE);

    if (hConsole != NULL && hConsole != INVALID_HANDLE_VALUE)
    {
        std::fwprintf (stderr, L"Error: %s\n", message);
    }
    else
    {
        MessageBoxW (NULL, message, L"Casso65", MB_OK | MB_ICONERROR);
    }
#else
    std::fwprintf (stderr, L"Error: %s\n", message);
#endif
}

#else

//
// Portable ANSI path — fprintf to stderr
//
////////////////////////////////////////////////////////////////////////////////

//
//  DEBUGMSG
//
////////////////////////////////////////////////////////////////////////////////

void DEBUGMSG (const char * pszFormat, ...)
{
#ifdef _DEBUG
    va_list vlArgs;

    va_start (vlArgs, pszFormat);
    std::vfprintf (stderr, pszFormat, vlArgs);
    va_end (vlArgs);
#else
    (void) pszFormat;
#endif
}





////////////////////////////////////////////////////////////////////////////////
//
//  RELEASEMSG
//
////////////////////////////////////////////////////////////////////////////////

void RELEASEMSG (const char * pszFormat, ...)
{
    va_list vlArgs;

    va_start (vlArgs, pszFormat);
    std::vfprintf (stderr, pszFormat, vlArgs);
    va_end (vlArgs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EhmNotifyUser
//
////////////////////////////////////////////////////////////////////////////////

void EhmNotifyUser (const char * message)
{
    if (g_pfnNotify != nullptr)
    {
        g_pfnNotify (message);
        return;
    }

    std::fprintf (stderr, "Error: %s\n", message);
}

#endif
