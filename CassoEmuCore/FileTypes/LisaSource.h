#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource
//
//  Source for Lazer's Interactive Symbolic Assembler as a text listing: v2,
//  a DOS 3.3 alternate B file of length-prefixed lines with mnemonic tokens;
//  v3, a ProDOS INT file at aux $1000 to $3FFF, with a packed symbol table
//  and tokenized operands; and v4 and v5, at aux $4000 to $5FFF, with a
//  symbol table of strings, the 65816's mnemonics and the columns in the
//  header. docs/file-formats/lisa-source.md is the format.
//
////////////////////////////////////////////////////////////////////////////////

class LisaSource
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

private:
    enum class Version { None, Two, Three, Four };

    //  The part of an operand that comes next.
    enum class Expect { Term, Operator, End, Failed };

    //  A listing line built a column at a time, and the listing's notes.
    struct Listing
    {
        DecodedFile  & file;
        std::string    text;
        int            columns[4] = { 0, 9, 18, 40 };

        void  Tab       (int column);
        void  Put       (char c);
        void  Put       (std::string_view text);
        void  PutByte   (Byte value);
        void  EndLine   ();
        void  Note      (const std::wstring & note);
    };

    //  A line's bytes, each read checked against its end.
    struct Cursor
    {
        std::span<const Byte>  bytes;
        size_t                 at     = 0;
        bool                   failed = false;

        bool      HasMore   () const { return at < bytes.size(); }
        size_t    GetLeft   () const { return bytes.size() - (std::min) (at, bytes.size()); }
        Byte      Peek      () const { return HasMore() ? bytes[at] : (Byte) 0; }
        Byte      Take      ();
        uint32_t  TakeValue (int count);
        void      Back      ()       { at--; }
    };

    static constexpr Byte    kDosAlternateB  = 0x40;
    static constexpr Byte    kProDosInt      = 0xFA;
    static constexpr Byte    kEndOfLine      = 0x0D;
    static constexpr Byte    kHighSemicolon  = 0xBB;

    static constexpr int     kLabel          = 0;
    static constexpr int     kMnemonic       = 1;
    static constexpr int     kOperand        = 2;
    static constexpr int     kComment        = 3;

    //  The v4 term and operator tables, a character a token from $00 to
    //  $7F, a space where the table is blank.
    static constexpr const char *  s_kTermFirst =
        "+-*/&|^=<>%<><~-"
        "0123456789      "
        "++++++++++      "
        " <>      *@#/^|\\"
        "<<<<<<<<<<??????"
        ">>>>>>>>>>???? ?"
        "                "
        "                ";

    static constexpr const char *  s_kTermSecond =
        "           ==>!!"
        "                "
        "0123456789      "
        " <>       !!!!!!"
        "0123456789012345"
        "01234567896789 #"
        "                "
        "                ";

    static constexpr const char *  s_kOperators =
        "+-*/&|^=<>%<><  "
        "                "
        "!!!!!!!!!!!!!!!!"
        " <>             "
        "                "
        "                "
        "                "
        "                ";

    static Version   GetVersion    (const FileFacts & facts, std::span<const Byte> bytes);
    static Word      ReadWord      (std::span<const Byte> bytes, size_t at);

    static void      DecodeVersion2   (std::span<const Byte> bytes, Listing & out);
    static void      DecodeLine2      (std::span<const Byte> line, Listing & out);

    static void      ReadSymbols3     (std::span<const Byte> table, std::vector<std::string> & outSymbols);
    static void      DecodeVersion3   (std::span<const Byte> bytes, Listing & out);
    static void      DecodeLine3      (Cursor & line, const std::vector<std::string> & symbols, Listing & out);
    static void      PutMnemonic3     (Byte token, Listing & out);
    static void      PutOperand3      (Cursor & line, Byte mnemonic, const std::vector<std::string> & symbols, Listing & out);
    static Expect    PutTerm3         (Cursor & line, Byte token, const std::vector<std::string> & symbols, Listing & out);

    static void      ReadSymbols4     (std::span<const Byte> table, size_t count, std::vector<std::string> & outSymbols, Listing & out);
    static void      DecodeVersion4   (std::span<const Byte> bytes, Listing & out);
    static void      DecodeLine4      (Cursor & line, const std::vector<std::string> & symbols, Listing & out);
    static void      PutMnemonic4     (Byte token, Listing & out);
    static void      PutOperand4      (Cursor & line, Byte mnemonic, const std::vector<std::string> & symbols, Listing & out);
    static Expect    PutTerm4         (Cursor & line, const std::vector<std::string> & symbols, Listing & out);
    static Expect    PutOperator4     (Cursor & line, const std::vector<std::string> & symbols, Listing & out);
    static Expect    PutValue4        (Cursor & line, Byte token, const std::vector<std::string> & symbols, Listing & out);

    static void      PutSymbol        (size_t index, const std::vector<std::string> & symbols, Listing & out);
    static void      PutNumber        (Cursor & line, char radix, int count, Listing & out);
    static void      PutHex           (Byte value, Listing & out);
    static void      PutBits          (Byte value, Listing & out);
    static Expect    PutString        (Cursor & line, Byte token, Listing & out);
    static void      PutRest          (Cursor & line, Listing & out);
};
