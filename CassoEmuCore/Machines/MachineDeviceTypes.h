#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDeviceTypes
//
//  The device-type tokens a machine's internal-device list is written in.
//
//  A machine class declares what it has by these names, the component
//  registry builds a device from them, and the settings panel captions them.
//  Most of the build reads a token it does not have to recognize -- it hands
//  it to the registry and takes back whatever comes out.
//
//  The exception is the MMU, which the build DOES have to recognize: it is a
//  coordinator that owns the auxiliary 64 KiB and rebinds the page table,
//  not a device on the bus, so it is constructed directly rather than
//  through the registry. That is one place where a machine's declaration and
//  the code reading it have to agree on a spelling, and a literal on each
//  side is two places for one word.
//
//  `-family-` reads as "debuted on this model and carried forward":
//  apple2e-family-mmu is the MMU the //e introduced and the //c inherited.
//
////////////////////////////////////////////////////////////////////////////////

class MachineDeviceTypes
{
public:

    static constexpr const char *  kMmu = "apple2e-family-mmu";
};
