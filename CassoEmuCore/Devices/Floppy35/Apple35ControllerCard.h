#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Devices/BlockDevice/IReplayMedia.h"
#include "Devices/Floppy35/ICatchUpDevice.h"
#include "Devices/Floppy35/Apple35CardMemory.h"
#include "Devices/Floppy35/DeviceCpu.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Machines/Apple2/Common/IExpansionRomCard.h"
#include "Machines/Apple2/Common/SlotRomPage.h"

class MemoryBus;
struct DeviceConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35ControllerCard
//
//  The Apple II 3.5 Disk Controller Card in slot n: a 65C02 at 2.046 MHz with
//  32 KB of RAM and 32 KB of ROM driving up to two Apple 3.5 Drives through
//  a SWIM. The Apple has no ROM to read on the card, only windows into the
//  card's RAM, which the card's CPU fills:
//
//      $C0n0-$C0nF   any access latches the address's low 4 bits as the bank
//      $Cn00-$CnFF   card RAM $7B00-$7BFF, read only
//      $C800-$CBFF   card RAM at the bank times $400
//      $CC00-$CFFE   card RAM $7C00-$7FFE; only $CFFF deselects the card
//
//  The card's CPU runs from the card's power-on, and is brought up to the
//  Apple's clock on each access the Apple makes to the card.
//
////////////////////////////////////////////////////////////////////////////////

class Apple35ControllerCard : public MemoryDevice, public IExpansionRomCard, public IFloppy35EventSource, public IMachineState, public IReplayMediaSource, public ICatchUpDevice
{
public:
    Apple35ControllerCard (int slot, MemoryBus & bus);
    ~Apple35ControllerCard() override;

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    // MemoryDevice: the $C0n0-$C0nF window.
    Byte  Read       (Word address) override;
    void  Write      (Word address, Byte value) override;
    Word  GetStart   () const override { return m_ioStart; }
    Word  GetEnd     () const override { return m_ioEnd; }
    void  Reset      () override;
    void  SoftReset  () override;
    bool  TryPeekIo  (Word address, Byte & value) override;

    // IExpansionRomCard
    Byte  OnSlotPageAccess       (Word address, bool isWrite, Byte value) override;
    bool  TryPeekSlotPage        (Word address, Byte & value) const override;
    bool  IsExpansionRomSelected () const override { return m_selected; }
    Byte  ReadExpansionRom       (Word address) override;
    bool  TryPeekExpansionRom    (Word address, Byte & value) const override;
    void  DeselectExpansionRom   () override { m_selected = false; }
    void  WriteExpansionRom      (Word address, Byte value) override;
    bool  UsesStrobePage         () const override { return true; }

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override;

    HRESULT  SetFirmware (std::vector<Byte> firmware);
    bool     HasFirmware () const { return m_memory.HasFirmware(); }

    //  The Apple's cycle count and clock rate, which the card's CPU follows.
    void  SetHostClock (const uint64_t * cycles, uint32_t clockHz);

    //  Runs the card's CPU up to the Apple's clock.
    void  CatchUp (CatchUpKind kind = CatchUpKind::Access) override;

    Apple35CardMemory  & GetCardMemory () { return m_memory; }
    DeviceCpu          & GetCardCpu    () { return m_cpu; }
    SlotRomPage        & GetSlotPage   () { return m_slotPage; }
    int                  GetSlot       () const override { return m_slot; }

    //  IMachineState: the card's CPU, RAM, IWM and drives, the bank, the
    //  expansion ROM's selection and the card's clock against the Apple's.
    HRESULT  SaveState      (StateWriter & writer) const override;
    HRESULT  LoadState      (StateReader & reader) override;
    void     GetReplayMedia (std::vector<IReplayMedia *> & outDrives) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('3', '5', 'C', 'C');
    static constexpr uint16_t  kStateVersion = 1;
    Byte                 GetBank       () const { return m_bank; }

private:
    static constexpr Word      kSlotIoBase     = 0xC080;
    static constexpr Word      kSlotIoStride   = 0x10;
    static constexpr Word      kBankWindow     = 0xC800;
    static constexpr Word      kFixedWindow    = 0xCC00;
    static constexpr Word      kBankBytes      = 0x400;
    static constexpr Word      kPageRam        = 0x7B00;
    static constexpr Word      kFixedRam       = 0x7C00;
    static constexpr uint32_t  kCardClockHz    = 2'045'454;
    static constexpr uint64_t  kMaxCatchUp     = 4'000'000;
    static constexpr Word      kIdleLoop       = 0xD1A6;
    static constexpr uint64_t  kIdlePassCycles = 14;

    Word      GetRamAddress   (Word address) const;
    void      MapCardPages    ();
    uint64_t  SkipIdleLoop    (uint64_t cyclesLeft);
    bool      IsIdleLoopInRom ();

    int                  m_slot       = 0;
    Word                 m_ioStart    = 0;
    Word                 m_ioEnd      = 0;
    MemoryBus          & m_bus;
    SlotRomPage          m_slotPage;
    Apple35CardMemory    m_memory;
    DeviceCpu            m_cpu;
    const uint64_t     * m_hostCycles = nullptr;
    uint32_t             m_hostHz     = 0;
    uint64_t             m_hostSynced = 0;
    uint64_t             m_cardTarget = 0;
    Byte                 m_bank       = 0;
    bool                 m_selected   = false;
};
