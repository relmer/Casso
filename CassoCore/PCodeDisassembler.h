#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeLine
//
//  One p-code instruction: its offset in the segment, its bytes, and its
//  mnemonic and operand. `isDefined` is false for an opcode that isn't in
//  the table and for an instruction cut off by the end of the code.
//
////////////////////////////////////////////////////////////////////////////////

struct PCodeLine
{
    size_t             offset    = 0;
    std::vector<Byte>  bytes;
    std::string        mnemonic;
    std::string        operand;
    bool               isDefined = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PCodeDisassembler
//
//  The p-code Apple Pascal 1.1 to 1.3 compile, version II.1 of the UCSD
//  P-machine, with words stored low byte first. The opcodes and their
//  operands are from the Apple Pascal Operating System Reference Manual.
//
//  A "big" operand is one byte below 128, or two bytes, the first with its
//  high bit set and the high seven bits of the value. A jump's operand is a
//  distance forward, or a negative offset from the procedure's attribute
//  table to a word of the jump table. A word of the jump table, like a case
//  of XJP, is the distance back from the word to its target. LDC and XJP
//  align their words on an even offset in the segment.
//
////////////////////////////////////////////////////////////////////////////////

class PCodeDisassembler
{
public:
    //  The instructions from `start` up to `end`, both offsets in the
    //  segment. `attributes` is the offset of the procedure's attribute
    //  table, the word with the procedure number.
    static void  Disassemble (std::span<const Byte>     segment,
                              size_t                    start,
                              size_t                    end,
                              size_t                    attributes,
                              std::vector<PCodeLine>  & outLines);

    //  `0012  C7 FF 00     LDCI  255`
    static std::string  FormatLine (const PCodeLine & line);

    //  The name of a standard procedure CSP calls, or nullptr.
    static const char *  GetStandardProcedure (Byte number);

    static bool  IsReturn (const PCodeLine & line);

    static constexpr const char *  kUndefinedMnemonic = "???";
    static constexpr const char *  kWordDirective     = "DW";

private:
    enum class Operand : Byte
    {
        None, UByte, Big, LexBig, SegBig, Jump, Word, TwoBytes, Chars,
        Constant, Case, Compare, Standard, Undefined,
    };

    struct Opcode
    {
        const char *  mnemonic;
        Operand       operand;
    };

    static constexpr Byte    kFirstOpcode   = 0x80;
    static constexpr Byte    kShortLocal    = 216;
    static constexpr Byte    kShortGlobal   = 232;
    static constexpr Byte    kShortIndirect = 248;
    static constexpr Byte    kRnp           = 173;
    static constexpr Byte    kRbp           = 193;
    static constexpr Byte    kCompareByte   = 10;
    static constexpr Byte    kCompareWord   = 12;
    static constexpr size_t  kListBytes     = 4;

    //  The reader of one instruction's operands.
    struct Cursor
    {
        std::span<const Byte>  segment;
        size_t                 at       = 0;
        size_t                 end      = 0;
        bool                   overrun  = false;

        int   ReadByte ();
        int   ReadBig  ();
        int   ReadWord ();
        void  Align    ();
    };

    static const Opcode &  GetOpcode       (Byte opcode);
    static std::string     FormatOffset    (size_t offset);
    static std::string     FormatJump      (std::span<const Byte> segment, int distance, size_t next, size_t attributes);
    static std::string     FormatCompare   (Cursor & cursor);
    static std::string     FormatChars     (Cursor & cursor);
    static std::string     FormatConstant  (Cursor & cursor);
    static std::string     FormatOperand   (Operand operand, Cursor & cursor, size_t attributes, int & outCases);
    static void            AddCases        (Cursor & cursor, int cases, std::vector<PCodeLine> & outLines);
};
