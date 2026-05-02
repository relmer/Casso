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
// Core helper macros
//

#define __CHRAExHelper(__arg_hrTest, __arg_fAssert, __arg_fReplaceHr, __arg_hrReplaceHr)    \
{                                                                                           \
    HRESULT __hr = __arg_hrTest;                                                            \
                                                                                            \
    if (FAILED (__hr))                                                                      \
    {                                                                                       \
        if (__arg_fAssert)                                                                  \
        {                                                                                   \
            ASSERT (false);                                                                 \
        }                                                                                   \
                                                                                            \
        if (__arg_fReplaceHr)                                                               \
        {                                                                                   \
            hr = __arg_hrReplaceHr;                                                         \
        }                                                                                   \
        else                                                                                \
        {                                                                                   \
            hr = __hr;                                                                      \
        }                                                                                   \
                                                                                            \
        goto ErrorLabel;                                                                    \
    }                                                                                       \
}

#define __CBRAExHelper(__arg_brTest, __arg_fAssert, __arg_hrReplaceHr)                      \
{                                                                                           \
    if (!(__arg_brTest))                                                                    \
    {                                                                                       \
        if (__arg_fAssert)                                                                  \
        {                                                                                   \
            ASSERT (false);                                                                 \
        }                                                                                   \
                                                                                            \
        hr = __arg_hrReplaceHr;                                                             \
                                                                                            \
        goto ErrorLabel;                                                                    \
    }                                                                                       \
}

#define __CPRAExHelper(__arg_prTest, __arg_fAssert, __arg_hrReplaceHr)                      \
{                                                                                           \
    if (__arg_prTest == nullptr)                                                            \
    {                                                                                       \
        if (__arg_fAssert)                                                                  \
        {                                                                                   \
            ASSERT (false);                                                                 \
        }                                                                                   \
                                                                                            \
        hr = __arg_hrReplaceHr;                                                             \
                                                                                            \
        goto ErrorLabel;                                                                    \
    }                                                                                       \
}



//
// CWR variants — Windows APIs that SetLastError
//

#ifdef _WINDOWS_

    #define __CWRAExHelper(__arg_fSuccess, __arg_fAssert, __arg_fReplaceHr, __arg_hrReplaceHr)  \
    {                                                                                           \
        if (!(__arg_fSuccess))                                                                  \
        {                                                                                       \
            if (__arg_fAssert)                                                                  \
            {                                                                                   \
                ASSERT (false);                                                                 \
            }                                                                                   \
                                                                                                \
            if (__arg_fReplaceHr)                                                               \
            {                                                                                   \
                hr = __arg_hrReplaceHr;                                                         \
            }                                                                                   \
            else                                                                                \
            {                                                                                   \
                hr = HRESULT_FROM_WIN32 (::GetLastError());                                     \
            }                                                                                   \
                                                                                                \
            goto ErrorLabel;                                                                    \
        }                                                                                       \
    }

#else

    #define __CWRAExHelper(__arg_fSuccess, __arg_fAssert, __arg_fReplaceHr, __arg_hrReplaceHr)  \
    {                                                                                           \
        if (!(__arg_fSuccess))                                                                  \
        {                                                                                       \
            if (__arg_fAssert)                                                                  \
            {                                                                                   \
                ASSERT (false);                                                                 \
            }                                                                                   \
                                                                                                \
            if (__arg_fReplaceHr)                                                               \
            {                                                                                   \
                hr = __arg_hrReplaceHr;                                                         \
            }                                                                                   \
            else                                                                                \
            {                                                                                   \
                hr = E_FAIL;                                                                    \
            }                                                                                   \
                                                                                                \
            goto ErrorLabel;                                                                    \
        }                                                                                       \
    }

#endif



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

#define CHRAEx(__arg_hrTest, __arg_hrReplaceHr)                                             \
{                                                                                           \
    __CHRAExHelper (__arg_hrTest, __EHM_ASSERT, true, __arg_hrReplaceHr)                    \
}

#define CHREx(__arg_hrTest, __arg_hrReplaceHr)                                              \
{                                                                                           \
    __CHRAExHelper (__arg_hrTest, __EHM_NO_ASSERT, true, __arg_hrReplaceHr)                 \
}

#define CHRA(__arg_hrTest)                                                                  \
{                                                                                           \
    __CHRAExHelper (__arg_hrTest, __EHM_ASSERT, false, 0)                                   \
}

#define CHR(__arg_hrTest)                                                                   \
{                                                                                           \
    __CHRAExHelper (__arg_hrTest, __EHM_NO_ASSERT, false, 0)                                \
}



//
// CWR variants — check Win32 BOOL (SetLastError-based)
//

