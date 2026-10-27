#include "Pch.h"

#include "Machines/Apple2/Common/ExpansionRomBus.h"
#include "Machines/Apple2/Common/IExpansionRomCard.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExpansionRomBus
//
////////////////////////////////////////////////////////////////////////////////

ExpansionRomBus::ExpansionRomBus (MemoryBus & bus)
    : m_bus (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte ExpansionRomBus::Read (Word address)
{
    constexpr Byte  kAllBitsHigh = 0xFF;
    Byte            result       = kAllBitsHigh;
    bool            anyDrive     = false;



    if (address >= kDeselectStart)
    {
        DeselectAll();
    }

    for (IExpansionRomCard * card : m_cards)
    {
        if (card->IsExpansionRomSelected())
        {
            result   = static_cast<Byte> (result & card->ReadExpansionRom (address));
            anyDrive = true;
        }
    }

    return anyDrive ? result : m_bus.GetFloatingBusValue();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
//  Read's byte with no card deselected: the selected cards' ROM bytes ANDed,
//  or the floating bus for $CF00-$CFFF and when no card is selected.
//
////////////////////////////////////////////////////////////////////////////////

bool ExpansionRomBus::TryPeekIo (Word address, Byte & value)
{
    constexpr Byte  kAllBitsHigh = 0xFF;
    Byte            result       = kAllBitsHigh;
    bool            anyDrive     = false;



    for (IExpansionRomCard * card : m_cards)
    {
        if (address < kDeselectStart && card->IsExpansionRomSelected())
        {
            result   = static_cast<Byte> (result & card->ReadExpansionRom (address));
            anyDrive = true;
        }
    }

    value = anyDrive ? result : m_bus.GetFloatingBusValue();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  The expansion ROM ignores writes; a write to $CF00-$CFFF deselects every
//  card just as a read does.
//
////////////////////////////////////////////////////////////////////////////////

void ExpansionRomBus::Write (Word address, Byte value)
{
    (void) value;

    if (address >= kDeselectStart)
    {
        DeselectAll();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  The cards' latches are reset by the Apple's RESET line; each card resets
//  its own. The bus has no state of its own.
//
////////////////////////////////////////////////////////////////////////////////

void ExpansionRomBus::Reset()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddCard
//
////////////////////////////////////////////////////////////////////////////////

void ExpansionRomBus::AddCard (IExpansionRomCard * card)
{
    if (card != nullptr)
    {
        m_cards.push_back (card);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSelectedSlot
//
////////////////////////////////////////////////////////////////////////////////

int ExpansionRomBus::GetSelectedSlot() const
{
    for (const IExpansionRomCard * card : m_cards)
    {
        if (card->IsExpansionRomSelected())
        {
            return card->GetSlot();
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeselectAll
//
////////////////////////////////////////////////////////////////////////////////

void ExpansionRomBus::DeselectAll()
{
    for (IExpansionRomCard * card : m_cards)
    {
        card->DeselectExpansionRom();
    }
}
