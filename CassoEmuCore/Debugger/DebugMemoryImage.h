#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugMemoryImage
//
//  The 64K the CPU sees, as the debugger reads it without side effects, taken
//  at one moment: each byte a peek would read, whether a peek reads it at
//  all (I/O is not read), and each 256-byte page's region.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugMemoryImage
{
    static constexpr size_t  kBytes     = 0x10000;
    static constexpr size_t  kPageBytes = 0x100;
    static constexpr size_t  kPages     = kBytes / kPageBytes;

    std::array<Byte, kBytes>          bytes    = {};
    std::bitset<kBytes>               readable;
    std::array<MemoryRegion, kPages>  regions  = {};
};
