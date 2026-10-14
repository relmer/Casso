#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventCache.h"
#include "Debugger/Events/DebugEventJob.h"
#include "Devices/IInputEventSink.h"
#include "Devices/InputEvent.h"
#include "Machines/Apple2/Common/Disk2Event.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineEventFeed
//
//  A machine's Disk II and input event feeds as debug events, from the
//  History slot (FR-211): each call becomes a Disk2Event or an InputEvent
//  stamped with the machine's position and cycle.
//
//  On the running machine the events go to the event caches, but only while
//  the recording test returns true, which the debugger makes true while the
//  machine is live and recording history: what the machine does in a replay
//  of its own history, or in a gap in recording, is not cached. On a machine
//  replaying history the events go to the entries of the stretch.
//
//  A Disk II event's drive is the controller's active drive, read at attach
//  and followed by drive selects after it. The History slot reports the
//  guest's reads alone, so the input events are the guest's; the host's
//  input comes from the input journal.
//
////////////////////////////////////////////////////////////////////////////////

class MachineEventFeed : public IDisk2EventSink, public IInputEventSink
{
public:
    using DiskEntry     = DebugEventEntry<Disk2Event>;
    using InputEntry    = DebugEventEntry<InputEvent>;
    using RecordingTest = std::function<bool()>;

    void  AttachToCaches  (MachineHost & machine, DebugEventCache<Disk2Event> & disk, DebugEventCache<InputEvent> & input, RecordingTest isRecording);
    void  AttachToEntries (MachineHost & machine, std::vector<DiskEntry> * disk, std::vector<InputEntry> * input);
    void  Detach          (MachineHost & machine);

    // IDisk2EventSink
    void  OnMotorCommandOn() override;
    void  OnMotorEngaged() override;
    void  OnMotorCommandOff() override;
    void  OnMotorDisengaged() override;
    void  OnHeadStep        (int prevQt, int newQt) override;
    void  OnHeadBump        (int atQt) override;
    void  OnAddressMark     (int track, int sector, int volume) override;
    void  OnDataMarkRead    (int track, int sector, int volume, int byteCount) override;
    void  OnDataMarkWrite   (int track, int sector, int volume, int byteCount) override;
    void  OnReadStall       (int quarterTrack) override;
    void  OnDriveSelect     (int drive) override;
    void  OnDiskInserted    (int drive) override;
    void  OnDiskEjected     (int drive) override;

    // IInputEventSink: the History slot reports the guest's reads alone.
    void  OnKbdDataRead    (Word address, Byte value, bool strobeSet) override;
    void  OnKbdStrobe      (Word address, Byte value, bool clearedStrobe) override;
    void  OnButtonRead     (Word address, Byte value) override;
    void  OnPaddleTrigger  (Word address) override;
    void  OnPaddleRead     (Word address, Byte value) override;
    void  OnHostAutoRepeat (Byte asciiChar) override;
    void  OnHostKeyDown    (Byte asciiChar) override;
    void  OnHostKeyUp      (Byte asciiChar) override;

private:
    void  Attach      (MachineHost & machine);
    bool  IsRecording () const;
    void  EmitDisk    (Disk2EventType type, int drive, const Disk2Event::Payload & payload);
    void  EmitInput   (InputEventType type, Word address, Byte value, uint8_t flags);

    MachineHost                    * m_machine    = nullptr;
    DebugEventCache<Disk2Event>    * m_diskCache  = nullptr;
    DebugEventCache<InputEvent>    * m_inputCache = nullptr;
    std::vector<DiskEntry>         * m_diskOut    = nullptr;
    std::vector<InputEntry>        * m_inputOut   = nullptr;
    RecordingTest                    m_isRecording;
    int                              m_drive      = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskEventProbe, InputEventProbe
//
//  The feed as a rebuild's probe, for one kind of event each. The input
//  probe resets the read edges at attach, as the running machine resets
//  them at each keyframe, so a replay from a keyframe reports the reads the
//  running machine reported after it.
//
////////////////////////////////////////////////////////////////////////////////

class DiskEventProbe : public IDebugEventProbe<Disk2Event>
{
public:
    HRESULT  Attach (MachineHost & machine, std::vector<DebugEventEntry<Disk2Event>> & entries) override;
    void     Detach (MachineHost & machine) override;

private:
    MachineEventFeed  m_feed;
};


class InputEventProbe : public IDebugEventProbe<InputEvent>
{
public:
    HRESULT  Attach (MachineHost & machine, std::vector<DebugEventEntry<InputEvent>> & entries) override;
    void     Detach (MachineHost & machine) override;

private:
    MachineEventFeed  m_feed;
};
