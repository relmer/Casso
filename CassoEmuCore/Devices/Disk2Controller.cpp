#include "Pch.h"

#include "Disk2Controller.h"
#include "Audio/IDriveAudioSink.h"
#include "IDisk2EventSink.h"
#include "Core/Prng.h"





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
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Apply soft-switch side effects then dispatch the data path per Q6/Q7.
//
////////////////////////////////////////////////////////////////////////////////

Byte Disk2Controller::Read (Word address)
{
    int   offset = (address - m_ioStart) & 0x0F;

    CatchUpToCpu();

    HandleSwitch (offset);

    return HandleReadDispatch();
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
    int   offset = (address - m_ioStart) & 0x0F;

    CatchUpToCpu();

    HandleSwitch (offset);

    if (m_q7 && m_q6)
    {
        // IWM (//c): Q6H + Q7H with the motor off loads the MODE register
        // instead of the write latch; with the motor on it is the data-field
        // write path, same as a Disk II card.
        if (m_iwmMode && !m_motorOn)
        {
            m_iwmModeReg = value;
        }
        else
        {
            m_engine[m_activeDrive].WriteLatch (value);
        }
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

            // Spec-006 FR-006: every $C0E8 strobe is a logical
            // "motor command off" event, including no-op strobes
            // (when the motor is already off or already spinning
            // down). OnMotorDisengaged fires later from Tick() when
            // the spindown timer actually expires.
            if (m_eventSink != nullptr)
            {
                m_eventSink->OnMotorCommandOff();
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

                if (edge)
                {
                    m_motorSpinupRemaining = kMotorSpinupCycles;
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
                if (m_eventSink != nullptr)
                {
                    m_eventSink->OnMotorCommandOn();

                    if (edge)
                    {
                        m_eventSink->OnMotorEngaged();
                    }
                }
            }
            break;
        case 0xA:
            m_activeDrive = 0;
            UpdateEngineSelection();

            // Spec-006 FR-006: drive-select event fires after the
            // engine selection has been propagated.
            if (m_eventSink != nullptr)
            {
                m_eventSink->OnDriveSelect (m_activeDrive);
            }
            break;
        case 0xB:
            m_activeDrive = 1;
            UpdateEngineSelection();

            if (m_eventSink != nullptr)
            {
                m_eventSink->OnDriveSelect (m_activeDrive);
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
            m_q7 = false;
            m_engine[m_activeDrive].SetWriteMode (false);
            break;
        case 0xF:
            m_q7 = true;
            m_engine[m_activeDrive].SetWriteMode (true);
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
    Byte     nibble = 0;
    uint8_t  fresh  = 0;

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
        if (m_engine[m_activeDrive].ConsumeFreshNibble (fresh))
        {
            m_addrMarkWatcher.ObserveNibble (fresh);
        }

        if (m_motorSpinupRemaining > 0)
        {
            return 0x80;
        }

        return nibble;
    }

    if (m_q6 && !m_q7)
    {
        Byte  sense = m_activeDisk[m_activeDrive]->IsWriteProtected() ? 0x80 : 0x00;

        // IWM (//c): Q6H + Q7L reads the STATUS register. Bit 7 is the sense
        // input (write protect here), bit 5 is the drive-enable/motor flag,
        // and bits 4-0 mirror the MODE register the firmware just wrote -- the
        // reset code writes the mode register then reads it back to confirm.
        if (m_iwmMode)
        {
            return sense | (m_motorOn ? 0x20 : 0x00) | (m_iwmModeReg & 0x1F);
        }

        return sense;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandlePhase
//
//  Update the phase mask, walk the head toward the newly-energized phase,
//  clamp to legal range, then push the resulting quarter-track index into
//  the active drive's engine.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::HandlePhase (int phase, bool on)
{
    // Stepper model ported clean-room from apple2js (MIT), disk2.ts
    // setPhase + PHASE_DELTA. UTAIIe ch. 9 / Sather p. 9-12.
    //
    // The cog tracks the last-energized phase magnet (m_phase), NOT a
    // position-derived magnet index. On each phase-ON event the head
    // moves PHASE_DELTA[m_phase][phase] half-tracks (= *2 quarter-tracks)
    // toward the newly-energized magnet, then remembers the new phase.
    // Phase-OFF events do not move the head -- the cog holds its detent.
    //
    // Because every delta is an even number of quarter-tracks, a normal
    // two-phase DOS step lands the head on a whole- or half-track detent
    // (quarter-track index even); it can never get marooned on an odd
    // quarter-track between detents. The previous position-derived model
    // (`(m_quarterTrack / 2) & 3`) could not distinguish qt N from qt N+1
    // and would leave the head stuck one quarter-track off a real track,
    // serving unformatted noise on narrow-band protected disks.
    static constexpr int  kPhaseDelta[kPhaseCount][kPhaseCount] =
    {
        {  0,  1,  2, -1 },
        { -1,  0,  1,  2 },
        { -2, -1,  0,  1 },
        {  1, -2, -1,  0 },
    };

    int  prevQt  = m_quarterTrack;
    int  qtDelta = 0;
    int  postRaw = prevQt;

    if (on)
    {
        m_phases = static_cast<uint8_t> (m_phases | (1 << phase));
    }
    else
    {
        m_phases = static_cast<uint8_t> (m_phases & ~(1 << phase));
    }

    if (on)
    {
        qtDelta = kPhaseDelta[m_phase][phase] * 2;
        m_phase = phase;
    }

    postRaw         = prevQt + qtDelta;
    m_quarterTrack += qtDelta;

    if (m_quarterTrack < 0)
    {
        m_quarterTrack = 0;
    }

    if (m_quarterTrack > kMaxQuarterTrack)
    {
        m_quarterTrack = kMaxQuarterTrack;
    }

    m_engine[m_activeDrive].SetCurrentTrack (m_quarterTrack);

    // Audio sink (FR-003 / FR-004). Fire only when the head actually
    // moved (qtDelta != 0). Distinguish a normal step from a track-0 /
    // max-track bump by checking the *unclamped* target position
    // against the legal range -- if the raw move would have walked
    // past a travel stop, the stepper is energized but the head can't
    // move, producing the audible "thunk." Mutually exclusive: a
    // single phase event produces either a step or a bump, never both.
    if (qtDelta != 0 && m_audioSink != nullptr)
    {
        bool  bumped = (postRaw < 0) || (postRaw > kMaxQuarterTrack);

        if (bumped)
        {
            m_audioSink->OnHeadBump();
        }
        else
        {
            m_audioSink->OnHeadStep (m_quarterTrack);
        }
    }

    // Spec-006 FR-006: head-step / head-bump are mutually exclusive
    // for any single HandlePhase invocation. Reuses the same bumped
    // discriminator the audio sink uses above so the two sinks stay
    // semantically aligned (a real bump is a bump for both).
    if (qtDelta != 0 && m_eventSink != nullptr)
    {
        bool  bumped = (postRaw < 0) || (postRaw > kMaxQuarterTrack);

        if (bumped)
        {
            m_eventSink->OnHeadBump (m_quarterTrack);
        }
        else
        {
            m_eventSink->OnHeadStep (prevQt, m_quarterTrack);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateEngineSelection
//
//  Drive select changes which engine sees motor-on. The non-selected
//  engine freezes; the selected engine inherits the controller's motor
//  state and the current track.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::UpdateEngineSelection()
{
    int   other = m_activeDrive ^ 1;

    m_engine[other].SetMotorOn (false);
    m_engine[m_activeDrive].SetMotorOn (m_motorOn);
    m_engine[m_activeDrive].SetCurrentTrack (m_quarterTrack);
    m_engine[m_activeDrive].SetShiftLoadMode (m_q6);
    m_engine[m_activeDrive].SetWriteMode    (m_q7);
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
            if (m_eventSink != nullptr)
            {
                m_eventSink->OnMotorDisengaged();
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
        m_engine[m_activeDrive].Tick (cpuCycles);
    }
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


    if (m_cpuCycleSource == nullptr)
    {
        return;
    }

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
        return;
    }

    delta         = now - m_lastCpuSync;
    m_lastCpuSync = now;

    m_engine[m_activeDrive].Tick (static_cast<uint32_t> (delta));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDisk / EjectDisk / GetDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::MountDisk (int drive, const string & path)
{
    HRESULT   hr = S_OK;

    if (drive < 0 || drive >= kDriveCount)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    hr = m_disks[drive].Load (path);
    CHR (hr);

    m_activeDisk[drive] = &m_disks[drive];
    m_engine[drive].SetDiskImage (m_activeDisk[drive]);

    // Spec-006 FR-006: disk-insert event fires after the mount
    // succeeds. Place before the Error label so we don't fire on a
    // failed mount.
    if (m_eventSink != nullptr)
    {
        m_eventSink->OnDiskInserted (drive);
    }

Error:
    return hr;
}


void Disk2Controller::EjectDisk (int drive)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    m_disks[drive].Eject();
    m_activeDisk[drive] = &m_disks[drive];
    m_engine[drive].SetDiskImage (m_activeDisk[drive]);

    // Spec-006 FR-006: disk-eject event fires after the slot has
    // been physically detached and the engine repointed.
    if (m_eventSink != nullptr)
    {
        m_eventSink->OnDiskEjected (drive);
    }
}


DiskImage * Disk2Controller::GetDisk (int drive)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return nullptr;
    }

    return m_activeDisk[drive];
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
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    m_activeDisk[drive] = (external != nullptr) ? external : &m_disks[drive];
    m_engine[drive].SetDiskImage (m_activeDisk[drive]);
}


bool Disk2Controller::HasExternalDisk (int drive) const
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return false;
    }

    return m_activeDisk[drive] != &m_disks[drive];
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

    if (m_eventSink != nullptr)
    {
        m_eventSink->OnDiskInserted (drive);
    }
}


void Disk2Controller::NotifyDiskEjected (int drive)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    if (m_eventSink != nullptr)
    {
        m_eventSink->OnDiskEjected (drive);
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
    m_quarterTrack = 0;
    m_motorOn      = false;
    m_motorSpindownCycles = 0;
    m_motorSpinupRemaining = 0;
    m_activeDrive  = 0;
    m_q6           = false;
    m_q7           = false;

    for (i = 0; i < kDriveCount; i++)
    {
        m_engine[i].Reset();
        m_engine[i].SetDiskImage (m_activeDisk[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4 / FR-034: //e soft reset clears the controller hardware state
//  but PRESERVES the disk mounts. Dirty images flush back to host storage
//  so a reset doesn't lose user writes (audit §10).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SoftReset()
{
    HRESULT   hrFlush = S_OK;
    int       drive   = 0;

    Reset();

    for (drive = 0; drive < kDriveCount; drive++)
    {
        if (m_activeDisk[drive]->IsLoaded())
        {
            hrFlush = m_activeDisk[drive]->Flush();
            IGNORE_RETURN_VALUE (hrFlush, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  Phase 4 / FR-035: a full power cycle ejects every drive (which itself
//  flushes dirty images first) and clears the controller hardware state.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::PowerCycle (Prng & prng)
{
    int   drive = 0;

    UNREFERENCED_PARAMETER (prng);

    Reset();

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
//  spec-005 audio-sink attach pattern). Pass nullptr to detach.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SetEventSink (IDisk2EventSink * sink) noexcept
{
    m_eventSink = sink;
    m_addrMarkWatcher.SetEventSink (sink);
}

