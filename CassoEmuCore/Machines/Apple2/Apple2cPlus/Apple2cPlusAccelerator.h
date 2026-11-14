#pragma once

#include "Pch.h"

#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlusAccelerator
//
//  The //c Plus's 4 MHz accelerator, compatible with the Zip Chip. The rest
//  of the machine runs at 1 MHz: in fast mode an instruction moves
//  the machine's clock by its cycles times 1.0205 MHz over 4 MHz, so the
//  CPU runs four times as fast against video, disk and speaker timing.
//
//  Its registers, from $C05A:
//
//      $C05A   $5A written four times unlocks; $A5 locks; another value
//              written while unlocked selects normal speed
//      $C05B   $08 written while unlocked selects fast
//      $C05C   the slowdown mask, written while unlocked: bit 0 the speaker,
//              bits 1-7 slots 1-7
//      $C05E   bit 7 written set turns the temporary slowdowns off
//      $C05F   paddle and cache flags, written while unlocked: bit 6 slows
//              the machine on a paddle trigger
//
//  An access to a masked slot's $C0n0-$C0nF runs the machine at 1 MHz for
//  52 ms; a speaker toggle or a paddle trigger, for 5 ms. Reset starts the
//  machine fast and locked.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2cPlusAccelerator : public IIoAccessObserver
{
public:
    static constexpr uint32_t  kFastHz = 4'000'000;

    Apple2cPlusAccelerator (const uint64_t * busCycles, uint32_t busHz);

    // IIoAccessObserver
    bool  OnIoAccess (Word address, bool isWrite, Byte value, Byte & outValue) override;

    //  The machine cycles an instruction of `cpuCycles` takes.
    Byte  Scale (Byte cpuCycles);

    void  Reset ();

    bool  IsFast      () const { return m_fast; }
    bool  IsUnlocked  () const { return m_unlocked; }
    bool  IsSlowedNow () const;
    Byte  GetMask     () const { return m_mask; }

private:
    static constexpr Byte      kUnlockValue   = 0x5A;
    static constexpr Byte      kLockValue     = 0xA5;
    static constexpr Byte      kFastValue     = 0x08;
    static constexpr int       kUnlockWrites  = 4;
    static constexpr Byte      kPowerOnMask   = 0xE4;
    static constexpr uint32_t  kSlotDelayMs   = 52;
    static constexpr uint32_t  kShortDelayMs  = 5;

    void  WriteRegister (Word address, Byte value);
    bool  ReadRegister  (Word address, Byte & outValue) const;
    void  SlowFor       (uint32_t milliseconds);

    const uint64_t  * m_busCycles   = nullptr;
    uint32_t          m_busHz       = 0;
    uint64_t          m_slowUntil   = 0;
    uint64_t          m_remainder   = 0;
    int               m_unlockCount = 0;
    bool              m_unlocked    = false;
    bool              m_fast        = true;
    bool              m_delaysOff   = false;
    Byte              m_mask        = kPowerOnMask;
    Byte              m_gameIo      = 0;
};
