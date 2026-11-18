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
        DeselectAll (address);
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
    Byte            byte         = 0;



    for (IExpansionRomCard * card : m_cards)
    {
        if (address < kDeselectStart && card->IsExpansionRomSelected())
        {
            if (!card->TryPeekExpansionRom (address, byte))
            {
                byte = card->ReadExpansionRom (address);
            }

            result   = static_cast<Byte> (result & byte);
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
//  A write to $CF00-$CFFF deselects the cards just as a read does, and a
//  write reaches a card that is still selected, for a card with RAM in its
//  window.
//
////////////////////////////////////////////////////////////////////////////////

void ExpansionRomBus::Write (Word address, Byte value)
{
    if (address >= kDeselectStart)
    {
        DeselectAll (address);
    }

    for (IExpansionRomCard * card : m_cards)
    {
        if (card->IsExpansionRomSelected())
        {
            card->WriteExpansionRom (address, value);
        }
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
//  DeselectAllBut
//
////////////////////////////////////////////////////////////////////////////////

void ExpansionRomBus::DeselectAllBut (const IExpansionRomCard * card)
{
    for (IExpansionRomCard * other : m_cards)
    {
        if (other != card)
        {
            other->DeselectExpansionRom();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeselectAll
//
////////////////////////////////////////////////////////////////////////////////

void ExpansionRomBus::DeselectAll (Word address)
{
    for (IExpansionRomCard * card : m_cards)
    {
        if (address == kEnd || !card->UsesStrobePage())
        {
            card->DeselectExpansionRom();
        }
    }
}
