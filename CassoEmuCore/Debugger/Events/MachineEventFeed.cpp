#include "Pch.h"

#include "Debugger/Events/MachineEventFeed.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AttachToCaches
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::AttachToCaches (
    MachineHost                  & machine,
    DebugEventCache<Disk2Event>  & disk,
    DebugEventCache<InputEvent>  & input,
    RecordingTest                  isRecording)
{
    m_diskCache   = &disk;
    m_inputCache  = &input;
    m_diskOut     = nullptr;
    m_inputOut    = nullptr;
    m_isRecording = std::move (isRecording);

    Attach (machine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachToEntries
//
//  Either kind may be null, and its events are dropped.
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::AttachToEntries (
    MachineHost              & machine,
    std::vector<DiskEntry>   * disk,
    std::vector<InputEntry>  * input)
{
    m_diskCache   = nullptr;
    m_inputCache  = nullptr;
    m_diskOut     = disk;
    m_inputOut    = input;
    m_isRecording = nullptr;

    Attach (machine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Attach
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::Attach (MachineHost & machine)
{
    Disk2Controller  * controller = machine.GetRefs().diskController;



    m_machine = &machine;
    m_drive   = (controller != nullptr) ? controller->GetActiveDrive() : 0;

    machine.AttachObservers ({ this, this }, ObserverSlot::History);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Detach
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::Detach (MachineHost & machine)
{
    machine.AttachObservers ({}, ObserverSlot::History);

    m_machine     = nullptr;
    m_diskCache   = nullptr;
    m_inputCache  = nullptr;
    m_diskOut     = nullptr;
    m_inputOut    = nullptr;
    m_isRecording = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRecording
//
////////////////////////////////////////////////////////////////////////////////

bool MachineEventFeed::IsRecording() const
{
    return !m_isRecording || m_isRecording();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitDisk
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::EmitDisk (
    Disk2EventType                type,
    int                           drive,
    const Disk2Event::Payload   & payload)
{
    uint64_t    position = 0;
    uint64_t    cycle    = 0;
    Disk2Event  event    = {};



    if (m_machine == nullptr || (m_diskCache == nullptr && m_diskOut == nullptr))
    {
        return;
    }

    position = m_machine->GetPosition();
    cycle    = m_machine->GetCpu()->GetTotalCycles();

    event.category = EventCategory::Controller;
    event.type     = type;
    event.drive    = static_cast<int8_t> (drive);
    event.cycle    = cycle;
    event.payload  = payload;

    if (m_diskOut != nullptr)
    {
        m_diskOut->push_back (DiskEntry { DebugEventEntryKind::Event, position, cycle, event });
    }
    else if (IsRecording())
    {
        m_diskCache->Append (position, cycle, event);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitInput
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::EmitInput (
    InputEventType  type,
    Word            address,
    Byte            value,
    uint8_t         flags)
{
    uint64_t    position = 0;
    uint64_t    cycle    = 0;
    InputEvent  event    = {};



    if (m_machine == nullptr || (m_inputCache == nullptr && m_inputOut == nullptr))
    {
        return;
    }

    position = m_machine->GetPosition();
    cycle    = m_machine->GetCpu()->GetTotalCycles();

    event.category           = InputEventCategory::Guest;
    event.type               = type;
    event.cycle              = cycle;
    event.payload.io.address = address;
    event.payload.io.value   = value;
    event.payload.io.flags   = flags;

    if (m_inputOut != nullptr)
    {
        m_inputOut->push_back (InputEntry { DebugEventEntryKind::Event, position, cycle, event });
    }
    else if (IsRecording())
    {
        m_inputCache->Append (position, cycle, event);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMotorCommandOn
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnMotorCommandOn()
{
    EmitDisk (Disk2EventType::MotorCommandOn, m_drive, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMotorEngaged
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnMotorEngaged()
{
    EmitDisk (Disk2EventType::MotorEngaged, m_drive, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMotorCommandOff
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnMotorCommandOff()
{
    EmitDisk (Disk2EventType::MotorCommandOff, m_drive, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMotorDisengaged
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnMotorDisengaged()
{
    EmitDisk (Disk2EventType::MotorDisengaged, m_drive, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeadStep
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnHeadStep (int prevQt, int newQt)
{
    Disk2Event::Payload  payload = {};



    payload.step = { prevQt, newQt };

    EmitDisk (Disk2EventType::HeadStep, m_drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeadBump
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnHeadBump (int atQt)
{
    Disk2Event::Payload  payload = {};



    payload.bump = { atQt };

    EmitDisk (Disk2EventType::HeadBump, m_drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAddressMark
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnAddressMark (int track, int sector, int volume)
{
    Disk2Event::Payload  payload = {};



    payload.addrMark = { track, sector, volume };

    EmitDisk (Disk2EventType::AddrMark, m_drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDataMarkRead
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDataMarkRead (int track, int sector, int volume, int byteCount)
{
    Disk2Event::Payload  payload = {};



    payload.dataMark = { track, sector, volume, byteCount };

    EmitDisk (Disk2EventType::DataRead, m_drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDataMarkWrite
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDataMarkWrite (int track, int sector, int volume, int byteCount)
{
    Disk2Event::Payload  payload = {};



    payload.dataMark = { track, sector, volume, byteCount };

    EmitDisk (Disk2EventType::DataWrite, m_drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnReadStall
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnReadStall (int quarterTrack)
{
    Disk2Event::Payload  payload = {};



    payload.bump = { quarterTrack };

    EmitDisk (Disk2EventType::ReadStall, m_drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDriveSelect
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDriveSelect (int drive)
{
    Disk2Event::Payload  payload = {};



    m_drive       = drive;
    payload.drive = { drive };

    EmitDisk (Disk2EventType::DriveSelect, drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskInserted
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDiskInserted (int drive)
{
    Disk2Event::Payload  payload = {};



    payload.drive = { drive };

    EmitDisk (Disk2EventType::DiskInserted, drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskEjected
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDiskEjected (int drive)
{
    Disk2Event::Payload  payload = {};



    payload.drive = { drive };

    EmitDisk (Disk2EventType::DiskEjected, drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKbdDataRead
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnKbdDataRead (Word address, Byte value, bool strobeSet)
{
    EmitInput (InputEventType::KbdDataRead, address, value, strobeSet ? InputEvent::kFlagStrobe : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKbdStrobe
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnKbdStrobe (Word address, Byte value, bool clearedStrobe)
{
    uint8_t  flags = clearedStrobe ? InputEvent::kFlagStrobe : 0;



    if ((value & 0x80) != 0)
    {
        flags |= InputEvent::kFlagAnyKeyDown;
    }

    EmitInput (InputEventType::KbdStrobe, address, value, flags);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnButtonRead
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnButtonRead (Word address, Byte value)
{
    EmitInput (InputEventType::ButtonRead, address, value, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPaddleTrigger
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnPaddleTrigger (Word address)
{
    EmitInput (InputEventType::PaddleTrigger, address, 0, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPaddleRead
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnPaddleRead (Word address, Byte value)
{
    EmitInput (InputEventType::PaddleRead, address, value, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostAutoRepeat
//
//  The History slot gets no host calls; these are empty.
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnHostAutoRepeat (Byte asciiChar)
{
    UNREFERENCED_PARAMETER (asciiChar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostKeyDown
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnHostKeyDown (Byte asciiChar)
{
    UNREFERENCED_PARAMETER (asciiChar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostKeyUp
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnHostKeyUp (Byte asciiChar)
{
    UNREFERENCED_PARAMETER (asciiChar);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskEventProbe::Attach
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskEventProbe::Attach (
    MachineHost                               & machine,
    std::vector<DebugEventEntry<Disk2Event>>  & entries)
{
    m_feed.AttachToEntries (machine, &entries, nullptr);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskEventProbe::Detach
//
////////////////////////////////////////////////////////////////////////////////

void DiskEventProbe::Detach (MachineHost & machine)
{
    m_feed.Detach (machine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputEventProbe::Attach
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InputEventProbe::Attach (
    MachineHost                               & machine,
    std::vector<DebugEventEntry<InputEvent>>  & entries)
{
    m_feed.AttachToEntries (machine, nullptr, &entries);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputEventProbe::Detach
//
////////////////////////////////////////////////////////////////////////////////

void InputEventProbe::Detach (MachineHost & machine)
{
    m_feed.Detach (machine);
}
