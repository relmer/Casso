#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Devices/BlockDevice/IBlockDeviceEventSink.h"
#include "Devices/BlockDevice/Ncr5380.h"
#include "Devices/BlockDevice/ScsiBus.h"
#include "Machines/Apple2/Common/IExpansionRomCard.h"
#include "Machines/Apple2/Common/SlotRomPage.h"

class MemoryBus;
class ScsiHardDisk;
struct DeviceConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleScsiCard
//
//  The Apple II SCSI Card in slot n, with Apple's 16 KB ROM and 8 KB of RAM:
//
//      $C0n0-$C0n7   the NCR 5380's registers
//      $C0n8         pseudo-DMA data, read and write
//      $C0n9         read: the card's SCSI ID as a bit mask, $80 for ID 7
//      $C0nA         write: bank latch; bits 0-3 the ROM bank, 4-6 the RAM bank
//      $C0nB         write: reset the 5380
//      $C0nC, $C0nD  write: IIgs block mode on and off, which has no effect
//      $C0nE         read: DRQ in bit 7
//      $Cn00-$CnFF   the ROM's first page
//      $C800-$CBFF   a 1 KB bank of RAM
//      $CC00-$CFFE   a 1 KB bank of ROM; only $CFFF deselects the card
//
//  Targets 0-6 are hard disks, each selectable once it has an image.
//
////////////////////////////////////////////////////////////////////////////////

class AppleScsiCard : public MemoryDevice, public IExpansionRomCard, public IBlockDeviceEventSource
{
public:
    static constexpr size_t  kRomSize   = 0x4000;
    static constexpr size_t  kRamSize   = 0x2000;
    static constexpr int     kCardId    = 7;
    static constexpr int     kTargets   = 7;

    AppleScsiCard (int slot, MemoryBus & bus);
    ~AppleScsiCard() override;

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    // MemoryDevice: the $C0n0-$C0nF window.
    Byte  Read       (Word address) override;
    void  Write      (Word address, Byte value) override;
    Word  GetStart   () const override { return m_ioStart; }
    Word  GetEnd     () const override { return m_ioEnd; }
    void  Reset      () override;
    void  SoftReset  () override;
    void  PowerCycle (Prng & prng) override;

    // IExpansionRomCard
    Byte  OnSlotPageAccess       (Word address, bool isWrite, Byte value) override;
    bool  IsExpansionRomSelected () const override { return m_selected; }
    Byte  ReadExpansionRom       (Word address) override;
    void  DeselectExpansionRom   () override { m_selected = false; }
    void  WriteExpansionRom      (Word address, Byte value) override;
    bool  UsesStrobePage         () const override { return true; }

    // IBlockDeviceEventSource
    void  SetBlockDeviceEventSink (IBlockDeviceEventSink * sink) override;

    //  Exactly kRomSize bytes, or the card has no firmware.
    HRESULT  SetFirmware (std::vector<Byte> firmware);
    bool     HasFirmware () const { return !m_rom.empty(); }

    void     SetCpuCycleSource (const uint64_t * cycles) { m_bus.SetClock (cycles); }

    ScsiHardDisk  & GetTarget   (int id);

    //  True while any target ran a command in the last `windowCycles`.
    bool            IsBusy      (uint64_t now, uint64_t windowCycles) const;

    //  True when a target is an HD20SC with or without an image, as the
    //  machine file lists it.
    bool            HasDrive    () const { return m_hasDrive; }

    //  The model number on the front of the HD20SC on the desk: the first
    //  target with an image, or "20SC" with none.
    std::wstring    GetModelLabel () const;
    void            SetHasDrive   (bool hasDrive) { m_hasDrive = hasDrive; }
    ScsiBus       & GetScsiBus    ()       { return m_bus; }
    Ncr5380       & GetNcr5380    ()       { return m_ncr; }
    SlotRomPage   & GetSlotPage   ()       { return m_slotPage; }
    int             GetSlot       () const { return m_slot; }
    Byte            GetBankLatch  () const { return m_bankLatch; }

private:
    static constexpr Word  kSlotIoBase   = 0xC080;
    static constexpr Word  kSlotIoStride = 0x10;
    static constexpr Word  kRamWindow    = 0xC800;
    static constexpr Word  kRomWindow    = 0xCC00;
    static constexpr Word  kBankBytes    = 0x400;

    void  ResetCard ();

    int                                          m_slot      = 0;
    Word                                         m_ioStart   = 0;
    Word                                         m_ioEnd     = 0;
    MemoryBus                                  & m_memoryBus;
    ScsiBus                                      m_bus;
    Ncr5380                                      m_ncr;
    SlotRomPage                                  m_slotPage;
    std::vector<std::unique_ptr<ScsiHardDisk>>   m_targets;
    std::vector<Byte>                            m_rom;
    std::vector<Byte>                            m_ram;
    Byte                                         m_bankLatch = 0;
    bool                                         m_selected  = false;
    bool                                         m_hasDrive  = false;
};
