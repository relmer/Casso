#include "Pch.h"

#include "Debugger/Basic/ApplesoftProgram.h"
#include "Debugger/Basic/OverlayCpu.h"
#include "ApplesoftTokenizer.h"





static constexpr size_t  kMaxLines        = 4096;
static constexpr Word    kProgramCeiling  = 0xC000;
static constexpr Byte    kTypeBit         = 0x80;
static constexpr int     kDigits          = 9;
static constexpr int     kFixedFirst      = -2;       // .01 is the smallest PRINT writes without an exponent
static constexpr int     kFixedLast       = 8;        // and 999999999 the largest
static constexpr uint32_t  kFoutSteps     = 20'000;
static constexpr size_t    kFoutLength    = 20;





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::TryGetListing
//
//  Each line is a link to the next, a line number and its tokens, ending in a
//  zero byte; a zero link ends the program.
//
////////////////////////////////////////////////////////////////////////////////

bool ApplesoftProgram::TryGetListing (std::vector<BasicLine> & lines) const
{
    Word  at     = PeekWord (kTxtTab);
    bool  isDone = false;



    lines.clear();

    while (!isDone && lines.size() < kMaxLines && at < kProgramCeiling)
    {
        Word       link = PeekWord (at);
        BasicLine  line;



        isDone = link == 0;

        if (isDone)
        {
            break;
        }

        line.address = at;
        line.number  = PeekWord ((Word) (at + 2));

        for (Word body = (Word) (at + 4); body < link && PeekByte (body) != 0; body++)
        {
            Byte          ch      = PeekByte (body);
            const char  * keyword = (ch >= ApplesoftTokenizer::kFirstToken) ? ApplesoftTokenizer::GetKeyword (ch) : nullptr;



            line.pieces.push_back ((keyword != nullptr) ? std::format (" {} ", keyword) : std::string (1, (char) (ch & 0x7F)));
            line.text += line.pieces.back();
        }

        lines.push_back (line);
        isDone = link <= at;
        at     = link;
    }

    return isDone && PeekWord (at) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::GetCurrentLine
//
//  None in direct mode, where CURLIN's high byte is $FF.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> ApplesoftProgram::GetCurrentLine() const
{
    Word  line = PeekWord (kCurLin);



    return ((line >> 8) == 0xFF) ? std::nullopt : std::optional<Word> (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::GetVariables
//
//  Seven bytes each: the name, whose high bits indicate the type, and five bytes
//  of value. A DEF FN entry is skipped.
//
////////////////////////////////////////////////////////////////////////////////

void ApplesoftProgram::GetVariables (std::vector<BasicVariable> & variables) const
{
    Word  end = PeekWord (kAryTab);



    variables.clear();

    for (Word at = PeekWord (kVarTab); at + kVariableBytes <= end && at < kProgramCeiling; at += kVariableBytes)
    {
        Byte           first    = PeekByte (at);
        Byte           second   = PeekByte ((Word) (at + 1));
        BasicVariable  variable;



        if (IsFunction (first, second))
        {
            continue;
        }

        variable.type    = GetType (first, second);
        variable.name    = GetName (first, second, variable.type);
        variable.address = (Word) (at + 2);
        variable.value   = FormatValue (variable.address, variable.type);
        variables.push_back (variable);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::GetArrays
//
//  Each array is its name, the offset to the next, the count of subscripts,
//  each subscript's size big-endian with the last subscript first, then the
//  elements with the first subscript varying fastest.
//
////////////////////////////////////////////////////////////////////////////////

void ApplesoftProgram::GetArrays (std::vector<BasicArray> & arrays) const
{
    Word  end = PeekWord (kStrEnd);
    Word  at  = PeekWord (kAryTab);



    arrays.clear();

    while (at + 5 <= end && at < kProgramCeiling)
    {
        Byte        first   = PeekByte (at);
        Byte        second  = PeekByte ((Word) (at + 1));
        Word        next    = PeekWord ((Word) (at + 2));
        int         count   = PeekByte ((Word) (at + 4));
        BasicArray  array;
        size_t      total   = 1;
        int         width   = 0;
        Word        element = (Word) (at + 5 + 2 * count);



        if (next == 0 || count == 0)
        {
            break;
        }

        array.type    = GetType (first, second);
        array.name    = GetName (first, second, array.type);
        array.address = at;
        width         = (array.type == BasicType::Real) ? 5 : (array.type == BasicType::Integer) ? 2 : 3;

        for (int dim = count - 1; dim >= 0; dim--)
        {
            Word  size = (Word) ((PeekByte ((Word) (at + 5 + 2 * dim)) << 8) | PeekByte ((Word) (at + 6 + 2 * dim)));



            array.sizes.push_back (size);
            total *= size;
        }

        for (size_t i = 0; i < total && element + width <= at + next; i++, element = (Word) (element + width))
        {
            array.values.push_back (FormatValue (element, array.type));
        }

        arrays.push_back (array);
        at = (Word) (at + next);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::GetFrames
//
//  From the newest frame down. A GOSUB frame is its token, the line and the
//  text pointer, over the return address of the statement that ran it; a FOR
//  frame is its token, the loop variable's address, the step as FOUT's
//  accumulator has it (the mantissa's top bit set, the sign in the byte
//  after), the step's sign, the limit packed as a variable is, the line and
//  the text pointer.
//
////////////////////////////////////////////////////////////////////////////////

void ApplesoftProgram::GetFrames (std::vector<BasicFrame> & frames) const
{
    Word  at = (Word) (0x0100 + PeekByte (kRemStk) + 1);



    frames.clear();

    while (at < kStackBottom)
    {
        Byte        token = PeekByte (at);
        BasicFrame  frame;



        if (token == kGosubToken)
        {
            frame.kind = BasicFrameKind::Gosub;
            frame.line = PeekWord ((Word) (at + 1));
            at         = (Word) (at + kGosubBytes);
        }
        else if (token == kForToken)
        {
            Word  value  = PeekWord ((Word) (at + 1));
            bool  isDown = PeekByte ((Word) (at + 8)) == 0xFF;
            std::array<Byte, 5>  step   = { PeekByte ((Word) (at + 3)), PeekByte ((Word) (at + 4)), PeekByte ((Word) (at + 5)),
                                            PeekByte ((Word) (at + 6)), PeekByte ((Word) (at + 7)) };



            step[1]        = (Byte) ((step[1] & ~kTypeBit) | (isDown ? kTypeBit : 0));
            frame.kind     = BasicFrameKind::For;
            frame.variable = GetName (PeekByte ((Word) (value - 2)), PeekByte ((Word) (value - 1)), BasicType::Real);
            frame.step     = FormatRealByRom (step);
            frame.limit    = FormatPacked ((Word) (at + 9));
            frame.line     = PeekWord ((Word) (at + 14));
            at             = (Word) (at + kForBytes);
        }
        else
        {
            break;
        }

        frames.push_back (frame);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::FormatReal
//
//  Nine significant digits, trailing zeros dropped, no zero before the point,
//  and an exponent outside .01 to 999999999: as PRINT writes 1/3 (.333333333),
//  1E10 (1E+10) and .001 (1E-03).
//
////////////////////////////////////////////////////////////////////////////////

std::string ApplesoftProgram::FormatReal (const std::array<Byte, 5> & bytes)
{
    uint32_t     mantissa = (uint32_t) ((bytes[1] | kTypeBit) << 24 | bytes[2] << 16 | bytes[3] << 8 | bytes[4]);
    bool         isMinus  = (bytes[1] & kTypeBit) != 0;
    double       value    = std::ldexp ((double) mantissa, bytes[0] - 128 - 32);
    std::string  text     = std::format ("{:.8e}", value);
    std::string  digits   = text.substr (0, 1) + text.substr (2, kDigits - 1);
    int          exponent = std::stoi (text.substr (text.find ('e') + 1));
    std::string  result   = isMinus ? "-" : "";



    if (bytes[0] == 0)
    {
        return "0";
    }

    while (digits.size() > 1 && digits.back() == '0')
    {
        digits.pop_back();
    }

    if (exponent < kFixedFirst || exponent > kFixedLast)
    {
        return result + digits.substr (0, 1) + ((digits.size() > 1) ? "." + digits.substr (1) : std::string()) +
               std::format ("E{}{:02}", (exponent < 0) ? '-' : '+', std::abs (exponent));
    }

    if (exponent < 0)
    {
        return result + "." + std::string ((size_t) (-exponent - 1), '0') + digits;
    }

    digits.resize (std::max (digits.size(), (size_t) exponent + 1), '0');

    return result + digits.substr (0, (size_t) exponent + 1) +
           ((digits.size() > (size_t) exponent + 1) ? "." + digits.substr ((size_t) exponent + 1) : std::string());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::GetListRows
//
//  LIST's own layout: a space, the line number and a space, then the body,
//  continued on a new row five columns in once a row is 33 columns long. The
//  column is tested before each stored byte, so a keyword is never split.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ApplesoftProgram::GetListRows (const std::vector<BasicLine> & lines)
{
    std::vector<std::string>  rows;



    for (const BasicLine & line : lines)
    {
        std::string  row = std::format (" {} ", line.number);



        for (const std::string & piece : line.pieces)
        {
            if ((int) row.size() >= kListWrap)
            {
                rows.push_back (row);
                row = std::string (kListIndent, ' ');
            }

            row += piece;
        }

        rows.push_back (row);
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::PeekWord
//
////////////////////////////////////////////////////////////////////////////////

Word ApplesoftProgram::PeekWord (Word address) const
{
    return (Word) (PeekByte (address) | (PeekByte ((Word) (address + 1)) << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::FormatValue
//
//  An integer is two bytes, high first; a string a length and a pointer to
//  its characters.
//
////////////////////////////////////////////////////////////////////////////////

std::string ApplesoftProgram::FormatValue (Word address, BasicType type) const
{
    std::string  text;
    int          length  = 0;
    Word         chars   = 0;



    switch (type)
    {
    case BasicType::Integer:
        return std::to_string ((int16_t) ((PeekByte (address) << 8) | PeekByte ((Word) (address + 1))));

    case BasicType::String:
        length = PeekByte (address);
        chars  = PeekWord ((Word) (address + 1));

        for (int i = 0; i < length; i++)
        {
            text += (char) (PeekByte ((Word) (chars + i)) & 0x7F);
        }

        return text;

    default:
        return FormatPacked (address);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::FormatPacked
//
////////////////////////////////////////////////////////////////////////////////

std::string ApplesoftProgram::FormatPacked (Word address) const
{
    std::array<Byte, 5>  bytes = {};



    for (size_t i = 0; i < bytes.size(); i++)
    {
        bytes[i] = PeekByte ((Word) (address + i));
    }

    return FormatRealByRom (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::FormatRealByRom
//
//  The value unpacked into FOUT's accumulator, FOUT run over an overlay of
//  the machine, and its text read back. A machine without Applesoft's FOUT
//  at its address, or a run that does not return, falls back to FormatReal.
//
////////////////////////////////////////////////////////////////////////////////

std::string ApplesoftProgram::FormatRealByRom (const std::array<Byte, 5> & bytes) const
{
    static constexpr Byte  kFoutFirst[] = { 0xA0, 0x01, 0xA9, 0x2D };      // LDY #$01, LDA #'-'
    OverlayCpu             cpu  (m_peek);
    std::string            text;
    bool                   isFout = true;



    for (size_t i = 0; i < std::size (kFoutFirst); i++)
    {
        isFout = isFout && PeekByte ((Word) (kFout + i)) == kFoutFirst[i];
    }

    if (bytes[0] == 0 || !isFout)
    {
        return FormatReal (bytes);
    }

    cpu.Poke (kFac,      bytes[0]);
    cpu.Poke (kFac + 1,  (Byte) (bytes[1] | kTypeBit));
    cpu.Poke (kFac + 2,  bytes[2]);
    cpu.Poke (kFac + 3,  bytes[3]);
    cpu.Poke (kFac + 4,  bytes[4]);
    cpu.Poke (kFacSign,  ((bytes[1] & kTypeBit) != 0) ? 0xFF : 0x00);
    cpu.Poke (kFacExtra, 0);

    if (!cpu.TryCall (kFout, kFoutSteps))
    {
        return FormatReal (bytes);
    }

    for (Word at = kFoutText; text.size() < kFoutLength && cpu.ReadByteSlow (at) != 0; at++)
    {
        text += (char) (cpu.ReadByteSlow (at) & 0x7F);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::GetType
//
//  The high bits of the name's two bytes: both set for an integer, the
//  second alone for a string, neither for a real.
//
////////////////////////////////////////////////////////////////////////////////

BasicType ApplesoftProgram::GetType (Byte first, Byte second)
{
    bool  isFirst  = (first  & kTypeBit) != 0;
    bool  isSecond = (second & kTypeBit) != 0;



    return (isFirst && isSecond) ? BasicType::Integer : isSecond ? BasicType::String : BasicType::Real;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::IsFunction
//
//  DEF FN's entries have the first byte's high bit alone.
//
////////////////////////////////////////////////////////////////////////////////

bool ApplesoftProgram::IsFunction (Byte first, Byte second)
{
    return (first & kTypeBit) != 0 && (second & kTypeBit) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgram::GetName
//
////////////////////////////////////////////////////////////////////////////////

std::string ApplesoftProgram::GetName (Byte first, Byte second, BasicType type)
{
    std::string  name (1, (char) (first & 0x7F));



    if ((second & 0x7F) != 0)
    {
        name += (char) (second & 0x7F);
    }

    return name + ((type == BasicType::Integer) ? "%" : (type == BasicType::String) ? "$" : "");
}
