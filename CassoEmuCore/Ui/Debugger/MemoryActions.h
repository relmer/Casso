#pragma once

#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryActions
//
//  The actions that change memory, the stack and the registers: pokes and
//  patches, typed bytes and words, fills, moves, searches and compares, and
//  binary loads and saves.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryActions
{
public:
    static DebuggerAction  GetPoke             (Word address, Byte value, CommandMode mode);
    static DebuggerAction  GetSetRegister      (const std::string & name, Word value, CommandMode mode);
    static DebuggerAction  GetPatch            (Word address, std::span<const Byte> bytes, CommandMode mode, std::span<const Byte> required = {});

    //  BLOAD and BSAVE: a file loaded in a format (BinaryImageReader's word,
    //  or empty for the format the file's contents indicate), at an address
    //  and for a length when given; and a range saved as raw bytes. A length
    //  runs from the address, which is required with it, and ends within
    //  memory.
    static DebuggerAction  GetLoadBinary       (const std::string & path, const std::string & format, std::optional<Word> address,
                                                std::optional<uint32_t> length, CommandMode mode);
    static DebuggerAction  GetSaveBinary       (const std::string & path, Word address, uint32_t length, CommandMode mode);

    //  PUSH, POP and PPOP, the Stack pane's push and pops.
    static DebuggerAction  GetPushByte         (Byte value, CommandMode mode);
    static DebuggerAction  GetPopByte          (CommandMode mode);
    static DebuggerAction  GetPopWord          (CommandMode mode);

    //  A Memory pane's S over all memory, F, M and MC. `items` are S's, as
    //  typed, which the session parses as it runs.
    static DebuggerAction  GetSearchMemory  (const std::string & items, CommandMode mode);
    static DebuggerAction  GetFillMemory    (Word first, Word last, std::span<const Byte> bytes, CommandMode mode);
    static DebuggerAction  GetMoveMemory    (Word destination, Word first, Word last, CommandMode mode);
    static DebuggerAction  GetCompareMemory (Word other, Word first, Word last, CommandMode mode);
    static DebuggerAction  GetEnterWord     (Word address, const std::string & value, CommandMode mode);
    static DebuggerAction  GetEnterWord     (Word address, Word value, CommandMode mode);
    static DebuggerAction  GetEnterByte     (Word address, const std::string & value, CommandMode mode);
    static DebuggerAction  GetEnterByte     (Word address, Byte value, CommandMode mode);
    static DebuggerAction  GetSetRegister   (const std::string & name, const std::string & value, CommandMode mode);

private:
    static void  SetRangeFrom (DebugCommand & command, Word other, Word first, Word last);
};
