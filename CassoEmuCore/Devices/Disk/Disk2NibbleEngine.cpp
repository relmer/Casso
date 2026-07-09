#include "Pch.h"

#include "Disk2NibbleEngine.h"
#include "DiskImage.h"




namespace
{
    // Logic State Sequencer clocking. The P6 sequencer runs at 2 MHz --
    // two LSS clocks per 1.023 MHz CPU cycle. Eight LSS clocks make one
    // bit cell, so the head advances one bit every four CPU cycles. The
    // read pulse is sampled once per bit cell, at clock 4.
    constexpr uint32_t   kLssClocksPerCpuCycle = 2;
    constexpr int        kLssReadClock         = 4;
    constexpr int        kLssMaxClock          = 7;
    constexpr uint8_t    kLssInitialState      = 2;

    // Data-latch MSB ("byte ready" / QA, 74LS323 QH). Doubles as the read
    // "byte ready" signal and the write shift register's serial output.
    constexpr uint8_t    kLatchMsbMask  = 0x80;

    // Sequencer ROM index bit positions (see "Understanding the Apple IIe"
    // Fig 9.11 column ordering): pulse-absent, latch MSB, Q6, Q7, then the
    // 4-bit current state shifted into the high nibble.
    constexpr uint8_t    kIdxNoPulse  = 0x01;
    constexpr uint8_t    kIdxLatchMsb = 0x02;
    constexpr uint8_t    kIdxQ6       = 0x04;
    constexpr uint8_t    kIdxQ7       = 0x08;
    constexpr int        kIdxStateShift = 4;

    // P6 sequencer command opcodes (low nibble of each ROM entry). See
    // UtA2e Table 9.3 "Logic State Sequencer Commands".
    constexpr uint8_t    kLssCmdClr       = 0x0;   // latch <- 0
    constexpr uint8_t    kLssCmdNop       = 0x8;   // hold
    constexpr uint8_t    kLssCmdShiftZero = 0x9;   // latch <- latch << 1
    constexpr uint8_t    kLssCmdShiftRight = 0xA;  // latch >>= 1 (WP -> 0xFF)
    constexpr uint8_t    kLssCmdLoad      = 0xB;   // latch <- bus
    constexpr uint8_t    kLssCmdShiftOne  = 0xD;   // latch <- (latch << 1) | 1

    constexpr uint8_t    kLssCommandMask  = 0x0F;
    constexpr int        kLssStateShift   = 4;
    constexpr uint8_t    kLssStateMask    = 0x0F;

