#include "Pch.h"

#include "PCodeDisassembler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::Cursor::ReadByte
//
////////////////////////////////////////////////////////////////////////////////

int PCodeDisassembler::Cursor::ReadByte()
{
    if (at >= end)
    {
        overrun = true;
        return 0;
    }

    return segment[at++];
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::Cursor::ReadBig
//
////////////////////////////////////////////////////////////////////////////////

int PCodeDisassembler::Cursor::ReadBig()
{
    int  first = ReadByte();
    int  low   = 0;



    if ((first & 0x80) == 0)
    {
        return first;
    }

    low = ReadByte();

    return ((first & 0x7F) << 8) | low;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::Cursor::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

int PCodeDisassembler::Cursor::ReadWord()
{
    int  low  = ReadByte();
    int  high = ReadByte();



    return (int16_t) (low | (high << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::Cursor::Align
//
////////////////////////////////////////////////////////////////////////////////

void PCodeDisassembler::Cursor::Align()
{
    if ((at & 1) != 0)
    {
        ReadByte();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::GetOpcode
//
//  Opcodes 128 to 215. Below them are the short constants, and above them
//  the short loads.
//
////////////////////////////////////////////////////////////////////////////////

const PCodeDisassembler::Opcode & PCodeDisassembler::GetOpcode (Byte opcode)
{
    static constexpr Opcode  kUndefined = { kUndefinedMnemonic, Operand::Undefined };
    static constexpr Opcode  kOpcodes[] =
    {
        { "ABI",  Operand::None     },   // 128
        { "ABR",  Operand::None     },
        { "ADI",  Operand::None     },
        { "ADR",  Operand::None     },
        { "LAND", Operand::None     },
        { "DIF",  Operand::None     },
        { "DVI",  Operand::None     },
        { "DVR",  Operand::None     },
        { "CHK",  Operand::None     },   // 136
        { "FLO",  Operand::None     },
        { "FLT",  Operand::None     },
        { "INN",  Operand::None     },
        { "INT",  Operand::None     },
        { "LOR",  Operand::None     },
        { "MODI", Operand::None     },
        { "MPI",  Operand::None     },
        { "MPR",  Operand::None     },   // 144
        { "NGI",  Operand::None     },
        { "NGR",  Operand::None     },
        { "LNOT", Operand::None     },
        { "SRS",  Operand::None     },
        { "SBI",  Operand::None     },
        { "SBR",  Operand::None     },
        { "SGS",  Operand::None     },
        { "SQI",  Operand::None     },   // 152
        { "SQR",  Operand::None     },
        { "STO",  Operand::None     },
        { "IXS",  Operand::None     },
        { "UNI",  Operand::None     },
        { "LDE",  Operand::SegBig   },
        { "CSP",  Operand::Standard },
        { "LDCN", Operand::None     },
        { "ADJ",  Operand::UByte    },   // 160
        { "FJP",  Operand::Jump     },
        { "INC",  Operand::Big      },
        { "IND",  Operand::Big      },
        { "IXA",  Operand::Big      },
        { "LAO",  Operand::Big      },
        { "LSA",  Operand::Chars    },
        { "LAE",  Operand::SegBig   },
        { "MOV",  Operand::Big      },   // 168
        { "LDO",  Operand::Big      },
        { "SAS",  Operand::UByte    },
        { "SRO",  Operand::Big      },
        { "XJP",  Operand::Case     },
        { "RNP",  Operand::UByte    },
        { "CIP",  Operand::UByte    },
        { "EQU",  Operand::Compare  },
        { "GEQ",  Operand::Compare  },   // 176
        { "GRT",  Operand::Compare  },
        { "LDA",  Operand::LexBig   },
        { "LDC",  Operand::Constant },
        { "LEQ",  Operand::Compare  },
        { "LES",  Operand::Compare  },
        { "LOD",  Operand::LexBig   },
        { "NEQ",  Operand::Compare  },
        { "STR",  Operand::LexBig   },   // 184
        { "UJP",  Operand::Jump     },
        { "LDP",  Operand::None     },
        { "STP",  Operand::None     },
        { "LDM",  Operand::UByte    },
        { "STM",  Operand::UByte    },
        { "LDB",  Operand::None     },
        { "STB",  Operand::None     },
        { "IXP",  Operand::TwoBytes },   // 192
        { "RBP",  Operand::UByte    },
        { "CBP",  Operand::UByte    },
        { "EQUI", Operand::None     },
        { "GEQI", Operand::None     },
        { "GRTI", Operand::None     },
        { "LLA",  Operand::Big      },
        { "LDCI", Operand::Word     },
        { "LEQI", Operand::None     },   // 200
        { "LESI", Operand::None     },
        { "LDL",  Operand::Big      },
        { "NEQI", Operand::None     },
        { "STL",  Operand::Big      },
        { "CXP",  Operand::TwoBytes },
        { "CLP",  Operand::UByte    },
        { "CGP",  Operand::UByte    },
        { "LPA",  Operand::Chars    },   // 208
        { "STE",  Operand::SegBig   },
        kUndefined,
        { "EFJ",  Operand::Jump     },
        { "NFJ",  Operand::Jump     },
        { "BPT",  Operand::Big      },
        { "XIT",  Operand::None     },
        { "NOP",  Operand::None     },
    };



    size_t  index = (size_t) opcode - kFirstOpcode;

    return (opcode >= kFirstOpcode && index < std::size (kOpcodes)) ? kOpcodes[index] : kUndefined;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::GetStandardProcedure
//
////////////////////////////////////////////////////////////////////////////////

const char * PCodeDisassembler::GetStandardProcedure (Byte number)
{
    static constexpr const char *  kNames[] =
    {
        "IOC",    "NEW",    "MVL",    "MVR",    "EXIT",   "UREAD",  "UWRITE", "IDS",       // 0
        "TRS",    "TIM",    "FLC",    "SCN",    "USTAT",  nullptr,  nullptr,  nullptr,     // 8
        nullptr,  nullptr,  nullptr,  nullptr,  nullptr,  "LDSEG",  "RELSEG", "TRC",       // 16
        "RND",    "SIN",    "COS",    "LOG",    "ATAN",   "LN",     "EXP",    "SQRT",      // 24
        "MRK",    "RLS",    "IOR",    "UBUSY",  "POT",    "UWAIT",  "UCLEAR", "HLT",       // 32
        "MEMAV",                                                                           // 40
    };



    return (number < std::size (kNames)) ? kNames[number] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::IsReturn
//
////////////////////////////////////////////////////////////////////////////////

bool PCodeDisassembler::IsReturn (const PCodeLine & line)
{
    return line.isDefined && !line.bytes.empty() && (line.bytes[0] == kRnp || line.bytes[0] == kRbp) && line.mnemonic != kWordDirective;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::FormatOffset
//
////////////////////////////////////////////////////////////////////////////////

std::string PCodeDisassembler::FormatOffset (size_t offset)
{
    return std::format ("${:04X}", offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::FormatJump
//
//  A target the jump table doesn't have is written as the operand itself.
//
////////////////////////////////////////////////////////////////////////////////

std::string PCodeDisassembler::FormatJump (std::span<const Byte> segment, int distance, size_t next, size_t attributes)
{
    size_t  back  = (size_t) -distance;
    size_t  entry = 0;
    size_t  word  = 0;



    if (distance >= 0)
    {
        return FormatOffset (next + (size_t) distance);
    }

    if (back <= attributes && attributes - back + 1 < segment.size())
    {
        entry = attributes - back;
        word  = (size_t) segment[entry] | ((size_t) segment[entry + 1] << 8);

        if (word <= entry)
        {
            return FormatOffset (entry - word);
        }
    }

    return std::format ("table{}", distance);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::FormatCompare
//
//  The type compared, and for bytes and words, their count.
//
////////////////////////////////////////////////////////////////////////////////

std::string PCodeDisassembler::FormatCompare (Cursor & cursor)
{
    static constexpr const char *  kTypes[] = { nullptr, "REAL", "STRG", "BOOL", "POWR", "BYTE", "WORD" };



    int          type  = cursor.ReadByte();
    int          index = type / 2;
    std::string  text;



    if ((type & 1) != 0 || index == 0 || index >= (int) std::size (kTypes))
    {
        return std::to_string (type);
    }

    text = kTypes[index];

    if (type == kCompareByte || type == kCompareWord)
    {
        text += ',';
        text += std::to_string (cursor.ReadBig());
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::FormatChars
//
//  A count and that many characters, in quotes.
//
////////////////////////////////////////////////////////////////////////////////

std::string PCodeDisassembler::FormatChars (Cursor & cursor)
{
    int          count = cursor.ReadByte();
    std::string  text  = "'";



    for (int i = 0; i < count && !cursor.overrun; i++)
    {
        int  c = cursor.ReadByte() & 0x7F;

        text += (c >= 0x20 && c < 0x7F) ? (char) c : '.';
    }

    text += '\'';

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::FormatConstant
//
//  A count and, on an even offset, that many words.
//
////////////////////////////////////////////////////////////////////////////////

std::string PCodeDisassembler::FormatConstant (Cursor & cursor)
{
    int          count = cursor.ReadByte();
    std::string  text;



    cursor.Align();

    for (int i = 0; i < count && !cursor.overrun; i++)
    {
        text += (i == 0) ? "" : ",";
        text += std::to_string (cursor.ReadWord());
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::FormatOperand
//
//  outCases is the size of XJP's table, which follows the jump after XJP.
//
////////////////////////////////////////////////////////////////////////////////

std::string PCodeDisassembler::FormatOperand (Operand operand, Cursor & cursor, size_t attributes, int & outCases)
{
    int           first  = 0;
    int           second = 0;
    const char *  name   = nullptr;



    switch (operand)
    {
        case Operand::UByte:
            return std::to_string (cursor.ReadByte());

        case Operand::Big:
            return std::to_string (cursor.ReadBig());

        case Operand::Word:
            return std::to_string (cursor.ReadWord());

        case Operand::LexBig:
        case Operand::SegBig:
            first  = cursor.ReadByte();
            second = cursor.ReadBig();
            return std::format ("{},{}", first, second);

        case Operand::TwoBytes:
            first  = cursor.ReadByte();
            second = cursor.ReadByte();
            return std::format ("{},{}", first, second);

        case Operand::Jump:
            first = (int8_t) cursor.ReadByte();
            return FormatJump (cursor.segment, first, cursor.at, attributes);

        case Operand::Chars:
            return FormatChars (cursor);

        case Operand::Constant:
            return FormatConstant (cursor);

        case Operand::Compare:
            return FormatCompare (cursor);

        case Operand::Case:
            cursor.Align();
            first    = cursor.ReadWord();
            second   = cursor.ReadWord();
            outCases = (second >= first) ? second - first + 1 : 0;
            return std::format ("{},{}", first, second);

        case Operand::Standard:
            first = cursor.ReadByte();
            name  = GetStandardProcedure ((Byte) first);
            return (name != nullptr) ? std::string (name) : std::to_string (first);

        default:
            break;
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::AddCases
//
//  The words of XJP's table, each written as its target.
//
////////////////////////////////////////////////////////////////////////////////

void PCodeDisassembler::AddCases (Cursor & cursor, int cases, std::vector<PCodeLine> & outLines)
{
    for (int i = 0; i < cases && cursor.at + 2 <= cursor.end; i++)
    {
        PCodeLine  line;
        size_t     at   = cursor.at;
        size_t     word = (size_t) cursor.segment[at] | ((size_t) cursor.segment[at + 1] << 8);

        line.offset    = at;
        line.bytes     = { cursor.segment[at], cursor.segment[at + 1] };
        line.mnemonic  = kWordDirective;
        line.operand   = (word <= at) ? FormatOffset (at - word) : std::to_string (word);
        line.isDefined = true;

        outLines.push_back (std::move (line));
        cursor.at += 2;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::Disassemble
//
////////////////////////////////////////////////////////////////////////////////

void PCodeDisassembler::Disassemble (std::span<const Byte>     segment,
                                     size_t                    start,
                                     size_t                    end,
                                     size_t                    attributes,
                                     std::vector<PCodeLine>  & outLines)
{
    Cursor  cursor  = { segment, start, (std::min) (end, segment.size()) };
    int     cases   = 0;
    int     pending = 0;



    while (cursor.at < cursor.end)
    {
        PCodeLine  line;
        size_t     first  = cursor.at;
        Byte       opcode = segment[cursor.at++];

        line.offset    = first;
        line.isDefined = true;

        if (opcode < kFirstOpcode)
        {
            line.mnemonic = "SLDC";
            line.operand  = std::to_string (opcode);
        }
        else if (opcode >= kShortIndirect)
        {
            line.mnemonic = "SIND";
            line.operand  = std::to_string (opcode - kShortIndirect);
        }
        else if (opcode >= kShortGlobal)
        {
            line.mnemonic = "SLDO";
            line.operand  = std::to_string (opcode - kShortGlobal + 1);
        }
        else if (opcode >= kShortLocal)
        {
            line.mnemonic = "SLDL";
            line.operand  = std::to_string (opcode - kShortLocal + 1);
        }
        else
        {
            const Opcode &  entry = GetOpcode (opcode);

            line.mnemonic  = entry.mnemonic;
            line.operand   = FormatOperand (entry.operand, cursor, attributes, cases);
            line.isDefined = entry.operand != Operand::Undefined;
        }

        if (cursor.overrun)
        {
            line.mnemonic  = kUndefinedMnemonic;
            line.isDefined = false;
            line.operand.clear();
        }

        line.bytes.assign (segment.begin() + first, segment.begin() + cursor.at);
        outLines.push_back (std::move (line));

        if (cursor.overrun)
        {
            break;
        }

        if (pending > 0)
        {
            AddCases (cursor, pending, outLines);
        }

        pending = cases;
        cases   = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler::FormatLine
//
//  Bytes past the column's width are written as two dots.
//
////////////////////////////////////////////////////////////////////////////////

std::string PCodeDisassembler::FormatLine (const PCodeLine & line)
{
    constexpr size_t  kBytesColumnWidth    = kListBytes * 3;
    constexpr size_t  kMnemonicColumnWidth = 5;



    std::string  text  = std::format ("{:04X}  ", line.offset);
    std::string  hex;
    size_t       count = (line.bytes.size() > kListBytes) ? kListBytes - 1 : line.bytes.size();



    for (size_t i = 0; i < count; i++)
    {
        hex += std::format ("{:02X} ", line.bytes[i]);
    }

    hex += (line.bytes.size() > kListBytes) ? ".." : "";
    hex.resize ((std::max) (hex.size(), kBytesColumnWidth), ' ');

    text += hex;
    text += ' ';
    text += line.mnemonic;

    if (!line.operand.empty())
    {
        text.append ((line.mnemonic.size() < kMnemonicColumnWidth) ? kMnemonicColumnWidth - line.mnemonic.size() : 0, ' ');
        text += ' ';
        text += line.operand;
    }

    return text;
}
