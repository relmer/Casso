#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Audio/IDriveAudioSink.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Core/MachineConfig.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  The head stop
//
////////////////////////////////////////////////////////////////////////////////

static_assert ((Disk2Controller::kMaxQuarterTrack & 1) == 0,                      "the head stop must be a half-track detent");
static_assert (Disk2Controller::kMaxQuarterTrack <= Disk2NibbleEngine::kMaxTrack,  "the head stop must lie inside the engine's range");
static_assert (Disk2Controller::kMaxQuarterTrack <  DiskImage::kQuarterTrackCount, "the head stop must lie inside the quarter-track map");





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
//  TryPeekIo
//
//  Q6, Q7 and the drive as they are after the switch at address, then the
//  byte HandleReadDispatch returns for those: the data latch, run forward
//  to the CPU's cycle on a copy of the drive; the sense line or the IWM
//  status; or 0 with Q7 on.
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
        value = m_iwmMode ? static_cast<Byte> (sense | ((m_motorOn && m_motorSpindownCycles == 0) ? 0x20 : 0x00) | (m_iwmModeReg & 0x1F))
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
    value = (m_motorSpinupRemaining > 0 || (offset == 0x9 && !m_motorOn)) ? 0x80 : m_engine[drive].PeekLatchAfter (static_cast<uint32_t> (delta));

    return true;
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
    int   offset      = (address - m_ioStart) & 0x0F;
    bool  isModeLoad  = false;
    bool  isLatchLoad = false;



    CatchUpToCpu();

    HandleSwitch (offset);

    if (m_q7 && m_q6)
    {
        isModeLoad  = m_iwmMode && !(m_motorOn && m_motorSpindownCycles == 0);
        isLatchLoad = !isModeLoad && m_motorOn;

        // IWM (//c): Q6H + Q7H with the motor off loads the MODE register
        // instead of the write latch; with the motor on it is the data-field
        // write path, same as a Disk II card.
        //
        // "Off" is the motor COMMAND, as it is for the status flag. The //c
        // reset routine turns the motor off, writes the mode register and
        // reads it back until the two agree; judged by the spin-down instead,
        // the write was refused for a second and the machine sat in that
        // loop at every reset, power cycle and launch.
        if (isModeLoad)
        {
            m_iwmModeReg = value;
        }
        else
        {
            m_engine[m_activeDrive].WriteLatch (value);
            m_addrMarkWatcher.ObserveWrittenNibble (value);
        }

        if (isLatchLoad)
        {
            OnLatchLoad (m_activeDrive, value, GetQuarterTrack(), m_activeDisk[m_activeDrive]->IsWriteProtected());
        }
    }
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
        sense = m_activeDisk[m_activeDrive]->IsWriteProtected() ? 0x80 : 0x00;

        // IWM (//c): Q6H + Q7L reads the STATUS register. Bit 7 is the sense
        // input (write protect here), bit 5 is the motor-on flag, and bits
        // 4-0 mirror the MODE register the firmware just wrote -- the reset
        // code writes the mode register then reads it back to confirm.
        // A plain Disk II has no status register, so it reports sense alone.
        //
        // Bit 5 follows the motor COMMAND, not the spin-down. After $C0E8 the
        // disk keeps turning for the timer's second so a read in that window
        // still finds nibbles, but the flag the firmware polls drops at once.
        // The //c reset routine strobes the motor on and off and then waits
        // for this bit to clear before it goes on; reported from the
        // spin-down it held the machine for a full second at every reset,
        // power cycle and launch, and a real //c comes up at once.
        result = m_iwmMode
                     ? static_cast<Byte> (sense
                                          | ((m_motorOn && m_motorSpindownCycles == 0) ? 0x20 : 0x00)
                                          | (m_iwmModeReg & 0x1F))
                     : sense;
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
//  high, and on the //c's IWM the motor commanded on as well. The IWM keeps
//  the disk turning through the spin-down after $C0E8 but writes nothing to
//  it then; with Q6 and Q7 high in that window it loads the mode register
//  instead. A Disk II card keeps its drive enabled through the spin-down, so
//  it writes there.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::UpdateEngineWriteMode()
{
    bool  isMotorCommandedOn = m_motorOn && m_motorSpindownCycles == 0;
    bool  isWriting          = m_q7 && (!m_iwmMode || isMotorCommandedOn);



    m_engine[m_activeDrive].SetWriteMode (isWriting);
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
        m_engine[m_activeDrive].Tick (cpuCycles);
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

            m_engine[m_activeDrive].Tick (static_cast<uint32_t> (delta));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDisk / EjectDisk / GetDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2Controller::MountDisk (int drive, const string & path)
{
    HRESULT   hr = S_OK;



    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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
    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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



    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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
    ASSERT_THREAD_OWNERSHIP (*m_ownership);

    if (drive >= 0 && drive < kDriveCount)
    {
        m_activeDisk[drive] = (external != nullptr) ? external : &m_disks[drive];
        m_engine[drive].SetDiskImage (m_activeDisk[drive]);
        m_nibbleRecord.NoteDiskChange (drive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasExternalDisk
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2Controller::HasExternalDisk (int drive) const
{
    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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
    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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
    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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



    ASSERT_THREAD_OWNERSHIP (*m_ownership);

    m_phases       = 0;
    m_phase        = 0;
    m_motorOn      = false;
    m_motorSpindownCycles = 0;
    m_motorSpinupRemaining = 0;
    m_activeDrive  = 0;
    m_q6           = false;
    m_q7           = false;

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
    ASSERT_THREAD_OWNERSHIP (*m_ownership);

    Reset();
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



    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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
//  Call only on the disk store's owning thread. Pass nullptr to detach. The
//  one-argument form sets the Panel slot.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SetEventSink (ObserverSlot slot, IDisk2EventSink * sink)
{
    ASSERT_THREAD_OWNERSHIP (*m_ownership);

    m_eventSinks.Set (slot, sink);
    m_addrMarkWatcher.SetEventSink (slot, sink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller::SetThreadOwnership
//
//  Points the controller at the token it checks from now on. Both the token it
//  checks today and the new one must be held by the calling thread: wiring a
//  drive to a store another thread holds would let two threads use it.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Controller::SetThreadOwnership (const ThreadOwnership & ownership)
{
    HRESULT  hr            = S_OK;
    bool     isCurrentMine = m_ownership->IsHeldByCurrentThread();
    bool     isNewMine     = ownership.IsHeldByCurrentThread();
    bool     isBothMine    = isCurrentMine && isNewMine;



    CBRA (isBothMine);

    m_ownership = &ownership;

Error:
    return;
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



    ASSERT_THREAD_OWNERSHIP (*m_ownership);

    drive.rows.push_back (MakeTextRow ("Active drive",   std::format ("{}", m_activeDrive + 1)));
    drive.rows.push_back (MakeTextRow ("Activity",       activity));
    drive.rows.push_back (MakeFlagRow ("Motor",          m_motorOn));
    drive.rows.push_back (MakeFlagRow ("At speed",       IsMotorAtSpeed()));
    drive.rows.push_back (MakeTextRow ("Spin-up left",   std::format ("{} cycles", m_motorSpinupRemaining)));
    drive.rows.push_back (MakeTextRow ("Spin-down left", std::format ("{} cycles", m_motorSpindownCycles)));
    drive.rows.push_back (MakeFlagRow ("Disk in drive",  m_activeDisk[m_activeDrive] != nullptr && m_activeDisk[m_activeDrive]->IsLoaded()));

    head.rows.push_back (MakeTextRow ("Quarter track", std::format ("{}", GetQuarterTrack())));
    head.rows.push_back (MakeTextRow ("Track",         std::format ("{}.{:02}", GetQuarterTrack() / kQuarters, (GetQuarterTrack() % kQuarters) * kHundredth)));
    head.rows.push_back (MakeTextRow ("Other drive",   std::format ("quarter track {}", m_quarterTrack[m_activeDrive ^ 1])));
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
    snapshot.visual = DiagnosticsDiskHead { GetQuarterTrack(), kMaxQuarterTrack, m_phases, m_motorOn, m_activeDrive };
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



    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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
    writer.WriteByte   (m_iwmModeReg);
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



    ASSERT_THREAD_OWNERSHIP (*m_ownership);

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
    m_iwmModeReg              = iwmModeReg;
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