    // DOS 3.3 / 16-sector P6 Logic State Sequencer ROM. 16 states (rows) x
    // 16 input combinations (columns). Column index = Q7<<3 | Q6<<2 |
    // QA<<1 | (pulse ? 0 : 1). High nibble of each entry is the next
    // state; low nibble is the command opcode above.
    //
    // Source: "Understanding the Apple IIe" (Sather) Fig 9.11; identical
    // bytes published in apple2js (MIT, (c) Will Scullin), js/cards/
    // disk2.ts SEQUENCER_ROM_16. The ROM contents are factual hardware
    // data (the physical P6 PROM image).
    constexpr uint8_t   s_kSequencerRom16[256] =
    {
        //                Q7 L (Read)                                     Q7 H (Write)
        //    Q6 L (Shift)            Q6 H (Load)             Q6 L (Shift)             Q6 H (Load)
        //  QA L        QA H        QA L        QA H        QA L        QA H        QA L        QA H
        0x18, 0x18, 0x18, 0x18, 0x0A, 0x0A, 0x0A, 0x0A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, // 0
        0x2D, 0x2D, 0x38, 0x38, 0x0A, 0x0A, 0x0A, 0x0A, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, // 1
        0xD8, 0x38, 0x08, 0x28, 0x0A, 0x0A, 0x0A, 0x0A, 0x39, 0x39, 0x39, 0x39, 0x3B, 0x3B, 0x3B, 0x3B, // 2
        0xD8, 0x48, 0x48, 0x48, 0x0A, 0x0A, 0x0A, 0x0A, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, // 3
        0xD8, 0x58, 0xD8, 0x58, 0x0A, 0x0A, 0x0A, 0x0A, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, // 4
        0xD8, 0x68, 0xD8, 0x68, 0x0A, 0x0A, 0x0A, 0x0A, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, // 5
        0xD8, 0x78, 0xD8, 0x78, 0x0A, 0x0A, 0x0A, 0x0A, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, // 6
        0xD8, 0x88, 0xD8, 0x88, 0x0A, 0x0A, 0x0A, 0x0A, 0x08, 0x08, 0x88, 0x88, 0x08, 0x08, 0x88, 0x88, // 7
        0xD8, 0x98, 0xD8, 0x98, 0x0A, 0x0A, 0x0A, 0x0A, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, // 8
        0xD8, 0x29, 0xD8, 0xA8, 0x0A, 0x0A, 0x0A, 0x0A, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, // 9
        0xCD, 0xBD, 0xD8, 0xB8, 0x0A, 0x0A, 0x0A, 0x0A, 0xB9, 0xB9, 0xB9, 0xB9, 0xBB, 0xBB, 0xBB, 0xBB, // A
        0xD9, 0x59, 0xD8, 0xC8, 0x0A, 0x0A, 0x0A, 0x0A, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, // B
        0xD9, 0xD9, 0xD8, 0xA0, 0x0A, 0x0A, 0x0A, 0x0A, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, // C
        0xD8, 0x08, 0xE8, 0xE8, 0x0A, 0x0A, 0x0A, 0x0A, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, // D
        0xFD, 0xFD, 0xF8, 0xF8, 0x0A, 0x0A, 0x0A, 0x0A, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, // E
        0xDD, 0x4D, 0xE0, 0xE0, 0x0A, 0x0A, 0x0A, 0x0A, 0x88, 0x88, 0x08, 0x08, 0x88, 0x88, 0x08, 0x08  // F
    };
}




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
//  SetDiskImage
//
//  Called by Disk2Controller on Mount / Eject / DriveSelect transitions.
//  Resets the bit cursor so the new image starts streaming from offset 0.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetDiskImage (DiskImage * disk)
{
    m_disk        = disk;
    m_bitPos      = 0;
    m_headWindow  = 0;
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
    m_motorOn = on;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteMode / SetShiftLoadMode
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetWriteMode (bool q7)
{
    m_writeMode = q7;
}


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
//  unformatted). Switching tracks preserves rotational position by
//  carrying the bit cursor modulo the new track's bit length.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetCurrentTrack (int track)
{
    int      clamped = track;
    size_t   newBits = 0;

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
        // tracks while the disk keeps spinning. The bit cursor (the
        // rotational position of the disk under the head) carries
        // over modulo the new track's bit length. Resetting m_bitPos
        // to 0 here used to corrupt every track-change read because
        // the read latch lost its sync alignment and had to spend
        // an entire revolution finding the next address-field sync
        // gap before any sector could be located -- which on a tight
        // RWTS read loop frequently times out and reports a checksum
        // error. Cap to the new track's bit length so we don't end
        // up past the wrap.
        m_currentTrack = clamped;
        newBits        = CurrentTrackBits ();

        if (newBits > 0)
        {
            m_bitPos = m_bitPos % newBits;
        }
        else
        {
            m_bitPos = 0;
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  CurrentTrackBits
//
//  Bit length of the stream under the head. A resolved slot reports its
//  real length; an unformatted position (slot -1) reports the nominal
//  blank-track length so the disk keeps spinning and the weak-bit model
//  keeps producing noise.
//
////////////////////////////////////////////////////////////////////////////////

size_t Disk2NibbleEngine::CurrentTrackBits () const
{
    int   slot = (m_disk != nullptr) ? m_disk->ResolveQuarterTrack (m_currentTrack) : -1;

    if (m_disk == nullptr)
    {
        return 0;
    }

    if (slot < 0)
    {
        return kUnformattedTrackBits;
    }

    return m_disk->GetTrackBitCount (slot);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::Reset()
{
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
    uint8_t   pulse      = 0;
    uint8_t   idx        = 0;
    uint8_t   command    = 0;
    bool      prevMsbSet = false;
    size_t    trackBits  = 0;
    int       slot       = (m_disk != nullptr) ? m_disk->ResolveQuarterTrack (m_currentTrack) : -1;

    if (m_lssClock == kLssReadClock)
    {
        uint8_t  rawBit = (slot >= 0)
                          ? m_disk->ReadBit (slot, m_bitPos)
                          : 0;

        pulse = ApplyHeadWindow (rawBit);
    }

    idx  = static_cast<uint8_t> (pulse ? 0 : kIdxNoPulse);
    idx |= static_cast<uint8_t> ((m_readLatch & kLatchMsbMask) ? kIdxLatchMsb : 0);
    idx |= static_cast<uint8_t> (m_shiftLoadMode ? kIdxQ6 : 0);
    idx |= static_cast<uint8_t> (m_writeMode ? kIdxQ7 : 0);
    idx |= static_cast<uint8_t> (m_lssState << kIdxStateShift);

    command    = s_kSequencerRom16[idx];
    prevMsbSet = (m_readLatch & kLatchMsbMask) != 0;

    switch (command & kLssCommandMask)
    {
        case kLssCmdClr:
            m_readLatch = 0;
            break;
        case kLssCmdNop:
            break;
        case kLssCmdShiftZero:
            m_readLatch = static_cast<uint8_t> ((m_readLatch << 1) & 0xFF);
            break;
        case kLssCmdShiftRight:
            m_readLatch = static_cast<uint8_t> (m_readLatch >> 1);

            if (m_disk != nullptr && m_disk->IsWriteProtected())
            {
                m_readLatch |= kLatchMsbMask;
            }
            break;
        case kLssCmdLoad:
            m_readLatch = m_bus;
            break;
        case kLssCmdShiftOne:
            m_readLatch = static_cast<uint8_t> (((m_readLatch << 1) | 0x01) & 0xFF);
            break;
        default:
            break;
    }

    m_lssState = static_cast<uint8_t> ((command >> kLssStateShift) & kLssStateMask);

    // Rising edge of the latch MSB in read-data mode is the LSS "byte
    // ready" signal: a full nibble just assembled. Mark it fresh for
    // ConsumeFreshNibble and bump the lifetime read counter. Gated to
    // read mode so the SR write-protect-sense path (Q6 high) does not
    // spuriously count.
    if (!m_shiftLoadMode && !m_writeMode && !prevMsbSet && (m_readLatch & kLatchMsbMask) != 0)
    {
        m_latchIsFresh = true;
        m_readNibbles++;
    }

    if (m_lssClock == kLssReadClock)
    {
        if (m_writeMode && slot >= 0 && !m_disk->IsWriteProtected())
        {
            // The bit committed to the track is the write shift register's
            // serial output -- i.e. the data-register MSB (74LS323 QH) -- NOT
            // the sequencer state's high bit. On real hardware the two track
            // each other because the P6 sequencer and the shift register are
            // clocked in lockstep by the 2 MHz Q3; a cycle-stepped emulator
            // that catches the LSS up in bursts at each soft-switch access
            // cannot hold that sub-clock lockstep, so sampling (state & 0x8)
            // desyncs and deposits ~AA garbage where FF sync belongs (GH #89).
            // Sourcing the bit straight from the latch MSB is the physically
            // correct write-head signal and is robust to catch-up granularity.
            // Shifts (SL0/SL1) only ever land on sequencer states 2 and A,
            // never on the clock-4 write phase, so the latch is stable here.
            uint8_t  outBit = static_cast<uint8_t> ((m_readLatch & kLatchMsbMask) ? 1 : 0);

            m_disk->WriteBit (slot, m_bitPos, outBit);
        }

        trackBits = (slot >= 0) ? m_disk->GetTrackBitCount (slot) : kUnformattedTrackBits;

        if (m_disk == nullptr)
        {
            trackBits = 0;
        }

        if (trackBits > 0)
        {
            m_bitPos = (m_bitPos + 1) % trackBits;
        }
    }

    m_lssClock++;

    if (m_lssClock > kLssMaxClock)
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
    if (!m_latchIsFresh)
    {
        return false;
    }

    if ((m_readLatch & kLatchMsbMask) == 0)
    {
        return false;
    }

    outNibble      = m_readLatch;
    m_latchIsFresh = false;

    return true;
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
    uint8_t   outBit = 0;

    m_headWindow = static_cast<uint8_t> (((m_headWindow << 1) | (inBit & 1)) & 0x0F);

    if ((m_headWindow & 0x0F) != 0)
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
