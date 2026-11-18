#include "Pch.h"

#include "Apple3Slots.h"
#include "Apple3SystemVias.h"
#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr Word  s_kSlotIoBase     = 0xC080;
static constexpr int   s_kSlotIoShift    = 4;
static constexpr int   s_kPageShift      = 8;
static constexpr int   s_kSlotPageMask   = 0x0F;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Slots
//
////////////////////////////////////////////////////////////////////////////////

Apple3Slots::Apple3Slots (MemoryBus & bus, Apple3SystemVias & vias)
    : m_bus  (bus),
      m_vias (vias)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCard
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Slots::SetCard (int slot, MemoryDevice * card)
{
    if (slot >= 1 && slot <= kSlotCount)
    {
        m_cards[(size_t) (slot - 1)] = card;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInterrupt
//
//  A card's interrupt request, passed to the VIAs' inputs. Slots 1 and 2 are
//  read back through the switch register, so the request is stored here too.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Slots::SetInterrupt (int slot, bool asserted)
{
    if (slot < 1 || slot > kSlotCount)
    {
        return;
    }

    m_requests[(size_t) (slot - 1)] = asserted;
    m_vias.SetSlotInterrupt (slot, asserted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRequesting
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Slots::IsRequesting (int slot) const
{
    return slot >= 1 && slot <= kSlotCount && m_requests[(size_t) (slot - 1)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSlotAddress
//
//  Whether an I/O space address is a slot address: their device I/O,
//  their memory spaces, the shared space and the deselect address.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Slots::IsSlotAddress (Word address) const
{
    bool  isIo     = address >= kFirstSlotIo    && address <= kLastSlotIo;
    bool  isRom    = address >= kFirstSlotRom   && address <= kLastSlotRom;
    bool  isShared = address >= kFirstSharedRom && address <= kLastSharedRom;



    return isIo || isRom || isShared || address == kDeselect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSlotOf
//
//  The slot of a device I/O or memory space address, or 0.
//
////////////////////////////////////////////////////////////////////////////////

int Apple3Slots::GetSlotOf (Word address) const
{
    if (address >= kFirstSlotIo && address <= kLastSlotIo)
    {
        return (address - s_kSlotIoBase) >> s_kSlotIoShift;
    }

    if (address >= kFirstSlotRom && address <= kLastSlotRom)
    {
        return (address >> s_kPageShift) & s_kSlotPageMask;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCardFor
//
//  The card an access goes to. An access to a slot's memory space selects
//  that slot for the shared space; $C020 and $CFFF clear the selection, and
//  the access to either goes to no card.
//
////////////////////////////////////////////////////////////////////////////////

MemoryDevice * Apple3Slots::GetCardFor (Word address)
{
    int  slot = GetSlotOf (address);



    if (address == kDeselect || address == kLastSharedRom)
    {
        m_selected = 0;
        return nullptr;
    }

    if (address >= kFirstSlotRom && address <= kLastSlotRom)
    {
        m_selected = slot;
    }

    if (address >= kFirstSharedRom)
    {
        slot = m_selected;
    }

    return (slot >= 1) ? m_cards[(size_t) (slot - 1)] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3Slots::Read (Word address)
{
    MemoryDevice  * card = GetCardFor (address);



    return (card != nullptr) ? card->Read (address) : m_bus.ReadFloatingBus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Slots::Write (Word address, Byte value)
{
    MemoryDevice  * card = GetCardFor (address);



    if (card != nullptr)
    {
        card->Write (address, value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
//  The shared space's selection and the interrupt requests. The cards save
//  themselves.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Slots::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte ((Byte) m_selected);

    for (bool request : m_requests)
    {
        writer.WriteBool (request);
    }

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Slots::LoadState (StateReader & reader)
{
    HRESULT   hr       = S_OK;
    uint16_t  version  = 0;
    Byte      selected = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte (selected);
    m_selected = selected;

    for (bool & request : m_requests)
    {
        reader.ReadBool (request);
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}
