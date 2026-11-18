#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Devices/BlockDevice/IReplayMedia.h"
#include "Devices/Floppy35/ICatchUpDevice.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Devices/Floppy35/SmartPortHost.h"
#include "Machines/Apple2/Common/IExpansionRomCard.h"
#include "Machines/Apple2/Common/SlotRomPage.h"

class MemoryBus;
struct DeviceConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  LironCard
//
//  The UniDisk 3.5 interface card, code name Liron: an IWM on a SmartPort
//  connector and a 4 KB ROM. The ROM has a page for each slot at $100-$7FF,
//  assembled for that slot, and the code at $800-$FFF that every slot page
//  calls into:
//
//      $C0n0-$C0nF   the IWM, in the Disk II's switch order
//      $Cn00-$CnFF   ROM page n
//      $C800-$CFFE   ROM $800-$FFE; only $CFFF deselects the card
//
////////////////////////////////////////////////////////////////////////////////

class LironCard : public MemoryDevice, public IExpansionRomCard, public IFloppy35EventSource, public IMachineState, public IReplayMediaSource, public ICatchUpDevice
{
public:
    static constexpr size_t  kRomSize = 0x1000;

    LironCard (int slot, MemoryBus & bus);
    ~LironCard() override;

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    // MemoryDevice: the IWM at $C0n0-$C0nF.
    Byte  Read       (Word address) override;
    void  Write      (Word address, Byte value) override;
    Word  GetStart   () const override { return m_ioStart; }
    Word  GetEnd     () const override { return m_ioEnd; }
    void  Reset      () override;
    bool  TryPeekIo  (Word address, Byte & value) override;

    // IExpansionRomCard
    Byte  OnSlotPageAccess       (Word address, bool isWrite, Byte value) override;
    Byte  PeekSlotPage           (Word address) const override;
    bool  IsExpansionRomSelected () const override { return m_selected; }
    Byte  ReadExpansionRom       (Word address) override;
    Byte  PeekExpansionRom       (Word address) const override;
    void  DeselectExpansionRom   () override { m_selected = false; }
    void  WriteExpansionRom      (Word address, Byte value) override { (void) address; (void) value; }
    bool  UsesStrobePage         () const override { return true; }

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override { m_port.SetFloppy35EventSink (sink); }

    HRESULT  SetFirmware (std::vector<Byte> firmware);
    bool     HasFirmware () const { return !m_rom.empty(); }

    void  SetHostClock (const uint64_t * cycles, uint32_t clockHz) { m_port.SetHostClock (cycles, clockHz); }
    void  AddDrive     (std::unique_ptr<UniDisk35Drive> drive) { m_port.AddDrive (std::move (drive)); }

    //  Runs the drives up to the Apple's time, so their firmware's timers run
    //  while the Apple doesn't access the card.
    void  CatchUp (CatchUpKind kind = CatchUpKind::Access) override { m_port.CatchUp (kind); }

    size_t            GetDriveCount () const { return m_port.GetDriveCount(); }
    UniDisk35Drive  & GetDrive      (size_t index) { return m_port.GetDrive (index); }
    SmartPortBus    & GetBus        () { return m_port.GetBus(); }
    Iwm             & GetIwm        () { return m_port.GetIwm(); }
    SlotRomPage     & GetSlotPage   () { return m_slotPage; }
    int               GetSlot       () const override { return m_slot; }

    //  IMachineState: the expansion ROM's selection and the SmartPort chain
    //  behind the card, its drives' CPUs, RAM and disks included.
    HRESULT  SaveState      (StateWriter & writer) const override;
    HRESULT  LoadState      (StateReader & reader) override;
    void     GetReplayMedia (std::vector<IReplayMedia *> & outDrives) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('L', 'I', 'R', 'N');
    static constexpr uint16_t  kStateVersion = 1;

private:
    static constexpr Word  kSlotIoBase   = 0xC080;
    static constexpr Word  kSlotIoStride = 0x10;
    static constexpr Word  kExpansion    = 0xC000;

    int                  m_slot     = 0;
    Word                 m_ioStart  = 0;
    Word                 m_ioEnd    = 0;
    MemoryBus          & m_hostBus;
    SlotRomPage          m_slotPage;
    SmartPortHost        m_port;
    std::vector<Byte>    m_rom;
    bool                 m_selected = false;
};
