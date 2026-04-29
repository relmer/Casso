#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryDevice
//
//  Abstract interface for all bus-attached components.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryDevice
{
public:
    virtual ~MemoryDevice () = default;

    virtual Byte Read     (Word address)              = 0;
    virtual void Write    (Word address, Byte value)  = 0;
    virtual Word GetStart () const                    = 0;
    virtual Word GetEnd   () const                    = 0;
    virtual void Reset    ()                          = 0;
};
