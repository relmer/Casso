#include "Pch.h"

#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DiagnosticsSnapshot.h"





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





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperSerialCard::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBool (m_expansionSelected);

    hr = m_acia.SaveState (writer);
    CHR (hr);

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperSerialCard::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBool (m_expansionSelected);

    hr = m_acia.LoadState (reader);
    CHR (hr);

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDiagnostics
//
//  The switch bytes as the guest reads them, with the floating bus as zero.
//
////////////////////////////////////////////////////////////////////////////////

void SuperSerialCard::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    DiagnosticsGroup  group { "Card", {} };



    group.rows.push_back (MakeTextRow ("Slot",          std::format ("{}", m_slot)));
    group.rows.push_back (MakeHexRow  ("SW1",           m_switches.GetDipSwitch1Byte (0), kByteDigits));
    group.rows.push_back (MakeHexRow  ("SW2",           m_switches.GetDipSwitch2Byte (m_acia.IsCtsTrue(), 0), kByteDigits));
    group.rows.push_back (MakeFlagRow ("IRQ connected", m_switches.IsIrqConnected()));
    group.rows.push_back (MakeFlagRow ("Expansion ROM", m_expansionSelected, "the firmware is at $C800-$CEFF"));
    group.rows.push_back (MakeFlagRow ("Firmware",      HasFirmware()));

    if (!m_attachmentProblem.empty())
    {
        group.rows.push_back (MakeTextRow ("Problem", m_attachmentProblem));
    }

    snapshot.groups.push_back (std::move (group));

    m_acia.AddDiagnostics (snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperSerialCard::AddIoRegisters
//
//  The two switch blocks and the 6551, under the card's name and slot.
//
////////////////////////////////////////////////////////////////////////////////

void SuperSerialCard::AddIoRegisters (std::vector<DiagnosticsIoRegister> & registers) const
{
    std::string  owner = std::format ("{}, slot {}", GetDiagnosticsTitle(), m_slot);



    registers.push_back ({ (Word) (m_ioStart + kDipSwitch1Offset), owner + ": SW1, the first block of DIP switches" });
    registers.push_back ({ (Word) (m_ioStart + kDipSwitch2Offset), owner + ": SW2, the second block of DIP switches and the CTS line" });
    m_acia.AddRegisterDescriptions (registers, owner);
}
