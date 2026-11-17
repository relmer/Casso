#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Floppy35/SmartPortBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BitLine::Write
//
////////////////////////////////////////////////////////////////////////////////

void BitLine::Write (Byte bit, uint64_t cellEndNs, uint32_t cellNs)
{
    if (bit == 0)
    {
        return;
    }

    (void) cellNs;

    m_events.push_back (cellEndNs);

    while (m_events.size() > kMaxEvents)
    {
        m_events.pop_front();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BitLine::Read
//
////////////////////////////////////////////////////////////////////////////////

Byte BitLine::Read (uint64_t cellEndNs, uint32_t cellNs)
{
    uint64_t  cellStart = (cellEndNs > cellNs) ? cellEndNs - cellNs : 0;
    Byte      bit       = 0;



    while (!m_events.empty() && m_events.front() < cellStart)
    {
        m_events.pop_front();
    }

    while (!m_events.empty() && m_events.front() < cellEndNs)
    {
        m_events.pop_front();
        bit = 1;
    }

    return bit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDevice
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortBus::AddDevice (ISmartPortDevice * device)
{
    device->Connect (this, m_devices.size());
    m_devices.push_back (device);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatchUp
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortBus::CatchUp (uint64_t nowNs, CatchUpKind kind)
{
    for (ISmartPortDevice * device : m_devices)
    {
        device->CatchUp (nowNs, kind);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPhases
//
//  A change of the reset condition goes to every device.
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortBus::SetPhases (bool ph0, bool ph1, bool ph2, bool ph3)
{
    bool  isReset = ph0 && ph2 && !ph1 && !ph3;



    m_phases = { ph0, ph1, ph2, ph3 };

    if (isReset != m_reset)
    {
        m_reset = isReset;

        for (ISmartPortDevice * device : m_devices)
        {
            device->SetBusReset (isReset);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSense
//
//  /ACK: low when any device driving the bus drives it low.
//
////////////////////////////////////////////////////////////////////////////////

bool SmartPortBus::GetSense() const
{
    bool  ack = true;



    for (ISmartPortDevice * device : m_devices)
    {
        if (device->IsDrivingBus())
        {
            ack = ack && device->GetAck();
        }
    }

    return ack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCell
//
//  The host reads what the driving devices write. A device that isn't
//  driving the bus has its writes dropped.
//
////////////////////////////////////////////////////////////////////////////////

Byte SmartPortBus::ReadCell (uint64_t cellEndNs)
{
    Byte  bit = 0;



    for (ISmartPortDevice * device : m_devices)
    {
        Byte  each = device->GetOutput().Read (cellEndNs, kCellNs);



        if (device->IsDrivingBus())
        {
            bit = (Byte) (bit | each);
        }
    }

    return bit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteCell
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortBus::WriteCell (Byte bit, uint64_t cellEndNs)
{
    for (ISmartPortDevice * device : m_devices)
    {
        device->GetInput().Write (bit, cellEndNs, kCellNs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsEnabledAt
//
////////////////////////////////////////////////////////////////////////////////

bool SmartPortBus::IsEnabledAt (size_t position) const
{
    bool  enabled = m_phases[1] && m_phases[3];



    for (size_t i = 0; enabled && i < position && i < m_devices.size(); i++)
    {
        enabled = m_devices[i]->PassesEnable();
    }

    return enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BitLine::SaveState
//
////////////////////////////////////////////////////////////////////////////////

void BitLine::SaveState (StateWriter & writer) const
{
    writer.WriteUInt32 (static_cast<uint32_t> (m_events.size()));

    for (uint64_t atNs : m_events)
    {
        writer.WriteUInt64 (atNs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BitLine::LoadState
//
//  More events than the line has room for is bad data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BitLine::LoadState (StateReader & reader)
{
    HRESULT   hr    = S_OK;
    uint32_t  count = 0;
    uint64_t  atNs  = 0;



    reader.ReadUInt32 (count);
    CBREx (count <= kMaxEvents, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_events.clear();

    for (uint32_t i = 0; i < count; i++)
    {
        reader.ReadUInt64 (atNs);
        m_events.push_back (atNs);
    }

    hr = reader.GetResult();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SmartPortBus::SaveState
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortBus::SaveState (StateWriter & writer) const
{
    for (bool phase : m_phases)
    {
        writer.WriteBool (phase);
    }

    writer.WriteBool (m_reset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SmartPortBus::LoadState
//
//  The devices are the owner's; only the lines are the bus's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SmartPortBus::LoadState (StateReader & reader)
{
    for (bool & phase : m_phases)
    {
        reader.ReadBool (phase);
    }

    reader.ReadBool (m_reset);

    return reader.GetResult();
}
