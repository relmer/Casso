#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Core/MachineConfig.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DiagnosticsSnapshot.h"
#include "Machines/Apple2/Common/IDiskPortHook.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::Disk2Controller
//
////////////////////////////////////////////////////////////////////////////////

Disk2Controller::Disk2Controller (int slot) :
    m_slot    (slot),
    m_ioStart (static_cast<Word> (0xC080 + slot * 16)),
    m_ioEnd   (static_cast<Word> (0xC08F + slot * 16))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::Read
//
//  The front end's byte, then the port hook's when the access is the
//  hook's.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2Controller::Read (Word address)
{
    int   offset = (address - m_ioStart) & 0x0F;
    Byte  value  = 0;
    Byte  port   = 0;



    value = m_iwmMode ? m_iwmPort.Access (offset, false, 0, m_portHook)
                      : m_card.Read (offset, m_portHook);

    if (m_portHook != nullptr && m_portHook->OnIwmAccess (offset, false, 0, port))
    {
        value = port;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::Write
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::Write (Word address, Byte value)
{
    int   offset  = (address - m_ioStart) & 0x0F;
    Byte  ignored = 0;



    if (m_iwmMode)
    {
        (void) m_iwmPort.Access (offset, true, value, m_portHook);
    }
    else
    {
        m_card.Write (offset, value, m_portHook);
    }

    if (m_portHook != nullptr)
    {
        m_portHook->OnIwmAccess (offset, true, value, ignored);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::TryPeekIo
//
//  Read's byte with no switch thrown and no drive moved on. In IWM mode the
//  peek is the //c's IWM as of its last access, with the port hook's byte.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2Controller::TryPeekIo (Word address, Byte & value)
{
    int  offset = (address - m_ioStart) & 0x0F;



    return m_iwmMode ? m_iwmPort.TryPeek (offset, m_portHook, value)
                     : m_card.TryPeek (offset, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::Tick
//
//  The front end's timing, then a turn for the idle callback, which is busy
//  during the card's spin-up.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::Tick (uint32_t cpuCycles)
{
    if (m_iwmMode)
    {
        m_iwmPort.Tick (cpuCycles);
    }
    else
    {
        m_card.Tick (cpuCycles);
    }

    m_signals.PumpIdle (cpuCycles, m_card.GetMotorSpinupRemaining() > 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::MountDisk
//
//  Loads a file into the drive's empty disk. The insert goes to the sinks
//  only after the load succeeds.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::MountDisk (int drive, const string & path)
{
    HRESULT  hr = S_OK;



    CBRAEx (drive >= 0 && drive < kDriveCount, E_INVALIDARG);

    hr = m_drives[(size_t) drive].Mount (path);
    CHR (hr);

    m_recorder.NoteDiskChange (drive);
    m_signals.OnDiskInserted (drive);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::EjectDisk
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::EjectDisk (int drive)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    m_drives[(size_t) drive].Eject();
    m_recorder.NoteDiskChange (drive);
    m_signals.OnDiskEjected (drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::SetExternalDisk
//
//  Points a drive at a disk the disk store has, so the store can write it
//  back on an eject, a machine switch, a power cycle or a shutdown. Every
//  call is a disk change for the recent nibbles.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SetExternalDisk (int drive, DiskImage * external)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    m_drives[(size_t) drive].SetExternalDisk (external);
    m_recorder.NoteDiskChange (drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::SetEventSink
//
//  Safe to call from the UI thread between CPU slices.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SetEventSink (ObserverSlot slot, IDisk2EventSink * sink) noexcept
{
    m_signals.SetEventSink (slot, sink);
    m_recorder.GetWatcher().SetEventSink (slot, sink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::Reset
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::Reset()
{
    m_lines.Reset();
    m_card.Reset();
    m_iwmPort.Reset();

    for (Disk2Drive & drive : m_drives)
    {
        drive.Reset();
    }

    m_recorder.GetWatcher().ResetState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::SoftReset
//
//  A //e soft reset clears the controller's hardware state, and the disks
//  stay in the drives. The disk store writes dirty images back first, from
//  MachineHost::SoftReset, so every safeguard on a write applies, the
//  reverse-execution pause and the replay flag among them.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SoftReset()
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::PowerCycle
//
//  Clears the hardware state and the recent nibbles, and puts each drive's
//  empty disk back in it. A disk attached through SetExternalDisk is
//  mounted in the disk store, which this call doesn't change;
//  MachineHost::PowerCycle reconnects the drives to it afterward.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::PowerCycle (Prng & prng)
{
    UNREFERENCED_PARAMETER (prng);

    Reset();
    m_recorder.GetNibbleRecord().Clear();

    for (Disk2Drive & drive : m_drives)
    {
        drive.Eject();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> Disk2Controller::Create (const DeviceConfig & config, MemoryBus & bus)
{
    constexpr int  kDefaultSlot = 6;
    int            slot         = config.hasSlot ? config.slot : kDefaultSlot;



    UNREFERENCED_PARAMETER (bus);

    return make_unique<Disk2Controller> (slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::GetDiagnostics
//
//  Q6 and Q7 together select what the sequencer does: read, sense the write
//  protect, shift out a write, or load the write latch. With the motor on,
//  Q7 is whether the drive is reading or writing. The last address field is
//  the one the watcher decoded last; a field not yet seen is "?".
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    static constexpr const char * kModes[]   = { "read", "sense write protect", "write", "load write latch" };
    constexpr int                    kQuarters  = 4;
    constexpr int                    kHundredth = 25;   // hundredths of a track in a quarter
    const Disk2Drive               & active     = m_lines.GetActive();
    const Disk2NibbleEngine        & engine     = active.GetEngine();
    const Disk2AddressMarkWatcher  & watcher    = m_recorder.GetWatcher();
    int                              quarter    = m_lines.GetQuarterTrack();
    int                              mode       = ((int) m_lines.IsQ7() << 1) | (int) m_lines.IsQ6();
    const char                  * activity   = !m_lines.IsMotorOn() ? "idle" : (m_lines.IsQ7() ? "writing" : "reading");
    DiagnosticsGroup              drive      { "Drive", {} };
    DiagnosticsGroup              head       { "Head", {} };
    DiagnosticsGroup              sequencer  { "Sequencer", {} };
    auto                          field      = [] (char prefix, int value) { return (value < 0) ? std::format ("{}?", prefix) : std::format ("{}{}", prefix, value); };



    drive.rows.push_back (MakeTextRow ("Active drive",   std::format ("{}", m_lines.GetActiveDrive() + 1)));
    drive.rows.push_back (MakeTextRow ("Activity",       activity));

    drive.rows.push_back (MakeFlagRow ("At speed",       m_card.IsMotorAtSpeed()));
    drive.rows.push_back (MakeTextRow ("Spin-up left",   std::format ("{} cycles", m_card.GetMotorSpinupRemaining())));
    drive.rows.push_back (MakeTextRow ("Spin-down left", std::format ("{} cycles", m_card.GetMotorSpindownCycles())));
    drive.rows.push_back (MakeFlagRow ("Disk in drive",  active.HasDisk()));

    head.rows.push_back (MakeTextRow ("Track",         std::format ("{}.{:02}", quarter / kQuarters, (quarter % kQuarters) * kHundredth)));
    head.rows.push_back (MakeTextRow ("Quarter track", std::format ("{}", quarter)));
    head.rows.push_back (MakeByteRow ("Phases",        m_lines.GetPhases(), { "", "", "", "", "PH3", "PH2", "PH1", "PH0" }));
    head.rows.push_back (MakeTextRow ("Last address field", std::format ("{} {} {}", field ('T', watcher.GetCachedTrack()),
                                                                                    field ('S', watcher.GetCachedSector()),
                                                                                    field ('V', watcher.GetCachedVolume()))));

    sequencer.rows.push_back (MakeTextRow ("Mode",            kModes[mode]));
    sequencer.rows.push_back (MakeFlagRow ("Q6",              m_lines.IsQ6()));
    sequencer.rows.push_back (MakeFlagRow ("Q7",              m_lines.IsQ7()));
    sequencer.rows.push_back (MakeHexRow  ("Read latch",      engine.PeekReadLatch(), kByteDigits));
    sequencer.rows.push_back (MakeTextRow ("Bit",             std::format ("{}", engine.GetBitPosition())));
    sequencer.rows.push_back (MakeTextRow ("Nibbles read",    std::format ("{}", engine.GetReadNibbles())));
    sequencer.rows.push_back (MakeTextRow ("Nibbles written", std::format ("{}", engine.GetWriteNibbles())));

    snapshot.groups.push_back (std::move (drive));
    snapshot.groups.push_back (std::move (head));
    snapshot.groups.push_back (std::move (sequencer));
    snapshot.visual = DiagnosticsDiskHead { quarter,
                                            Disk2Drive::kMaxQuarterTrack,
                                            m_lines.GetPhases(),
                                            m_lines.IsMotorOn(),
                                            m_lines.GetActiveDrive(),
                                            { m_drives[0].GetQuarterTrack(), m_drives[1].GetQuarterTrack() } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::SaveState
//
//  Each part's fields, in the order of the section's layout.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte   (m_lines.GetPhases());
    writer.WriteByte   (static_cast<Byte> (m_lines.GetPhase()));
    writer.WriteByte   (static_cast<Byte> (m_drives[0].GetQuarterTrack()));
    writer.WriteByte   (static_cast<Byte> (m_drives[1].GetQuarterTrack()));
    writer.WriteBool   (m_lines.IsMotorOn());
    writer.WriteUInt32 (m_card.GetMotorSpindownCycles());
    writer.WriteUInt32 (m_card.GetMotorSpinupRemaining());
    writer.WriteByte   (static_cast<Byte> (m_lines.GetActiveDrive()));
    writer.WriteBool   (m_lines.IsQ6());
    writer.WriteBool   (m_lines.IsQ7());
    writer.WriteBool   (m_iwmMode);
    writer.WriteByte   (m_iwmPort.GetIwm().GetMode());
    writer.WriteUInt32 (m_signals.GetCyclesSinceIdle());
    writer.WriteBool   (m_signals.IsBusySinceIdle());
    writer.WriteUInt64 (m_clock.GetLastSync());

    for (const Disk2Drive & drive : m_drives)
    {
        hr = drive.GetEngine().SaveState (writer);
        CHR (hr);
    }

    hr = m_recorder.GetWatcher().SaveState (writer);
    CHR (hr);

    hr = m_recorder.GetNibbleRecord().SaveState (writer);
    CHR (hr);

    m_iwmPort.SaveState (writer);

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::LoadState
//
//  Checks the phase mask, phase, head positions and drive select, and that
//  the saved machine had the same IWM wiring, then sets each part. The
//  engines, the watcher, the record and the IWM load inside this section
//  (LoadParts). Before version 3 there was one head position for both
//  drives, which each drive takes. Nothing goes through Reset or the switch
//  handlers, so no motor or drive change goes to a sink and the motor-off
//  flush does not run.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::LoadState (StateReader & reader)
{
    constexpr Byte  kPhaseMaskLimit = 1 << Disk2Drive::kPhaseCount;



    HRESULT   hr                      = S_OK;
    uint16_t  version                 = 0;
    Byte      phases                  = 0;
    Byte      phase                   = 0;
    Byte      quarterTrack            = 0;     // drive 1's head, or both drives' before version 3
    Byte      quarterTrack2           = 0;
    bool      motorOn                 = false;
    uint32_t  motorSpindownCycles     = 0;
    uint32_t  motorSpinupRemaining    = 0;
    Byte      activeDrive             = 0;
    bool      q6                      = false;
    bool      q7                      = false;
    bool      iwmMode                 = false;
    Byte      iwmModeReg              = 0;
    uint32_t  cyclesSinceIdleCallback = 0;
    bool      busySinceIdleCallback   = false;
    uint64_t  lastCpuSync             = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    CBREx (version != kUnreleasedStateVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    reader.ReadByte (phases);
    reader.ReadByte (phase);
    reader.ReadByte (quarterTrack);

    if (version >= kPerDriveHeadStateVersion)
    {
        reader.ReadByte (quarterTrack2);
    }
    else
    {
        quarterTrack2 = quarterTrack;
    }

    reader.ReadBool   (motorOn);
    reader.ReadUInt32 (motorSpindownCycles);
    reader.ReadUInt32 (motorSpinupRemaining);
    reader.ReadByte   (activeDrive);
    reader.ReadBool   (q6);
    reader.ReadBool   (q7);
    reader.ReadBool   (iwmMode);
    reader.ReadByte   (iwmModeReg);
    reader.ReadUInt32 (cyclesSinceIdleCallback);
    reader.ReadBool   (busySinceIdleCallback);
    reader.ReadUInt64 (lastCpuSync);

    hr = LoadParts (reader, version, quarterTrack);
    CHR (hr);

    hr = reader.EndSection();
    CHR (hr);

    CBREx (phases        <  kPhaseMaskLimit,              HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (phase         <  Disk2Drive::kPhaseCount,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (quarterTrack  <= Disk2Drive::kMaxQuarterTrack, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (quarterTrack2 <= Disk2Drive::kMaxQuarterTrack, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (activeDrive   <  kDriveCount,                  HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (iwmMode       == m_iwmMode,                    HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_lines.Restore (phases, phase, motorOn, activeDrive, q6, q7);
    m_drives[0].SetQuarterTrack (quarterTrack);
    m_drives[1].SetQuarterTrack (quarterTrack2);
    m_card.Restore (motorSpindownCycles, motorSpinupRemaining);
    m_signals.SetIdleWindow (cyclesSinceIdleCallback, busySinceIdleCallback);
    m_clock.SetLastSync (lastCpuSync);

    // The drive not selected when an older state was saved had its engine on
    // whatever track it was last selected on; both heads are at the one saved
    // position now.
    if (version < kPerDriveHeadStateVersion)
    {
        m_drives[(size_t) (activeDrive ^ 1)].GetEngine().SetCurrentTrack (quarterTrack);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::LoadParts
//
//  The sections nested in the controller's: both engines, then from version
//  2 the address-mark watcher (an older state starts it idle on the saved
//  track), from version 3 the recent nibbles, and from version 4 the IWM.
//  A //c state from before version 4 can't be put back, since it has no
//  IWM; another machine's has no IWM to put back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::LoadParts (StateReader & reader, uint16_t version, Byte quarterTrack)
{
    HRESULT  hr = S_OK;



    for (Disk2Drive & drive : m_drives)
    {
        hr = drive.GetEngine().LoadState (reader);
        CHR (hr);
    }

    if (version >= kWatcherStateVersion)
    {
        hr = m_recorder.GetWatcher().LoadState (reader);
        CHR (hr);
    }
    else
    {
        m_recorder.GetWatcher().ResetState();
        m_recorder.GetWatcher().NoteHeadMoved (quarterTrack);
    }

    if (version >= kNibbleRecordStateVersion)
    {
        hr = m_recorder.GetNibbleRecord().LoadState (reader);
        CHR (hr);
    }
    else
    {
        m_recorder.GetNibbleRecord().Clear();
    }

    CBREx (version >= kIwmStateVersion || !m_iwmMode, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (version >= kIwmStateVersion)
    {
        hr = m_iwmPort.LoadState (reader);
        CHR (hr);
    }

Error:
    return hr;
}
