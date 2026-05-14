#include "Pch.h"

#include "DiskIINibbleEngine.h"
#include "DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIINibbleEngine
//
////////////////////////////////////////////////////////////////////////////////

DiskIINibbleEngine::DiskIINibbleEngine ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDiskImage
//
//  Called by DiskIIController on Mount / Eject / DriveSelect transitions.
//  Resets the bit cursor so the new image starts streaming from offset 0.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIINibbleEngine::SetDiskImage (DiskImage * disk)
{
    m_disk   = disk;
    m_bitPos = 0;
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

void DiskIINibbleEngine::SetMotorOn (bool on)
{
    m_motorOn    = on;
    m_cycleAccum = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteMode / SetShiftLoadMode
//
////////////////////////////////////////////////////////////////////////////////

void DiskIINibbleEngine::SetWriteMode (bool q7)
{
    m_writeMode = q7;
}


void DiskIINibbleEngine::SetShiftLoadMode (bool q6)
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

void DiskIINibbleEngine::SetCurrentTrack (int track)
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

void DiskIINibbleEngine::Reset ()
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
    m_readNibbles     = 0;
    m_writeNibbles    = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Advance the bit cursor by floor (cycles / 4). One bit per 4 CPU cycles
//  matches the real Disk II ~250 kbps data rate at 1.023 MHz.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIINibbleEngine::Tick (uint32_t cpuCycles)
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
        AdvanceOneBit ();
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

void DiskIINibbleEngine::AdvanceOneBit ()
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
        ShiftWriteBit ();
    }
    else
    {
        bit = m_disk->ReadBit (m_currentTrack, m_bitPos);
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

void DiskIINibbleEngine::ShiftReadBit (uint8_t bit)
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
            m_latchDelayBits = 2;
            m_workingShift   = 0;
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

void DiskIINibbleEngine::ShiftWriteBit ()
{
    uint8_t   outBit = 0;

    outBit       = static_cast<uint8_t> ((m_writeLatch >> 7) & 1);
    m_writeLatch = static_cast<uint8_t> (m_writeLatch << 1);

    if (m_disk != nullptr && !m_disk->IsWriteProtected ())
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

uint8_t DiskIINibbleEngine::ReadLatch ()
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

void DiskIINibbleEngine::WriteLatch (uint8_t value)
{
    m_writeLatch = value;
    m_writeNibbles++;
}
