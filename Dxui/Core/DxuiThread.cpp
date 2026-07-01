#include "Pch.h"

#include "Core/DxuiThread.h"





static std::atomic<DWORD>  s_uiThreadId { 0 };





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAssertUiThread
//
//  Captures the first caller's thread ID, then asserts every subsequent
//  call comes from the same thread. Lock-free via a single atomic CAS.
//  Compiled into both debug and release; the macro DXUI_ASSERT_UI_THREAD
//  in the umbrella header is the gate that elides the call in release.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAssertUiThread()
{
    HRESULT  hr       = S_OK;
    DWORD    captured = 0;
    DWORD    current  = 0;


    current  = GetCurrentThreadId();
    captured = s_uiThreadId.load (std::memory_order_acquire);

    if (captured == 0)
    {
        s_uiThreadId.compare_exchange_strong (captured,
                                              current,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire);
    }
    else
    {
        CBRA (captured == current);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiResetUiThreadIdForTest
//
//  Test-only seam. Clears the captured thread ID so a subsequent
//  DxuiAssertUiThread() call rebinds to the next caller's thread.
//  Production code must never call this.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiResetUiThreadIdForTest()
{
    s_uiThreadId.store (0, std::memory_order_release);
}
