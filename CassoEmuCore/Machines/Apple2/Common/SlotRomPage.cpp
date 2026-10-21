#include "Pch.h"

#include "Machines/Apple2/Common/SlotRomPage.h"
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
    return m_card.OnSlotPageAccess (address, false, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  A write selects the card just as a read does: the latch is set by the
//  address decode, not by the direction.
//
////////////////////////////////////////////////////////////////////////////////

void SlotRomPage::Write (Word address, Byte value)
{
    (void) m_card.OnSlotPageAccess (address, true, value);
}
