#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Applesoft program types
//
//  A program line as LIST prints it, without LIST's wrapping; a simple
//  variable or an array with its values as PRINT prints them; and a GOSUB or
//  FOR frame on the 6502 stack.
//
////////////////////////////////////////////////////////////////////////////////

struct BasicLine
{
    Word                      address = 0;       // the line's link field
    Word                      number  = 0;
    std::string               text;              // the body, tokens written out with LIST's spacing
    std::vector<std::string>  pieces;            // each stored byte as LIST prints it, the body's parts
};

enum class BasicType
{
    Real,
    Integer,
    String,
};

struct BasicVariable
{
    std::string  name;              // as a program writes it: A, B%, C$
    BasicType    type    = BasicType::Real;
    Word         address = 0;       // the value's first byte
    std::string  value;
};

struct BasicArray
{
    std::string               name;
    BasicType                 type    = BasicType::Real;
    Word                      address = 0;
    std::vector<int>          sizes;    // each subscript's count, first subscript first
    std::vector<std::string>  values;   // first subscript varying fastest
};

enum class BasicFrameKind
{
    Gosub,
    For,
};

//  A simple variable or an array element as a program writes it: A, B%, C$,
//  X(2,3). Applesoft compares only the first two characters of a variable's
//  name, so the field has those and the type's sign.
struct BasicReference
{
    std::string       name;
    BasicType         type = BasicType::Real;
    std::vector<int>  subscripts;
};

struct BasicFrame
{
    BasicFrameKind  kind     = BasicFrameKind::Gosub;
    Word            address  = 0;     // the frame's first byte on the stack
    Word            line     = 0;     // the line the GOSUB or FOR is on
    std::string     variable;         // FOR only
    std::string     limit;
    std::string     step;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram
//
//  The program and its state, read from Applesoft's zero page and tables
//  through a peek that changes nothing (research R-043): TXTTAB the program,
//  VARTAB to ARYTAB the simple variables, ARYTAB to STREND the arrays, CURLIN
//  the line being run, and the frames on the stack from where NEWSTT last
//  left the stack pointer (REMSTK) up to the bottom Applesoft sets on RUN.
//
////////////////////////////////////////////////////////////////////////////////

class ApplesoftProgram
{
public:
    using Peek = std::function<Byte (Word)>;

    static constexpr Word  kTxtTab     = 0x67;
    static constexpr Word  kVarTab     = 0x69;
    static constexpr Word  kAryTab     = 0x6B;
    static constexpr Word  kStrEnd     = 0x6D;
    static constexpr Word  kCurLin     = 0x75;
    static constexpr Word  kTxtPtr     = 0xB8;
    static constexpr Word  kErrFlag    = 0xD8;
    static constexpr Word  kRemStk     = 0xF8;
    static constexpr Word  kNewStt     = 0xD7D2;
    static constexpr Word  kError      = 0xD412;
    static constexpr Word  kFout       = 0xED34;   // the number-to-text routine PRINT calls
    static constexpr Word  kFac        = 0x9D;   // its accumulator: exponent, four mantissa bytes
    static constexpr Word  kFacSign    = 0xA2;
    static constexpr Word  kFacExtra   = 0xAC;
    static constexpr Word  kFoutText   = 0x0100;   // where FOUT writes the text
    static constexpr int   kListWrap   = 33;   // the column LIST continues a line at
    static constexpr int   kListIndent = 5;   // and the indent it continues at

    explicit ApplesoftProgram (Peek peek) : m_peek (std::move (peek)) {}

    //  False when the link chain from TXTTAB does not end in a zero link
    //  inside memory: no program, or one being edited.
    bool                 TryGetListing (std::vector<BasicLine> & lines) const;
    std::optional<Word>  GetCurrentLine() const;

    //  Whether the interpreter is waiting at its ] prompt for a line, with
    //  the stack pointer given: RESTART's call of GETLN, whose return address
    //  is then on the stack. CURLIN has the last line run until a line is
    //  typed, for CONT.
    bool                 IsAtPrompt (Byte stackPointer) const;
    void                 GetVariables  (std::vector<BasicVariable> & variables) const;
    void                 GetArrays     (std::vector<BasicArray> & arrays) const;
    void                 GetFrames     (std::vector<BasicFrame> & frames) const;

    //  A variable or array element written as a program writes it; false for
    //  text that is not one.
    static bool          TryParseReference (const std::string & text, BasicReference & reference);

    //  The value's bytes, a string's being its characters, with the value
    //  Applesoft reads for a variable that does not exist yet, 0 or "", as
    //  none: so two reads compare equal exactly when the value is the same.
    std::vector<Byte>    GetValueBytes (const BasicReference & reference) const;

    //  The value as PRINT prints it, 0 or empty for one that does not exist.
    std::string          GetValueText  (const BasicReference & reference) const;

    //  Whether the variable or array element exists.
    bool                 HasValue      (const BasicReference & reference) const { return FindValue (reference).has_value(); }

    //  A real as PRINT prints it, from its five bytes as Applesoft stores a
    //  variable: the exponent, then the mantissa with the sign in its top bit.
    //  FormatReal approximates PRINT in host arithmetic; a reader with the
    //  machine's ROM runs FOUT itself, for PRINT's own rounding.
    static std::string               FormatReal (const std::array<Byte, 5> & bytes);
    std::string                      FormatRealByRom (const std::array<Byte, 5> & bytes) const;

    //  The rows LIST prints for the lines.
    static std::vector<std::string>  GetListRows (const std::vector<BasicLine> & lines);

private:
    static constexpr int   kVariableBytes = 7;
    static constexpr int   kGosubBytes    = 7;       // the frame and the statement's return address under it
    static constexpr int   kForBytes      = 18;
    static constexpr Byte  kGosubToken    = 0xB0;
    static constexpr Byte  kForToken      = 0x81;
    static constexpr Word  kStackBottom   = 0x01F8;
    static constexpr Word  kPromptReturn  = 0xD443;   // RESTART's JSR GETLN at $D441, as it is pushed

    Byte                 PeekByte     (Word address) const { return m_peek (address); }
    Word                 PeekWord     (Word address) const;
    std::string          FormatValue  (Word address, BasicType type) const;
    std::string          FormatPacked (Word address) const;

    //  Where the value of a variable or array element is, none while it does
    //  not exist.
    std::optional<Word>  FindValue    (const BasicReference & reference) const;

    static std::string  GetName    (Byte first, Byte second, BasicType type);
    static BasicType    GetType    (Byte first, Byte second);
    static bool         IsFunction (Byte first, Byte second);

    Peek  m_peek;
};
