#include "Pch.h"

#include "Debugger/Handlers/DataDirectiveHandlers.h"

#include "Debugger/DebugSession.h"
#include "Debugger/EffectiveAddress.h"
#include "Debugger/SymbolDescriptions.h"
#include "Disassembler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool DataDirectiveHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::DefineBytes:
    case DebugVerb::DefineWords:
    case DebugVerb::DefineAddress:
    case DebugVerb::DefineText:
    case DebugVerb::DefineFloat:    Define         (session, command, reply); return true;
    case DebugVerb::RemoveData:     Remove         (session, command, reply); return true;
    case DebugVerb::ListData:       ListBlocks     (session, reply);          return true;
    case DebugVerb::Disassemble:    List           (session, command, reply); return true;
    case DebugVerb::EnterAssembler: EnterAssembler (session, command, reply); return true;
    default:                                                                  return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::Disassemble
//
//  Each address is an instruction, or, inside a data block, one line of
//  that block's items: a block marked by a data directive, or one the debug
//  file maps to a data directive. An instruction running over pc is its
//  bytes before pc instead, the way the line list cuts it; so is one
//  running over a label or the target of an executed branch, unless it
//  starts at pc, so a symbol's address and a branch's target each have a
//  row.
//
////////////////////////////////////////////////////////////////////////////////

