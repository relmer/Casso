#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Devices/Floppy35/SmartPortHost.h"
#include "Devices/Floppy35/Sony35Drive.h"

class MigChip;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlusDiskPort
//
//  The //c Plus's IWM at $C0E0-$C0EF, its internal Apple 3.5 Drive, and the
//  disk port's SmartPort chain. With drive 2 selected and the MIG's internal
//  drive on, the IWM drives the internal drive, its head the MIG's head
//  select; otherwise it drives the disk port. The phases go only to the one
//  the IWM drives, so the internal drive's register selects are never on
//  the chain.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2cPlusDiskPort : public MemoryDevice, public IDiskDriveBus, public IFloppy35EventSource
{
public:
    static constexpr Word  kIoStart = 0xC0E0;
    static constexpr Word  kIoEnd   = 0xC0EF;

    Apple2cPlusDiskPort();
    ~Apple2cPlusDiskPort() override;

    // MemoryDevice
    Byte  Read     (Word address) override;
    void  Write    (Word address, Byte value) override;
    Word  GetStart () const override { return kIoStart; }
    Word  GetEnd   () const override { return kIoEnd; }
    void  Reset    () override { m_port.Reset(); }

    // IDiskDriveBus: the IWM's drive 2.
    void      SetPhases (bool ph0, bool ph1, bool ph2, bool ph3) override;
    bool      GetSense  () const override;
    Byte      ReadCell  (uint64_t cellEndNs) override;
    void      WriteCell (Byte bit, uint64_t cellEndNs) override;
    void      Advance   (uint64_t elapsedNs) override { m_internal.Advance (elapsedNs); }
    uint32_t  GetCellNs () const override;

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override;

    //  The MIG that switches the internal drive, which this class deletes.
    void  AdoptMig (std::unique_ptr<MigChip> mig);
    MigChip * GetMig () const { return m_mig; }
    void  ResetIwm ()              { m_port.GetIwm().Reset(); }

    Sony35Drive    & GetInternalDrive () { return m_internal; }
    SmartPortHost  & GetPort          () { return m_port; }

private:
    static constexpr Word  kSwitchMask = 0x0F;

    bool  IsInternal () const;
    void  SyncHead   ();

    SmartPortHost          m_port;
    Sony35Drive            m_internal { 0 };
    std::unique_ptr<MigChip>  m_ownedMig;
    MigChip              * m_mig       = nullptr;
    std::array<bool, 4>    m_phases    = {};
    bool                   m_wasInternal = false;
};
