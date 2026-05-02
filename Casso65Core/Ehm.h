#pragma once





//
// EHM — Error Handling Macros
//
// Portable version: uses standard C++ by default.
// If <windows.h> has been included (via Pch.h), uses native Windows APIs.
//





#define ErrorLabel          Error

#define __EHM_ASSERT        true
#define __EHM_NO_ASSERT     false





//
// Platform detection — _WINDOWS_ is defined by <windows.h> after inclusion
//

#ifdef _WINDOWS_
    // Windows path — full Win32 support
    #include <strsafe.h>
#else
    // Portable C++ path — define HRESULT and friends if not already present

    #ifndef _HRESULT_DEFINED
        #define _HRESULT_DEFINED
        typedef long HRESULT;
    #endif

    #ifndef S_OK
        #define S_OK              ((HRESULT) 0L)
    #endif
    #ifndef S_FALSE
        #define S_FALSE           ((HRESULT) 1L)
    #endif
    #ifndef E_FAIL
        #define E_FAIL            ((HRESULT) 0x80004005L)
    #endif
    #ifndef E_OUTOFMEMORY
        #define E_OUTOFMEMORY     ((HRESULT) 0x8007000EL)
    #endif
    #ifndef E_INVALIDARG
        #define E_INVALIDARG      ((HRESULT) 0x80070057L)
    #endif

    #ifndef SUCCEEDED
        #define SUCCEEDED(hr)     (((HRESULT)(hr)) >= 0)
    #endif
    #ifndef FAILED
        #define FAILED(hr)        (((HRESULT)(hr)) < 0)
    #endif

#endif



//
// Character set detection — UNICODE is set by project CharacterSet=Unicode
//

#if defined(UNICODE) && defined(_WINDOWS_)
    #define WIDEN2(x)       L ## x
    #define WIDEN(x)        WIDEN2(x)
    #define __WFUNCTION__   WIDEN(__FUNCTION__)
    #define __WFILE__       WIDEN(__FILE__)

    void DEBUGMSG   (LPCWSTR pszFormat, ...);
    void RELEASEMSG (LPCWSTR pszFormat, ...);
#elif defined(UNICODE)
    #define WIDEN2(x)       L ## x
    #define WIDEN(x)        WIDEN2(x)
    #define __WFUNCTION__   WIDEN(__FUNCTION__)
    #define __WFILE__       WIDEN(__FILE__)

    void DEBUGMSG   (const wchar_t * pszFormat, ...);
    void RELEASEMSG (const wchar_t * pszFormat, ...);
#else
    void DEBUGMSG   (const char * pszFormat, ...);
    void RELEASEMSG (const char * pszFormat, ...);
#endif



typedef void (*EHM_BREAKPOINT_FUNC)(void);

extern EHM_BREAKPOINT_FUNC g_pfnBreakpoint;

void SetBreakpointFunction (EHM_BREAKPOINT_FUNC func);
void EhmBreakpoint         (void);



#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
    #define EHM_BREAKPOINT EhmBreakpoint()
#else
    #define EHM_BREAKPOINT
#endif