Word DataDirectiveHandlers::Disassemble (DebugSession & session, Word first, std::optional<Word> last, int count, DisassemblyData & data, std::optional<Word> pc)
{
    IDebugTarget  & target      = session.GetTarget();
    Disassembler    disassembler (target.GetInstructionSet());
    SymbolTableId   symbolTable = SymbolTableId::Main;
    uint32_t        address     = first;
    uint32_t        cutAt       = 0;
    HRESULT         hr          = S_OK;



    while ((int) data.lines.size() < count && (!last.has_value() || address <= *last) && address <= 0xFFFF)
    {
        DisassemblyLine  line;
        DataBlockEntry   block;
        Byte             bytes[Disassembler::kMaxInstructionBytes] = {};



        if (session.TryFindData ((Word) address, block, line.dataOrigin))
        {
            MakeDataLine (session, block, (Word) address, line);
        }
        else
        {
            for (size_t i = 0; i < std::size (bytes); ++i)
            {
                bytes[i] = Peek (target, (Word) (address + i));
            }

            hr = disassembler.DisassembleOne ((Word) address, bytes, line.instruction);
            IGNORE_RETURN_VALUE (hr, S_OK);

            cutAt = 0;

            for (uint32_t inside = 1; inside < line.instruction.bytes.size() && cutAt == 0 && address + inside <= 0xFFFF; inside++)
            {
                bool  isPc    = pc.has_value() && *pc == address + inside;
                bool  isLabel = !(pc.has_value() && *pc == address) &&
                                (IsCodeLabel (session, (Word) (address + inside)) || session.IsBranchTarget ((Word) (address + inside)));

                cutAt = (isPc || isLabel) ? address + inside : 0;
            }

            if (cutAt != 0)
            {
                DataBlockEntry  cut;



                cut.first = (Word) address;
                cut.last  = (Word) (cutAt - 1);
                line      = DisassemblyLine();
                MakeDataLine (session, cut, (Word) address, line);
            }

            session.GetSymbols().TryFindName ((Word) address, line.label, symbolTable);

            if (line.instruction.hasOperandAddress)
            {
                ChooseOperandSymbol (session, line);
            }
        }

        address += (uint32_t) line.instruction.bytes.size();
        data.lines.push_back (line);
    }

    return (Word) address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::IsCodeLabel
//
//  Whether an address has a symbol that could label code: one in a table
//  that is on, not a constant, and outside the I/O page.
//
////////////////////////////////////////////////////////////////////////////////

bool DataDirectiveHandlers::IsCodeLabel (const DebugSession & session, Word address)
{
    SymbolTableId  table = SymbolTableId::Main;
    std::string    name;
    SymbolInfo     symbol;



    if ((address >= kIoFirst && address <= kIoLast) || !session.GetSymbols().TryFindName (address, name, table))
    {
        return false;
    }

    return !session.GetSymbols().TryFindSymbolIn (table, name, symbol) || !symbol.isConstant;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::ChooseOperandSymbol
//
//  ONE SOFT-SWITCH ADDRESS IS TWO SWITCHES. Reading $C000 gives the keyboard;
//  writing it turns 80STORE off, and the tables hold a symbol for each.
//  Taking whichever came first showed `STA KBD` -- a store to the keyboard,
//  which the machine cannot do -- so the opcode's own direction picks between
//  them.
//
//  Everywhere else an address has one symbol and the direction never arises.
//
////////////////////////////////////////////////////////////////////////////////

void DataDirectiveHandlers::ChooseOperandSymbol (DebugSession & session, DisassemblyLine & line)
{
    const Microcode           * instructionSet = session.GetTarget().GetInstructionSet();
    SymbolTableId               table          = SymbolTableId::Main;
    std::vector<std::string>    names;
    Byte                        opcode         = 0;



    if (line.instruction.operandAddress < kIoFirst || line.instruction.operandAddress > kIoLast ||
        instructionSet == nullptr || line.instruction.bytes.empty())
    {
        session.GetSymbols().TryFindName (line.instruction.operandAddress, line.operandSymbol, table);
        return;
    }

    session.GetSymbols().FindNames (line.instruction.operandAddress, names);
    opcode = line.instruction.bytes[0];

    line.operandSymbol = SymbolDescriptions::ChooseByDirection (
        names, EffectiveAddress::ClassifyOperand (instructionSet[opcode]) != PredictedAccess::Read);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::Define
//
//  [name] [addr | range] or name = addr. A lone address covers one item of
//  the directive's width; no address at all lists the blocks. A block with
//  no name given is called after the symbol at its start, where there is one.
//
////////////////////////////////////////////////////////////////////////////////

void DataDirectiveHandlers::Define (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    static constexpr uint32_t  kLastAddress = 0xFFFF;
    DataBlockKind              kind         = DataBlockKind::Bytes;
    int                        perLine      = kBytesPerLine;
    int                        itemBytes    = 1;
    Word                       last         = 0;
    std::string                name;
    SymbolTableId              table        = SymbolTableId::Main;
    DataBlockListData          list;
    DataBlockEntry             entry;



    TryGetShape (command.sourceName, kind, perLine, itemBytes);

    if (!command.hasA1)
    {
        ListBlocks (session, reply);
        return;
    }

    if (!command.hasA2 && (uint32_t) command.a1 + itemBytes - 1 > kLastAddress)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("One item at ${:04X} runs past $FFFF.", command.a1));
        return;
    }

    if (command.hasA2 && command.a2 < command.a1)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "The range ends before it begins.");
        return;
    }

    last = command.hasA2 ? command.a2 : (Word) (command.a1 + itemBytes - 1);
    name = command.text;

    if (name.empty())
    {
        session.GetSymbols().TryFindName (command.a1, name, table);
    }

    session.GetDataBlocks().Add (name, command.a1, last, kind, perLine);
    session.GetDataBlocks().TryFindAt (command.a1, entry);

    list.blocks.push_back ({ entry.name, entry.first, entry.last, entry.kind });
    reply.data = list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::Remove
//
////////////////////////////////////////////////////////////////////////////////

void DataDirectiveHandlers::Remove (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word  last = command.hasA2 ? command.a2 : command.a1;



    if (!command.hasA1)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "X takes the address or range to mark as code again.");
        return;
    }

    session.RemoveData (command.a1, last);
    ListBlocks (session, reply);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::ListBlocks
//
////////////////////////////////////////////////////////////////////////////////

