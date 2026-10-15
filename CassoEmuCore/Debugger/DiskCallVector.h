#pragma once

#include "Pch.h"

#include "Debugger/Reply.h"

class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector
//
//  RWTS and driver breakpoints follow their vectors: RWTS's routine is the
//  target of the JMP at $03D9, and ProDOS's Disk II driver the address its
//  device vector has for the slot and drive, $BF10 plus twice the slot for
//  drive 1 and $BF20 plus twice the slot for drive 2, once ProDOS's global
//  page begins with its JMP. Everything is read by side-effect-free peeks.
//
//  A call matches when its request equals the breakpoint's spec: the
//  IOB at Y (low) and A (high) for RWTS, with the drive at +2, the track at
//  +4, the sector at +5 and the command at +$0C; $42 to $47 for the driver,
//  the command at $42, the unit at $43, whose bit 7 is drive 2, and the
//  block at $46-$47.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCallVector
{
public:
    static constexpr Word  kRwtsVector   = 0x03D9;
    static constexpr Word  kProDosGlobal = 0xBF00;
    static constexpr Word  kDriveOneBase = 0xBF10;
    static constexpr Word  kDriveTwoBase = 0xBF20;
    static constexpr Byte  kJmp          = 0x4C;

    static Word                 GetVectorAddress (const DiskBreakSpec & spec, int slot);
    static std::optional<Word>  TryResolve       (const DiskBreakSpec & spec, int slot, const IDebugExpressionContext & memory);
    static void                 AddVectorBytes   (const DiskBreakSpec & spec, int slot, std::vector<Word> & bytes);
    static bool                 IsRequestMatch   (const DiskBreakSpec & spec, const IDebugExpressionContext & context);
    static DiskCallRequest      ReadRequest      (const DiskBreakSpec & spec, const IDebugExpressionContext & context);

private:
    static Byte  Peek   (const IDebugExpressionContext & memory, Word address);
    static Word  Peek16 (const IDebugExpressionContext & memory, Word address);
    static Word  GetIob (const IDebugExpressionContext & context);
};
