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
//  On the //c Plus the controller's drives are the 5.25-inch drives at the
//  end of the port's chain, enabled only while the IWM isn't driving the
//  internal drive or the SmartPort bus. Before each access the controller
//  calls AreFloppiesEnabled; while it returns false, the access moves
//  neither a head nor a motor, and the hook returns the byte.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskPortHook
{
public:
    virtual ~IDiskPortHook() = default;

    virtual bool  OnIwmAccess        (int offset, bool isWrite, Byte value, Byte & outValue) = 0;
    virtual bool  AreFloppiesEnabled () { return true; }

    //  What AreFloppiesEnabled returns, without recording it. A false
    //  return makes the controller's peek fail.
    virtual bool  PeekFloppiesEnabled () const { return false; }

    //  What OnIwmAccess returns for a read of `offset`, and the byte it
    //  returns in `outValue` when it returns true, without the access. False
    //  where the hook has no peek.
    virtual bool  TryPeekIwmAccess (int offset, bool & outIsPorts, Byte & outValue) const
    {
        (void) offset;
        outIsPorts = false;
        outValue   = 0;

        return false;
    }
};
