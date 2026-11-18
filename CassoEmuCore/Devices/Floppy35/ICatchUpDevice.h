#pragma once

#include "Pch.h"

enum class CatchUpKind;





////////////////////////////////////////////////////////////////////////////////
//
//  ICatchUpDevice
//
//  A device with a separate processor and clock, which runs up to the Apple's
//  time when the Apple accesses it and, between accesses, at fixed points of
//  the Apple's cycle count (MachineRunner::CatchUpCards), so its firmware's
//  timers run while the guest doesn't access it.
//
////////////////////////////////////////////////////////////////////////////////

class ICatchUpDevice
{
public:
    virtual ~ICatchUpDevice() = default;

    virtual void  CatchUp (CatchUpKind kind) = 0;
};
