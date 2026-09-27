#include "Pch.h"

#include "Core/DramPowerOnPattern.h"
#include "Core/Prng.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Fill
//
//  A DRAM cell is a capacitor, and after power has been off a while every one
//  of them has leaked flat. That reads back as 0 or 1 depending on which side
//  of its sense amplifier the cell sits on, which the chip's layout fixes, so
//  power-on RAM is a pattern rather than noise. Through the Apple II's address
//  wiring the pattern commonly observed on the ][+ and //e is FF FF 00 00,
//  repeating.
//
//  Real machines are not perfectly regular: a //e observed at power-on showed
//  scattered odd bytes. Four bytes in every 512 ($xx28, $xx29, $xx68, $xx69)
//  stand in for them, drawn from the seeded Prng, so a run still differs from
//  the next and --seed still replays one exactly.
//
//  RAM was filled with random bytes before this, and it was wrong in a way
//  that showed:
//  one power-on in 256 left the autostart ROM's power-up byte looking valid,
//  and the ROM took a warm reset into garbage.
//
////////////////////////////////////////////////////////////////////////////////

void DramPowerOnPattern::Fill (Byte * dst, size_t size, Prng & prng)
{
    size_t  offset = 0;



    for (offset = 0; offset < size; offset++)
    {
        dst[offset] = IsHole (offset) ? prng.NextByte() : GetPatternByte (offset);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPatternByte
//
////////////////////////////////////////////////////////////////////////////////

Byte DramPowerOnPattern::GetPatternByte (size_t offset)
{
    constexpr size_t  kHighPairBit = 0x02;
    constexpr Byte    kSetByte     = 0xFF;
    constexpr Byte    kClearByte   = 0x00;



    return (offset & kHighPairBit) == 0 ? kSetByte : kClearByte;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHole
//
////////////////////////////////////////////////////////////////////////////////

bool DramPowerOnPattern::IsHole (size_t offset)
{
    constexpr size_t  kFirstHole  = 0x28;
    constexpr size_t  kSecondHole = 0x68;



    size_t  inBlock = offset % kHoleStride;



    return inBlock == kFirstHole  || inBlock == kFirstHole  + 1
        || inBlock == kSecondHole || inBlock == kSecondHole + 1;
}
