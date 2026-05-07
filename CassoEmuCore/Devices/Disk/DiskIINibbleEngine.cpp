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
        m_currentTrack = clamped;
        m_bitPos       = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void DiskIINibbleEngine::Reset ()
{
    m_motorOn       = false;
    m_writeMode     = false;
    m_shiftLoadMode = false;
    m_bitPos        = 0;
    m_cycleAccum    = 0;
    m_readLatch     = 0;
    m_writeLatch    = 0;
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
    if (m_readLatch & 0x80)
    {
        return;
    }

    m_readLatch = static_cast<uint8_t> ((m_readLatch << 1) | (bit & 1));
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
    uint8_t   value = m_readLatch;

    if (value & 0x80)
    {
        m_readLatch = 0;
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
}
