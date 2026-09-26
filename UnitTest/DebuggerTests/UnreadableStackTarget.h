#pragma once

#include "MockDebugTarget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UnreadableStackTarget
//
//  A mock machine whose stack page cannot be peeked, so a command that reads
//  the stack has nothing to read.
//
////////////////////////////////////////////////////////////////////////////////

class UnreadableStackTarget : public MockDebugTarget
{
public:
    static constexpr Word  kStackFirst = 0x0100;
    static constexpr Word  kStackLast  = 0x01FF;

    bool TryPeek (Word address, Byte & value) const override
    {
        if (address >= kStackFirst && address <= kStackLast)
        {
            return false;
        }

        return MockDebugTarget::TryPeek (address, value);
    }
};
