#pragma once

#include "Pch.h"

class DiskImage;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleEngine
//
//  Faithful port of the Disk II Logic State Sequencer (LSS) -- the P6
//  state machine inside the controller that turns the raw magnetic flux
//  stream into the CPU-visible data register. Owns the per-drive stream
//  cursor, the 8-bit data latch, and the LSS state/clock. Head position,
//  motor state, drive selection, write protect, and Q6/Q7 latches are
//  owned by Disk2Controller and pushed in via setters.
//
//  The sequencer runs at 2 MHz (two LSS clocks per CPU cycle). Eight LSS
//  clocks make one bit cell, so the head advances one bit every four CPU
//  cycles -- the standard ~250 kbps Disk II data rate at 1.023 MHz. The
//  read pulse is sampled once per bit cell, at LSS clock 4.
//
//  References:
//    - "Understanding the Apple IIe" (Sather), Fig 9.11 (DOS 3.3 / 16-
//      sector P6 Logic State Sequencer) and Table 9.3 (LSS commands).
//    - WOZ disk image spec, incl. "Freaking Out Like a MC3470":
//        https://applesauce.codes/woz/
//    - Reference LSS stepping loop and P6 sequencer ROM adapted from
//      apple2js (MIT, (c) Will Scullin):
//        https://github.com/whscullin/apple2js
//        js/cards/disk2.ts (SEQUENCER_ROM_16)
//        js/cards/drivers/WozDiskDriver.ts (moveHead)
//      The sequencer ROM itself is factual hardware data (the physical
//      contents of the P6 PROM); the surrounding code is an independent
//      C++ implementation.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2NibbleEngine
{
public:
    static constexpr int   kCyclesPerBit = 4;
    static constexpr int   kMinTrack     = 0;
    static constexpr int   kMaxTrack     = 159;

    // Nominal bit length of an unformatted quarter-track. When the head
    // sits over a position the image holds no data for, the disk still
    // spins: the bit cursor advances over this many cells of "no flux"
    // (rawBit 0), which the head-window weak-bit model turns into the
    // ~30% random stream a real drive reads off blank surface.
    static constexpr size_t kUnformattedTrackBits = 51200;

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
    // tooltip. The read counter increments once per assembled nibble
    // (LSS "byte ready" rising edge); the write counter increments on
    // each CPU latch write while in write mode.
    uint64_t   GetReadNibbles() const { return m_readNibbles; }
    uint64_t   GetWriteNibbles() const { return m_writeNibbles; }

    // Diagnostic / test peek at the current data latch contents. ReadLatch
    // is itself a pure sample (no side effect), so this is identical to
    // ReadLatch but const and counter-free.
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
    void       StepLss();
    uint8_t    ApplyHeadWindow (uint8_t inBit);
    uint8_t    NextWeakBit();
    size_t     CurrentTrackBits() const;

    DiskImage *  m_disk          = nullptr;
    int          m_currentTrack  = 0;
    bool         m_motorOn       = false;
    bool         m_writeMode     = false;
    bool         m_shiftLoadMode = false;
    size_t       m_bitPos        = 0;

    // Logic State Sequencer registers. m_lssState is the 4-bit P6
    // sequencer state (high nibble of the ROM command); m_lssClock is
    // the 0..7 bit-cell clock; m_readLatch is the 8-bit data register
    // the CPU reads at $C0EC; m_bus is the value last written by the
    // CPU, loaded into the latch by the LSS LOAD command during writes.
    // m_lssState starts at 2 (UtA2e p.9-29) so the sequencer produces
    // correctly synced nibbles immediately rather than emitting a
    // spurious leading 1.
    uint8_t      m_lssState      = 2;
    int          m_lssClock      = 0;
    uint8_t      m_readLatch     = 0;
    uint8_t      m_bus           = 0;
    bool         m_latchIsFresh  = false;

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
