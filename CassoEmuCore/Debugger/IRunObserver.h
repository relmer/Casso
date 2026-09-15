#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IRunObserver
//
//  Receives the stop that ends a run, or a stop while the machine runs freely,
//  on the thread that ran the machine.
//
////////////////////////////////////////////////////////////////////////////////

class IRunObserver
{
public:
    virtual ~IRunObserver() = default;

    virtual void  OnStopped (const StopEvent & stop) = 0;
};
