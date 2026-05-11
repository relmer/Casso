#include "Pch.h"

#include "DiskIIController.h"
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

    for (i = 0; i < DiskIIController::kPhaseCount; i++)
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
//  DiskIIController
//
////////////////////////////////////////////////////////////////////////////////

DiskIIController::DiskIIController (int slot)
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

Byte DiskIIController::Read (Word address)
{
    int   offset = (address - m_ioStart) & 0x0F;

    HandleSwitch (offset);

    return HandleReadDispatch ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Soft-switch side effects fire on writes too. When Q7=1 + Q6=1, the
//  written value loads the engine's write latch (data field write path).
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIController::Write (Word address, Byte value)
{
    int   offset = (address - m_ioStart) & 0x0F;

    HandleSwitch (offset);

    if (m_q7 && m_q6)
    {
        m_engine[m_activeDrive].WriteLatch (value);
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

void DiskIIController::HandleSwitch (int offset)
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
            break;
        case 0x9:
            // Motor-on command: cancel any pending spindown so the
            // engine keeps producing nibbles continuously across the
            // motor-off / motor-on toggle DOS issues between sectors.
            m_motorSpindownCycles = 0;
            m_motorOn = true;
            m_engine[m_activeDrive].SetMotorOn (true);
            break;
        case 0xA:
            m_activeDrive = 0;
            UpdateEngineSelection ();
            break;
        case 0xB:
            m_activeDrive = 1;
            UpdateEngineSelection ();
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

Byte DiskIIController::HandleReadDispatch ()
{
    if (!m_q6 && !m_q7)
    {
        return m_engine[m_activeDrive].ReadLatch ();
    }

    if (m_q6 && !m_q7)
    {
        if (m_activeDisk[m_activeDrive]->IsWriteProtected ())
        {
            return 0x80;
        }

        return 0x00;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandlePhase
//
//  Update the phase mask, walk the head one quarter-track toward the
//  newly-energized phase, clamp to legal range, then push the resulting
//  full-track index into the active drive's engine.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIController::HandlePhase (int phase, bool on)
{
    if (on)
    {
        m_phases = static_cast<uint8_t> (m_phases | (1 << phase));
    }
    else
    {
        m_phases = static_cast<uint8_t> (m_phases & ~(1 << phase));
    }

    // Disk II stepper model (UTAIIe ch. 9; AppleWin ControlStepperDeferred):
    //   - 4 phase magnets arranged 90 degrees apart around the cog.
    //   - The head's rotational position (which magnet it's nearest)
    //     cycles every full track. Casso represents position as a
    //     quarter-track count (0..kMaxQuarterTrack); two consecutive
    //     quarter-tracks lie under different magnet positions, so
    //     `(m_quarterTrack / 2) & 3` is the current "phase index" the
    //     head is nearest.
    //   - Movement is determined by which adjacent magnets (rot+/-1)
    //     are currently energized. A single adjacent magnet pulls the
    //     head one half-track toward it (= 2 quarter-tracks). Two
    //     adjacent magnets ($3=0+1, $6=1+2, $C=2+3, $9=3+0) pull the
    //     head only halfway, i.e. one quarter-track (the cog rests
    //     between the two magnet positions).
    //   - Opposing-only magnet pairs ($5=0+2, $A=1+3) cancel out and
    //     leave the head where it is.
    //
    // The previous "highest set bit" model only stepped by one quarter-
    // track per phase event regardless of the magnet topology, which
    // walked the head ~1.5x too fast on standard DOS step sequences and
    // dropped multi-track seeks intermittently.
    int  rot       = (m_quarterTrack / 2) & 3;
    int  direction = 0;

    if (m_phases & (1 << ((rot + 1) & 3)))
    {
        direction += 1;
    }
    if (m_phases & (1 << ((rot + 3) & 3)))
    {
        direction -= 1;
    }

    int  qtDelta = direction * 2;

    if (m_phases == 0x3 || m_phases == 0x6 ||
        m_phases == 0xC || m_phases == 0x9)
    {
        qtDelta = direction;
    }

    m_quarterTrack += qtDelta;

    if (m_quarterTrack < 0)
    {
        m_quarterTrack = 0;
    }

    if (m_quarterTrack > kMaxQuarterTrack)
    {
        m_quarterTrack = kMaxQuarterTrack;
    }

    m_engine[m_activeDrive].SetCurrentTrack (m_quarterTrack / 4);
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

void DiskIIController::UpdateEngineSelection ()
{
    int   other = m_activeDrive ^ 1;

    m_engine[other].SetMotorOn (false);
    m_engine[m_activeDrive].SetMotorOn (m_motorOn);
    m_engine[m_activeDrive].SetCurrentTrack (m_quarterTrack / 4);
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

void DiskIIController::Tick (uint32_t cpuCycles)
{
    if (m_motorSpindownCycles > 0)
    {
        if (cpuCycles >= m_motorSpindownCycles)
        {
            m_motorSpindownCycles = 0;
            m_motorOn             = false;
            m_engine[0].SetMotorOn (false);
            m_engine[1].SetMotorOn (false);
        }
        else
        {
            m_motorSpindownCycles -= cpuCycles;
        }
    }

    m_engine[m_activeDrive].Tick (cpuCycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDisk / EjectDisk / GetDisk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIIController::MountDisk (int drive, const string & path)
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

Error:
    return hr;
}


void DiskIIController::EjectDisk (int drive)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    m_disks[drive].Eject ();
    m_activeDisk[drive] = &m_disks[drive];
    m_engine[drive].SetDiskImage (m_activeDisk[drive]);
}


DiskImage * DiskIIController::GetDisk (int drive)
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

void DiskIIController::SetExternalDisk (int drive, DiskImage * external)
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    m_activeDisk[drive] = (external != nullptr) ? external : &m_disks[drive];
    m_engine[drive].SetDiskImage (m_activeDisk[drive]);
}


bool DiskIIController::HasExternalDisk (int drive) const
{
    if (drive < 0 || drive >= kDriveCount)
    {
        return false;
    }

    return m_activeDisk[drive] != &m_disks[drive];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIController::Reset ()
{
    int   i = 0;

    m_phases       = 0;
    m_quarterTrack = 0;
    m_motorOn      = false;
    m_motorSpindownCycles = 0;
    m_activeDrive  = 0;
    m_q6           = false;
    m_q7           = false;

    for (i = 0; i < kDriveCount; i++)
    {
        m_engine[i].Reset ();
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

void DiskIIController::SoftReset ()
{
    HRESULT   hrFlush = S_OK;
    int       drive   = 0;

    Reset ();

    for (drive = 0; drive < kDriveCount; drive++)
    {
        if (m_activeDisk[drive]->IsLoaded ())
        {
            hrFlush = m_activeDisk[drive]->Flush ();
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

void DiskIIController::PowerCycle (Prng & prng)
{
    int   drive = 0;

    UNREFERENCED_PARAMETER (prng);

    Reset ();

    for (drive = 0; drive < kDriveCount; drive++)
    {
        m_disks[drive].Eject ();
        m_activeDisk[drive] = &m_disks[drive];
        m_engine[drive].SetDiskImage (m_activeDisk[drive]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> DiskIIController::Create (const DeviceConfig & config, MemoryBus & bus)
{
    int   slot = config.hasSlot ? config.slot : 6;

    UNREFERENCED_PARAMETER (bus);

    return make_unique<DiskIIController> (slot);
}