void DataDirectiveHandlers::ListBlocks (DebugSession & session, Reply & reply)
{
    DataBlockListData  list;



    for (const DataBlockEntry & entry : session.GetDataBlocks().GetAll())
    {
        list.blocks.push_back ({ entry.name, entry.first, entry.last, entry.kind });
    }

    reply.data = list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::List
//
//  U range lists the range; U addr lists 20 lines from it; U alone continues
//  from where the last listing ended.
//
////////////////////////////////////////////////////////////////////////////////

void DataDirectiveHandlers::List (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    DisassemblyData      data;
    Word                 first = command.hasA1 ? command.a1 : m_nextList;
    std::optional<Word>  last;
    Word                 next  = 0;



    if (command.hasA2)
    {
        last = command.a2;
    }

    next       = Disassemble (session, first, last, last.has_value() ? kMaxLines : kDefaultLines, data);
    m_nextList = next;

    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::EnterAssembler
//
//  A [addr]: the address defaults to the program counter.
//
////////////////////////////////////////////////////////////////////////////////

void DataDirectiveHandlers::EnterAssembler (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word  address = command.hasA1 ? command.a1 : session.GetTarget().GetRegisters().pc;



    session.BeginAssembly (address);
    reply.data = MessageData { { std::format ("Assembling at ${:04X}. Enter a blank line to stop.", address) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::TryGetShape
//
//  The directive's kind, items per line and bytes per item from its name:
//  DB2, DB4 and DB8 set bytes per line, DW2 and DW4 words per line.
//
////////////////////////////////////////////////////////////////////////////////

bool DataDirectiveHandlers::TryGetShape (const std::string & name, DataBlockKind & kind, int & perLine, int & itemBytes)
{
    std::string  upper (name);
    char         last  = '\0';



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    last = upper.empty() ? '\0' : upper.back();

    if (upper == "ASC")
    {
        kind      = DataBlockKind::Text;
        perLine   = kTextPerLine;
        itemBytes = 1;
    }
    else if (upper == "DA")
    {
        kind      = DataBlockKind::Address;
        perLine   = 1;
        itemBytes = kWordBytes;
    }
    else if (upper == "DF")
    {
        kind      = DataBlockKind::Float;
        perLine   = 1;
        itemBytes = kFloatBytes;
    }
    else if (upper.starts_with ("DW"))
    {
        kind      = DataBlockKind::Words;
        perLine   = isdigit ((unsigned char) last) ? last - '0' : kWordsPerLine;
        itemBytes = kWordBytes;
    }
    else
    {
        kind      = DataBlockKind::Bytes;
        perLine   = isdigit ((unsigned char) last) ? last - '0' : kBytesPerLine;
        itemBytes = 1;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::MakeDataLine
//
//  One line of the block from address: as many whole items as fit on a line
//  and remain in the block, at least one byte. Its label is the symbol at
//  its address, or on the block's first line the block's name.
//
////////////////////////////////////////////////////////////////////////////////

void DataDirectiveHandlers::MakeDataLine (DebugSession & session, const DataBlockEntry & block, Word address, DisassemblyLine & line)
{
    static constexpr const char * kMnemonics[] = { "DB", "DW", "DA", "ASC", "DF" };
    IDebugTarget                & target       = session.GetTarget();
    SymbolTableId                 table        = SymbolTableId::Main;
    int                           itemBytes    = 1;
    int                           remaining    = block.last - address + 1;
    int                           count        = 0;



    switch (block.kind)
    {
    case DataBlockKind::Words:
    case DataBlockKind::Address: itemBytes = kWordBytes;  break;
    case DataBlockKind::Float:   itemBytes = kFloatBytes; break;
    default:                                              break;
    }

    count = std::max (1, std::min (block.perLine * itemBytes, remaining));

    line.instruction.address    = address;
    line.instruction.mnemonic   = kMnemonics[(int) block.kind];
    line.instruction.documented = true;

    for (int i = 0; i < count; ++i)
    {
        line.instruction.bytes.push_back (Peek (target, (Word) (address + i)));
    }

    line.instruction.operand = FormatItems (block, line.instruction.bytes);

    if (!session.GetSymbols().TryFindName (address, line.label, table) && address == block.first)
    {
        line.label = block.name;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::IsControl
//
//  Whether a text byte, with its high bit dropped, is a control character.
//
////////////////////////////////////////////////////////////////////////////////

bool DataDirectiveHandlers::IsControl (Byte b)
{
    Byte  ch = (Byte) (b & kLowBits);



    return ch < kFirstPrint || ch == kDelete;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::FormatItems
//
//  Bytes as $xx, words and addresses as $xxxx low byte first, text in
//  quotes with the high bit dropped and a control character as a period,
//  and a float as its decimal value to Applesoft's nine significant digits.
//  A trailing partial item is written as bytes. Control characters at the end of
//  text, such as the zero byte that ends a string, follow the quotes as
//  bytes, `"HELLO",$00`, so a period isn't taken for part of the text.
//
////////////////////////////////////////////////////////////////////////////////

std::string DataDirectiveHandlers::FormatItems (const DataBlockEntry & block, std::span<const Byte> bytes)
{
    static constexpr int  kByteBits = 8;
    std::string           text;
    size_t                i         = 0;



    if (block.kind == DataBlockKind::Text)
    {
        size_t  end = bytes.size();



        while (end > 0 && IsControl (bytes[end - 1]))
        {
            --end;
        }

        text = "\"";

        for (Byte b : bytes.first (end))
        {
            text += IsControl (b) ? '.' : (char) (b & kLowBits);
        }

        text += "\"";

        for (Byte b : bytes.subspan (end))
        {
            text += std::format (",${:02X}", b);
        }

        return text;
    }

    if (block.kind == DataBlockKind::Float && bytes.size() == kFloatBytes)
    {
        return std::format ("{:.9g}", DecodeFloat (bytes));
    }

    for (i = 0; i < bytes.size(); ++i)
    {
        bool  isWord = (block.kind == DataBlockKind::Words || block.kind == DataBlockKind::Address) && i + 1 < bytes.size();



        text += text.empty() ? "" : ",";

        if (isWord)
        {
            text += std::format ("${:04X}", bytes[i] | (bytes[i + 1] << kByteBits));
            ++i;
        }
        else
        {
            text += std::format ("${:02X}", bytes[i]);
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::DecodeFloat
//
//  Applesoft's five-byte form: an exponent in excess 128, then a 32-bit
//  mantissa in [0.5, 1) with the sign in its top bit and a leading one
//  implied. A zero exponent is zero.
//
////////////////////////////////////////////////////////////////////////////////

double DataDirectiveHandlers::DecodeFloat (std::span<const Byte> bytes)
{
    static constexpr int     kExcess       = 128;
    static constexpr double  kMantissaOne  = 4294967296.0;
    static constexpr int     kByteBits     = 8;
    uint32_t                 mantissa      = 0;
    bool                     isNegative    = (bytes[1] & kHighBit) != 0;
    int                      exponent      = bytes[0];



    if (exponent == 0)
    {
        return 0.0;
    }

    mantissa = ((uint32_t) (bytes[1] | kHighBit) << (kByteBits * 3)) |
               ((uint32_t) bytes[2] << (kByteBits * 2)) |
               ((uint32_t) bytes[3] << kByteBits) |
               (uint32_t) bytes[4];

    return (isNegative ? -1.0 : 1.0) * ((double) mantissa / kMantissaOne) * std::ldexp (1.0, exponent - kExcess);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataDirectiveHandlers::Peek
//
////////////////////////////////////////////////////////////////////////////////

Byte DataDirectiveHandlers::Peek (IDebugTarget & target, Word address)
{
    Byte  value = 0;



    target.TryPeek (address, value);
    return value;
}
