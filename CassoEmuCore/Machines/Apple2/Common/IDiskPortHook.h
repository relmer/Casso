#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskPortHook
//
//  A device on the //c's disk port that uses the IWM's switches along with
//  the built-in drive: a SmartPort chain. Every access to the IWM goes
//  to the hook after the controller. The hook returns true, with the byte a
//  read returns, when the access is the port's.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskPortHook
{
public:
    virtual ~IDiskPortHook() = default;

    virtual bool  OnIwmAccess (int offset, bool isWrite, Byte value, Byte & outValue) = 0;
};
