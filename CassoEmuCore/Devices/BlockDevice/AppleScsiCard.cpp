#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"




//  Offsets in the card's $C0n0-$C0nF window.
static constexpr Word  s_kDmaPort    = 0x8;
static constexpr Word  s_kIdPort     = 0x9;
static constexpr Word  s_kBankLatch  = 0xA;
static constexpr Word  s_kChipReset  = 0xB;
static constexpr Word  s_kDrqPort    = 0xE;
static constexpr Word  s_kNcrLast    = 0x7;
static constexpr Byte  s_kDrqBit     = 0x80;
static constexpr Byte  s_kRomBankMask = 0x0F;
static constexpr Byte  s_kRamBankMask = 0x07;
static constexpr int   s_kRamShift    = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleScsiCard
//
////////////////////////////////////////////////////////////////////////////////

AppleScsiCard::AppleScsiCard (int slot, MemoryBus & bus) :
    m_slot      (slot),
    m_memoryBus (bus),
    m_ncr       (m_bus),
    m_slotPage  (slot, *this),
    m_ram       (kRamSize, 0)
{
    m_ioStart = (Word) (kSlotIoBase + slot * kSlotIoStride);
    m_ioEnd   = (Word) (m_ioStart + kSlotIoStride - 1);

    for (int id = 0; id < kTargets; id++)
    {
        m_targets.push_back (std::make_unique<ScsiHardDisk> (id, slot));
        m_bus.AttachTarget (m_targets.back().get());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~AppleScsiCard
//
////////////////////////////////////////////////////////////////////////////////

AppleScsiCard::~AppleScsiCard()
{
    m_bus.DetachTargets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  ComponentRegistry factory. The card defaults to slot 7, where Apple's
//  manual installs it.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> AppleScsiCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    constexpr int  kDefaultSlot = 7;
    int            slot         = config.hasSlot ? config.slot : kDefaultSlot;



    return std::make_unique<AppleScsiCard> (slot, bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleScsiCard::Read (Word address)
{
    Word  offset = (Word) (address - m_ioStart);
    Byte  value  = m_memoryBus.GetFloatingBusValue();



    if (offset <= s_kNcrLast)
    {
        value = m_ncr.ReadRegister (offset);
    }
    else if (offset == s_kDmaPort)
    {
        value = m_ncr.ReadDma();
    }
    else if (offset == s_kIdPort)
    {
        value = (Byte) (1 << kCardId);
    }
    else if (offset == s_kDrqPort)
    {
        value = m_ncr.IsDrq() ? s_kDrqBit : (Byte) 0;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
//  Read's byte, with the 5380's registers, its DMA port and its DRQ line read
//  by their peeks.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleScsiCard::TryPeekIo (Word address, Byte & value)
{
    Word  offset = (Word) (address - m_ioStart);



    value = m_memoryBus.GetFloatingBusValue();

    if (offset <= s_kNcrLast)
    {
        value = m_ncr.PeekRegister (offset);
    }
    else if (offset == s_kDmaPort)
    {
        value = m_ncr.PeekDma();
    }
    else if (offset == s_kIdPort)
    {
        value = (Byte) (1 << kCardId);
    }
    else if (offset == s_kDrqPort)
    {
        value = m_ncr.PeekDrq() ? s_kDrqBit : (Byte) 0;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void AppleScsiCard::Write (Word address, Byte value)
{
    Word  offset = (Word) (address - m_ioStart);



    if (offset <= s_kNcrLast)
    {
        m_ncr.WriteRegister (offset, value);
    }
    else if (offset == s_kDmaPort)
    {
        m_ncr.WriteDma (value);
    }
    else if (offset == s_kBankLatch)
    {
        m_bankLatch = value;
    }
    else if (offset == s_kChipReset)
    {
        m_ncr.Reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleScsiCard::Reset()
{
    ResetCard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  The Apple's RESET line clears the bank latch and resets the 5380.
//
////////////////////////////////////////////////////////////////////////////////

void AppleScsiCard::SoftReset()
{
    ResetCard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
////////////////////////////////////////////////////////////////////////////////

void AppleScsiCard::PowerCycle (Prng & prng)
{
    (void) prng;

    std::fill (m_ram.begin(), m_ram.end(), (Byte) 0);
    ResetCard();

    for (auto & target : m_targets)
    {
        target->ResetTarget();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSlotPageAccess
//
//  An access to $Cn00-$CnFF selects the card's $C800 window and reads the
//  ROM's first page.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleScsiCard::OnSlotPageAccess (Word address, bool isWrite, Byte value)
{
    (void) isWrite;
    (void) value;

    m_selected = true;

    return PeekSlotPage (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PeekSlotPage
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleScsiCard::PeekSlotPage (Word address) const
{
    constexpr Word  kPageMask = 0xFF;



    return m_rom.empty() ? m_memoryBus.GetFloatingBusValue() : m_rom[address & kPageMask];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadExpansionRom
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleScsiCard::ReadExpansionRom (Word address)
{
    return PeekExpansionRom (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PeekExpansionRom
//
//  The RAM bank below $CC00 and the ROM bank from it, as the bank latch
//  selects them.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleScsiCard::PeekExpansionRom (Word address) const
{
    size_t  ramBank = (size_t) ((m_bankLatch >> s_kRamShift) & s_kRamBankMask);
    size_t  romBank = (size_t) (m_bankLatch & s_kRomBankMask);



    if (address < kRomWindow)
    {
        return m_ram[ramBank * kBankBytes + (address - kRamWindow)];
    }

    return m_rom.empty() ? m_memoryBus.GetFloatingBusValue() : m_rom[romBank * kBankBytes + (address - kRomWindow)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteExpansionRom
//
////////////////////////////////////////////////////////////////////////////////

void AppleScsiCard::WriteExpansionRom (Word address, Byte value)
{
    size_t  ramBank = (size_t) ((m_bankLatch >> s_kRamShift) & s_kRamBankMask);



    if (address >= kRamWindow && address < kRomWindow)
    {
        m_ram[ramBank * kBankBytes + (address - kRamWindow)] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetBlockDeviceEventSink
//
////////////////////////////////////////////////////////////////////////////////

void AppleScsiCard::SetBlockDeviceEventSink (IBlockDeviceEventSink * sink)
{
    for (auto & target : m_targets)
    {
        target->SetBlockDeviceEventSink (sink);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFirmware
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleScsiCard::SetFirmware (std::vector<Byte> firmware)
{
    HRESULT  hr     = S_OK;
    size_t   length = firmware.size();



    m_rom.clear();
    CBREx (length == kRomSize, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    m_rom = std::move (firmware);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTarget
//
////////////////////////////////////////////////////////////////////////////////

ScsiHardDisk & AppleScsiCard::GetTarget (int id)
{
    return *m_targets[(size_t) id];
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBusy
//
////////////////////////////////////////////////////////////////////////////////

bool AppleScsiCard::IsBusy (uint64_t now, uint64_t windowCycles) const
{
    for (const auto & target : m_targets)
    {
        uint64_t  last = target->GetLastActivityCycle();



        if (last != 0 && now >= last && now - last < windowCycles)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModelLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleScsiCard::GetModelLabel() const
{
    for (const auto & target : m_targets)
    {
        if (target->IsSelectable())
        {
            return target->GetModelLabel();
        }
    }

    return m_targets.front()->GetModelLabel();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetCard
//
////////////////////////////////////////////////////////////////////////////////

void AppleScsiCard::ResetCard()
{
    m_bankLatch = 0;
    m_selected  = false;
    m_ncr.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleScsiCard::GetReplayMedia
//
//  Each target, by SCSI ID.
//
////////////////////////////////////////////////////////////////////////////////

void AppleScsiCard::GetReplayMedia (std::vector<IReplayMedia *> & outDrives)
{
    int  id = 0;



    for (id = 0; id < kTargets; id++)
    {
        outDrives.push_back (&GetTarget (id).GetMedia());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleScsiCard::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleScsiCard::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBytes  (m_ram.data(), m_ram.size());
    writer.WriteByte   (m_bankLatch);
    writer.WriteBool   (m_selected);

    hr = m_ncr.SaveState (writer);
    CHR (hr);

    hr = m_bus.SaveState (writer);
    CHR (hr);

    writer.WriteUInt32 (static_cast<uint32_t> (m_targets.size()));

    for (const std::unique_ptr<ScsiHardDisk> & target : m_targets)
    {
        hr = target->SaveState (writer);
        CHR (hr);
    }

    hr = writer.EndSection();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleScsiCard::LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleScsiCard::LoadState (StateReader & reader)
{
    HRESULT   hr          = S_OK;
    bool      isSameCount = false;
    uint16_t  version     = 0;
    uint32_t  targetCount = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBytes  (m_ram.data(), m_ram.size());
    reader.ReadByte   (m_bankLatch);
    reader.ReadBool   (m_selected);

    hr = m_ncr.LoadState (reader);
    CHR (hr);

    hr = m_bus.LoadState (reader);
    CHR (hr);

    reader.ReadUInt32 (targetCount);
    isSameCount = targetCount == m_targets.size();
    CBREx (isSameCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (std::unique_ptr<ScsiHardDisk> & target : m_targets)
    {
        hr = target->LoadState (reader);
        CHR (hr);
    }

    hr = reader.EndSection();

Error:
    return hr;
}
