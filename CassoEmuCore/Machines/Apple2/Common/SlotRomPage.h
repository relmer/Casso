#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"

class ExpansionRomBus;
class IExpansionRomCard;





////////////////////////////////////////////////////////////////////////////////
//
//  SlotRomPage
//
//  The $Cs00-$CsFF page of a card that implements IExpansionRomCard, as a
//  memory device. On the ][ and ][+ it is a bus device; on the //e the
//  CxxxRomRouter delegates the slot's page to it, so an access is passed to
//  the card only when the //e's INTCXROM and SLOTC3ROM allow it.
//
////////////////////////////////////////////////////////////////////////////////

class SlotRomPage : public MemoryDevice
{
public:
    SlotRomPage (int slot, IExpansionRomCard & card);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return m_start; }
    Word GetEnd   () const override { return m_end; }
    void Reset    () override {}

    // The shared expansion space, which gives $C800-$CFFF to this card when
    // its page is accessed. Null leaves the other cards' latches alone.
    void SetExpansionBus (ExpansionRomBus * bus) { m_expansionBus = bus; }

private:
    IExpansionRomCard  & m_card;
    ExpansionRomBus    * m_expansionBus = nullptr;
    Word                 m_start        = 0;
    Word                 m_end          = 0;
};
