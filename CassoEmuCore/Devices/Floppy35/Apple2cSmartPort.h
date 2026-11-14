#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Devices/Floppy35/SmartPortHost.h"
#include "Machines/Apple2/Common/IDiskPortHook.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cSmartPort
//
//  A SmartPort chain on the //c's disk port. The //c's IWM is its built-in
//  drive's controller, so a second IWM here follows every access to the
//  IWM's switches and drives the bus; with drive 2 selected, its reads are
//  the ones the Apple gets. With a 5.25-inch drive at the end of the chain,
//  the drives pass the port through to it unless the bus is enabled or in
//  reset, as the UniDisk 3.5 does.
//
//  A MemoryDevice only so the machine can delete it with its other devices;
//  it isn't on the memory bus.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2cSmartPort : public MemoryDevice, public IDiskPortHook, public IFloppy35EventSource
{
public:
    static constexpr int  kDriveTwo = 1;

    explicit Apple2cSmartPort (bool hasFloppyAfter) : m_hasFloppyAfter (hasFloppyAfter) {}

    // MemoryDevice
    Byte  Read     (Word address) override { (void) address; return 0xFF; }
    void  Write    (Word address, Byte value) override { (void) address; (void) value; }
    Word  GetStart () const override { return 0; }
    Word  GetEnd   () const override { return 0; }
    void  Reset    () override { m_port.Reset(); }

    // IDiskPortHook
    bool  OnIwmAccess (int offset, bool isWrite, Byte value, Byte & outValue) override;

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override { m_port.SetFloppy35EventSink (sink); }

    SmartPortHost  & GetPort () { return m_port; }

private:
    SmartPortHost  m_port;
    bool           m_hasFloppyAfter = false;
};
