#include "Pch.h"

#include "Shell/MachineObserverWiring.h"
#include "Devices/Acia6551.h"
#include "Devices/BlockDevice/IBlockDeviceEventSink.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/UthernetCard.h"
#include "Shell/MachineParts.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineObserverWiring::AttachObservers
//
//  The //e soft-switch bank reports the switches the keyboard doesn't have
//  (80COL, 80STORE, ALTCHARSET); a ][ or ][+ doesn't have the bank. The
//  block devices and 3.5-inch drives have one sink each, whichever slot
//  attached it last.
//
////////////////////////////////////////////////////////////////////////////////

void MachineObserverWiring::AttachObservers (const MachineObservers & observers, ObserverSlot slot)
{
    MachineRefs  & refs = m_parts.refs;



    if (refs.diskController != nullptr)
    {
        refs.diskController->SetEventSink (slot, observers.disk);
    }

    if (refs.keyboard != nullptr)
    {
        refs.keyboard->SetInputEventSink (slot, observers.input);
    }

    if (refs.iieSoftSwitches != nullptr)
    {
        refs.iieSoftSwitches->SetInputEventSink (slot, observers.input);
    }

    if (refs.gamePort != nullptr)
    {
        refs.gamePort->SetInputEventSink (slot, observers.input);
    }

    if (m_parts.memoryBus != nullptr)
    {
        m_parts.memoryBus->SetIoAccessSink (slot,
                                            (observers.ioFilter != nullptr) ? observers.io : nullptr,
                                            (observers.ioFilter != nullptr) ? *observers.ioFilter : IoAccessFilter());
    }

    if (slot == ObserverSlot::History)
    {
        AttachCommEventSink (observers.comm);
    }

    for (IBlockDeviceEventSource * source : refs.blockDeviceSources)
    {
        source->SetBlockDeviceEventSink (observers.blockDevice);
    }

    for (IFloppy35EventSource * source : refs.floppy35Sources)
    {
        source->SetFloppy35EventSink (observers.floppy35);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineObserverWiring::AttachCommEventSink
//
//  Each serial chip, numbered as the input journal numbers it, and the
//  network card.
//
////////////////////////////////////////////////////////////////////////////////

void MachineObserverWiring::AttachCommEventSink (ICommEventSink * sink)
{
    m_commSink = sink;

    for (Acia6551 * acia : m_parts.refs.serialChips)
    {
        acia->SetCommEventSink (sink);
    }

    if (m_parts.refs.uthernet != nullptr)
    {
        m_parts.refs.uthernet->SetCommEventSink (sink);
    }
}
