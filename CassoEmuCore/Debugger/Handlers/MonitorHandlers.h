#pragma once

#include "Debugger/IDebugCommandHandler.h"

class IDebugTarget;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorHandlers
//
//  The commands only Monitor mode has: examine and deposit, list, verify,
//  hex arithmetic, inverse and normal text, the input and output hooks, the
//  BASIC entries and the user vector, the register display and edit, and
//  host file read and write.
//
//  The rest of Monitor mode is already answered elsewhere, which is the
//  point of one engine behind two modes: `dest<start.endM` and
//  `value<start.endS` parse to the same commands `M` and `S` do in AppleWin
//  mode and reach MemoryHandlers; `!` and `F666G` reach the line assembler
//  through the same verb `A` uses; and step, trace and go are the session's
//  own.
//
//  EVERY EFFECT IS APPLIED DIRECTLY, never by jumping into the ROM (FR-018).
//  The Monitor's own routines are what a ][ ran, and a //e runs different
//  ones for the same command; doing the documented thing to the documented
//  address is the same on every machine and works on a machine whose ROM has
//  no such command at all.
//
////////////////////////////////////////////////////////////////////////////////

class MonitorHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    //  Rows broken on eight-byte boundaries, so `303.30F` is five bytes
    //  against $0303 and then eight against $0308. The Monitor labels a row
    //  with the address that starts it rather than with wherever the reader
    //  began, which is why this differs from the AppleWin dump.
    static MemoryData  MakeRows (IDebugTarget & target, Word first, Word last);

private:
    //  The zero page the Monitor keeps its display and hook state in.
    static constexpr Word  kInverseFlag   = 0x32;   // INVFLG
    static constexpr Word  kOutputHook    = 0x36;   // CSWL / CSWH
    static constexpr Word  kInputHook     = 0x38;   // KSWL / KSWH
    static constexpr Word  kRegisterSave  = 0x45;   // A, X, Y, P, S at $45-$49

    static constexpr Byte  kInverseValue  = 0x3F;
    static constexpr Byte  kNormalValue   = 0xFF;

    //  What slot 0 restores the hooks to: the ROM's own keyboard and screen.
    static constexpr Word  kKeyIn         = 0xFD1B;
    static constexpr Word  kCharacterOut  = 0xFDF0;

    static constexpr Word  kBasicCold     = 0xE000;
    static constexpr Word  kBasicWarm     = 0xE003;
    static constexpr Word  kUserVector    = 0x03F8;

    static constexpr Word  kSlotBase      = 0xC000;
    static constexpr Word  kSlotStride    = 0x0100;
    static constexpr int   kLastSlot      = 7;
    static constexpr int   kListLines     = 20;
    static constexpr int   kBytesPerRow   = 8;
    static constexpr Byte  kLowByte       = 0xFF;

    static void  Examine        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Deposit        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  List           (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Verify         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Arithmetic     (const DebugCommand & command, Reply & reply);
    static void  SetTextMode    (DebugSession & session, bool isInverse, Reply & reply);
    static void  SetHook        (DebugSession & session, const DebugCommand & command, bool isInput, Reply & reply);
    static void  ShowRegisters  (DebugSession & session, Reply & reply);
    static void  EditRegisters  (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ReadFile       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  WriteFile      (DebugSession & session, const DebugCommand & command, Reply & reply);

    //  Sets the program counter and starts a run, which is what the
    //  Monitor's BASIC entries and user vector do.
    static void  RunAt          (DebugSession & session, Word address, Reply & reply);

    static bool  TryGetFiles    (DebugSession & session, Reply & reply, IFileSystem *& files);
    static bool  TryPokeRange   (DebugSession & session, Word first, std::span<const Byte> bytes, Reply & reply);
    static Word  GetLast        (const DebugCommand & command);
    static Byte  Peek           (IDebugTarget & target, Word address);
    static void  PokeWord       (IDebugTarget & target, Word address, Word value);
};
