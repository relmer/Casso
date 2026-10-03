#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IWorkQueue
//
//  Runs work handed to it away from the caller, one item at a time, in the
//  order submitted. An item is a function and a context pointer the caller
//  keeps alive until the item has run, so submitting allocates nothing.
//  WaitAll returns once every item submitted so far has run.
//
//  ThreadPoolWorkQueue runs items on the Windows thread pool; a test can
//  supply its own queue to run them on its own thread, in a fixed order, at
//  a moment it chooses.
//
////////////////////////////////////////////////////////////////////////////////

class IWorkQueue
{
public:
    using WorkFunction = void (*) (void * context);

    virtual          ~IWorkQueue() = default;

    virtual HRESULT  Submit  (WorkFunction function, void * context) = 0;
    virtual void     WaitAll () = 0;
};
