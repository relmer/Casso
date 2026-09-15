#pragma once

#include "Debugger/Reply.h"

class IRunObserver;





////////////////////////////////////////////////////////////////////////////////
//
//  IRunDriver
//
//  How a run executes. A synchronous driver finishes the run inside Start; the
//  emulator's driver starts the CPU thread and delivers the stop later.
//
////////////////////////////////////////////////////////////////////////////////

class IRunDriver
{
public:
    virtual ~IRunDriver() = default;

    virtual void     SetRunObserver (IRunObserver * observer) = 0;
    virtual HRESULT  Start          (const RunRequest & request) = 0;
    virtual void     Pause          () = 0;
};
