#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource
//
//  Source for Lazer's Interactive Symbolic Assembler, its tokens listed in
//  columns: v2, a DOS 3.3 alternate B file, lines of a length byte, a label,
//  a mnemonic token, an address mode and text; v3, a ProDOS INT file at aux
//  $1000 to $3FFF, with a packed symbol table and tokenized operands; and
//  v4 and v5, at aux $4000 to $5FFF, with a symbol table of strings, the
//  65816's mnemonics and the column stops in the header. Ported from
//  CiderPress II's LisaAsm, under its Apache license.
//
////////////////////////////////////////////////////////////////////////////////

class LisaSource
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    static constexpr Byte    kDosAlternateB = 0x40;
    static constexpr Byte    kProDosInt     = 0xFA;
    static constexpr size_t  kMaxBytes      = 64 * 1024;
    static constexpr Byte    kEndOfLine     = 0x0D;

    static constexpr int     kLabel         = 0;
    static constexpr int     kOpcode        = 1;
    static constexpr int     kOperand       = 2;
    static constexpr int     kComment       = 3;

    enum class Next { Operator, Operand, Failed };

    //  The bytes, read as 0 past their end, the symbols, the line being
    //  built in columns, and the listing.
    struct Context
    {
        std::span<const Byte>      bytes;
        std::vector<std::string>   symbols;
        std::array<int, 4>         stops = {};
        std::string                line;
        DecodedFile              * file  = nullptr;

        Byte  At            (size_t offset) const { return (offset < bytes.size()) ? bytes[offset] : 0; }
        void  Tab           (int column);
        void  Append        (char c);
        void  Append        (const std::string & text);
        void  AppendSymbol  (size_t index);
        void  EndLine       ();
        void  Note          (const std::wstring & note);
    };

    static bool  IsVersion3 (const FileFacts & facts, std::span<const Byte> bytes);
    static bool  IsVersion4 (const FileFacts & facts, std::span<const Byte> bytes);

    static void  DecodeVersion2      (Context & context);
    static void  DecodeVersion2Line  (Context & context, size_t offset, size_t length);

    static void  DecodeVersion3      (Context & context);
    static void  DecodeVersion3Line  (Context & context, size_t offset, int length);
    static void  ConvertOperand3     (Context & context, Byte mnemonic, size_t & offset, int & length);
    static Next  PrintNumber3        (Context & context, int addressMode, Byte value, size_t & offset, int & length);
    static void  PrintComment3       (Context & context, int addressMode, size_t offset, int length);
    static bool  PrintMnemonic3      (Context & context, Byte value);

    static void  DecodeVersion4      (Context & context);
    static void  DecodeVersion4Line  (Context & context, size_t offset, int length);
    static bool  PrintMnemonic4      (Context & context, Byte value);
    static void  ConvertOperand4     (Context & context, Byte mnemonic, size_t & offset, int & length);
    static Next  PrintComplex4       (Context & context, Byte value, size_t & offset, int & length);
    static Next  PrintNumber4        (Context & context, Byte value, size_t & offset, int & length);

    static std::string  ReadString   (Context & context, size_t & offset, int & length, int count);
    static std::string  FormatValue  (Context & context, char radix, int count, size_t & offset, int & length);
};
