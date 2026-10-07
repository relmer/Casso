#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatSpace
//
//  The address spaces the heat map counts in. Cpu is the 64 KB the CPU
//  addresses, whatever is banked in. The others are where an access really
//  lands, laid out as the //e's DRAM holds them: Main and Aux are 64 KB
//  each, $0000-$BFFF the RAM there, then the language card's bank 1 at
//  $C000-$CFFF, its bank 2 at $D000-$DFFF and its high RAM at $E000-$FFFF.
//  Rom is the ROM a read of $C100-$FFFF reaches, by the address it is read
//  at, a write there that no RAM takes included.
//
////////////////////////////////////////////////////////////////////////////////

enum class HeatSpace
{
    Cpu,
    Main,
    Aux,
    Rom,
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatLocation
//
////////////////////////////////////////////////////////////////////////////////

struct HeatLocation
{
    HeatSpace  space = HeatSpace::Cpu;
    Word       index = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatLastAccess
//
//  The instruction that last wrote or last read an address: its PC, where
//  it stood in the machine's run of instructions and the cycle it began on.
//  A stamp of zero is none.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatLastAccess
{
    static constexpr int       kPositionShift = 16;
    static constexpr uint64_t  kForgotten     = UINT64_MAX;     // dropped with the future it was in

    uint64_t  stamp = 0;                  // (position + 1) << kPositionShift | PC
    uint64_t  cycle = 0;

    static HeatLastAccess  Make (Word pc, uint64_t position, uint64_t cycle) { return { ((position + 1) << kPositionShift) | pc, cycle }; }

    bool      IsNone      () const { return stamp == 0; }
    bool      IsForgotten () const { return stamp == kForgotten; }
    Word      GetPc       () const { return (Word) (stamp & 0xFFFF); }
    uint64_t  GetPosition () const { return (stamp >> kPositionShift) - 1; }

    bool operator== (const HeatLastAccess & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessState
//
//  What the map can say of an address's last access as of where the
//  machine stands: the access, that there was none, or that the access it
//  holds is from later on, so the one before here is not in it.
//
////////////////////////////////////////////////////////////////////////////////

enum class HeatAccessState
{
    Found,
    None,
    Unknown,
};
