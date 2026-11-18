#pragma once

#include "Pch.h"

class StateReader;
class StateWriter;

#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Devices/Floppy35/Iwm.h"
#include "Devices/Floppy35/SmartPortBus.h"
#include "Devices/Floppy35/UniDisk35Drive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SmartPortHost
//
//  The Apple's side of a SmartPort connector: an IWM whose drive 1 and drive
//  2 are the bus, since the phases are on the connector whichever drive is
//  selected, and the drives on the chain, which this class deletes. Before
//  each access to the IWM the drives run up to the Apple's time: while the
//  Apple writes, its IWM's cells go out first; while it reads, the drives'
//  cells come in first.
//
////////////////////////////////////////////////////////////////////////////////

class SmartPortHost : public IFloppy35EventSource
{
public:
    SmartPortHost();

    //  An access to switch `offset` (0-15) of the IWM; returns the byte a
    //  read returns.
    Byte  Access (int offset, bool isWrite, Byte value);

    //  The byte a read of switch `offset` returns, as of the last access or
    //  catch-up: the drives don't run, and the IWM doesn't change. False
    //  where the byte cannot be worked out (Iwm::TryPeekAccess).
    bool  TryPeekAccess (int offset, Byte & value) const { return m_iwm.TryPeekAccess (offset, value); }

    //  The IWM's switches after an access to `offset`.
    Iwm::SwitchState  GetSwitchesAfter (int offset) const { return m_iwm.GetSwitchesAfter (offset); }

    //  Runs the drives up to the Apple's time.
    void  CatchUp (CatchUpKind kind = CatchUpKind::Access);

    //  The Apple's power-on resets the IWM. The drives are separate devices
    //  on the bus and continue to run, so the time continues from where it
    //  was.
    void  Reset ();

    //  The Apple's cycle count and clock rate.
    void  SetHostClock (const uint64_t * cycles, uint32_t clockHz);

    //  A drive added to the end of the chain.
    void  AddDrive (std::unique_ptr<UniDisk35Drive> drive);

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override;

    size_t                GetDriveCount () const { return m_drives.size(); }
    UniDisk35Drive      & GetDrive      (size_t index) { return *m_drives[index]; }
    SmartPortBus        & GetBus        () { return m_bus; }
    const SmartPortBus  & GetBus        () const { return m_bus; }
    Iwm                 & GetIwm        () { return m_iwm; }
    const Iwm           & GetIwm        () const { return m_iwm; }

    //  The IWM, the bus, each drive on the chain and the chain's clock
    //  against the Apple's, written into and read from the owner's state
    //  section. A state saved with another number of drives doesn't load.
    HRESULT  SaveState (StateWriter & writer) const;
    HRESULT  LoadState (StateReader & reader);

private:
    uint64_t  GetNowNs ();

    Iwm                                           m_iwm;
    SmartPortBus                                  m_bus;
    std::vector<std::unique_ptr<UniDisk35Drive>>  m_drives;
    const uint64_t                              * m_hostCycles  = nullptr;
    uint32_t                                      m_hostHz      = 0;
    uint64_t                                      m_epochCycles = 0;
    uint64_t                                      m_epochNs     = 0;
    uint64_t                                      m_lastNs      = 0;
    uint64_t                                      m_lastCycles  = 0;
};
