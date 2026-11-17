#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Devices/BlockDevice/IBlockDeviceEventSink.h"
#include "Devices/BlockDevice/ProFileDrive.h"
#include "Machines/Apple2/Common/IExpansionRomCard.h"
#include "Machines/Apple2/Common/SlotRomPage.h"

class MemoryBus;
struct DeviceConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  ProFileCard
//
//  Apple's ProFile interface card in slot n, with its 2 KB ROM (341-0271-A for
//  the 5 MB drive, 341-0299-B for the 10 MB one), read from the ROM:
//
//      $C0n0         write: the data out latch, strobed to the drive
//      $C0n1         read: the data in latch, strobed from the drive
//      $C0n2         read: status; bit 7 the drive's BSY line, 1 when idle,
//                    and bit 0 set when no drive is connected
//      $C0n3         read and written around each transfer, with no effect
//                    here
//      $Cn00-$CnFF   the ROM's last page; $C800-$CEFF its first $700 bytes
//
//  The handshake lines are a latch set by accesses to $Cn00-$Cn07: the
//  address's bits 1 and 2 select a line and bit 0 is its level. Line 2
//  falling while line 3 is set starts a handshake, and an access to $Cn00
//  ends one. Instruction fetches from the page's first bytes, at boot, pass
//  with line 3 clear, so they start nothing.
//
////////////////////////////////////////////////////////////////////////////////

class ProFileCard : public MemoryDevice, public IExpansionRomCard, public IBlockDeviceEventSource
{
public:
    static constexpr size_t  kRomSize      = 0x800;
    static constexpr Word    kRomPageAt    = 0x700;

    ProFileCard (int slot, MemoryBus & bus);

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    // MemoryDevice: the $C0n0-$C0nF window.
    Byte  Read       (Word address) override;
    void  Write      (Word address, Byte value) override;
    Word  GetStart   () const override { return m_ioStart; }
    Word  GetEnd     () const override { return m_ioEnd; }
    void  Reset      () override;
    void  SoftReset  () override;

    // IExpansionRomCard
    Byte  OnSlotPageAccess       (Word address, bool isWrite, Byte value) override;
    bool  IsExpansionRomSelected () const override { return m_selected; }
    Byte  ReadExpansionRom       (Word address) override;
    void  DeselectExpansionRom   () override { m_selected = false; }

    // IBlockDeviceEventSource
    void  SetBlockDeviceEventSink (IBlockDeviceEventSink * sink) override { m_drive->SetBlockDeviceEventSink (sink); }

    //  Exactly kRomSize bytes, or the card has no firmware.
    HRESULT  SetFirmware (std::vector<Byte> firmware);
    bool     HasFirmware () const { return !m_rom.empty(); }

    //  The drive model sets the capacity; the card starts with a 5 MB drive.
    void            SetDrive      (std::unique_ptr<ProFileDrive> drive) { m_drive = std::move (drive); }
    ProFileDrive  & GetDrive      () { return *m_drive; }
    SlotRomPage   & GetSlotPage   () { return m_slotPage; }
    Byte            GetLines      () const { return m_lines; }

private:
    static constexpr Word  kSlotIoBase   = 0xC080;
    static constexpr Word  kSlotIoStride = 0x10;
    static constexpr Word  kExpansionAt  = 0xC800;

    void  OnLineAccess (Byte offset);

    int                             m_slot     = 0;
    Word                            m_ioStart  = 0;
    Word                            m_ioEnd    = 0;
    MemoryBus                     & m_bus;
    SlotRomPage                     m_slotPage;
    std::unique_ptr<ProFileDrive>   m_drive;
    std::vector<Byte>               m_rom;
    Byte                            m_lines    = 0;
    bool                            m_selected = false;
};
