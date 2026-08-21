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
//  EhmBreakpoint  (wide)
//
//  What a failed EHM assertion actually does: format the site, log it, and
//  hand it to whatever host is installed.
//
//  The HOST HOOK is the point. A raw __debugbreak is fine under a debugger,
//  but with none attached it becomes a bare "stopped working" crash with no
//  detail at all -- the assertion text, the file, and the line are all lost
//  exactly when they are needed. Routing through g_pfnBreakpoint lets the GUI
//  shell show the text with Abort / Retry / Ignore, and lets the test harness
//  record a failure instead of killing the run.
//
//  With no host installed it still breaks, so a library consumer that never
//  registers one gets the traditional behavior rather than silence.
//
//  A failed format yields an EMPTY message rather than an uninitialized
//  buffer, so the host is never handed garbage; the break still happens.
//
//  Two spellings of this function exist because EHM is used from both wide and
//  narrow translation units; they are identical apart from character type.
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





////////////////////////////////////////////////////////////////////////////////
//
//  EhmBreakpoint  (narrow)
//
//  The narrow-character build of the same function -- see the wide overload
//  above for why the host hook exists and what happens without one.
//
//  It is a separate definition rather than a TCHAR-style macro so the format
//  strings and the buffer type are both plainly visible, and so the two can
//  diverge if a host ever needs different behavior on one of them.
//
////////////////////////////////////////////////////////////////////////////////

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
