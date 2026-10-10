#include "Pch.h"

#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "Devices/Disk/DiskImage.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleEngine
//
////////////////////////////////////////////////////////////////////////////////

Disk2NibbleEngine::Disk2NibbleEngine()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~Disk2NibbleEngine
//
//  A write still open when the drive goes away belongs in the image, and the
//  image must not keep a pointer to a drive that no longer exists.
//
////////////////////////////////////////////////////////////////////////////////

Disk2NibbleEngine::~Disk2NibbleEngine()
{
    CommitPendingWrite();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDiskImage
//
//  Called by Disk2Controller on Mount / Eject / DriveSelect transitions.
//  Resets the bit cursor so the new image starts streaming from offset 0.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetDiskImage (DiskImage * disk)
{
    CommitPendingWrite();

    m_disk        = disk;
    m_bitPos      = 0;
    m_headWindow  = 0;

    ResolveSlot();
    PlaceHead (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMotorOn
//
//  Motor-off freezes the bit cursor; the next motor-on resumes from the
//  same position (real Disk II behavior -- the disk keeps spinning for ~1s
//  after motor-off, which the controller models with its spindown timer).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetMotorOn (bool on)
{
    if (!on)
    {
        CommitPendingWrite();
    }

    m_motorOn = on;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteMode
//
//  Leaving write mode ends a write, so one held for a flux track goes in.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetWriteMode (bool q7)
{
    if (!q7)
    {
        CommitPendingWrite();
    }

    m_writeMode = q7;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetShiftLoadMode
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetShiftLoadMode (bool q6)
{
    m_shiftLoadMode = q6;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCurrentTrack
//
//  Clamps to [kMinTrack, kMaxTrack]. Track is a quarter-track index
//  (0..159); the controller passes the head's physical quarter-track
//  position. ResolveQuarterTrack maps it to a backing storage slot (-1 ==
//  unformatted). Switching tracks preserves rotational position as the
//  fraction of a revolution, whatever kind and length either track is.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetCurrentTrack (int track)
{
    int      clamped  = track;
    double   angle    = 0;



    if (clamped < kMinTrack)
    {
        clamped = kMinTrack;
    }

    if (clamped > kMaxTrack)
    {
        clamped = kMaxTrack;
    }

    if (clamped != m_currentTrack)
    {
        // Real Disk II behavior: the head physically moves between
        // tracks while the disk keeps spinning, so the head lands on
        // the new track at the same angle it left the old one.
        // Resetting m_bitPos to 0 here used to corrupt every
        // track-change read because the read latch lost its sync
        // alignment and had to spend an entire revolution finding the
        // next address-field sync gap before any sector could be
        // located -- which on a tight RWTS read loop frequently times
        // out and reports a checksum error.
        CommitPendingWrite();

        angle          = GetAngle();
        m_currentTrack = clamped;

        ResolveSlot();
        PlaceHead (angle);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveSlot
//
//  Looks up the slot under the head once, so the sequencer does not have to
//  on each of the 410,000 clocks a revolution takes.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::ResolveSlot()
{
    m_slot             = (m_disk != nullptr) ? m_disk->ResolveQuarterTrack (m_currentTrack) : -1;
    m_isFluxSlot       = (m_slot >= 0) && (m_disk->GetTrackKind (m_slot) == TrackKind::Flux);
    m_layoutGeneration = (m_disk != nullptr) ? m_disk->GetLayoutGeneration() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshSlot
//
//  The disk's layout changed under the head -- a reload, a new map, a write
//  spliced into a flux track. Look up the slot again and put the head back
//  at the same point of the revolution.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::RefreshSlot()
{
    double  angle = GetAngle();



    ResolveSlot();
    PlaceHead (angle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAngle
//
//  How far round the revolution the head is, from 0 up to 1.
//
////////////////////////////////////////////////////////////////////////////////

double Disk2NibbleEngine::GetAngle() const
{
    double  angle = 0;
    size_t  bits  = 0;



    if (m_isFluxSlot && m_fluxRevUnits > 0)
    {
        angle = static_cast<double> (m_fluxNow % m_fluxRevUnits) / static_cast<double> (m_fluxRevUnits);
    }
    else
    {
        bits  = GetCurrentTrackBits();
        angle = (bits > 0) ? static_cast<double> (m_bitPos % bits) / static_cast<double> (bits) : 0;
    }

    return angle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlaceHead
//
//  Puts the head on the newly resolved track at the given angle. Every track
//  change goes by the fraction of a revolution: tracks differ in length by up
//  to a tenth, so carrying the bit index instead would land the head at the
//  wrong point of the disk, and the error would grow with each step.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::PlaceHead (double angle)
{
    size_t  newBits = 0;



    if (m_isFluxSlot)
    {
        SeekFlux (angle);
        return;
    }

    newBits = GetCurrentTrackBits();

    if (newBits == 0)
    {
        m_bitPos = 0;
        return;
    }

    m_bitPos = static_cast<size_t> (llround (angle * static_cast<double> (newBits))) % newBits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SeekFlux
//
//  Starts flux playback at the given angle. The clock restarts at that point
//  of the first revolution, and the cursor at the first transition at or
//  after it.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SeekFlux (double angle)
{
    const FluxTrack  &  track     = m_disk->GetFluxTrack (m_slot);
    uint64_t            revTicks  = track.GetRevolutionTicks();
    uint64_t            tickInRev = static_cast<uint64_t> (angle * static_cast<double> (revTicks));



    if (tickInRev >= revTicks)
    {
        tickInRev = 0;
    }

    m_fluxRevUnits  = revTicks * kFluxUnitsPerTick;
    m_fluxNow       = tickInRev * kFluxUnitsPerTick;
    m_fluxLastPulse = m_fluxNow;
    m_fluxCursor    = track.FindTransitionAtOrAfter (tickInRev);
    m_fluxDue       = track.HasTransitions() ? m_fluxCursor.tick * kFluxUnitsPerTick : UINT64_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepFluxPulse
//
//  Reports whether a transition reached the head during the sequencer clock
//  that just moved flux time on. With no real transition for longer than the
//  read amplifier holds its gain -- four cells, the same window the bit-track
//  model uses -- the amplifier turns noise into pulses, once per cell and
//  with the same odds as on a bit track.
//
////////////////////////////////////////////////////////////////////////////////

uint8_t Disk2NibbleEngine::StepFluxPulse()
{
    constexpr uint64_t  kWeakWindowUnits = kHeadWindowCells * kFluxUnitsPerCell;



    uint8_t  pulse = 0;



    if (m_fluxNow >= m_fluxDue)
    {
        const FluxTrack  &  track = m_disk->GetFluxTrack (m_slot);

        pulse           = 1;
        m_fluxLastPulse = m_fluxDue;

        // Two transitions inside one clock reach the sequencer as one pulse.
        while (m_fluxDue <= m_fluxNow)
        {
            track.AdvanceCursor (m_fluxCursor);
            m_fluxDue = m_fluxCursor.tick * kFluxUnitsPerTick;
        }
    }
    else if (m_lssClock == kLssReadClock && m_fluxNow - m_fluxLastPulse > kWeakWindowUnits)
    {
        pulse = NextWeakBit();
    }

    if (pulse != 0)
    {
        m_fluxPulseClock = m_fluxNow;
        m_fluxPulseCount++;
    }

    return pulse;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordFluxWriteBit
//
//  One written cell on a flux track. The cells are held as bits and spliced
//  into the flux in one pass when the write ends, rather than re-encoding the
//  track a bit at a time.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::RecordFluxWriteBit (uint8_t bit)
{
    if (!m_burstActive)
    {
        m_burstActive    = true;
        m_burstSlot      = m_slot;
        m_burstStartTick = (m_fluxNow % m_fluxRevUnits) / kFluxUnitsPerTick;
        m_burstBits.clear();
        m_disk->SetPendingWriteOwner (this);
    }

    m_burstBits.push_back (bit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitPendingWrite
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::CommitPendingWrite()
{
    if (!m_burstActive)
    {
        return;
    }

    m_burstActive = false;

    if (m_disk != nullptr)
    {
        m_disk->SetPendingWriteOwner (nullptr);
        m_disk->SpliceFluxWrite (m_burstSlot, m_burstStartTick, m_burstBits);
    }

    m_burstBits.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCurrentTrackBits
//
//  Bit length of the stream under the head. A resolved slot reports its
//  real length; an unformatted position (slot -1) reports the nominal
//  blank-track length so the disk keeps spinning and the weak-bit model
//  keeps producing noise.
//
////////////////////////////////////////////////////////////////////////////////

size_t Disk2NibbleEngine::GetCurrentTrackBits() const
{
    int      slot = (m_disk != nullptr) ? m_disk->ResolveQuarterTrack (m_currentTrack) : -1;
    size_t   bits = 0;



    // No disk at all reports 0 (the drive is empty and must not spin), while
    // an unformatted position on a real disk reports the nominal blank length
    // so rotation and the weak-bit noise model keep running.
    if (m_disk != nullptr)
    {
        bits = (slot < 0) ? kUnformattedTrackBits
                          : m_disk->GetTrackBitCount (slot);
    }

    return bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::Reset()
{
    constexpr uint8_t    kLssInitialState      = 2;



    CommitPendingWrite();

    m_motorOn        = false;
    m_writeMode      = false;
    m_shiftLoadMode  = false;
    m_bitPos         = 0;
    m_lssState       = kLssInitialState;
    m_lssClock       = 0;
    m_readLatch      = 0;
    m_bus            = 0;
    m_latchIsFresh   = false;
    m_readNibbles    = 0;
    m_writeNibbles   = 0;
    m_headWindow     = 0;
    m_weakRngState   = 0xDEADBEEFu;

    ResolveSlot();
    PlaceHead (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Advance the Logic State Sequencer by two LSS clocks per CPU cycle.
//  Motor-off freezes the sequencer (the controller models the ~1s
//  post-command spindown separately).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::Tick (uint32_t cpuCycles)
{
    constexpr uint32_t   kLssClocksPerCpuCycle = 2;



    uint32_t   lssClocks = 0;
    uint32_t   i         = 0;

    if (!m_motorOn)
    {
        return;
    }

    lssClocks = cpuCycles * kLssClocksPerCpuCycle;

    for (i = 0; i < lssClocks; i++)
    {
        StepLss();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepLss
//
//  One 2 MHz Logic State Sequencer clock. Faithful port of the P6 state
//  machine: sample the read pulse (clock 4 only), index the sequencer ROM
//  by {pulse, latch MSB, Q6, Q7, state}, execute the resulting command,
//  advance to the next state, and -- at clock 4 -- write any outgoing bit
//  and advance the head one bit cell.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::StepLss()
{
    constexpr int      kLssMaxClock    = 7;
    constexpr uint8_t  kIdxNoPulse     = 0x01;
    constexpr uint8_t  kIdxLatchMsb    = 0x02;
    constexpr uint8_t  kIdxQ6          = 0x04;
    constexpr uint8_t  kIdxQ7          = 0x08;
    constexpr int      kIdxStateShift  = 4;
    constexpr uint8_t  kLssCommandMask = 0x0F;
    constexpr int      kLssStateShift  = 4;
    constexpr uint8_t  kLssStateMask   = 0x0F;



    bool     readClock = (m_lssClock == kLssReadClock);
    bool     hasTrack  = false;
    bool     writing   = false;
    bool     prevMsb   = false;
    uint8_t  pulse     = 0;
    uint8_t  outBit    = 0;
    uint8_t  command   = 0;
    size_t   trackBits = 0;

    if (m_disk != nullptr && m_disk->GetLayoutGeneration() != m_layoutGeneration)
    {
        RefreshSlot();
    }

    // A mapped track implies a disk; with no disk the slot is -1.
    hasTrack = (m_slot >= 0);

    // Read side. A flux track can deliver a pulse on any clock; a bit track
    // delivers one bit per cell, on the read clock. With no track under the
    // head the head window turns silence into noise, as an empty drive does.
    if (m_isFluxSlot)
    {
        m_fluxNow += kFluxUnitsPerLssClock;

        if (readClock || m_fluxNow >= m_fluxDue)
        {
            pulse = StepFluxPulse();
        }
    }
    else if (readClock)
    {
        pulse = ApplyHeadWindow (hasTrack ? m_disk->ReadBit (m_slot, m_bitPos) : 0);
    }

    // The sequencer: index the P6 ROM by {state, Q7, Q6, latch MSB, no pulse}
    // and run the command it gives.
    prevMsb = (m_readLatch & kLatchMsbMask) != 0;
    command = kSequencerRom16[(m_lssState << kIdxStateShift)
                              | (m_writeMode     ? kIdxQ7       : 0)
                              | (m_shiftLoadMode ? kIdxQ6       : 0)
                              | (prevMsb         ? kIdxLatchMsb : 0)
                              | (pulse           ? 0            : kIdxNoPulse)];

    switch (command & kLssCommandMask)
    {
        case kLssCmdClr:
            m_readLatch = 0;
            break;
        case kLssCmdShiftZero:
            m_readLatch = static_cast<uint8_t> (m_readLatch << 1);
            break;
        case kLssCmdShiftOne:
            m_readLatch = static_cast<uint8_t> ((m_readLatch << 1) | 0x01);
            break;
        case kLssCmdShiftRight:
            // Write-protect sense: the switch reads back as the latch MSB.
            m_readLatch = static_cast<uint8_t> (m_readLatch >> 1);

            if (m_disk != nullptr && m_disk->IsWriteProtected())
            {
                m_readLatch |= kLatchMsbMask;
            }

            break;
        case kLssCmdLoad:
            m_readLatch = m_bus;
            break;
        default:
            break;
    }

    m_lssState = static_cast<uint8_t> ((command >> kLssStateShift) & kLssStateMask);

    // A rising latch MSB in read mode means a whole nibble has assembled.
    if (!m_shiftLoadMode && !m_writeMode && !prevMsb && (m_readLatch & kLatchMsbMask) != 0)
    {
        m_latchIsFresh = true;
        m_readNibbles++;
    }

    // Write side and head motion, once per cell. The bit written is the latch
    // MSB -- the shift register's serial output -- not the sequencer state's
    // high bit: the two only agree in the hardware's sub-clock lockstep,
    // which catching the sequencer up in bursts cannot hold (GH #89).
    if (readClock)
    {
        writing = m_writeMode && hasTrack && !m_disk->IsWriteProtected();
        outBit  = (m_readLatch & kLatchMsbMask) ? 1 : 0;

        if (m_isFluxSlot)
        {
            // Flux keeps its own time, so there is no cursor to move. The
            // cells are collected and spliced in when the write ends.
            if (writing)
            {
                RecordFluxWriteBit (outBit);
            }
            else if (m_burstActive)
            {
                CommitPendingWrite();
            }
        }
        else if (hasTrack)
        {
            if (writing)
            {
                m_disk->WriteBit (m_slot, m_bitPos, outBit);
            }

            trackBits = m_disk->GetTrackBitCount (m_slot);

            if (trackBits > 0)
            {
                m_bitPos = (m_bitPos + 1) % trackBits;
            }
        }
        else if (m_disk != nullptr)
        {
            // A disk with nothing recorded at this position still turns
            // under the head.
            m_bitPos = (m_bitPos + 1) % kUnformattedTrackBits;
        }
    }

    if (++m_lssClock > kLssMaxClock)
    {
        m_lssClock = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadLatch
//
//  Pure sample of the data register, exactly as the 6502 sees it when it
//  reads $C0EC. The LSS has already been ticked forward to the current
//  cycle by the controller, so the latch holds the correct value. There
//  is NO side effect: the CPU spins a tight LDA $C0EC / BPL loop and must
//  see the same byte on repeated reads until the next nibble assembles.
//
////////////////////////////////////////////////////////////////////////////////

uint8_t Disk2NibbleEngine::ReadLatch()
{
    return m_readLatch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLatch
//
//  Stores the CPU-written byte on the controller bus. The LSS LOAD command
//  (Q6 high, Q7 high) copies the bus into the data latch, after which the
//  shift path (Q6 low, Q7 high) streams it onto the track one bit per cell.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::WriteLatch (uint8_t value)
{
    m_bus = value;
    m_writeNibbles++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsumeFreshNibble
//
//  Passive-watcher side channel: returns true exactly once per LSS
//  "byte ready" rising edge. The controller calls this AFTER ReadLatch
//  so the watcher's address-mark / data-mark state machines see exactly
//  one nibble per assembly cycle instead of the CPU-visible repeat
//  stream. Does NOT touch m_readLatch, so the CPU-visible byte returned
//  by ReadLatch is unchanged.
//
//  Returns false unless both the latch is fresh AND its MSB is set.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2NibbleEngine::ConsumeFreshNibble (uint8_t & outNibble)
{
    // Both conditions matter: fresh means this is a new assembly cycle, and
    // the MSB means the LSS has finished shifting a whole nibble in.
    bool  ready = m_latchIsFresh && (m_readLatch & kLatchMsbMask) != 0;



    if (ready)
    {
        outNibble      = m_readLatch;
        m_latchIsFresh = false;
    }

    return ready;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyHeadWindow
//
//  MC3470 read-amplifier model, ported from AppleWin's DataLatchReadWOZ
//  (Disk.cpp). Maintains a sliding 4-bit window of the most-recent bits
//  read off the surface. Two effects:
//
//    1. One-bit pipeline delay. When the window has at least one 1-bit,
//       the amplifier outputs the bit read on the PREVIOUS call (window
//       bit 1, not the just-shifted-in bit 0). This is hardware behavior
//       -- the amp needs a cell of integration time.
//
//    2. Weak bits / floating output. When all four window bits are zero
//       (an unformatted region or intentional weak-bit gap), the amplifier
//       has no signal to lock to and floats. AppleWin models this as a
//       ~30% chance of a 1-bit per cell. WOZ-2.0 protection schemes
//       (Karateka RWTS18, Lode Runner, etc.) key off this randomness to
//       detect copies that trimmed the floating region to a deterministic
//       value during duplication. The WOZ spec calls this "Freaking Out
//       Like a MC3470".
//
//  RNG is a per-engine LCG (not the global rand() AppleWin uses) so that
//  tests remain deterministic per engine instance.
//
////////////////////////////////////////////////////////////////////////////////

uint8_t Disk2NibbleEngine::ApplyHeadWindow (uint8_t inBit)
{
    constexpr uint8_t  kWindowMask = (1 << kHeadWindowCells) - 1;



    uint8_t   outBit = 0;



    m_headWindow = static_cast<uint8_t> (((m_headWindow << 1) | (inBit & 1)) & kWindowMask);

    if ((m_headWindow & kWindowMask) != 0)
    {
        outBit = static_cast<uint8_t> ((m_headWindow >> 1) & 1);
    }
    else
    {
        outBit = NextWeakBit();
    }

    return outBit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NextWeakBit
//
//  Per-engine deterministic LCG (Numerical Recipes constants). Returns
//  a 1-bit with ~30% probability -- the WOZ-2.0 reference value for
//  MC3470 floating-output behavior.
//
////////////////////////////////////////////////////////////////////////////////

uint8_t Disk2NibbleEngine::NextWeakBit()
{
    static constexpr uint32_t   kLcgMultiplier = 1664525u;
    static constexpr uint32_t   kLcgIncrement  = 1013904223u;
    // 0x4CCCCCCC = floor (0.3 * 2^32). Compare m_weakRngState (unsigned
    // 32-bit) against this for a ~30% probability of returning 1.
    static constexpr uint32_t   kWeakThreshold = 0x4CCCCCCCu;



    m_weakRngState = m_weakRngState * kLcgMultiplier + kLcgIncrement;

    return (m_weakRngState < kWeakThreshold) ? 1 : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2NibbleEngine::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte   (static_cast<Byte> (m_currentTrack));
    writer.WriteBool   (m_motorOn);
    writer.WriteBool   (m_writeMode);
    writer.WriteBool   (m_shiftLoadMode);
    writer.WriteUInt64 (m_bitPos);
    writer.WriteByte   (m_lssState);
    writer.WriteByte   (static_cast<Byte> (m_lssClock));
    writer.WriteByte   (m_readLatch);
    writer.WriteByte   (m_bus);
    writer.WriteBool   (m_latchIsFresh);
    writer.WriteByte   (m_headWindow);
    writer.WriteUInt32 (m_weakRngState);
    writer.WriteUInt64 (m_readNibbles);
    writer.WriteUInt64 (m_writeNibbles);

    WriteFluxState (writer);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  Reads into locals and range-checks the head position, the sequencer state
//  and clock, and the bit cursor before committing anything, so a bad blob
//  leaves the engine as it was. The cursor is checked against the track the
//  loaded head position resolves to on the disk now attached; an empty drive
//  or an unformatted position has no length to check against. The flux fields
//  are checked against the flux track the head is over, and the slot under the
//  head is looked up again afterward without moving the head, since the saved
//  position is exact.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2NibbleEngine::LoadState (StateReader & reader)
{
    constexpr uint8_t  kLssStateCount = 16;
    constexpr Byte     kLssClockCount = 8;



    HRESULT   hr            = S_OK;
    uint16_t  version       = 0;
    Byte      currentTrack  = 0;
    bool      motorOn       = false;
    bool      writeMode     = false;
    bool      shiftLoadMode = false;
    uint64_t  bitPos        = 0;
    Byte      lssState      = 0;
    Byte      lssClock      = 0;
    Byte      readLatch     = 0;
    Byte      bus           = 0;
    bool      latchIsFresh  = false;
    Byte      headWindow    = 0;
    uint32_t  weakRngState  = 0;
    uint64_t  readNibbles   = 0;
    uint64_t  writeNibbles  = 0;
    int       slot          = -1;
    size_t    trackBits     = 0;
    SavedFlux flux;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    // Version 1 had no flux timeline.
    CBREx (version == kStateVersion, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    reader.ReadByte   (currentTrack);
    reader.ReadBool   (motorOn);
    reader.ReadBool   (writeMode);
    reader.ReadBool   (shiftLoadMode);
    reader.ReadUInt64 (bitPos);
    reader.ReadByte   (lssState);
    reader.ReadByte   (lssClock);
    reader.ReadByte   (readLatch);
    reader.ReadByte   (bus);
    reader.ReadBool   (latchIsFresh);
    reader.ReadByte   (headWindow);
    reader.ReadUInt32 (weakRngState);
    reader.ReadUInt64 (readNibbles);
    reader.ReadUInt64 (writeNibbles);

    hr = ReadFluxState (reader, flux);
    CHR (hr);

    hr = reader.EndSection();
    CHR (hr);

    CBREx (currentTrack <= kMaxTrack,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (lssState     <  kLssStateCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (lssClock     <  kLssClockCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (m_disk != nullptr)
    {
        slot      = m_disk->ResolveQuarterTrack (currentTrack);
        trackBits = (slot < 0) ? kUnformattedTrackBits : m_disk->GetTrackBitCount (slot);
    }

    CBREx (trackBits == 0 || bitPos < trackBits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = CheckFluxState (flux, slot);
    CHR (hr);

    m_currentTrack  = currentTrack;
    m_motorOn       = motorOn;
    m_writeMode     = writeMode;
    m_shiftLoadMode = shiftLoadMode;
    m_bitPos        = static_cast<size_t> (bitPos);
    m_lssState      = lssState;
    m_lssClock      = lssClock;
    m_readLatch     = readLatch;
    m_bus           = bus;
    m_latchIsFresh  = latchIsFresh;
    m_headWindow    = headWindow;
    m_weakRngState  = weakRngState;
    m_readNibbles   = readNibbles;
    m_writeNibbles  = writeNibbles;

    ApplyFluxState (flux);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFluxState
//
//  The flux timeline and cursor, the pulse diagnostics, and a write held for
//  a flux track with its cells so far. A held write is saved rather than put
//  into the track, since putting it in would change the disk and the head
//  position at a moment the machine did not.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::WriteFluxState (StateWriter & writer) const
{
    writer.WriteUInt64 (m_fluxNow);
    writer.WriteUInt64 (m_fluxDue);
    writer.WriteUInt64 (m_fluxLastPulse);
    writer.WriteUInt64 (m_fluxRevUnits);
    writer.WriteUInt64 (m_fluxCursor.nextIndex);
    writer.WriteUInt64 (m_fluxCursor.tick);
    writer.WriteUInt64 (m_fluxPulseClock);
    writer.WriteUInt64 (m_fluxPulseCount);
    writer.WriteBool   (m_burstActive);
    writer.WriteUInt32 (static_cast<uint32_t> (m_burstSlot));
    writer.WriteUInt64 (m_burstStartTick);
    writer.WriteUInt32 (static_cast<uint32_t> (m_burstBits.size()));
    writer.WriteBytes  (m_burstBits.data(), m_burstBits.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFluxState
//
//  The fields WriteFluxState saved. The held write's length is capped before
//  its buffer is sized.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2NibbleEngine::ReadFluxState (StateReader & reader, SavedFlux & outFlux)
{
    HRESULT   hr        = S_OK;
    uint64_t  nextIndex = 0;
    uint32_t  burstSlot = 0;
    uint32_t  bitCount  = 0;



    reader.ReadUInt64 (outFlux.now);
    reader.ReadUInt64 (outFlux.due);
    reader.ReadUInt64 (outFlux.lastPulse);
    reader.ReadUInt64 (outFlux.revUnits);
    reader.ReadUInt64 (nextIndex);
    reader.ReadUInt64 (outFlux.cursor.tick);
    reader.ReadUInt64 (outFlux.pulseClock);
    reader.ReadUInt64 (outFlux.pulseCount);
    reader.ReadBool   (outFlux.burstActive);
    reader.ReadUInt32 (burstSlot);
    reader.ReadUInt64 (outFlux.burstStartTick);
    reader.ReadUInt32 (bitCount);

    CBREx (bitCount <= DiskImage::kMaxStateTrackSize, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outFlux.cursor.nextIndex = static_cast<size_t> (nextIndex);
    outFlux.burstSlot        = static_cast<int> (burstSlot);

    outFlux.burstBits.resize (bitCount);
    reader.ReadBytes (outFlux.burstBits.data(), bitCount);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckFluxState
//
//  Over a flux track, the saved revolution must be that track's and the
//  cursor must lie within its bytes. A held write must be for a flux track on
//  the disk now attached.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2NibbleEngine::CheckFluxState (const SavedFlux & flux, int slot) const
{
    HRESULT   hr          = S_OK;
    bool      isFluxSlot  = (m_disk != nullptr) && (slot >= 0) && (m_disk->GetTrackKind (slot) == TrackKind::Flux);
    bool      isBurstFlux = false;
    uint64_t  revUnits    = 0;
    size_t    fluxBytes   = 0;



    if (isFluxSlot)
    {
        revUnits  = m_disk->GetFluxTrack (slot).GetRevolutionTicks() * kFluxUnitsPerTick;
        fluxBytes = m_disk->GetFluxTrack (slot).GetBytes().size();

        CBREx (flux.revUnits         == revUnits,  HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        CBREx (flux.cursor.nextIndex <= fluxBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    BAIL_OUT_IF (!flux.burstActive, S_OK);

    CBREx (m_disk, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isBurstFlux = (m_disk->GetTrackKind (flux.burstSlot) == TrackKind::Flux);
    CBREx (isBurstFlux, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyFluxState
//
//  Commits checked flux fields. The slot under the head is looked up again
//  for the loaded head position and the disk as now loaded, and the disk is
//  told whether this drive holds a write for it.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::ApplyFluxState (SavedFlux & flux)
{
    m_fluxNow        = flux.now;
    m_fluxDue        = flux.due;
    m_fluxLastPulse  = flux.lastPulse;
    m_fluxRevUnits   = flux.revUnits;
    m_fluxCursor     = flux.cursor;
    m_fluxPulseClock = flux.pulseClock;
    m_fluxPulseCount = flux.pulseCount;
    m_burstActive    = flux.burstActive;
    m_burstSlot      = flux.burstSlot;
    m_burstStartTick = flux.burstStartTick;

    m_burstBits.swap (flux.burstBits);

    ResolveSlot();

    if (m_disk != nullptr)
    {
        m_disk->SetPendingWriteOwner (m_burstActive ? this : nullptr);
    }
}

