#pragma once

#include "Pch.h"

class DiskImage;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIINibbleEngine
//
//  Bit-stream LSS-style engine. Owns the per-drive stream cursor and the
//  read/write data latches. The engine ticks through the current track
//  at the standard Disk II bit rate (~4 cycles per bit at 1.023 MHz).
//  Head position, motor state, drive selection, write protect, and Q6/Q7
//  latches are owned by DiskIIController and pushed in via setters.
//
////////////////////////////////////////////////////////////////////////////////

class DiskIINibbleEngine
{
public:
    static constexpr int   kCyclesPerBit = 4;
    static constexpr int   kMinTrack     = 0;
    static constexpr int   kMaxTrack     = 39;

    DiskIINibbleEngine ();

    void       SetDiskImage     (DiskImage * disk);
    void       SetMotorOn       (bool on);
    void       SetWriteMode     (bool q7);
    void       SetShiftLoadMode (bool q6);
    void       SetCurrentTrack  (int track);
    void       Reset            ();

    bool       IsMotorOn        () const { return m_motorOn; }
    bool       IsWriteMode      () const { return m_writeMode; }
    int        GetCurrentTrack  () const { return m_currentTrack; }
    size_t     GetBitPosition   () const { return m_bitPos; }

    void       Tick             (uint32_t cpuCycles);
    uint8_t    ReadLatch        ();
    void       WriteLatch       (uint8_t value);

private:
    void       AdvanceOneBit    ();
    void       ShiftReadBit     (uint8_t bit);
    void       ShiftWriteBit    ();

    DiskImage *  m_disk          = nullptr;
    int          m_currentTrack  = 0;
    bool         m_motorOn       = false;
    bool         m_writeMode     = false;
    bool         m_shiftLoadMode = false;
    size_t       m_bitPos        = 0;
    uint32_t     m_cycleAccum    = 0;
    uint8_t      m_readLatch     = 0;
    uint8_t      m_writeLatch    = 0;
};
