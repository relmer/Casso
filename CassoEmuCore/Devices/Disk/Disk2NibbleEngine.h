#pragma once

#include "Pch.h"

class DiskImage;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleEngine
//
//  Bit-stream LSS-style engine. Owns the per-drive stream cursor and the
//  read/write data latches. The engine ticks through the current track
//  at the standard Disk II bit rate (~4 cycles per bit at 1.023 MHz).
//  Head position, motor state, drive selection, write protect, and Q6/Q7
//  latches are owned by Disk2Controller and pushed in via setters.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2NibbleEngine
{
public:
    static constexpr int   kCyclesPerBit = 4;
    static constexpr int   kMinTrack     = 0;
    static constexpr int   kMaxTrack     = 39;

    Disk2NibbleEngine();

    void       SetDiskImage (DiskImage * disk);
    void       SetMotorOn (bool on);
    void       SetWriteMode (bool q7);
    void       SetShiftLoadMode (bool q6);
    void       SetCurrentTrack (int track);
    void       Reset();

    bool       IsMotorOn() const { return m_motorOn; }
    bool       IsWriteMode() const { return m_writeMode; }
    int        GetCurrentTrack() const { return m_currentTrack; }
    size_t     GetBitPosition() const { return m_bitPos; }

    // Lifetime nibble I/O counters surfaced to the UI status-bar
    // tooltip. Increment on each successful CPU latch read (MSB-set)
    // and each CPU latch write while in write mode.
    uint64_t   GetReadNibbles() const { return m_readNibbles; }
    uint64_t   GetWriteNibbles() const { return m_writeNibbles; }

    // Diagnostic / test peek at the current read latch contents without
    // the read-clears-on-MSB side effect ReadLatch carries.
    uint8_t    PeekReadLatch() const { return m_readLatch; }

    void       Tick (uint32_t cpuCycles);
    uint8_t    ReadLatch();
    void       WriteLatch (uint8_t value);

    // Passive-watcher hook: returns true exactly once per fully-
    // assembled MSB-set nibble. ReadLatch is a pure sample with no
    // consume side effect (the CPU polls $C0EC in a tight BPL loop
    // and sees the same byte many times); feeding every sampled
    // ReadLatch return into the Disk2AddressMarkWatcher fills the
    // watcher with garbage repeats and partial-assembly bytes, and
    // its state machines never match a real D5 AA 96 prologue.
    // ConsumeFreshNibble is the side-channel the controller uses
    // to feed the watcher exactly one nibble per LSS assembly
    // rising edge -- byte-identical to the real "byte ready"
    // signal. Does NOT affect ReadLatch's CPU-visible value.
    bool       ConsumeFreshNibble (uint8_t & outNibble);

private:
    void       AdvanceOneBit();
    void       ShiftReadBit (uint8_t bit);
    void       ShiftWriteBit();
    uint8_t    ApplyHeadWindow (uint8_t inBit);
    uint8_t    NextWeakBit();

    DiskImage *  m_disk          = nullptr;
    int          m_currentTrack  = 0;
    bool         m_motorOn       = false;
    bool         m_writeMode     = false;
    bool         m_shiftLoadMode = false;
    size_t       m_bitPos        = 0;
    uint32_t     m_cycleAccum    = 0;
    uint8_t      m_readLatch       = 0;
    uint8_t      m_workingShift    = 0;
    int          m_latchDelayBits  = 0;
    uint8_t      m_writeLatch      = 0;
    bool         m_latchIsFresh    = false;

    // MC3470 read-amplifier model (mirrors AppleWin's FloppyDrive::
    // m_headWindow). Sliding 4-bit window of the most-recent bit cells
    // read off the surface. When all four are zero (unformatted region
    // or intentional weak-bit gap), the amplifier "floats" and the
    // output bit is randomized (~30% chance of a 1). Otherwise the
    // output is the bit read on the previous call -- a one-bit pipeline
    // delay through the head. Both behaviors are required for fidelity
    // with WOZ protection schemes that key off unformatted-region read
    // variability (Karateka RWTS18, Lode Runner, etc.).
    uint8_t      m_headWindow     = 0;
    uint32_t     m_weakRngState   = 0xDEADBEEFu;

    // Lifetime nibble I/O counters (increment on CPU read of $C0EC
    // when MSB-set, and on CPU write of $C0ED in write mode). Surfaced
    // to the status-bar tooltip via the controller.
    uint64_t     m_readNibbles   = 0;
    uint64_t     m_writeNibbles  = 0;
};
