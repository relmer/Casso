#include "Pch.h"

#include "Machines/Apple2/Apple2cPlus/Apple2cPlusAccelerator.h"




static constexpr Word      s_kSpeakerStart  = 0xC030;
static constexpr Word      s_kSpeakerEnd    = 0xC03F;
static constexpr Word      s_kRegisterStart = 0xC058;
static constexpr Word      s_kRegisterEnd   = 0xC05F;
static constexpr Word      s_kPaddleStart   = 0xC070;
static constexpr Word      s_kPaddleEnd     = 0xC07F;
static constexpr Word      s_kSlotIoStart   = 0xC080;
static constexpr Byte      s_kPaddleDelay   = 0x40;
static constexpr Byte      s_kDelaysOffBit  = 0x80;
static constexpr Byte      s_kCacheOffBit   = 0x80;
static constexpr Byte      s_kCacheSize     = 0x03;
static constexpr Byte      s_kSlowedBit     = 0x08;
static constexpr Byte      s_kOffBit        = 0x10;
static constexpr Byte      s_kCacheOffFlag  = 0x20;
static constexpr Byte      s_kTickBit       = 0x80;
static constexpr uint64_t  s_kTickShift     = 9;
static constexpr uint64_t  s_kMsPerSecond   = 1000;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlusAccelerator
//
////////////////////////////////////////////////////////////////////////////////

Apple2cPlusAccelerator::Apple2cPlusAccelerator (const uint64_t * busCycles, uint32_t busHz) :
    m_busCycles (busCycles),
    m_busHz     (busHz)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnIoAccess
//
//  Watches the slowdown triggers and takes the register accesses: writes to
//  $C05A-$C05F while unlocked or while unlocking, and reads of them while
//  unlocked.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusAccelerator::OnIoAccess (Word address, bool isWrite, Byte value, Byte & outValue)
{
    constexpr Word  kSlotShift = 4;
    constexpr Word  kSlotMask  = 0x07;
    bool            isRegister = address >= s_kRegisterStart && address <= s_kRegisterEnd;
    bool            taken      = false;
    int             slot       = 0;



    if (address >= s_kSpeakerStart && address <= s_kSpeakerEnd && (m_mask & 1) != 0)
    {
        SlowFor (kShortDelayMs);
    }
    else if (address >= s_kPaddleStart && address <= s_kPaddleEnd && (m_gameIo & s_kPaddleDelay) != 0)
    {
        SlowFor (kShortDelayMs);
    }
    else if (address >= s_kSlotIoStart)
    {
        slot = (address >> kSlotShift) & kSlotMask;

        if (slot != 0 && (m_mask & (1 << slot)) != 0)
        {
            SlowFor (kSlotDelayMs);
        }
    }

    if (isWrite)
    {
        taken = isRegister && (m_unlocked || (address == s_kRegisterStart + 2 && value == kUnlockValue));

        if (isRegister)
        {
            WriteRegister (address, value);
        }
        else
        {
            m_unlockCount = 0;
        }
    }
    else if (isRegister && m_unlocked)
    {
        taken = ReadRegister (address, outValue);
    }

    return taken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Scale
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple2cPlusAccelerator::Scale (Byte cpuCycles)
{
    uint64_t  machine = 0;



    if (!m_fast || IsSlowedNow())
    {
        return cpuCycles;
    }

    m_remainder += (uint64_t) cpuCycles * m_busHz;
    machine      = m_remainder / kFastHz;
    m_remainder %= kFastHz;

    return (Byte) machine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusAccelerator::Reset()
{
    m_fast        = true;
    m_unlocked    = false;
    m_unlockCount = 0;
    m_slowUntil   = 0;
    m_remainder   = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSlowedNow
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusAccelerator::IsSlowedNow() const
{
    return m_busCycles != nullptr && *m_busCycles < m_slowUntil;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteRegister
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusAccelerator::WriteRegister (Word address, Byte value)
{
    constexpr Word  kLockOffset  = 2;
    constexpr Word  kFastOffset  = 3;
    constexpr Word  kMaskOffset  = 4;
    constexpr Word  kDelayOffset = 6;
    constexpr Word  kGameOffset  = 7;
    Word            offset       = (Word) (address - s_kRegisterStart);



    if (offset == kLockOffset && value == kUnlockValue)
    {
        m_unlockCount++;
        m_unlocked = m_unlocked || m_unlockCount >= kUnlockWrites;
        return;
    }

    m_unlockCount = 0;

    if (offset == kDelayOffset)
    {
        m_delaysOff = (value & s_kDelaysOffBit) != 0;
        m_slowUntil = m_delaysOff ? 0 : m_slowUntil;
        return;
    }

    if (!m_unlocked)
    {
        return;
    }

    if (offset == kLockOffset)
    {
        m_unlocked = value != kLockValue;
        m_fast     = (value == kLockValue) ? m_fast : false;
    }
    else if (offset == kFastOffset && value == kFastValue)
    {
        m_fast = true;
    }
    else if (offset == kMaskOffset)
    {
        m_mask = value;
    }
    else if (offset == kGameOffset)
    {
        m_gameIo = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
//  The values the accelerator returns while unlocked. $C05B is its status:
//  the cache size, the slowdown and off flags, and a bit that changes every
//  512 cycles.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple2cPlusAccelerator::ReadRegister (Word address, Byte & outValue) const
{
    constexpr Word  kStatusOffset = 3;
    constexpr Word  kMaskOffset   = 4;
    constexpr Word  kMmuOffset    = 6;
    constexpr Byte  kC058Value    = 0xC0;
    constexpr Byte  kC059Value    = 0x20;
    Word            offset        = (Word) (address - s_kRegisterStart);
    uint64_t        now           = (m_busCycles != nullptr) ? *m_busCycles : 0;



    switch (offset)
    {
        case 0:             outValue = kC058Value;  break;
        case 1:             outValue = kC059Value;  break;
        case kMaskOffset:   outValue = m_mask;      break;

        case kStatusOffset:
            outValue = (Byte) (s_kCacheSize                                   |
                               (IsSlowedNow() ? s_kSlowedBit : 0)             |
                               (m_fast ? 0 : s_kOffBit)                       |
                               ((m_gameIo & s_kCacheOffBit) != 0 ? s_kCacheOffFlag : 0) |
                               (((now >> s_kTickShift) & 1) != 0 ? s_kTickBit : 0));
            break;

        case kMmuOffset:
            return false;

        default:
            outValue = 0;
            break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SlowFor
//
////////////////////////////////////////////////////////////////////////////////

void Apple2cPlusAccelerator::SlowFor (uint32_t milliseconds)
{
    uint64_t  now   = (m_busCycles != nullptr) ? *m_busCycles : 0;
    uint64_t  until = now + (uint64_t) m_busHz * milliseconds / s_kMsPerSecond;



    if (m_fast && !m_delaysOff)
    {
        m_slowUntil = std::max (m_slowUntil, until);
    }
}
