#pragma once

#include "Pch.h"

#include "Core/ObserverSlots.h"

class IBlockDeviceEventSink;
class ICommEventSink;
class IDisk2EventSink;
class IFloppy35EventSink;
class IInputEventSink;
class IIoAccessSink;
struct IoAccessFilter;
struct MachineParts;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineObservers
//
//  The sinks that watch the machine run. The debug panels implement them,
//  and the devices have only the interfaces, so a test can watch a machine
//  through the same seam the panels use, and a machine switch can attach
//  the same sinks to the new devices.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineObservers
{
    IDisk2EventSink         *  disk        = nullptr;
    IInputEventSink         *  input       = nullptr;
    IIoAccessSink           *  io          = nullptr;
    const IoAccessFilter    *  ioFilter    = nullptr;      // the I/O io takes; with none, io takes nothing
    ICommEventSink          *  comm        = nullptr;      // the serial ports' and network card's events; History slot only
    IBlockDeviceEventSink   *  blockDevice = nullptr;
    IFloppy35EventSink      *  floppy35    = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineObserverWiring
//
//  Points the machine's devices at the observers, or at nothing. Each
//  device is attached independently: the input panel is useful alone, and
//  it once received nothing unless the disk panel was open as well. A
//  machine switch rebuilds every device, so the wiring runs again
//  afterward, or an open panel goes quiet against the new machine.
//
////////////////////////////////////////////////////////////////////////////////

class MachineObserverWiring
{
public:
    explicit MachineObserverWiring (MachineParts & parts) : m_parts (parts) {}

    MachineObserverWiring (const MachineObserverWiring &)             = delete;
    MachineObserverWiring & operator= (const MachineObserverWiring &) = delete;

    //  The emulator's panels take the Panel slot; the debugger's event cache,
    //  or a rebuild's probe on a machine replaying history, the History slot.
    void  AttachObservers     (const MachineObservers & observers, ObserverSlot slot = ObserverSlot::Panel);
    void  AttachCommEventSink (ICommEventSink * sink);

    //  The comm sink, attached to the devices of a machine the builder
    //  switched to.
    void  AttachAgain() { AttachCommEventSink (m_commSink); }

private:
    MachineParts    & m_parts;
    ICommEventSink  * m_commSink = nullptr;   // the debugger's serial and network logs
};
