#pragma once

#include "Debugger/DataBlockTable.h"
#include "Debugger/IDebugCommandHandler.h"

class IDebugTarget;





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers
//
//  The data directives Z, DB, DB2, DB4, DB8, DW, DW2, DW4, ASC, DF and DA,
//  X to remove a range and B to list the blocks; U, which disassembles and
//  shows a data block's range as its directive; and A, which enters the
//  line assembler.
//
//  U with no range continues from where the last listing ended.
//
////////////////////////////////////////////////////////////////////////////////

class DataDirectiveHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    // Lines from first, until last or count lines, whichever comes first
    // when both are given. The end address of the listing is returned.
    static Word  Disassemble (DebugSession & session, Word first, std::optional<Word> last, int count, DisassemblyData & data);

private:
    // The symbol for a line's operand. In $C000-$C0FF the direction of the
    // access decides which of the address's two soft switches it is (FR-112).
    static void  ChooseOperandSymbol (DebugSession & session, DisassemblyLine & line);

    static constexpr int   kDefaultLines   = 20;
    static constexpr int   kMaxLines       = 0x10000;
    static constexpr int   kFloatBytes     = 5;
    static constexpr int   kWordBytes      = 2;
    static constexpr int   kBytesPerLine   = 8;
    static constexpr int   kWordsPerLine   = 4;
    static constexpr int   kTextPerLine    = 32;
    static constexpr Word  kIoFirst        = 0xC000;
    static constexpr Word  kIoLast         = 0xC0FF;
    static constexpr Byte  kHighBit        = 0x80;
    static constexpr Byte  kLowBits        = 0x7F;
    static constexpr Byte  kFirstPrint     = 0x20;
    static constexpr Byte  kDelete         = 0x7F;

    void         Define        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Remove        (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ListBlocks    (DebugSession & session, Reply & reply);
    void         List          (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  EnterAssembler (DebugSession & session, const DebugCommand & command, Reply & reply);

    static bool  TryGetShape   (const std::string & name, DataBlockKind & kind, int & perLine, int & itemBytes);
    static void  MakeDataLine  (IDebugTarget & target, const DataBlockEntry & block, Word address, DisassemblyLine & line);
    static std::string  FormatItems (const DataBlockEntry & block, std::span<const Byte> bytes);
    static double       DecodeFloat (std::span<const Byte> bytes);
    static Byte         Peek        (IDebugTarget & target, Word address);

    Word  m_nextList = 0;
};
