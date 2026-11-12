#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LeftoverKind / LeftoverRun
//
//  What a free block has in it, and a run of free blocks side by side with
//  the same kind.
//
////////////////////////////////////////////////////////////////////////////////

enum class LeftoverKind
{
    None,
    Text,
    Code,
};


struct LeftoverRun
{
    uint32_t      first = 0;
    uint32_t      count = 0;
    LeftoverKind  kind  = LeftoverKind::None;

    bool  Contains (uint32_t block) const { return block >= first && block - first < count; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockLeftovers
//
//  Text or 6502 code left in free blocks by files that were deleted. A block
//  is text when nearly every byte that isn't zero is a printable character,
//  a return or a tab, with or without its high bit. A block is code when a
//  linear disassembly from its first byte decodes almost every instruction
//  as a documented one and has the calls, jumps, returns, loads and stores
//  a 6502 program is made of. A block of either kind has at least
//  kMinBytes that aren't zero, so a stray byte or two isn't counted. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class BlockLeftovers
{
public:
    static constexpr size_t  kMinBytes = 64;

    static LeftoverKind  Classify (BlockBytes bytes);

    //  The runs among `kinds`, a block each, in block order.
    static vector<LeftoverRun>  FindRuns (const vector<LeftoverKind> & kinds);

    static LPCWSTR  GetKindName (LeftoverKind kind);

private:
    static bool  IsText (BlockBytes bytes, size_t nonZero);
    static bool  IsCode (BlockBytes bytes);
};
