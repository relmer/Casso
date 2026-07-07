#pragma once

#include "Pch.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiThread
//
//  UI-thread guard for the Dxui framework. Every public Dxui API must
//  be called on the host window message-pump thread. The first call to
//  DxuiAssertUiThread() captures the calling thread's ID; subsequent
//  calls assert that they are made on the same thread.
//
//  Debug builds only -- release builds compile to a no-op via the
//  DXUI_ASSERT_UI_THREAD() macro (defined in Dxui.h umbrella).
//
//  Test seam: DxuiResetUiThreadIdForTest() forgets the captured ID so
//  unit tests in arbitrary threads can exercise public APIs without
//  the framework asserting against the first test's thread.
//
////////////////////////////////////////////////////////////////////////////////

void  DxuiAssertUiThread          ();
void  DxuiResetUiThreadIdForTest  ();


#if defined(_DEBUG)
    #define DXUI_ASSERT_UI_THREAD() DxuiAssertUiThread()
#else
    #define DXUI_ASSERT_UI_THREAD() ((void) 0)
#endif
