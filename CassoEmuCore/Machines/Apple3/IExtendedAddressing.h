#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IExtendedAddressing
//
//  The interface between the CPU core and the memory map for the Apple ///'s
//  enhanced indirect addressing.
//
//  After an indirect instruction reads its pointer, the core passes the zero
//  page offset of the pointer's high byte and the address the pointer
//  produced. When the extension byte beside the pointer enables it, the
//  memory map sends that instruction's data access to the bank the byte
//  selects, and the core ends the redirection once the instruction is done.
//
////////////////////////////////////////////////////////////////////////////////

class IExtendedAddressing
{
public:
    virtual ~IExtendedAddressing() = default;

    virtual bool  TryBeginExtendedAccess (Byte pointerHigh, Word effectiveAddress) = 0;
    virtual void  EndExtendedAccess      ()                                        = 0;
};
