#pragma once

#include "Pch.h"

#include "ICpu.h"

class MemoryBus;




////////////////////////////////////////////////////////////////////////////////
//
//  CpuFactory
//
//  Maps a machine profile's `cpu` string onto a concrete ICpu strategy bound
//  to the emulator MemoryBus. "6502" builds the NMOS MemoryBusCpu; "65C02"
//  builds the CMOS core once it exists. Unknown types are rejected so a
//  malformed profile fails loudly rather than silently running the wrong part.
//
////////////////////////////////////////////////////////////////////////////////

class CpuFactory
{
public:
    static HRESULT Create (const string & cpuType, MemoryBus & bus, unique_ptr<ICpu> & outCpu);
};