#define CWRAEx(__arg_fSuccess, __arg_hrReplaceHr)                                           \
{                                                                                           \
    __CWRAExHelper (__arg_fSuccess, __EHM_ASSERT, true, __arg_hrReplaceHr)                  \
}

#define CWREx(__arg_fSuccess, __arg_hrReplaceHr)                                            \
{                                                                                           \
    __CWRAExHelper (__arg_fSuccess, __EHM_NO_ASSERT, true, __arg_hrReplaceHr)               \
}

#define CWRA(__arg_fSuccess)                                                                \
{                                                                                           \
    __CWRAExHelper (__arg_fSuccess, __EHM_ASSERT, false, 0)                                 \
}

#define CWR(__arg_fSuccess)                                                                 \
{                                                                                           \
    __CWRAExHelper (__arg_fSuccess, __EHM_NO_ASSERT, false, 0)                              \
}



//
// CBR variants — check bool condition
//

#define CBRAEx(__arg_brTest, __arg_hrReplaceHr)                                             \
{                                                                                           \
    __CBRAExHelper (__arg_brTest, __EHM_ASSERT, __arg_hrReplaceHr)                          \
}

#define CBREx(__arg_brTest, __arg_hrReplaceHr)                                              \
{                                                                                           \
    __CBRAExHelper (__arg_brTest, __EHM_NO_ASSERT, __arg_hrReplaceHr)                       \
}

#define CBRA(__arg_brTest)                                                                  \
{                                                                                           \
    __CBRAExHelper (__arg_brTest, __EHM_ASSERT, E_FAIL)                                     \
}

#define CBR(__arg_brTest)                                                                   \
{                                                                                           \
    __CBRAExHelper (__arg_brTest, __EHM_NO_ASSERT, E_FAIL)                                  \
}

#define BAIL_OUT_IF(__arg_boolTest, __arg_hrReplaceHr)                                      \
{                                                                                           \
    __CBRAExHelper (!(__arg_boolTest), __EHM_NO_ASSERT, __arg_hrReplaceHr)                  \
}

#define CBR_SetError(__arg_brTest, __arg_onFailure)                                         \
{                                                                                           \
    if (!(__arg_brTest))                                                                    \
    {                                                                                       \
        __arg_onFailure;                                                                    \
        hr = E_FAIL;                                                                        \
        goto ErrorLabel;                                                                    \
    }                                                                                       \
}

#define CHR_SetError(__arg_hrTest, __arg_onFailure)                                          \
{                                                                                           \
    hr = (__arg_hrTest);                                                                    \
    if (FAILED (hr))                                                                        \
    {                                                                                       \
        __arg_onFailure;                                                                    \
        goto ErrorLabel;                                                                    \
    }                                                                                       \
}



//
// CPR variants — check pointer for null
//

#define CPRAEx(__arg_prTest, __arg_hrReplaceHr)                                             \
{                                                                                           \
    __CPRAExHelper (__arg_prTest, true, __arg_hrReplaceHr)                                  \
}

#define CPREx(__arg_prTest, __arg_hrReplaceHr)                                              \
{                                                                                           \
    __CPRAExHelper (__arg_prTest, false, __arg_hrReplaceHr)                                 \
}

#define CPRA(__arg_prTest)                                                                  \
{                                                                                           \
    __CPRAExHelper (__arg_prTest, true, E_OUTOFMEMORY)                                      \
}

#define CPR(__arg_prTest)                                                                   \
{                                                                                           \
    __CPRAExHelper (__arg_prTest, false, E_OUTOFMEMORY)                                     \
}



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
// CHRN / CBRN — check result and Notify user on failure
//
// These macros check an HRESULT or bool, and on failure show a user-facing
// notification (MessageBox in GUI apps, stderr in console apps) before
// jumping to the Error label.  The message expression is only evaluated
// on the failure path.
//

#define CHRN(__arg_hrTest, __arg_msg)                                                       \
{                                                                                           \
    HRESULT __hr = __arg_hrTest;                                                            \
                                                                                            \
    if (FAILED (__hr))                                                                      \
    {                                                                                       \
        EhmNotifyUser (__arg_msg);                                                          \
        hr = __hr;                                                                          \
        goto ErrorLabel;                                                                    \
    }                                                                                       \
}

#define CBRN(__arg_brTest, __arg_msg)                                                       \
{                                                                                           \
    if (!(__arg_brTest))                                                                    \
    {                                                                                       \
        EhmNotifyUser (__arg_msg);                                                          \
        hr = E_FAIL;                                                                        \
        goto ErrorLabel;                                                                    \
    }                                                                                       \
}
