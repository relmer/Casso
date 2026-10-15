#pragma once

#include "Pch.h"

#include "Core/IIoAccessSink.h"
#include "Debugger/Events/DebugEventCache.h"
#include "Debugger/Events/DebugEventJob.h"
#include "Debugger/Events/IoEventDecoder.h"
#include "Debugger/IoEventLog.h"
#include "Devices/IInputEventSink.h"
#include "Devices/InputEvent.h"
#include "Machines/Apple2/Common/Disk2Event.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineEventFeed
//
//  A machine's Disk II, input and I/O feeds as debug events, from the
//  History slot: each call becomes a Disk2Event, an InputEvent, or the
//  video or sound log's IoEvent (IoEventDecoder), stamped with the
//  machine's position and cycle. The bus reports I/O to the feed only
//  while it has somewhere for the video or sound log's events to go.
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

class MachineEventFeed : public IDisk2EventSink, public IInputEventSink, public IIoAccessSink
{
public:
    using DiskEntry     = DebugEventEntry<Disk2Event>;
    using InputEntry    = DebugEventEntry<InputEvent>;
    using IoEntry       = DebugEventEntry<IoEvent>;
    using RecordingTest = std::function<bool()>;

    //  Where each kind of event goes; a kind with none is dropped.
    struct Caches
    {
        DebugEventCache<Disk2Event>  * disk  = nullptr;
        DebugEventCache<InputEvent>  * input = nullptr;
        DebugEventCache<IoEvent>     * video = nullptr;
        DebugEventCache<IoEvent>     * sound = nullptr;
    };

    struct Entries
    {
        std::vector<DiskEntry>   * disk  = nullptr;
        std::vector<InputEntry>  * input = nullptr;
        std::vector<IoEntry>     * video = nullptr;
        std::vector<IoEntry>     * sound = nullptr;
    };

    void  AttachToCaches  (MachineHost & machine, const Caches & caches, RecordingTest isRecording);
    void  AttachToEntries (MachineHost & machine, const Entries & entries);
    void  Detach          (MachineHost & machine);

    //  The video mode and the Mockingboard's ports are taken from the machine
    //  again, after its state was replaced without the accesses that made it.
    void  Reseed          ();

    // IDisk2EventSink
    void  OnMotorCommandOn() override;
    void  OnMotorEngaged() override;
    void  OnMotorCommandOff() override;
    void  OnMotorDisengaged() override;
    void  OnHeadStep        (int prevQt, int newQt) override;
    void  OnHeadBump        (int atQt) override;
    void  OnAddressMark          (int track, int sector, int volume) override;
    void  OnDataMarkRead         (int track, int sector, int volume, std::span<const Byte> bytes) override;
    void  OnDataMarkWrite        (int track, int sector, int volume, std::span<const Byte> bytes) override;
    void  OnReadStall            (int quarterTrack) override;
    void  OnAddressChecksumError (int track, int sector, int volume) override;
    void  OnDataChecksumError    (int track, int sector, int volume, std::span<const Byte> bytes) override;
    void  OnDataFieldCutOff      (int track, int sector, int volume) override;
    void  OnDataWithoutAddress   (std::span<const Byte> bytes) override;
    void  OnWriteProtected       () override;
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

    // IIoAccessSink
    void  OnIoAccess       (Word address, Byte value, bool isWrite) override;

private:
    void  Attach      (MachineHost & machine);
    bool  IsRecording () const;
    void  EmitDisk    (Disk2EventType type, int drive, const Disk2Event::Payload & payload, std::span<const Byte> bytes = {});
    void  EmitInput   (InputEventType type, Word address, Byte value, uint8_t flags);
    void  EmitIo      (const IoEvent & event);

    MachineHost                    * m_machine    = nullptr;
    const uint64_t                 * m_position   = nullptr;
    DebugEventCache<Disk2Event>    * m_diskCache  = nullptr;
    DebugEventCache<InputEvent>    * m_inputCache = nullptr;
    std::vector<DiskEntry>         * m_diskOut    = nullptr;
    std::vector<InputEntry>        * m_inputOut   = nullptr;
    DebugEventCache<IoEvent>       * m_videoCache = nullptr;
    DebugEventCache<IoEvent>       * m_soundCache = nullptr;
    std::vector<IoEntry>           * m_videoOut   = nullptr;
    std::vector<IoEntry>           * m_soundOut   = nullptr;
    RecordingTest                    m_isRecording;
    IoEventDecoder                   m_decoder;
    IoEventDecoder::Events           m_ioEvents;
    IoAccessFilter                   m_ioFilter;
    int                              m_drive      = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskEventProbe, InputEventProbe, IoEventProbe
//
//  The feed as a rebuild's probe, for one kind of event each, the video log
//  or the sound log for IoEventProbe. The read edges are part of the
//  keyframe a replay loads, so a replay reports the reads the running
//  machine reported after it; the I/O decode is seeded from the keyframe.
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


class IoEventProbe : public IDebugEventProbe<IoEvent>
{
public:
    explicit IoEventProbe (bool isVideo) : m_isVideo (isVideo) {}

    HRESULT  Attach (MachineHost & machine, std::vector<DebugEventEntry<IoEvent>> & entries) override;
    void     Detach (MachineHost & machine) override;

private:
    MachineEventFeed  m_feed;
    bool              m_isVideo = false;
};
