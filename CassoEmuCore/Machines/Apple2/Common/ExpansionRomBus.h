#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"

class IExpansionRomCard;
class MemoryBus;





////////////////////////////////////////////////////////////////////////////////
//
//  ExpansionRomBus
//
//  The $C800-$CFFF expansion space shared by every slot card, as Apple's
//  protocol arbitrates it. A read of $C800-$CEFF returns the byte of the card
//  whose expansion ROM is selected, or the floating bus when none is. Any
//  access to $CF00-$CFFF deselects every card and reads the floating bus.
//
//  Two cards selected at once is a program error the protocol exists to
//  prevent; the bytes are ANDed, approximating two TTL drivers on one line,
//  where the one driving low generally pulls it low.
//
//  On the ][ and ][+ this is a bus device. On the //e the CxxxRomRouter calls
//  Read and Write only when INTCXROM and INTC8ROM are both reset, which is
//  when the motherboard asserts I/O STROBE'.
//
////////////////////////////////////////////////////////////////////////////////

class ExpansionRomBus : public MemoryDevice
{
public:
    static constexpr Word  kStart          = 0xC800;
    static constexpr Word  kDeselectStart  = 0xCF00;
    static constexpr Word  kEnd            = 0xCFFF;

    explicit ExpansionRomBus (MemoryBus & bus);

    Byte Read     (Word address) override;
    bool TryPeekIo (Word address, Byte & value) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return kStart; }
    Word GetEnd   () const override { return kEnd; }
    void Reset    () override;

    // Caller-owned; registered for the life of the machine.
    void AddCard (IExpansionRomCard * card);

private:
    void DeselectAll ();

    MemoryBus &                    m_bus;
    vector<IExpansionRomCard *>    m_cards;
};