#ifdef UNICODE
    #define ASSERT(__condition)                                             \
        if (!(__condition))                                                 \
        {                                                                   \
            DEBUGMSG ((L"%s(%d) - %s - Assertion failed:  %s\n"),           \
                        __WFILE__, __LINE__, __WFUNCTION__, L#__condition); \
            EHM_BREAKPOINT;                                                 \
        }
#else
    #define ASSERT(__condition)                                             \
        if (!(__condition))                                                 \
        {                                                                   \
            DEBUGMSG ("%s(%d) - %s - Assertion failed:  %s\n",              \
                       __FILE__, __LINE__, __FUNCTION__, #__condition);     \
            EHM_BREAKPOINT;                                                 \
        }
#endif



//
// Core base implementation — all EHM macros route through this
//
// fFailed:    condition already evaluated to true = failure
// fAssert:    if true, ASSERT on failure (debug builds only)
// hrDefault:  HRESULT to set if fReplaceHr is false
// fReplaceHr: if true, use hrReplace instead of hrDefault
// hrReplace:  replacement HRESULT (only used when fReplaceHr is true)
// onFailure:  expression to execute on failure (e.g., set error string)
//

#define __EHM_NO_ACTION     ((void) 0)

#define __EHM_Base(__fFailed, __fAssert, __hrDefault, __fReplaceHr, __hrReplace, __onFailure) \
{                                                                                           \
    if (__fFailed)                                                                          \
    {                                                                                       \
        if (__fAssert)                                                                      \
        {                                                                                   \
            ASSERT (false);                                                                 \
        }                                                                                   \
                                                                                            \
        __onFailure;                                                                        \
        hr = (__fReplaceHr) ? (__hrReplace) : (__hrDefault);                                \
        goto ErrorLabel;                                                                    \
    }                                                                                       \
}



//
// Family wrappers — each computes its condition and default hr,
// then delegates to __EHM_Base
//

// CHR — check HRESULT.  Captures hrTest in a temp to avoid
// double evaluation and to preserve the original HRESULT.
#define __CHR(__hrTest, __fAssert, __fReplaceHr, __hrReplace, __onFailure)                  \
{                                                                                           \
    HRESULT __hr = (__hrTest);                                                              \
    __EHM_Base (FAILED (__hr), __fAssert, __hr, __fReplaceHr, __hrReplace, __onFailure)     \
}

// CBR — check bool.  Default hr is E_FAIL.
#define __CBR(__brTest, __fAssert, __fReplaceHr, __hrReplace, __onFailure)                  \
    __EHM_Base (!(__brTest), __fAssert, E_FAIL, __fReplaceHr, __hrReplace, __onFailure)

// CWR — check Win32 BOOL.  Captures GetLastError() immediately
// before onFailure can overwrite it.
#ifdef _WINDOWS_
    #define __CWR_DEFAULT_HR    HRESULT_FROM_WIN32 (::GetLastError())
#else
    #define __CWR_DEFAULT_HR    E_FAIL
#endif

#define __CWR(__fSuccess, __fAssert, __fReplaceHr, __hrReplace, __onFailure)                \
{                                                                                           \
    if (!(__fSuccess))                                                                      \
    {                                                                                       \
        HRESULT __cwrHr = __CWR_DEFAULT_HR;                                                 \
        __EHM_Base (true, __fAssert, __cwrHr, __fReplaceHr, __hrReplace, __onFailure)       \
    }                                                                                       \
}

// CPR — check pointer for null.  Default hr is E_OUTOFMEMORY.
#define __CPR(__prTest, __fAssert, __fReplaceHr, __hrReplace, __onFailure)                  \
    __EHM_Base ((__prTest) == nullptr, __fAssert, E_OUTOFMEMORY, __fReplaceHr, __hrReplace, __onFailure)



//
// IGNORE_RETURN_VALUE
//

#define IGNORE_RETURN_VALUE(__result, __new_value)                                          \
{                                                                                           \
    __result = __new_value;                                                                 \
}



//
// CHR variants — check HRESULT
//
// Suffix key:  A = assert, Ex = replace hr, F = failure action, N = notify user
//

#define CHR(__hrTest)                   __CHR (__hrTest, __EHM_NO_ASSERT, false, 0, __EHM_NO_ACTION)
#define CHRA(__hrTest)                  __CHR (__hrTest, __EHM_ASSERT,    false, 0, __EHM_NO_ACTION)
#define CHREx(__hrTest, __hrReplace)    __CHR (__hrTest, __EHM_NO_ASSERT, true,  __hrReplace, __EHM_NO_ACTION)
#define CHRAEx(__hrTest, __hrReplace)   __CHR (__hrTest, __EHM_ASSERT,    true,  __hrReplace, __EHM_NO_ACTION)
#define CHRF(__hrTest, __onFailure)     __CHR (__hrTest, __EHM_NO_ASSERT, false, 0, __onFailure)
#define CHRAF(__hrTest, __onFailure)    __CHR (__hrTest, __EHM_ASSERT,    false, 0, __onFailure)
#define CHRN(__hrTest, __msg)           __CHR (__hrTest, __EHM_NO_ASSERT, false, 0, EhmNotifyUser (__msg))



//
// CWR variants — check Win32 BOOL (SetLastError-based)
//

#define CWR(__fSuccess)                 __CWR (__fSuccess, __EHM_NO_ASSERT, false, 0, __EHM_NO_ACTION)
#define CWRA(__fSuccess)                __CWR (__fSuccess, __EHM_ASSERT,    false, 0, __EHM_NO_ACTION)
#define CWREx(__fSuccess, __hrReplace)  __CWR (__fSuccess, __EHM_NO_ASSERT, true,  __hrReplace, __EHM_NO_ACTION)
#define CWRAEx(__fSuccess, __hrReplace) __CWR (__fSuccess, __EHM_ASSERT,    true,  __hrReplace, __EHM_NO_ACTION)
#define CWRF(__fSuccess, __onFailure)   __CWR (__fSuccess, __EHM_NO_ASSERT, false, 0, __onFailure)
#define CWRAF(__fSuccess, __onFailure)  __CWR (__fSuccess, __EHM_ASSERT,    false, 0, __onFailure)



//
// CBR variants — check bool condition
//

#define CBR(__brTest)                   __CBR (__brTest, __EHM_NO_ASSERT, false, 0, __EHM_NO_ACTION)
#define CBRA(__brTest)                  __CBR (__brTest, __EHM_ASSERT,    false, 0, __EHM_NO_ACTION)
#define CBREx(__brTest, __hrReplace)    __CBR (__brTest, __EHM_NO_ASSERT, true,  __hrReplace, __EHM_NO_ACTION)
#define CBRAEx(__brTest, __hrReplace)   __CBR (__brTest, __EHM_ASSERT,    true,  __hrReplace, __EHM_NO_ACTION)
#define CBRF(__brTest, __onFailure)     __CBR (__brTest, __EHM_NO_ASSERT, false, 0, __onFailure)
#define CBRAF(__brTest, __onFailure)    __CBR (__brTest, __EHM_ASSERT,    false, 0, __onFailure)
#define CBRN(__brTest, __msg)           __CBR (__brTest, __EHM_NO_ASSERT, false, 0, EhmNotifyUser (__msg))

#define BAIL_OUT_IF(__boolTest, __hrReplace) \
    __CBR (!(__boolTest), __EHM_NO_ASSERT, true, __hrReplace, __EHM_NO_ACTION)



//
// CPR variants — check pointer for null
//

#define CPR(__prTest)                   __CPR (__prTest, __EHM_NO_ASSERT, false, 0, __EHM_NO_ACTION)
#define CPRA(__prTest)                  __CPR (__prTest, __EHM_ASSERT,    false, 0, __EHM_NO_ACTION)
#define CPREx(__prTest, __hrReplace)    __CPR (__prTest, __EHM_NO_ASSERT, true,  __hrReplace, __EHM_NO_ACTION)
#define CPRAEx(__prTest, __hrReplace)   __CPR (__prTest, __EHM_ASSERT,    true,  __hrReplace, __EHM_NO_ACTION)
#define CPRF(__prTest, __onFailure)     __CPR (__prTest, __EHM_NO_ASSERT, false, 0, __onFailure)
#define CPRAF(__prTest, __onFailure)    __CPR (__prTest, __EHM_ASSERT,    false, 0, __onFailure)



//
// User notification — auto-detects GUI vs console at runtime
//

#ifdef UNICODE
    typedef void (*EHM_NOTIFY_FUNC)(const wchar_t * message);
#else
    typedef void (*EHM_NOTIFY_FUNC)(const char * message);
#endif

extern EHM_NOTIFY_FUNC g_pfnNotify;

void SetNotifyFunction (EHM_NOTIFY_FUNC func);

#ifdef UNICODE
    void EhmNotifyUser (const wchar_t * message);
#else
    void EhmNotifyUser (const char * message);
#endif



//
// Legacy aliases — CBR_SetError/CHR_SetError map to the new F variants
//

#define CBR_SetError(__brTest, __onFailure)  CBRF (__brTest, __onFailure)
#define CHR_SetError(__hrTest, __onFailure)  CHRF (__hrTest, __onFailure)
