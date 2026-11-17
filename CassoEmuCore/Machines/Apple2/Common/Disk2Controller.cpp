#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Audio/IDriveAudioSink.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Core/MachineConfig.h"
#include "Debugger/DiagnosticsSnapshot.h"




//  The drive select, Q6 and Q7: the switches the IWM has whatever drive is
//  enabled.
static constexpr int  s_kFirstSelectSwitch = 0xA;





////////////////////////////////////////////////////////////////////////////////
//
//  Phase-mask → energized-phase resolution
//
//  Real Disk II steppers read the 4-bit phase mask and walk the head one
//  quarter-track per energized neighbor. Phase 9 keeps the simpler
//  "highest-set phase chooses direction" model from Phase 8 — sufficient
//  for the tests; precise quarter-track stepping arrives
//  with Phase 11.
//
////////////////////////////////////////////////////////////////////////////////

static int FindHighestPhase (uint8_t phases)
{
    int   i      = 0;
    int   result = -1;



    for (i = 0; i < Disk2Controller::kPhaseCount; i++)
    {
        if (phases & (1 << i))
        {
            result = i;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller
//
////////////////////////////////////////////////////////////////////////////////

Disk2Controller::Disk2Controller (int slot)
    : m_slot    (slot),
      m_ioStart (static_cast<Word> (0xC080 + slot * 16)),
      m_ioEnd   (static_cast<Word> (0xC08F + slot * 16))
{
    m_activeDisk[0] = &m_disks[0];
    m_activeDisk[1] = &m_disks[1];
    m_engine[0].SetDiskImage (m_activeDisk[0]);
    m_engine[1].SetDiskImage (m_activeDisk[1]);
    m_iwm.AttachDrive (0, &m_iwmDrives[0]);
    m_iwm.AttachDrive (1, &m_iwmDrives[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Apply soft-switch side effects then dispatch the data path per Q6/Q7.
//  With the drives not enabled, only the drive select, Q6 and Q7 change.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2Controller::Read (Word address)
{
    int   offset  = (address - m_ioStart) & 0x0F;
    Byte  value   = 0;
    Byte  port    = 0;
    bool  enabled = false;



    if (m_iwmMode)
    {
        value = AccessIwm (offset, false, 0);
    }
    else
    {
        CatchUpToCpu();

        enabled = PollFloppyEnable();

        if (enabled || offset >= s_kFirstSelectSwitch)
        {
            HandleSwitch (offset);
        }

        if (enabled)
        {
            value = HandleReadDispatch();
        }
    }

    if (m_portHook != nullptr && m_portHook->OnIwmAccess (offset, false, 0, port))
    {
        value = port;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
//  Q6, Q7 and the drive as they are after the switch at address, then the
//  byte HandleReadDispatch returns for those: the data latch, run forward
//  to the CPU's cycle on a copy of the drive; the sense line; or 0 with Q7
//  on. The //c's IWM has no peek: a read runs its drives up to the CPU's
//  time, and nothing runs a copy of them.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2Controller::TryPeekIo (Word address, Byte & value)
{
    int       offset = (address - m_ioStart) & 0x0F;
    bool      q6     = m_q6;
    bool      q7     = m_q7;
    int       drive  = m_activeDrive;
    uint64_t  delta  = 0;
    Byte      sense  = 0;



    if (m_iwmMode)
    {
        value = 0;
        return false;
    }

    switch (offset)
    {
        case 0xA: drive = 0;     break;
        case 0xB: drive = 1;     break;
        case 0xC: q6    = false; break;
        case 0xD: q6    = true;  break;
        case 0xE: q7    = false; break;
        case 0xF: q7    = true;  break;
        default:                 break;
    }

    value = 0;

    if (q7)
    {
        return true;
    }

    if (q6)
    {
        sense = m_activeDisk[drive]->IsWriteProtected() ? 0x80 : 0x00;
        value = m_iwmMode ? static_cast<Byte> (sense | ((m_motorOn && m_motorSpindownCycles == 0) ? 0x20 : 0x00) | (m_iwm.GetMode() & 0x1F))
                          : sense;
        return true;
    }

    if (m_engine[drive].IsWriteMode())
    {
        return false;
    }

    if (drive == m_activeDrive && m_cpuCycleSource != nullptr && *m_cpuCycleSource > m_lastCpuSync)
    {
        delta = *m_cpuCycleSource - m_lastCpuSync;
    }

    //  A motor turned on from rest starts the spin-up, which reads $80.
    value = (m_motorSpinupRemaining > 0 || (offset == 0x9 && !m_motorOn)) ? 0x80 : PeekLatch (drive, delta);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PeekLatch
//
//  The copy is taken again when the drive or the access it was taken at
//  differs, when the drive has moved in any way since (IsSameReadState), or
//  when the peek is for an earlier cycle than the copy has been run on to.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2Controller::PeekLatch (int drive, uint64_t delta)
{
    bool  isSame = m_peekDrive == drive && m_peekSync == m_lastCpuSync && delta >= m_peekDelta &&
                   m_engine[drive].IsSameReadState (m_peekBase);



    if (!isSame)
    {
        m_peekBase   = m_engine[drive].MakePeekCopy();
        m_peekEngine = m_peekBase;
        m_peekDrive  = drive;
        m_peekSync   = m_lastCpuSync;
        m_peekDelta  = 0;
    }

    m_peekEngine.AdvancePeekCopy (static_cast<uint32_t> (delta - m_peekDelta));
    m_peekDelta = delta;

    return m_peekEngine.PeekReadLatch();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Soft-switch side effects fire on writes too. When Q7=1 + Q6=1, the
//  written value loads the engine's write latch (data field write path).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::Write (Word address, Byte value)
{
    int   offset  = (address - m_ioStart) & 0x0F;
    bool  enabled = false;



    if (m_iwmMode)
    {
        (void) AccessIwm (offset, true, value);
    }
    else
    {
        CatchUpToCpu();

        enabled = PollFloppyEnable();

        if (enabled || offset >= s_kFirstSelectSwitch)
        {
            HandleSwitch (offset);
        }

        if (enabled && m_q7 && m_q6)
        {
            m_engine[m_activeDrive].WriteLatch (value);
            m_addrMarkWatcher.ObserveWrittenNibble (value);
            CountLatchLoad();

            if (m_motorOn)
            {
                OnLatchLoad (m_activeDrive, value, GetQuarterTrack(), m_activeDisk[m_activeDrive]->IsWriteProtected());
            }
        }
    }

    if (m_portHook != nullptr)
    {
        Byte  ignored = 0;



        m_portHook->OnIwmAccess (offset, true, value, ignored);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AccessIwm
//
//  An access to the //c's IWM. With the drives not enabled, only the drive
//  select, Q6 and Q7 change, and a read returns 0. A write then goes to the
//  device that has the port, so it doesn't load the mode register. A byte
//  the IWM finished reading goes to the address mark watcher and the access
//  log.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2Controller::AccessIwm (int offset, bool isWrite, Byte value)
{
    bool  enabled = m_portHook == nullptr || m_portHook->AreFloppiesEnabled();
    Byte  result  = 0;
    Byte  fresh   = 0;



    if (!enabled && m_iwm.IsEnabled())
    {
        m_iwm.Disable();
    }

    if (enabled)
    {
        result = m_iwm.Access (offset, isWrite, value, GetIwmNow());
    }
    else if (offset >= s_kFirstSelectSwitch)
    {
        (void) m_iwm.Access (offset, false, 0, GetIwmNow());
    }

    m_activeDrive = m_iwm.GetDrive();
    m_q6          = m_iwm.IsQ6();
    m_q7          = m_iwm.IsWriting();

    if (enabled && m_eventSinks.IsAny())
    {
        if (offset == 0x8)
        {
            m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnMotorCommandOff(); });
        }
        else if (offset == 0x9)
        {
            m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnMotorCommandOn(); });
        }
        else if (offset == 0xA || offset == 0xB)
        {
            m_eventSinks.ForEach ([this] (IDisk2EventSink & sink) { sink.OnDriveSelect (m_activeDrive); });
        }
    }

    SyncIwmMotor();

    if (enabled && isWrite && m_q6 && m_q7 && m_iwm.IsMotorSwitchOn())
    {
        m_engine[m_activeDrive].WriteLatch (value);
        m_addrMarkWatcher.ObserveWrittenNibble (value);
        CountLatchLoad();
        OnLatchLoad (m_activeDrive, value, GetQuarterTrack(), m_activeDisk[m_activeDrive]->IsWriteProtected());
    }

    if (enabled && !isWrite && !m_q6 && !m_q7 && m_iwm.ConsumeFreshByte (fresh))
    {
        m_engine[m_activeDrive].CountReadNibble();
        m_addrMarkWatcher.ObserveNibble (fresh, (m_cpuCycleSource != nullptr) ? *m_cpuCycleSource : 0, GetInstructionAddress());
        m_access[m_activeDrive].ObserveRead (m_quarterTrack[m_activeDrive], fresh, static_cast<uint32_t> (m_engine[m_activeDrive].GetBitPosition()),
                                             static_cast<uint32_t> (m_engine[m_activeDrive].GetCurrentTrackBits()));

        if (m_activeDisk[m_activeDrive]->IsLoaded())
        {
            m_nibbleRecord.AddRead (m_activeDrive, fresh, 0, GetInstructionAddress(), m_engine[m_activeDrive].GetReadNibbles());
        }
    }

    return enabled ? result : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetIwmNow
//
//  The IWM's time, moved on by the CPU's cycles since the last access. With
//  no cycle source, Tick moves it.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Disk2Controller::GetIwmNow()
{
    uint64_t  now = 0;



    if (m_cpuCycleSource != nullptr)
    {
        now = *m_cpuCycleSource;

        if (now > m_lastCpuSync)
        {
            m_iwmCycles += now - m_lastCpuSync;
        }

        m_lastCpuSync = now;
    }

    return m_iwmCycles * kIwmNsPerCycle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickIwm
//
//  While a drive is enabled, its cells move with the CPU, so its motor stops
//  when the IWM's timer runs out.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::TickIwm (uint32_t cpuCycles)
{
    if (m_cpuCycleSource == nullptr)
    {
        m_iwmCycles += cpuCycles;
    }

    if (m_iwm.IsEnabled())
    {
        m_iwm.AdvanceTo (GetIwmNow());
        SyncIwmMotor();
    }

    PumpIdleCallback (cpuCycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnIwmPhases
//
//  Each phase line that changed steps the drive's head.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::OnIwmPhases (int drive, uint8_t phases)
{
    uint8_t  changed = static_cast<uint8_t> (phases ^ m_phases);



    m_activeDrive = drive;

    for (int phase = 0; phase < kPhaseCount; phase++)
    {
        if ((changed & (1 << phase)) != 0)
        {
            HandlePhase (phase, (phases & (1 << phase)) != 0);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnIwmEnable
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::OnIwmEnable (int drive, bool isEnabled)
{
    m_iwmEnabled[(size_t) drive] = isEnabled;
    m_engine[drive].SetMotorOn (isEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncIwmMotor
//
//  The motor runs while either drive is enabled. Checked after an access
//  rather than at each enable, since a drive select disables one drive
//  before it enables the other.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SyncIwmMotor()
{
    bool  isOn = m_iwmEnabled[0] || m_iwmEnabled[1];



    if (isOn && !m_motorOn)
    {
        m_motorOn = true;

        if (m_audioSink != nullptr)
        {
            m_audioSink->OnMotorEngaged();
        }

        m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnMotorEngaged(); });
    }
    else if (!isOn && m_motorOn)
    {
        m_motorOn = false;
        m_access[0].EndRead();
        m_access[1].EndRead();

        if (m_audioSink != nullptr)
        {
            m_audioSink->OnMotorDisengaged();
        }

        m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnMotorDisengaged(); });

        if (m_motorOffFlushCallback)
        {
            m_motorOffFlushCallback();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IwmDrive::SetPhases
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::IwmDrive::SetPhases (bool ph0, bool ph1, bool ph2, bool ph3)
{
    m_controller.OnIwmPhases (m_drive, static_cast<uint8_t> ((ph0 ? 1 : 0) | (ph1 ? 2 : 0) | (ph2 ? 4 : 0) | (ph3 ? 8 : 0)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IwmDrive::GetSense
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2Controller::IwmDrive::GetSense() const
{
    return m_controller.m_activeDisk[m_drive]->IsWriteProtected();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IwmDrive::ReadCell
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2Controller::IwmDrive::ReadCell (uint64_t cellEndNs)
{
    UNREFERENCED_PARAMETER (cellEndNs);

    return m_controller.m_engine[m_drive].StepCell (false, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IwmDrive::WriteCell
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::IwmDrive::WriteCell (Byte bit, uint64_t cellEndNs)
{
    UNREFERENCED_PARAMETER (cellEndNs);

    (void) m_controller.m_engine[m_drive].StepCell (true, bit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IwmDrive::SetEnabled
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::IwmDrive::SetEnabled (bool isEnabled)
{
    m_controller.OnIwmEnable (m_drive, isEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PollFloppyEnable
//
//  Whether the drives are enabled for this access: always, with no port
//  hook. A drive whose enable drops stops at the next tick, without the
//  spin-down a motor-off command starts.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2Controller::PollFloppyEnable()
{
    bool  enabled = m_portHook == nullptr || m_portHook->AreFloppiesEnabled();



    if (!enabled && m_motorOn)
    {
        m_motorSpindownCycles = 1;
    }

    return enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLatchLoad
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::OnLatchLoad (int drive, Byte nibble, int quarterTrack, bool isDropped)
{
    Byte  flags = static_cast<Byte> (Disk2NibbleRecord::kWritten | (isDropped ? Disk2NibbleRecord::kDropped : 0));



    UNREFERENCED_PARAMETER (quarterTrack);

    if (m_activeDisk[drive]->IsLoaded())
    {
        m_nibbleRecord.Add (drive, nibble, flags, GetInstructionAddress());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleSwitch
//
//  Decode the 16-byte slot soft-switch page into the controller's state
//  machine: phase magnets, motor, drive select, Q6/Q7 latches.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::HandleSwitch (int offset)
{
    m_switchAccesses++;

    switch (offset)
    {
        case 0x0: HandlePhase (0, false); break;
        case 0x1: HandlePhase (0, true);  break;
        case 0x2: HandlePhase (1, false); break;
        case 0x3: HandlePhase (1, true);  break;
        case 0x4: HandlePhase (2, false); break;
        case 0x5: HandlePhase (2, true);  break;
        case 0x6: HandlePhase (3, false); break;
        case 0x7: HandlePhase (3, true);  break;
        case 0x8:
            // Motor-off command: real Disk II keeps the disk physically
            // spinning for ~1 second after this so DOS RWTS retries and
            // back-to-back command sequences don't lose rotational
            // sync (UTAIIe ch. 9). Arm a spindown timer rather than
            // killing the engine immediately; the visible m_motorOn
            // flag flips once the timer expires in Tick().
            if (m_motorOn && m_motorSpindownCycles == 0)
            {
                m_motorSpindownCycles = kMotorSpindownCycles;
            }

            UpdateEngineWriteMode();

            // Spec-006 FR-006: every $C0E8 strobe is a logical
            // "motor command off" event, including no-op strobes
            // (when the motor is already off or already spinning
            // down). OnMotorDisengaged fires later from Tick() when
            // the spindown timer actually expires.
            if (m_eventSinks.IsAny())
            {
                m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnMotorCommandOff(); });
            }

            break;
        case 0x9:
            // Motor-on command: cancel any pending spindown so the
            // engine keeps producing nibbles continuously across the
            // motor-off / motor-on toggle DOS issues between sectors.
            //
            // Audio sink (FR-001): fire OnMotorEngaged only on the
            // off->on edge so brief intra-sector toggles inside the
            // spindown window don't restart the motor sound. (Renamed
            // from OnMotorStart in spec-006 to align with the new
            // four-event motor lifecycle on IDisk2EventSink.)
            //
            // Issue #67: on a true off->on edge (motor genuinely cold,
            // not just inside the spindown window), arm the spin-up
            // window so reads return zeros for ~70 ms while the
            // physical disk reaches reading speed. During spindown
            // the disk is still spinning at 300 RPM, so a motor-on
            // that merely cancels a pending spindown does NOT need
            // a fresh spin-up.
            {
                bool  edge = (!m_motorOn);

                m_motorSpindownCycles = 0;
                m_motorOn = true;
                m_engine[m_activeDrive].SetMotorOn (true);
                UpdateEngineWriteMode();

                //  The drive enabled now, the phases already on pull its
                //  head.
                if (edge)
                {
                    m_motorSpinupRemaining = kMotorSpinupCycles;
                    MoveHeadToPull();
                }

                if (edge && m_audioSink != nullptr)
                {
                    m_audioSink->OnMotorEngaged();
                }

                // Spec-006 FR-006: OnMotorCommandOn fires on every
                // $C0E9 strobe (including no-op re-strobes on an
                // already-engaged motor); OnMotorEngaged fires only
                // on the off->on edge, co-located with the audio
                // edge detector above.
                if (m_eventSinks.IsAny())
                {
                    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnMotorCommandOn(); });

                    if (edge)
                    {
                        m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnMotorEngaged(); });
                    }
                }
            }

            break;
        case 0xA:
            m_activeDrive = 0;
            UpdateEngineSelection();
            MoveHeadIfEnabled();

            // Spec-006 FR-006: drive-select event fires after the
            // engine selection has been propagated.
            if (m_eventSinks.IsAny())
            {
                m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDriveSelect (m_activeDrive); });
            }

            break;
        case 0xB:
            m_activeDrive = 1;
            UpdateEngineSelection();
            MoveHeadIfEnabled();

            if (m_eventSinks.IsAny())
            {
                m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDriveSelect (m_activeDrive); });
            }

            break;
        case 0xC:
            m_q6 = false;
            m_engine[m_activeDrive].SetShiftLoadMode (false);
            break;
        case 0xD:
            m_q6 = true;
            m_engine[m_activeDrive].SetShiftLoadMode (true);
            break;
        case 0xE:
            if (m_q7 && m_eventSinks.IsAny())
            {
                m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnWriteMode (m_activeDrive, false); });
            }

            m_q7 = false;
            UpdateEngineWriteMode();
            break;
        case 0xF:
            if (!m_q7 && m_eventSinks.IsAny())
            {
                m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnWriteMode (m_activeDrive, true); });
            }

            // Writing turned on with a write-protected disk in the drive:
            // the nibbles go nowhere. RWTS checks the sense line first, so
            // only a program that does not check gets here.
            if (!m_q7 && m_eventSinks.IsAny() && m_activeDisk[m_activeDrive] != nullptr && m_activeDisk[m_activeDrive]->IsWriteProtected())
            {
                m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnWriteProtected(); });
            }

            m_q7 = true;
            UpdateEngineWriteMode();
            break;
        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleReadDispatch
//
//  Q7=0, Q6=0: read data latch (returns next assembled nibble).
//  Q7=0, Q6=1: sense write protect (bit 7 = WP state).
//  Q7=1, Q6=0: shift-load (real HW prepares the write latch). Phase 9
//              returns 0 — the LSS write path uses Q7=1+Q6=1 + Write().
//  Q7=1, Q6=1: write mode read returns 0 (write-only path).
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2Controller::HandleReadDispatch()
{
    Byte     nibble  = 0;
    Byte     sense   = 0;
    Byte     result  = 0;     // Q7=1 (both shift-load and write mode) reads 0
    uint8_t  fresh   = 0;
    bool     isFresh = false;



    if (!m_q6 && !m_q7)
    {
        // Issue #67 deliverable 1: during the LSS-stability window
        // after the motor's off->on edge, force the CPU-visible
        // latch to 0x80 (MSB set, but data is garbage). The bit
        // cursor and address-mark watcher still run normally so
        // rotational position and sync detection stay correct;
        // only the byte the CPU reads is overridden. Matches
        // AppleWin's MOTOR_ON_UNTIL_LSS_STABLE_CYCLES path
        // (GH#864).
        nibble = m_engine[m_activeDrive].ReadLatch();

        // Spec-006 T032 / FR-008: feed the passive watcher exactly
        // one nibble per LSS "byte ready" rising edge -- NOT every
        // CPU poll. ReadLatch is a pure sample with no consume
        // side effect (the 6502 spins LDA $C0EC / BPL until MSB),
        // so feeding its return value here would flood the
        // address-mark state machine with repeated bytes and
        // partial-assembly garbage, and zero real prologues would
        // ever match. ConsumeFreshNibble gates on the engine's
        // own rising-edge marker so each assembled nibble reaches
        // the watcher exactly once, in order. Byte-identical for
        // the CPU-visible read path.
        isFresh = m_engine[m_activeDrive].ConsumeFreshNibble (fresh);

        if (isFresh)
        {
            m_addrMarkWatcher.ObserveNibble (fresh, (m_cpuCycleSource != nullptr) ? *m_cpuCycleSource : 0, GetInstructionAddress());
            m_access[m_activeDrive].ObserveRead (m_quarterTrack[m_activeDrive], fresh, static_cast<uint32_t> (m_engine[m_activeDrive].GetBitPosition()),
                                                 static_cast<uint32_t> (m_engine[m_activeDrive].GetCurrentTrackBits()));
        }

        // The recent nibbles: each one the CPU takes, with the instruction
        // that took it, flagged while the motor spins up and the CPU reads $80.
        if (isFresh && m_activeDisk[m_activeDrive]->IsLoaded())
        {
            m_nibbleRecord.AddRead (m_activeDrive, fresh, (m_motorSpinupRemaining > 0) ? Disk2NibbleRecord::kSpinUp : 0, GetInstructionAddress(),
                                    m_engine[m_activeDrive].GetReadNibbles());
        }

        // During the spin-up window the CPU sees 0x80 (MSB set, data garbage)
        // while the real latch value above still drove the watcher.
        result = (m_motorSpinupRemaining > 0) ? 0x80 : nibble;
    }
    else if (m_q6 && !m_q7)
    {
        sense  = m_activeDisk[m_activeDrive]->IsWriteProtected() ? 0x80 : 0x00;
        result = sense;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandlePhase
//
//  Update the phase mask and, while the selected drive is enabled, move its
//  head to where the energized phases pull it (MoveHeadToPull).
//
//  A DRIVE'S STEPPER WORKS ONLY WHILE THE DRIVE IS ENABLED: ENABLE' low,
//  which is the motor running, its spin-down included (Sather, Understanding
//  the Apple IIe, chapter 9, "The Drive ENABLE' Input": at the enabled drive
//  "head positioning is enabled"). The controller latches the phases either
//  way, so the ones on when the drive is next enabled pull its head then
//  (GH #176).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::HandlePhase (int phase, bool on)
{
    if (on)
    {
        m_phases = static_cast<uint8_t> (m_phases | (1 << phase));
        m_phase  = phase;
    }
    else
    {
        m_phases = static_cast<uint8_t> (m_phases & ~(1 << phase));
    }

    if (m_motorOn)
    {
        MoveHeadToPull();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveHeadToPull
//
//  Move the selected drive's head to where the energized phases pull it,
//  clamp to the legal range, then push the resulting quarter-track index
//  into that drive's engine. The other drive's head stays where it is.
//
//  THE HEAD FOLLOWS THE MAGNETS THAT ARE ON. Phase p's magnets sit at the
//  quarter tracks where qt mod 8 is 2p, so each energized phase pulls the
//  head toward its nearest magnet, up to three quarter tracks either way,
//  and the head comes to rest at the average of the pulls (GetPull). With
//  one phase on, the head rests on that phase's track; with two adjacent
//  phases on, it rests on the quarter track between them, the only way to a
//  quarter track. A phase directly opposite the head pulls neither way, and
//  with none on, or with two opposite ones, the head does not move. A phase
//  turned off can move the head as well as one turned on: a DOS seek turns
//  the next phase on before it turns the last one off, so the head stops on
//  the quarter track between, and the last one going off finishes the
//  half-track step.
//
//  The step sound fires on a move that ends on a half-track position, so a
//  DOS seek's two moves sound as the one step they make (UTAIIe ch. 9,
//  Sather p. 9-12).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::MoveHeadToPull()
{
    int   & head    = m_quarterTrack[m_activeDrive];
    int     prevQt  = head;
    int     target  = prevQt;
    bool    bumped  = false;



    target  = prevQt + GetPull (m_phases, prevQt);
    bumped  = target < 0 || target > kMaxQuarterTrack;
    head    = std::clamp (target, 0, kMaxQuarterTrack);

    m_engine[m_activeDrive].SetCurrentTrack (head);

    if (target == prevQt)
    {
        return;
    }

    m_addrMarkWatcher.NoteHeadMoved (head);

    //  Each arrival at a quarter track counts once per drive, for the
    //  inspector's Reads marks.
    m_visits[m_activeDrive][head]++;

    // Audio sink (FR-003 / FR-004). Distinguish a normal step from a
    // track-0 / max-track bump by checking the *unclamped* target
    // position against the legal range -- if the move would have walked
    // past a travel stop, the stepper is energized but the head can't
    // move, producing the audible "thunk." Mutually exclusive: a single
    // phase event produces either a step or a bump, never both.
    if (m_audioSink != nullptr)
    {
        if (bumped)
        {
            m_audioSink->OnHeadBump();
        }
        else if (head % kQuarterTracksPerHalfTrack == 0)
        {
            m_audioSink->OnHeadStep (head);
        }
    }

    // Spec-006 FR-006: head-step / head-bump are mutually exclusive for
    // any single HandlePhase invocation; every quarter-track move is a
    // step.
    if (m_eventSinks.IsAny())
    {
        if (bumped)
        {
            m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnHeadBump (head); });
        }
        else
        {
            m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnHeadStep (prevQt, head); });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveHeadIfEnabled
//
//  A drive selected while the motor runs is enabled at once, so the phases
//  on pull its head.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::MoveHeadIfEnabled()
{
    if (m_motorOn)
    {
        MoveHeadToPull();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPull
//
//  How far, in quarter tracks, the energized phases move a head at
//  quarterTrack: the average of each phase's distance to its nearest
//  magnet, rounded to the nearest quarter track. A phase four quarter tracks
//  away, directly opposite, is left out, since it pulls neither way.
//
////////////////////////////////////////////////////////////////////////////////

int Disk2Controller::GetPull (uint8_t phases, int quarterTrack)
{
    constexpr int  kMagnetCycle = kPhaseCount * kQuarterTracksPerHalfTrack;
    constexpr int  kOpposite    = kMagnetCycle / 2;
    int            sum          = 0;
    int            count        = 0;
    int            distance     = 0;



    for (int phase = 0; phase < kPhaseCount; phase++)
    {
        if ((phases & (1 << phase)) == 0)
        {
            continue;
        }

        distance = ((phase * kQuarterTracksPerHalfTrack - quarterTrack) % kMagnetCycle + kMagnetCycle) % kMagnetCycle;

        if (distance == kOpposite)
        {
            continue;
        }

        sum += (distance > kOpposite) ? distance - kMagnetCycle : distance;
        count++;
    }

    if (count == 0)
    {
        return 0;
    }

    return (2 * sum + ((sum >= 0) ? count : -count)) / (2 * count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateEngineSelection
//
//  Drive select changes which engine sees motor-on. The non-selected
//  engine freezes; the selected engine is set to the controller's motor
//  state and to its own drive's head position. Each Disk II drive has its
//  own stepper, so selecting a drive moves neither head.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::UpdateEngineSelection()
{
    int   other = m_activeDrive ^ 1;



    m_engine[other].SetMotorOn (false);
    m_engine[m_activeDrive].SetMotorOn (m_motorOn);
    m_engine[m_activeDrive].SetCurrentTrack (m_quarterTrack[m_activeDrive]);
    m_engine[m_activeDrive].SetShiftLoadMode (m_q6);
    UpdateEngineWriteMode();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateEngineWriteMode
//
//  Whether the selected drive takes the cells the data latch shifts out: Q7
//  high. A Disk II card's drive is enabled through the spin-down, so it
//  writes there.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::UpdateEngineWriteMode()
{
    m_engine[m_activeDrive].SetWriteMode (m_q7);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Pumps the active drive's engine. Inactive drive's engine sits idle
//  (motor off via SetMotorOn (false)).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::Tick (uint32_t cpuCycles)
{
    if (m_iwmMode)
    {
        TickIwm (cpuCycles);
        return;
    }

    // Issue #67 deliverable 1: drain the spin-up counter before any
    // other state advances. The bit cursor inside the per-drive
    // engine still ticks normally (rotational position must be
    // correct when the window closes) -- only the CPU-visible read
    // dispatch suppresses real data during this window.
    if (m_motorSpinupRemaining > 0)
    {
        if (cpuCycles >= m_motorSpinupRemaining)
        {
            m_motorSpinupRemaining = 0;
        }
        else
        {
            m_motorSpinupRemaining -= cpuCycles;
        }
    }

    if (m_motorSpindownCycles > 0)
    {
        if (cpuCycles >= m_motorSpindownCycles)
        {
            m_motorSpindownCycles = 0;
            m_motorOn             = false;
            m_engine[0].SetMotorOn (false);
            m_engine[1].SetMotorOn (false);
            m_access[0].EndRead();
            m_access[1].EndRead();

            // FR-002: motor sound fades out only when the spindown
            // timer actually expires -- NOT at the raw $C0E8 access.
            // (Renamed from OnMotorStop in spec-006.)
            if (m_audioSink != nullptr)
            {
                m_audioSink->OnMotorDisengaged();
            }

            // Spec-006 FR-006: OnMotorDisengaged fires on the
            // true->false motor transition, which happens only here
            // when the spindown counter actually expires.
            if (m_eventSinks.IsAny())
            {
                m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnMotorDisengaged(); });
            }

            // Motor-idle auto-flush: the operation is complete and this is
            // the CPU thread that owns the disk writes, so it's a race-free
            // moment to persist any dirty images (see
            // SetMotorOffFlushCallback). Fires after the sinks so a
            // debug-panel observer still records MotorDisengaged first.
            if (m_motorOffFlushCallback)
            {
                m_motorOffFlushCallback();
            }
        }
        else
        {
            m_motorSpindownCycles -= cpuCycles;
        }
    }

    // When a CPU cycle source is attached, the engine bit cursor is
    // driven exclusively via CatchUpToCpu at each $C0Ex access. Skip the
    // per-instruction engine advance here to avoid double-counting
    // cycles. Motor spin-up/spindown timers above still run off the bulk
    // per-instruction count -- they're coarse-grained and don't need
    // sub-instruction precision.
    if (m_cpuCycleSource == nullptr)
    {
        AdvanceEngine (cpuCycles);
    }

    PumpIdleCallback (cpuCycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PumpIdleCallback
//
//  Offers the CPU thread to whoever wants a quiet moment on it.
//
//  THE SPINDOWN HOOK CANNOT SERVE THIS. It fires on a motor-on to motor-off
//  transition, so a guest that never touched the drive -- a machine sitting at
//  a BASIC prompt, which is where a build loop leaves it -- reaches it never.
//
//  "NO OPERATION IN FLIGHT" IS SPIN-UP AND NOTHING ELSE. The controller has no
//  notion of a transfer in progress; what it has is a window during which reads
//  are suppressed because the drive has not come up to speed, and that window
//  is precisely the moment when swapping the bytes under the guest would be
//  seen. Between accesses with the motor running is otherwise as quiet a moment
//  as the motor being off.
//
//  RATE-LIMITED, because Tick is pumped per instruction and the condition is
//  nearly always true. Without the gate this would be an indirect call on
//  essentially every instruction the machine executes.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::PumpIdleCallback (uint32_t cpuCycles)
{
    bool  quiet = false;



    //  Remembered across the whole window, not sampled at its end. Spin-up
    //  lasts a few hundred cycles and the window is a frame, so a sample at
    //  the boundary would nearly always find the drive quiet again and hand
    //  out a turn in the middle of the very operation this avoids.
    if (m_motorSpinupRemaining > 0)
    {
        m_busySinceIdleCallback = true;
    }

    m_cyclesSinceIdleCallback += cpuCycles;

    if (m_cyclesSinceIdleCallback >= kIdleCallbackCycles)
    {
        quiet = !m_busySinceIdleCallback;

        m_cyclesSinceIdleCallback = 0;
        m_busySinceIdleCallback   = false;

        if (quiet && m_idleCallback)
        {
            m_idleCallback();
        }
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatchUpToCpu
//
//  Issue #67. When a CPU cycle source is attached, pull the active
//  drive's bit-stream engine forward to the CPU's current cycle count.
//  Called from the top of Read/Write so by the time HandleSwitch /
//  HandleReadDispatch run, the engine's m_bitPos reflects elapsed CPU
//  time since the last access. The source is the sub-instruction bus
//  cycle (Cpu6502::m_busCycle), current to the in-flight $C0Ex access
//  rather than the instruction boundary -- matching AppleWin attributing
//  the disk update to the exact bus cycle.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::CatchUpToCpu()
{
    uint64_t  now   = 0;
    uint64_t  delta = 0;



    // With no cycle source attached, Tick() drives the engine off the bulk
    // per-instruction count instead and there is nothing to catch up to.
    if (m_cpuCycleSource != nullptr)
    {
        now = *m_cpuCycleSource;

        // A power cycle zeroes the CPU cycle counter (m_totalCycles, and
        // hence the m_busCycle source this anchor tracks) but does NOT
        // rewind m_lastCpuSync. Without re-anchoring, every catch-up after
        // a power cycle sees now < m_lastCpuSync and bails, freezing the
        // bit cursor until the counter climbs back past the stale anchor --
        // a dead disk for as long as the prior session ran (the boot ROM
        // spins on $C0EC reading a non-advancing latch the whole time).
        // Re-anchor on any non-forward move so the next access resumes the
        // normal forward advance. The == case is a same-cycle no-op.
        if (now <= m_lastCpuSync)
        {
            m_lastCpuSync = now;
        }
        else
        {
            delta         = now - m_lastCpuSync;
            m_lastCpuSync = now;

            AdvanceEngine (static_cast<uint32_t> (delta));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceEngine
//
//  Moves the active drive's engine on by `cycles`, and with it the time
//  since the CPU last loaded the write latch, which the IWM's HANDSHAKE
//  register reports.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::AdvanceEngine (uint32_t cycles)
{
    m_engine[m_activeDrive].Tick (cycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDisk / EjectDisk / GetDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::MountDisk (int drive, const string & path)
{
    HRESULT   hr = S_OK;



    CBRAEx (drive >= 0 && drive < kDriveCount, E_INVALIDARG);

    hr = m_disks[drive].Load (path);
    CHR (hr);

    m_activeDisk[drive] = &m_disks[drive];
    m_engine[drive].SetDiskImage (m_activeDisk[drive]);
    m_nibbleRecord.NoteDiskChange (drive);

    // Spec-006 FR-006: disk-insert event fires after the mount
    // succeeds. Place before the Error label so we don't fire on a
    // failed mount.
    if (m_eventSinks.IsAny())
    {
        m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDiskInserted (drive); });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EjectDisk
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::EjectDisk (int drive)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    m_disks[drive].Eject();
    m_activeDisk[drive] = &m_disks[drive];
    m_engine[drive].SetDiskImage (m_activeDisk[drive]);
    m_nibbleRecord.NoteDiskChange (drive);

    // Spec-006 FR-006: disk-eject event fires after the slot has
    // been physically detached and the engine repointed.
    if (m_eventSinks.IsAny())
    {
        m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDiskEjected (drive); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDisk
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * Disk2Controller::GetDisk (int drive)
{
    bool  inRange = (drive >= 0 && drive < kDriveCount);



    return inRange ? m_activeDisk[drive] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetExternalDisk
//
//  Phase 11 / T097. Re-points drive `drive` at an externally-owned
//  DiskImage (typically owned by EmulatorShell's DiskImageStore so the
//  store can drive auto-flush on eject / machine switch / power cycle /
//  shutdown). Pass nullptr to restore the controller's internal disk.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SetExternalDisk (int drive, DiskImage * external)
{
    DiskImage *  next = nullptr;



    if (drive >= 0 && drive < kDriveCount)
    {
        next = (external != nullptr) ? external : &m_disks[drive];

        //  Every call is a bay change: another disk, an eject, or the same
        //  file reloaded into the same image object at the same address.
        //  Each starts the counts over.
        m_visits[drive].fill (0);
        m_access[drive].Clear();
        m_isWriteBlocked[drive] = false;

        m_activeDisk[drive] = next;
        m_engine[drive].SetDiskImage (m_activeDisk[drive]);
        m_nibbleRecord.NoteDiskChange (drive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountLatchLoad
//
//  The CPU loaded the write latch. With the motor running, spin-down
//  included, the nibble goes to the record under the head and counts there;
//  on a disk that cannot be written it goes nowhere, counts nothing, and
//  the drive shows the write blocked. With the motor stopped nothing reaches
//  the disk, and the //c's mode-register load never comes here.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::CountLatchLoad()
{
    DiskImage *  disk = m_activeDisk[m_activeDrive];



    if (m_motorOn && disk != nullptr && disk->IsLoaded())
    {
        m_isWriteBlocked[m_activeDrive] = disk->IsWriteProtected();

        if (!m_isWriteBlocked[m_activeDrive])
        {
            disk->CountGuestWrite (m_engine[m_activeDrive].GetSlot());
            m_access[m_activeDrive].ObserveWrite (m_quarterTrack[m_activeDrive], static_cast<uint32_t> (m_engine[m_activeDrive].GetBitPosition()));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetVisitCount
//
//  How many times the head arrived at a quarter track of a drive since its
//  disk went in.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Disk2Controller::GetVisitCount (int drive, int quarterTrack) const
{
    bool  isIn = drive >= 0 && drive < kDriveCount && quarterTrack >= 0 && quarterTrack <= kMaxQuarterTrack;



    return isIn ? m_visits[drive][quarterTrack] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsWriteBlocked
//
//  True once the guest's last write to the drive was dropped because the
//  disk cannot be written.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2Controller::IsWriteBlocked (int drive) const
{
    return drive >= 0 && drive < kDriveCount && m_isWriteBlocked[drive];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasExternalDisk
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2Controller::HasExternalDisk (int drive) const
{
    // An out-of-range drive has no disk at all, external or otherwise.
    return drive >= 0
        && drive < kDriveCount
        && m_activeDisk[drive] != &m_disks[drive];
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDiskInserted / NotifyDiskEjected
//
//  Spec-006 bug 14b. Fires the IDisk2EventSink hooks without touching
//  controller state. Used by EmulatorShell on the DiskImageStore +
//  SetExternalDisk hot path so the debug window still sees user-facing
//  insert / eject events on disks mounted outside MountDisk.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::NotifyDiskInserted (int drive)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    if (m_eventSinks.IsAny())
    {
        m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDiskInserted (drive); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDiskEjected
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::NotifyDiskEjected (int drive)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    if (m_eventSinks.IsAny())
    {
        m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDiskEjected (drive); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::Reset()
{
    int   i = 0;



    m_phases       = 0;
    m_phase        = 0;
    m_motorOn      = false;
    m_motorSpindownCycles = 0;
    m_motorSpinupRemaining = 0;
    m_activeDrive  = 0;
    m_q6           = false;
    m_q7           = false;
    m_iwmEnabled.fill (false);
    m_iwm.Reset();

    for (i = 0; i < kDriveCount; i++)
    {
        m_quarterTrack[i] = 0;
        m_engine[i].Reset();
        m_engine[i].SetDiskImage (m_activeDisk[i]);
    }

    m_addrMarkWatcher.ResetState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  A //e soft reset clears the controller's hardware state and keeps the
//  disks in the drives. Writing dirty images back belongs to the disk
//  store, which MachineHost::SoftReset calls first, so every safeguard on a
//  write applies, the reverse-execution hold and the replay flag among them.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SoftReset()
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Clears the controller hardware state and ejects the controller's internal
//  disks, then selects them in both drives. A disk attached through
//  SetExternalDisk is mounted in the DiskImageStore, which this call doesn't
//  change; MachineHost::PowerCycle reconnects the drives to it afterward.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::PowerCycle (Prng & prng)
{
    int   drive = 0;



    UNREFERENCED_PARAMETER (prng);

    Reset();
    m_nibbleRecord.Clear();

    for (drive = 0; drive < kDriveCount; drive++)
    {
        m_disks[drive].Eject();
        m_activeDisk[drive] = &m_disks[drive];
        m_engine[drive].SetDiskImage (m_activeDisk[drive]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> Disk2Controller::Create (const DeviceConfig & config, MemoryBus & bus)
{
    int   slot = config.hasSlot ? config.slot : 6;



    UNREFERENCED_PARAMETER (bus);

    return make_unique<Disk2Controller> (slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetEventSink
//
//  Spec-006 FR-007: caller-owned IDisk2EventSink pointer (default
//  nullptr). The controller never deletes it and never invokes
//  anything on it from outside its own fire sites. Sink is also
//  propagated to the embedded Disk2AddressMarkWatcher so the
//  watcher fires its OnAddressMark / OnDataMarkRead notifications
//  through the same sink, keeping the event stream chronologically
//  interleaved on the dialog's ring.
//
//  Safe to call from the UI thread between CPU slices (mirrors the
//  spec-005 audio-sink attach pattern). Pass nullptr to detach. The
//  one-argument form sets the Panel slot.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SetEventSink (ObserverSlot slot, IDisk2EventSink * sink) noexcept
{
    m_eventSinks.Set (slot, sink);
    m_addrMarkWatcher.SetEventSink (slot, sink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::GetDiagnostics
//
//  Q6 and Q7 together select what the sequencer does: read, sense the write
//  protect, shift out a write, or load the write latch. With the motor on,
//  Q7 is whether the drive is reading or writing (FR-216). The last address
//  field is the one the watcher decoded last; a field not yet seen is "?".
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::GetDiagnostics (DiagnosticsSnapshot & snapshot) const
{
    static constexpr const char * kModes[]   = { "read", "sense write protect", "write", "load write latch" };
    constexpr int                 kQuarters  = 4;
    constexpr int                 kHundredth = 25;      // hundredths of a track in a quarter
    const Disk2NibbleEngine     & engine     = m_engine[m_activeDrive];
    int                           mode       = ((int) m_q7 << 1) | (int) m_q6;
    const char                  * activity   = !m_motorOn ? "idle" : (m_q7 ? "writing" : "reading");
    DiagnosticsGroup              drive      { "Drive", {} };
    DiagnosticsGroup              head       { "Head", {} };
    DiagnosticsGroup              sequencer  { "Sequencer", {} };
    auto                          field      = [] (char prefix, int value) { return (value < 0) ? std::format ("{}?", prefix) : std::format ("{}{}", prefix, value); };



    drive.rows.push_back (MakeTextRow ("Active drive",   std::format ("{}", m_activeDrive + 1)));
    drive.rows.push_back (MakeTextRow ("Activity",       activity));

    drive.rows.push_back (MakeFlagRow ("At speed",       IsMotorAtSpeed()));
    drive.rows.push_back (MakeTextRow ("Spin-up left",   std::format ("{} cycles", m_motorSpinupRemaining)));
    drive.rows.push_back (MakeTextRow ("Spin-down left", std::format ("{} cycles", m_motorSpindownCycles)));
    drive.rows.push_back (MakeFlagRow ("Disk in drive",  m_activeDisk[m_activeDrive] != nullptr && m_activeDisk[m_activeDrive]->IsLoaded()));

    head.rows.push_back (MakeTextRow ("Track",         std::format ("{}.{:02}", GetQuarterTrack() / kQuarters, (GetQuarterTrack() % kQuarters) * kHundredth)));
    head.rows.push_back (MakeTextRow ("Quarter track", std::format ("{}", GetQuarterTrack())));
    head.rows.push_back (MakeByteRow ("Phases",        m_phases, { "", "", "", "", "PH3", "PH2", "PH1", "PH0" }));
    head.rows.push_back (MakeTextRow ("Last address field", std::format ("{} {} {}", field ('T', m_addrMarkWatcher.GetCachedTrack()),
                                                                                    field ('S', m_addrMarkWatcher.GetCachedSector()),
                                                                                    field ('V', m_addrMarkWatcher.GetCachedVolume()))));

    sequencer.rows.push_back (MakeTextRow ("Mode",            kModes[mode]));
    sequencer.rows.push_back (MakeFlagRow ("Q6",              m_q6));
    sequencer.rows.push_back (MakeFlagRow ("Q7",              m_q7));
    sequencer.rows.push_back (MakeHexRow  ("Read latch",      engine.PeekReadLatch(), kByteDigits));
    sequencer.rows.push_back (MakeTextRow ("Bit",             std::format ("{}", engine.GetBitPosition())));
    sequencer.rows.push_back (MakeTextRow ("Nibbles read",    std::format ("{}", engine.GetReadNibbles())));
    sequencer.rows.push_back (MakeTextRow ("Nibbles written", std::format ("{}", engine.GetWriteNibbles())));

    snapshot.groups.push_back (std::move (drive));
    snapshot.groups.push_back (std::move (head));
    snapshot.groups.push_back (std::move (sequencer));
    snapshot.visual = DiagnosticsDiskHead { GetQuarterTrack(), kMaxQuarterTrack, m_phases, m_motorOn, m_activeDrive, { GetQuarterTrack (0), GetQuarterTrack (1) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::SaveState (StateWriter & writer) const
{
    HRESULT  hr    = S_OK;
    int      drive = 0;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte   (m_phases);
    writer.WriteByte   (static_cast<Byte> (m_phase));
    writer.WriteByte   (static_cast<Byte> (m_quarterTrack[0]));
    writer.WriteByte   (static_cast<Byte> (m_quarterTrack[1]));
    writer.WriteBool   (m_motorOn);
    writer.WriteUInt32 (m_motorSpindownCycles);
    writer.WriteUInt32 (m_motorSpinupRemaining);
    writer.WriteByte   (static_cast<Byte> (m_activeDrive));
    writer.WriteBool   (m_q6);
    writer.WriteBool   (m_q7);
    writer.WriteBool   (m_iwmMode);
    writer.WriteByte   (m_iwm.GetMode());
    writer.WriteUInt32 (m_cyclesSinceIdleCallback);
    writer.WriteBool   (m_busySinceIdleCallback);
    writer.WriteUInt64 (m_lastCpuSync);

    for (drive = 0; drive < kDriveCount; drive++)
    {
        hr = m_engine[drive].SaveState (writer);
        CHR (hr);
    }

    hr = m_addrMarkWatcher.SaveState (writer);
    CHR (hr);

    hr = m_nibbleRecord.SaveState (writer);
    CHR (hr);

    m_iwm.SaveState (writer);
    writer.WriteBool   (m_iwmEnabled[0]);
    writer.WriteBool   (m_iwmEnabled[1]);
    writer.WriteUInt64 (m_iwmCycles);

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  The engines load inside this section and check their own fields; this
//  checks the phase mask, phase, head positions and drive select, and that
//  the saved machine had the same IWM wiring. The address-mark watcher's
//  section is read inside it too, from version 2 on. Before version 3 there
//  was one head position for both drives, which each drive takes. Nothing goes through Reset or
//  the switch handlers, so no sink hears a motor or drive change and the
//  motor-off flush does not run.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::LoadState (StateReader & reader)
{
    constexpr Byte  kPhaseMaskLimit = 1 << kPhaseCount;



    HRESULT   hr                      = S_OK;
    uint16_t  version                 = 0;
    int       drive                   = 0;
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

    reader.ReadByte   (phases);
    reader.ReadByte   (phase);
    reader.ReadByte   (quarterTrack);

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

    for (drive = 0; drive < kDriveCount; drive++)
    {
        hr = m_engine[drive].LoadState (reader);
        CHR (hr);
    }

    // Version 1 had no watcher state: its machines start from Idle.
    if (version >= kWatcherStateVersion)
    {
        hr = m_addrMarkWatcher.LoadState (reader);
        CHR (hr);
    }
    else
    {
        m_addrMarkWatcher.ResetState();
        m_addrMarkWatcher.NoteHeadMoved (quarterTrack);
    }

    // Before version 3 the state had no recent nibbles.
    if (version >= kNibbleRecordStateVersion)
    {
        hr = m_nibbleRecord.LoadState (reader);
        CHR (hr);
    }
    else
    {
        m_nibbleRecord.Clear();
    }

    //  Before version 4 the state had no IWM, so a //c's state from then
    //  can't be put back; another machine's has no IWM to put back.
    CBREx (version >= kIwmStateVersion || !m_iwmMode, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (version >= kIwmStateVersion)
    {
        hr = m_iwm.LoadState (reader);
        CHR (hr);

        reader.ReadBool   (m_iwmEnabled[0]);
        reader.ReadBool   (m_iwmEnabled[1]);
        reader.ReadUInt64 (m_iwmCycles);
    }

    hr = reader.EndSection();
    CHR (hr);

    CBREx (phases        <  kPhaseMaskLimit,  HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (phase         <  kPhaseCount,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (quarterTrack  <= kMaxQuarterTrack, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (quarterTrack2 <= kMaxQuarterTrack, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (activeDrive   <  kDriveCount,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (iwmMode       == m_iwmMode,        HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_phases                  = phases;
    m_phase                   = phase;
    m_quarterTrack[0]         = quarterTrack;
    m_quarterTrack[1]         = quarterTrack2;
    m_motorOn                 = motorOn;
    m_motorSpindownCycles     = motorSpindownCycles;
    m_motorSpinupRemaining    = motorSpinupRemaining;
    m_activeDrive             = activeDrive;
    m_q6                      = q6;
    m_q7                      = q7;
    m_cyclesSinceIdleCallback = cyclesSinceIdleCallback;
    m_busySinceIdleCallback   = busySinceIdleCallback;
    m_lastCpuSync             = lastCpuSync;

    // The drive not selected when an older state was saved had its engine on
    // whatever track it was last selected on; both heads are at the one saved
    // position now.
    if (version < kPerDriveHeadStateVersion)
    {
        m_engine[m_activeDrive ^ 1].SetCurrentTrack (quarterTrack);
    }

Error:
    return hr;
}


