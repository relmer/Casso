#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Devices/Floppy35/Iwm.h"
#include "Devices/Floppy35/SmartPortBus.h"
#include "Devices/Floppy35/UniDisk35Drive.h"
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
//  The drives on the card's chain are the card's. Before each access to the
//  IWM, the drives run up to the Apple's time.
//
////////////////////////////////////////////////////////////////////////////////

class LironCard : public MemoryDevice, public IExpansionRomCard, public IFloppy35EventSource
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

    // IExpansionRomCard
    Byte  OnSlotPageAccess       (Word address, bool isWrite, Byte value) override;
    bool  IsExpansionRomSelected () const override { return m_selected; }
    Byte  ReadExpansionRom       (Word address) override;
    void  DeselectExpansionRom   () override { m_selected = false; }
    void  WriteExpansionRom      (Word address, Byte value) override { (void) address; (void) value; }
    bool  UsesStrobePage         () const override { return true; }

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override;

    HRESULT  SetFirmware (std::vector<Byte> firmware);
    bool     HasFirmware () const { return !m_rom.empty(); }

    //  The Apple's cycle count and clock rate, which the IWM and the drives
    //  follow.
    void  SetHostClock (const uint64_t * cycles, uint32_t clockHz);

    //  A drive added to the end of the chain.
    void  AddDrive (std::unique_ptr<UniDisk35Drive> drive);

    //  Runs the drives up to the Apple's time, so their firmware's timers run
    //  while the Apple doesn't access the card.
    void  CatchUp ();

    size_t            GetDriveCount () const { return m_drives.size(); }
    UniDisk35Drive  & GetDrive      (size_t index) { return *m_drives[index]; }
    SmartPortBus    & GetBus        () { return m_bus; }
    Iwm             & GetIwm        () { return m_iwm; }
    SlotRomPage     & GetSlotPage   () { return m_slotPage; }

private:
    static constexpr Word  kSlotIoBase   = 0xC080;
    static constexpr Word  kSlotIoStride = 0x10;
    static constexpr Word  kExpansion    = 0xC000;

    Byte      Access   (Word address, bool isWrite, Byte value);
    uint64_t  GetNowNs ();

    int                                           m_slot        = 0;
    Word                                          m_ioStart     = 0;
    Word                                          m_ioEnd       = 0;
    MemoryBus                                   & m_hostBus;
    SlotRomPage                                   m_slotPage;
    Iwm                                           m_iwm;
    SmartPortBus                                  m_bus;
    std::vector<std::unique_ptr<UniDisk35Drive>>  m_drives;
    std::vector<Byte>                             m_rom;
    const uint64_t                              * m_hostCycles  = nullptr;
    uint32_t                                      m_hostHz      = 0;
    uint64_t                                      m_epochCycles = 0;
    uint64_t                                      m_epochNs     = 0;
    uint64_t                                      m_lastNs      = 0;
    bool                                          m_selected    = false;
};
