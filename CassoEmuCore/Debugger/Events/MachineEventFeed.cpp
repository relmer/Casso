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
    MachineHost    & machine,
    const Caches   & caches,
    RecordingTest    isRecording)
{
    m_diskCache   = caches.disk;
    m_inputCache  = caches.input;
    m_videoCache   = caches.video;
    m_soundCache   = caches.sound;
    m_serialCache  = caches.serial;
    m_networkCache = caches.network;
    m_diskOut      = nullptr;
    m_inputOut     = nullptr;
    m_videoOut     = nullptr;
    m_soundOut     = nullptr;
    m_serialOut    = nullptr;
    m_networkOut   = nullptr;
    m_isRecording  = std::move (isRecording);

    Attach (machine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachToEntries
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::AttachToEntries (
    MachineHost    & machine,
    const Entries  & entries)
{
    m_diskCache   = nullptr;
    m_inputCache  = nullptr;
    m_videoCache   = nullptr;
    m_soundCache   = nullptr;
    m_serialCache  = nullptr;
    m_networkCache = nullptr;
    m_diskOut      = entries.disk;
    m_inputOut     = entries.input;
    m_videoOut     = entries.video;
    m_soundOut     = entries.sound;
    m_serialOut    = entries.serial;
    m_networkOut   = entries.network;
    m_isRecording  = nullptr;

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
    bool               isIo       = m_videoCache != nullptr || m_soundCache != nullptr || m_videoOut != nullptr || m_soundOut != nullptr;
    bool               isComm     = m_serialCache != nullptr || m_networkCache != nullptr || m_serialOut != nullptr || m_networkOut != nullptr;



    m_machine  = &machine;
    m_position = machine.GetPositionPtr();
    m_drive    = (controller != nullptr) ? controller->GetActiveDrive() : 0;

    m_decoder.Seed (machine);
    m_decoder.GetFilter (m_ioFilter);

    machine.AttachObservers ({ this, this, isIo ? this : nullptr, isIo ? &m_ioFilter : nullptr, isComm ? this : nullptr }, ObserverSlot::History);
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
    m_position    = nullptr;
    m_diskCache   = nullptr;
    m_inputCache  = nullptr;
    m_videoCache   = nullptr;
    m_soundCache   = nullptr;
    m_serialCache  = nullptr;
    m_networkCache = nullptr;
    m_diskOut      = nullptr;
    m_inputOut     = nullptr;
    m_videoOut     = nullptr;
    m_soundOut     = nullptr;
    m_serialOut    = nullptr;
    m_networkOut   = nullptr;
    m_isRecording  = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reseed
//
//  After the machine's state is replaced: a state loaded from a keyframe
//  selects its drive with no OnDriveSelect, so the drive is read again.
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::Reseed()
{
    Disk2Controller  * controller = nullptr;



    if (m_machine == nullptr)
    {
        return;
    }

    controller = m_machine->GetRefs().diskController;
    m_drive    = (controller != nullptr) ? controller->GetActiveDrive() : 0;

    m_decoder.Seed (*m_machine);
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
//  A sector's bytes are copied once, into a block the entry shares.
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::EmitDisk (
    Disk2EventType                type,
    int                           drive,
    const Disk2Event::Payload   & payload,
    std::span<const Byte>         bytes)
{
    uint64_t                                  position = 0;
    uint64_t                                  cycle    = 0;
    Disk2Event                                event    = {};
    std::shared_ptr<const std::vector<Byte>>  data;



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

    if (m_diskOut == nullptr && !IsRecording())
    {
        return;
    }

    if (!bytes.empty())
    {
        data = std::make_shared<const std::vector<Byte>> (bytes.begin(), bytes.end());
    }

    if (m_diskOut != nullptr)
    {
        m_diskOut->push_back (DiskEntry { DebugEventEntryKind::Event, position, cycle, event, std::move (data) });
    }
    else
    {
        m_diskCache->Append (position, cycle, event, std::move (data));
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
//  EmitIo
//
//  A video entry goes to the video log and the rest to the sound log.
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::EmitIo (const IoEvent & event)
{
    bool                        isVideo  = event.kind == IoEventKind::VideoMode;
    DebugEventCache<IoEvent>  * cache    = isVideo ? m_videoCache : m_soundCache;
    std::vector<IoEntry>      * out      = isVideo ? m_videoOut   : m_soundOut;
    uint64_t                    position = *m_position;



    if (out != nullptr)
    {
        out->push_back (IoEntry { DebugEventEntryKind::Event, position, event.cycle, event });
    }
    else if (cache != nullptr && IsRecording())
    {
        cache->Append (position, event.cycle, event);
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

void MachineEventFeed::OnDataMarkRead (int track, int sector, int volume, std::span<const Byte> bytes)
{
    Disk2Event::Payload  payload = {};



    payload.dataMark = { track, sector, volume, (int) bytes.size() };

    EmitDisk (Disk2EventType::DataRead, m_drive, payload, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDataMarkWrite
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDataMarkWrite (int track, int sector, int volume, std::span<const Byte> bytes)
{
    Disk2Event::Payload  payload = {};



    payload.dataMark = { track, sector, volume, (int) bytes.size() };

    EmitDisk (Disk2EventType::DataWrite, m_drive, payload, bytes);
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
//  OnAddressChecksumError
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnAddressChecksumError (int track, int sector, int volume)
{
    Disk2Event::Payload  payload = {};



    payload.addrMark = { track, sector, volume };

    EmitDisk (Disk2EventType::AddrChecksumError, m_drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDataChecksumError
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDataChecksumError (int track, int sector, int volume, std::span<const Byte> bytes)
{
    Disk2Event::Payload  payload = {};



    payload.dataMark = { track, sector, volume, (int) bytes.size() };

    EmitDisk (Disk2EventType::DataChecksumError, m_drive, payload, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDataFieldCutOff
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDataFieldCutOff (int track, int sector, int volume)
{
    Disk2Event::Payload  payload = {};



    payload.addrMark = { track, sector, volume };

    EmitDisk (Disk2EventType::DataCutOff, m_drive, payload);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDataWithoutAddress
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnDataWithoutAddress (std::span<const Byte> bytes)
{
    Disk2Event::Payload  payload = {};



    payload.dataMark = { -1, -1, -1, (int) bytes.size() };

    EmitDisk (Disk2EventType::DataNoAddress, m_drive, payload, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnWriteProtected
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnWriteProtected()
{
    EmitDisk (Disk2EventType::WriteProtected, m_drive, Disk2Event::Payload {});
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
//  OnIoAccess
//
//  The decode follows every access, recording or not, so the video mode
//  and the card's ports are current when recording resumes.
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnIoAccess (Word address, Byte value, bool isWrite)
{
    size_t  count = 0;
    size_t  i     = 0;



    if (m_machine == nullptr)
    {
        return;
    }

    count = m_decoder.Decode (*m_machine, address, value, isWrite, m_ioEvents);


    for (i = 0; i < count; i++)
    {
        EmitIo (m_ioEvents[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineEventFeed::OnCommEvent
//
//  Stamped with the machine's position and cycle; a socket's data is
//  copied once, into a block the entry shares.
//
////////////////////////////////////////////////////////////////////////////////

void MachineEventFeed::OnCommEvent (
    const IoEvent          & event,
    std::span<const Byte>    bytes)
{
    bool                                      isSerial = event.kind < IoEventKind::SocketState;
    DebugEventCache<IoEvent>                * cache    = isSerial ? m_serialCache : m_networkCache;
    std::vector<IoEntry>                    * out      = isSerial ? m_serialOut   : m_networkOut;
    IoEvent                                   stamped  = event;
    uint64_t                                  position = 0;
    std::shared_ptr<const std::vector<Byte>>  data;



    if (m_machine == nullptr || (cache == nullptr && out == nullptr) || (out == nullptr && !IsRecording()))
    {
        return;
    }

    position      = *m_position;
    stamped.cycle = m_machine->GetCpu()->GetTotalCycles();

    if (!bytes.empty())
    {
        data = std::make_shared<const std::vector<Byte>> (bytes.begin(), bytes.end());
    }

    if (out != nullptr)
    {
        out->push_back (IoEntry { DebugEventEntryKind::Event, position, stamped.cycle, stamped, std::move (data) });
    }
    else
    {
        cache->Append (position, stamped.cycle, stamped, std::move (data));
    }
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
    m_feed.AttachToEntries (machine, { .disk = &entries });

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
    m_feed.AttachToEntries (machine, { .input = &entries });

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





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventProbe::Attach
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IoEventProbe::Attach (
    MachineHost                            & machine,
    std::vector<DebugEventEntry<IoEvent>>  & entries)
{
    MachineEventFeed::Entries  outputs;



    switch (m_kind)
    {
    case IoLogKind::Video:   outputs.video   = &entries; break;
    case IoLogKind::Sound:   outputs.sound   = &entries; break;
    case IoLogKind::Serial:  outputs.serial  = &entries; break;
    case IoLogKind::Network: outputs.network = &entries; break;
    }

    m_feed.AttachToEntries (machine, outputs);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventProbe::Detach
//
////////////////////////////////////////////////////////////////////////////////

void IoEventProbe::Detach (MachineHost & machine)
{
    m_feed.Detach (machine);
}
