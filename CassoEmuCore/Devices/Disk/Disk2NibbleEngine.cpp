#include "Pch.h"

#include "Disk2NibbleEngine.h"
#include "DiskImage.h"





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
//  same position (real Disk II behavior — the disk keeps spinning for ~1s
//  after motor-off but Phase 9 models the simpler "freeze" semantics).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetMotorOn (bool on)
{
    m_motorOn    = on;
    m_cycleAccum = 0;
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
//  Clamps to [0, kMaxTrack]. Track is full-track index (controller maps
//  half-tracks → full tracks). Switching tracks resets the bit cursor.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::SetCurrentTrack (int track)
{
    int   clamped = track;

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
        size_t  newBits = (m_disk != nullptr)
                          ? m_disk->GetTrackBitCount (clamped)
                          : 0;

        m_currentTrack = clamped;

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
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::Reset()
{
    m_motorOn         = false;
    m_writeMode       = false;
    m_shiftLoadMode   = false;
    m_bitPos          = 0;
    m_cycleAccum      = 0;
    m_readLatch       = 0;
    m_workingShift    = 0;
    m_latchDelayBits  = 0;
    m_writeLatch      = 0;
    m_latchIsFresh    = false;
    m_readNibbles     = 0;
    m_writeNibbles    = 0;
    m_headWindow      = 0;
    m_weakRngState    = 0xDEADBEEFu;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Advance the bit cursor by floor (cycles / 4). One bit per 4 CPU cycles
//  matches the real Disk II ~250 kbps data rate at 1.023 MHz.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::Tick (uint32_t cpuCycles)
{
    uint32_t   bitsToAdvance = 0;
    uint32_t   i             = 0;

    if (!m_motorOn)
    {
        return;
    }

    m_cycleAccum  += cpuCycles;
    bitsToAdvance  = m_cycleAccum / kCyclesPerBit;
    m_cycleAccum  %= kCyclesPerBit;

    for (i = 0; i < bitsToAdvance; i++)
    {
        AdvanceOneBit();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceOneBit
//
//  Per-bit clock. In read mode: shift the read latch left and OR in the
//  next bit from the track stream; if the latch high bit becomes 1 the
//  caller will harvest it via ReadLatch. In write mode: stream the write
//  latch's MSB out to the track and shift the latch left.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::AdvanceOneBit()
{
    uint8_t   bit       = 0;
    size_t    trackBits = 0;

    if (m_disk == nullptr)
    {
        return;
    }

    trackBits = m_disk->GetTrackBitCount (m_currentTrack);

    if (trackBits == 0)
    {
        return;
    }

    if (m_writeMode)
    {
        ShiftWriteBit();
    }
    else
    {
        bit = m_disk->ReadBit (m_currentTrack, m_bitPos);
        bit = ApplyHeadWindow (bit);
        ShiftReadBit (bit);
    }

    m_bitPos = (m_bitPos + 1) % trackBits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShiftReadBit
//
//  Standard Disk II LSS read: shift left, OR in bit. When the latch
//  hits an MSB-set state, it stays "full" until the CPU consumes it
//  (a subsequent ReadLatch return) and then continues shifting.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::ShiftReadBit (uint8_t bit)
{
    // Port of the canonical Disk II Logic State Sequencer read model.
    //
    // The Disk II's LSS shifts every bit-cell. When the working
    // shift register's MSB becomes 1 (a complete nibble assembled),
    // the visible latch is updated and the shift register clears so
    // the next nibble can start assembling. A latch-delay (~7 µs ≈
    // 2 bit-cells) holds the visible latch stable for the CPU to
    // read before the latch resumes tracking the (now-resetting)
    // shift register.
    m_workingShift = static_cast<uint8_t> ((m_workingShift << 1) | (bit & 1));

    if (m_latchDelayBits > 0)
    {
        m_latchDelayBits--;

        if (m_workingShift == 0)
        {
            // No leading 1-bit yet for the next nibble (sync-gap
            // zero bits). Extend the delay so the CPU keeps seeing
            // the just-completed nibble until something interesting
            // arrives.
            m_latchDelayBits++;
        }
    }

    // SEPARATE check (not else): when the delay reaches zero in this
    // same call, we update the latch immediately rather than waitingfor the next
    // bit-cell -- which would have lost the data.
    if (m_latchDelayBits == 0)
    {
        m_readLatch = m_workingShift;

        if ((m_workingShift & 0x80) != 0)
        {
            // Rising edge of the LSS "byte ready" signal: a full
            // nibble just assembled and latched. Mark it fresh so
            // ConsumeFreshNibble (the passive-watcher side channel)
            // hands this exact byte to the address-mark state
            // machine exactly once, instead of seeing every CPU
            // poll's repeated sample.
            m_latchDelayBits = 2;
            m_workingShift   = 0;
            m_latchIsFresh   = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShiftWriteBit
//
//  Streams the MSB of the write latch onto the disk and shifts the latch
//  left. The disk's WriteBit honors the image's write-protect flag.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::ShiftWriteBit()
{
    uint8_t   outBit = 0;

    outBit       = static_cast<uint8_t> ((m_writeLatch >> 7) & 1);
    m_writeLatch = static_cast<uint8_t> (m_writeLatch << 1);

    if (m_disk != nullptr && !m_disk->IsWriteProtected())
    {
        m_disk->WriteBit (m_currentTrack, m_bitPos, outBit);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadLatch
//
//  Returns the current latch value. If the latch is "full" (MSB set) we
//  reset it after reporting so subsequent bits start a fresh nibble.
//
////////////////////////////////////////////////////////////////////////////////

uint8_t Disk2NibbleEngine::ReadLatch()
{
    // Real P5A behavior: reading $C0EC is a pure sample of the shift
    // register's current state. There is NO side effect on the read --
    // the shift register keeps shifting bits in regardless of whether
    // the CPU read it. The "byte ready" signal is just the MSB.
    //
    // The CPU is responsible for spinning a tight LDA/BPL loop until
    // it catches the latch with MSB-set, which it then knows is a
    // complete nibble. The next read after that will catch the latch
    // about 32 µs later (8 bit-cells) when the next nibble has
    // assembled.
    //
    // Earlier model "clear on MSB-set read" was wrong: it broke the
    // boot ROM's address-prolog scan because each read cleared the
    // latch back to 0, and the CPU's tight loop ran faster than the
    // engine could produce complete nibbles, so the CPU never saw
    // MSB-set.
    uint8_t   value = m_readLatch;

    if (value & 0x80)
    {
        m_readNibbles++;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLatch
//
////////////////////////////////////////////////////////////////////////////////

void Disk2NibbleEngine::WriteLatch (uint8_t value)
{
    m_writeLatch = value;
    m_writeNibbles++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsumeFreshNibble
//
//  Passive-watcher side channel: returns true exactly once per
//  LSS "byte ready" rising edge. The controller calls this AFTER
//  ReadLatch so the watcher's address-mark / data-mark state
//  machines see exactly one nibble per assembly cycle instead of
//  the CPU-visible repeat stream. Does NOT touch m_readLatch, so
//  the CPU-visible byte returned by ReadLatch is unchanged.
//
//  Returns false unless both the latch is fresh AND its MSB is set.
//  The MSB guard handles the intermediate partial-assembly latch
//  updates that ShiftReadBit can produce between two "byte ready"
//  events (the latch is overwritten with sub-nibble values when
//  the latch-delay has expired but the working shift register has
//  not yet hit MSB).
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2NibbleEngine::ConsumeFreshNibble (uint8_t & outNibble)
{
    if (!m_latchIsFresh)
    {
        return false;
    }

    if ((m_readLatch & 0x80) == 0)
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
//  MC3470 read-amplifier model, ported from AppleWin's
//  DataLatchReadWOZ (Disk.cpp). Maintains a sliding 4-bit window of
//  the most-recent bits read off the surface. Two effects:
//
//    1. One-bit pipeline delay. When the window has at least one
//       1-bit, the amplifier outputs the bit read on the PREVIOUS
//       call (window bit 1, not the just-shifted-in bit 0). This is
//       hardware behavior -- the amp needs a cell of integration
//       time -- and is what AppleWin reproduces.
//
//    2. Weak bits / floating output. When all four window bits are
//       zero (an unformatted region or intentional weak-bit gap),
//       the amplifier has no signal to lock to and floats. AppleWin
//       models this as a ~30% chance of a 1-bit per cell. WOZ-2.0
//       protection schemes (Karateka RWTS18, Lode Runner, etc.) key
//       off this randomness to detect copies, which trim the floating
//       region to a deterministic value during duplication.
//
//  RNG is a per-engine LCG (not the global rand() AppleWin uses) so
//  that tests remain deterministic per engine instance.
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

