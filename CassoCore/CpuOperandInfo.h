#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuOperandInfo
//
//  An instruction's operand as the CPU fetched it: where it is, the address
//  it resolves to, and the value there.
//
////////////////////////////////////////////////////////////////////////////////

struct CpuOperandInfo
{
    Word  location;
    Word  effectiveAddress;
    Word  operand;
};
