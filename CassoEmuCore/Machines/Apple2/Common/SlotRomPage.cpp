#include "Pch.h"

#include "Machines/Apple2/Common/SlotRomPage.h"
#include "Machines/Apple2/Common/ExpansionRomBus.h"
#include "Machines/Apple2/Common/IExpansionRomCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SlotRomPage
//
////////////////////////////////////////////////////////////////////////////////

SlotRomPage::SlotRomPage (int slot, IExpansionRomCard & card)
    : m_card (card)
{
    constexpr Word  kSlotPageBase = 0xC000;
    constexpr Word  kPageSize     = 0x0100;



    m_start = static_cast<Word> (kSlotPageBase + slot * kPageSize);
    m_end   = static_cast<Word> (m_start + kPageSize - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte SlotRomPage::Read (Word address)
{
    if (m_expansionBus != nullptr)
    {
        m_expansionBus->DeselectAllBut (&m_card);
    }

    return m_card.OnSlotPageAccess (address, false, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  A write selects the card just as a read does: the latch is set by the
//  address decode, not by the direction. With the expansion bus set, the
//  card whose page is accessed has the expansion space to itself, as MAME
//  gives it, so a card left selected cannot drive $C800 over the next one.
//  On the //e an access to $CFFF while INTC8ROM is set does not reach the
//  cards, so the deselect a card's firmware does on entry can be lost.
//
////////////////////////////////////////////////////////////////////////////////

void SlotRomPage::Write (Word address, Byte value)
{
    if (m_expansionBus != nullptr)
    {
        m_expansionBus->DeselectAllBut (&m_card);
    }

    (void) m_card.OnSlotPageAccess (address, true, value);
}
