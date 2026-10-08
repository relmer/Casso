#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadName
//
//  A name for the calling thread, which the debugger's Threads window and a
//  crash dump show in place of the thread's entry point.
//
////////////////////////////////////////////////////////////////////////////////

class ThreadName
{
public:
    static HRESULT  SetForCurrentThread (const wchar_t * name);
};
