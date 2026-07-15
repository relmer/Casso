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

#ifdef UNICODE
void EhmBreakpoint (const wchar_t * file, int line, const wchar_t * func, const wchar_t * expr)
{
    wchar_t  msg[1024];

#ifdef _WINDOWS_
    HRESULT  hrFmt = StringCchPrintfW (msg, sizeof (msg) / sizeof (msg[0]),
                                       L"%s(%d) - %s - Assertion failed:  %s",
                                       file, line, func, expr);
    if (FAILED (hrFmt))
    {
        msg[0] = L'\0';
    }
#else
    if (swprintf (msg, sizeof (msg) / sizeof (msg[0]),
                  L"%ls(%d) - %ls - Assertion failed:  %ls",
                  file, line, func, expr) < 0)
    {
        msg[0] = L'\0';
    }
#endif

    // Log to the debugger Output window / stderr, as before.
    DEBUGMSG (L"%s\n", msg);

    // Hand the formatted text to the host (GUI dialog, test harness, ...).
    // With no host installed, fall back to the raw debugger break.
    if (g_pfnBreakpoint != nullptr)
    {
        g_pfnBreakpoint (msg);
        return;
    }

#if defined(_WINDOWS_) || defined(_MSC_VER)
    __debugbreak();
#endif
}
#else
void EhmBreakpoint (const char * file, int line, const char * func, const char * expr)
{
    char  msg[1024];

#ifdef _WINDOWS_
    HRESULT  hrFmt = StringCchPrintfA (msg, sizeof (msg),
                                       "%s(%d) - %s - Assertion failed:  %s",
                                       file, line, func, expr);
    if (FAILED (hrFmt))
    {
        msg[0] = '\0';
    }
#else
    if (snprintf (msg, sizeof (msg),
                  "%s(%d) - %s - Assertion failed:  %s",
                  file, line, func, expr) < 0)
    {
        msg[0] = '\0';
    }
#endif

    DEBUGMSG ("%s\n", msg);

    if (g_pfnBreakpoint != nullptr)
    {
        g_pfnBreakpoint (msg);
        return;
    }

#if defined(_WINDOWS_) || defined(_MSC_VER)
    __debugbreak();
#endif
}
#endif



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
        MessageBoxW (NULL, message, L"Casso", MB_OK | MB_ICONERROR);
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
