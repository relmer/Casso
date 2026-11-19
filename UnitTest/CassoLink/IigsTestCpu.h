#pragma once

#include "Pch.h"

#include "../TestHelpers.h"
#include "FakeScc8530.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IigsTestCpu
//
//  A 6502 with flat memory and what the Apple side checks for a IIgs's
//  ports: the fake 8530 at $C036 to $C03B, and an empty $C080 to $C0FF, so
//  no slot has a 6551 or a network chip. SetUpIigs adds the enhanced //e's
//  identification bytes and, for a IIgs, an ID call at $FE1F that clears
//  the C flag; for an enhanced //e it is an RTS. Slots 1 and 2 have a port's
//  firmware: the Pascal signature and the Super Serial Card's ID byte, with
//  each entry an RTS.
//
//  $FE20, the monitor's LT, is overwritten by the ID call's RTS. Nothing on
//  the Apple side calls it.
//
////////////////////////////////////////////////////////////////////////////////

class IigsTestCpu : public TestCpu
{
public:
    FakeScc8530  scc;

    Byte  ReadByteSlow (Word address) override
    {
        Byte  value = 0;



        if (address >= scc.GetStart() && address <= scc.GetEnd())
        {
            value = scc.Read (address);
        }
        else if (address < 0xC080 || address > 0xC0FF)
        {
            value = memory[address];
        }

        return value;
    }

    void  WriteByte (Word address, Byte value) override
    {
        if (address >= scc.GetStart() && address <= scc.GetEnd())
        {
            scc.Write (address, value);
        }
        else if (address < 0xC080 || address > 0xC0FF)
        {
            memory[address] = value;
        }
    }

    void  SetUpIigs (bool iigs = true)
    {
        Poke (0xFBB3, 0x06);
        Poke (0xFBC0, 0xE0);
        Poke (0xFE1F, iigs ? 0x18 : 0x60);          // CLC, or a //e's RTS
        Poke (0xFE20, 0x60);

        for (Word card : { (Word) 0xC100, (Word) 0xC200 })
        {
            Poke ((Word) (card + 0x05), 0x38);
            Poke ((Word) (card + 0x07), 0x18);
            Poke ((Word) (card + 0x0B), 0x01);
            Poke ((Word) (card + 0x0C), 0x31);

            for (Word entry = 0x0D; entry <= 0x10; entry++)
            {
                Poke ((Word) (card + entry), 0x40);
            }

            Poke ((Word) (card + 0x40), 0x60);
        }
    }

    //  Steps until `done` is true, at most `limit` instructions; false at the
    //  limit.
    bool  StepUntil (const std::function<bool()> & done, uint32_t limit)
    {
        uint32_t  steps = 0;



        while (!done() && steps < limit)
        {
            Step();
            steps++;
        }

        return done();
    }
};
