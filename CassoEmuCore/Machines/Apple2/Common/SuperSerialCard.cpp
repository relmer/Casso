#include "Pch.h"

#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SuperSerialCard
//
////////////////////////////////////////////////////////////////////////////////

SuperSerialCard::SuperSerialCard (int slot, MemoryBus & bus)
    : m_bus      (bus),
      m_acia     (static_cast<Word> (kSlotIoBase + slot * kSlotIoStride + kAciaOffset)),
      m_slotPage (slot, *this)
{
    m_slot    = slot;
    m_ioStart = static_cast<Word> (kSlotIoBase + slot * kSlotIoStride);
    m_ioEnd   = static_cast<Word> (m_ioStart + kSlotIoSize - 1);

    m_acia.SetIrqConnected (m_switches.IsIrqConnected());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  ComponentRegistry factory. The card defaults to slot 2, Apple's modem
//  slot, when the config does not pin one.
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> SuperSerialCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    constexpr int  kDefaultSlot = 2;
    int            slot         = config.hasSlot ? config.slot : kDefaultSlot;



    return make_unique<SuperSerialCard> (slot, bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte SuperSerialCard::Read (Word address)
{
    Word  offset   = static_cast<Word> (address - m_ioStart);
    Byte  floating = m_bus.GetFloatingBusValue();
    Byte  result   = floating;



    if (offset == kDipSwitch1Offset)
    {
        result = m_switches.GetDipSwitch1Byte (floating);
    }
    else if (offset == kDipSwitch2Offset)
    {
        result = m_switches.GetDipSwitch2Byte (m_acia.IsCtsTrue(), floating);
    }
    else if (offset >= kAciaOffset && offset < kAciaOffset + Acia6551::kRegisterCount)
    {
        result = m_acia.Read (address);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Only the 6551 accepts writes; the switch registers are read-only buffers.
//
////////////////////////////////////////////////////////////////////////////////

void SuperSerialCard::Write (Word address, Byte value)
{
    Word  offset = static_cast<Word> (address - m_ioStart);



    if (offset >= kAciaOffset && offset < kAciaOffset + Acia6551::kRegisterCount)
    {
        m_acia.Write (address, value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  The Apple's RESET line resets the 6551 and the expansion-ROM latch.
//
////////////////////////////////////////////////////////////////////////////////

void SuperSerialCard::Reset()
{
    m_acia.Reset();
    m_expansionSelected = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
////////////////////////////////////////////////////////////////////////////////

void SuperSerialCard::SoftReset()
{
    m_acia.SoftReset();
    m_expansionSelected = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
////////////////////////////////////////////////////////////////////////////////

void SuperSerialCard::PowerCycle (Prng & prng)
{
    m_acia.PowerCycle (prng);
    m_expansionSelected = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSlotPageAccess
//
//  Any access to $Cs00-$CsFF selects the expansion ROM. A read returns the
//  firmware's last page, or the floating bus with no firmware.
//
////////////////////////////////////////////////////////////////////////////////

Byte SuperSerialCard::OnSlotPageAccess (Word address, bool isWrite, Byte value)
{
    constexpr Word  kPageMask = 0x00FF;
    Byte            result    = m_bus.GetFloatingBusValue();



    (void) isWrite;
    (void) value;

    m_expansionSelected = true;

    if (!m_firmware.empty())
    {
        result = m_firmware[kFirmwarePageOffset + (address & kPageMask)];
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadExpansionRom
//
//  $C800-$CEFF is firmware $000-$6FF. $CF00-$CFFF is not mapped.
//
////////////////////////////////////////////////////////////////////////////////

Byte SuperSerialCard::ReadExpansionRom (Word address)
{
    Word  offset = static_cast<Word> (address - kExpansionRomStart);
    Byte  result = m_bus.GetFloatingBusValue();



    if (!m_firmware.empty() && address >= kExpansionRomStart && offset < kFirmwarePageOffset)
    {
        result = m_firmware[offset];
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFirmware
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperSerialCard::SetFirmware (vector<Byte> firmware)
{
    HRESULT  hr        = S_OK;
    size_t   size      = firmware.size();
    bool     sizeValid = (size == kFirmwareSize || size == 0);



    m_firmware.clear();

    CBREx (sizeValid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_firmware = move (firmware);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSwitches
//
////////////////////////////////////////////////////////////////////////////////

void SuperSerialCard::SetSwitches (const SscSwitches & switches)
{
    m_switches = switches;
    m_acia.SetIrqConnected (m_switches.IsIrqConnected());
}
