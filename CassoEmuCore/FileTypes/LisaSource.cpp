#include "Pch.h"

#include "FileTypes/LisaSource.h"
#include "FileTypes/AppleWorks.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Context::Tab
//
//  Pads to the column's stop, or a space when the line is already past it.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Context::Tab (int column)
{
    size_t  stop = (size_t) stops[(size_t) column];



    if (line.size() < stop)
    {
        line.append (stop - line.size(), ' ');
    }
    else if (!line.empty())
    {
        line += ' ';
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Context::Append
//
//  A control character becomes a space.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Context::Append (char c)
{
    line += ((unsigned char) c < 0x20 || c == 0x7F) ? ' ' : c;
}



void LisaSource::Context::Append (const std::string & text)
{
    for (char c : text)
    {
        Append (c);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Context::AppendSymbol
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Context::AppendSymbol (size_t index)
{
    if (index >= symbols.size())
    {
        line += "!BAD SYM!";
        Note (std::format (L"Symbol {} isn't in the symbol table.", index));
        return;
    }

    line += symbols[index];
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Context::EndLine
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Context::EndLine()
{
    while (!line.empty() && line.back() == ' ')
    {
        line.pop_back();
    }

    file->lines.push_back (TextEncoding::NarrowToWide (line));
    line.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Context::Note
//
//  A note is added once, however many lines have the problem.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Context::Note (const std::wstring & note)
{
    if (std::find (file->notes.begin(), file->notes.end(), note) == file->notes.end())
    {
        file->notes.push_back (note);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::IsVersion3
//
//  The code's length and the symbol table's, at most 512 entries of 8
//  bytes, fit in the file.
//
////////////////////////////////////////////////////////////////////////////////

bool LisaSource::IsVersion3 (const FileFacts & facts, std::span<const Byte> bytes)
{
    size_t  code    = 0;
    size_t  symbols = 0;



    if (!facts.HasProDosType (kProDosInt) || !facts.hasAuxType || facts.auxType < 0x1000 || facts.auxType >= 0x4000 || bytes.size() < 6 || bytes.size() > kMaxBytes)
    {
        return false;
    }

    code    = AppleWorks::ReadWord (bytes, 0);
    symbols = AppleWorks::ReadWord (bytes, 2);

    return (symbols & 0x03) == 0 && symbols <= 512 * 8 && 4 + code + symbols <= bytes.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::IsVersion4
//
//  The symbol table ends inside the file, and the three column stops are
//  in order of their least values.
//
////////////////////////////////////////////////////////////////////////////////

bool LisaSource::IsVersion4 (const FileFacts & facts, std::span<const Byte> bytes)
{
    size_t  end   = 0;
    size_t  count = 0;



    if (!facts.HasProDosType (kProDosInt) || !facts.hasAuxType || facts.auxType < 0x4000 || facts.auxType >= 0x6000 || bytes.size() < 18 || bytes.size() > kMaxBytes)
    {
        return false;
    }

    end   = AppleWorks::ReadWord (bytes, 2);
    count = AppleWorks::ReadWord (bytes, 4);

    return end != 0 && end <= bytes.size() && count * 2 <= end
        && bytes[6] >= 1 && bytes[6] < 128 && bytes[7] >= 2 && bytes[7] < 128 && bytes[8] >= 3 && bytes[8] < 128;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Matches
//
//  A v2 file's second word is its length.
//
////////////////////////////////////////////////////////////////////////////////

bool LisaSource::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    if (facts.IsDos33() && facts.type == kDosAlternateB)
    {
        size_t  length = (bytes.size() >= 4) ? AppleWorks::ReadWord (bytes, 2) : 0;

        return bytes.size() <= kMaxBytes && length >= 7 && length <= bytes.size();
    }

    return IsVersion3 (facts, bytes) || IsVersion4 (facts, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::ReadString
//
//  A string in high ASCII is delimited by double quotes, in low ASCII by
//  single ones; a delimiter inside it is doubled.
//
////////////////////////////////////////////////////////////////////////////////

std::string LisaSource::ReadString (Context & context, size_t & offset, int & length, int count)
{
    char         delimiter = (context.At (offset) >= 0x80) ? '"' : '\'';
    std::string  text (1, delimiter);



    while (count-- > 0)
    {
        char  c = (char) (context.At (offset++) & 0x7F);

        if (c == delimiter)
        {
            text += delimiter;
        }

        text += c;
        length--;
    }

    return text + delimiter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::FormatValue
//
//  A number of `count` bytes, low byte first: decimal, or hex or binary from
//  its high byte down.
//
////////////////////////////////////////////////////////////////////////////////

std::string LisaSource::FormatValue (Context & context, char radix, int count, size_t & offset, int & length)
{
    std::string  text;
    uint32_t     value = 0;



    for (int i = 0; i < count; i++)
    {
        value |= (uint32_t) context.At (offset + (size_t) i) << (8 * i);
    }

    if (radix == 'd')
    {
        text = std::to_string (value);
    }
    else if (radix == 'h')
    {
        text = std::format ("${:0{}X}", value, count * 2);
    }
    else
    {
        text = "%";

        for (int bit = count * 8 - 1; bit >= 0; bit--)
        {
            text += ((value >> bit) & 1) ? '1' : '0';
        }
    }

    offset += (size_t) count;
    length -= count;

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion2
//
//  A length of $FF ends the file.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion2 (Context & context)
{
    size_t  end    = std::min<size_t> (AppleWorks::ReadWord (context.bytes, 2), context.bytes.size());
    size_t  offset = 4;



    context.stops = { 0, 9, 13, 29 };

    while (offset < end)
    {
        size_t  length = context.At (offset++);

        if (length == 0xFF)
        {
            if (end - offset > 2)
            {
                context.Note (L"The file ends early, at its end marker.");
            }

            break;
        }

        if (length == 0)
        {
            context.Note (L"A line has a length of 0.");
            break;
        }

        DecodeVersion2Line (context, offset, length);
        context.EndLine();
        offset += length;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion2Line
//
//  A line starting * or ; is a comment. A label's spaces are dropped; then
//  a mnemonic token, an address mode, the operand's text, and a high-ASCII
//  semicolon before a comment. A space follows each mnemonic, as LISA 2.5's
//  WRITE command adds one.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion2Line (Context & context, size_t offset, size_t length)
{
    static constexpr char  kOpcodes[] =
        "BGEBLTBMIBCCBCSBPLBNEBEQBVSBVCBSBBNMBM1BNZBIZBIMBIPBICBNCBRABTRBFLBRKBKS"
        "CLVCLCCLDCLIDEXDEYINXINYNOPPHAPLAPHPPLPRTSRTIRSBRTNSECSEISEDTAXTAYTSXTXA"
        "TXSTYAADDCPRDCRINRSUBLDDPOPPPDSTDSTPLDRSTOSET___ADCANDORABITCMPCPXCPYDEC"
        "EORINCJMPJSR___LDALDXLDYSTASTXSTYXORLSRRORROLASLADREQUORGOBJEPZSTRDCMASC"
        "ICLENDLSTNLSHEXBYTHBYPAUDFSDCI...PAGINVBLKDBYTTLSBC___LET.IF.EL.FI=  PHS"
        "DPH.DAGENNOGUSR_________";



    size_t  start = offset;
    char    c     = (char) context.At (offset);
    Byte    index = 0;
    Byte    mode  = 0;
    Byte    mark  = 0;



    if (context.At (offset + length - 1) != kEndOfLine)
    {
        context.Note (L"A line doesn't end in a carriage return.");
        return;
    }

    if (c == '*' || c == ';')
    {
        while (offset - start < length - 1)
        {
            context.Append ((char) context.At (offset++));
        }

        return;
    }

    if ((c >= 'A' && c <= 'Z') || c == '^')
    {
        while ((Byte) c < 0x80 && (Byte) c != kEndOfLine)
        {
            if (c != ' ')
            {
                context.Append (c);
            }

            c = (char) context.At (++offset);
        }
    }
    else if ((Byte) c < 0x80)
    {
        context.Note (L"A line starts with a character that isn't a label or a token.");
    }

    if ((Byte) c == kEndOfLine)
    {
        return;
    }

    context.Tab (kOpcode);
    index = context.At (offset++);

    if (index < 0x80)
    {
        context.Note (L"A mnemonic token is below $80.");
    }
    else
    {
        context.Append (std::string (kOpcodes + (index - 0x80) * 3, 3) + " ");
    }

    mode = context.At (offset++);

    if (mode != 0 && mode <= 0x20)
    {
        c = (char) context.At (offset);

        while ((Byte) c != kEndOfLine && (Byte) c < 0x80 && offset < context.bytes.size())
        {
            context.Append (c);
            c = (char) context.At (++offset);
        }
    }

    mark = context.At (offset++);

    if (mark == (';' | 0x80))
    {
        context.Tab (kComment);
        context.Append (';');

        while (offset - start < length - 1)
        {
            context.Append ((char) context.At (offset++));
        }
    }
    else if (mark != kEndOfLine)
    {
        context.Note (L"A comment marker isn't a high-ASCII semicolon.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion3
//
//  The header is the code's length and the symbol table's; each 8-byte
//  symbol packs eight 6-bit characters into its first six bytes. A line's
//  first byte below $80 is its length; $F0 to $F9 are local labels, $FA
//  and $FB a label, $FC and $FD a macro and $FE and $FF a full-line
//  comment; any other byte is a mnemonic alone.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion3 (Context & context)
{
    size_t  code    = AppleWorks::ReadWord (context.bytes, 0);
    size_t  table   = AppleWorks::ReadWord (context.bytes, 2);
    size_t  offset  = 4 + table;
    size_t  end     = std::min (offset + code, context.bytes.size());



    context.stops = { 0, 9, 18, 40 };

    for (size_t at = 4; at + 8 <= 4 + table; at += 8)
    {
        Byte         b[6] = {};
        int          packed[8];
        std::string  name;

        for (size_t i = 0; i < 6; i++)
        {
            b[i] = context.At (at + i);
        }

        packed[0] = b[0] >> 2;
        packed[1] = ((b[0] << 4) & 0x3C) | (b[1] >> 4);
        packed[2] = ((b[1] << 2) & 0x3C) | (b[2] >> 6);
        packed[3] = b[2] & 0x3F;
        packed[4] = b[3] >> 2;
        packed[5] = ((b[3] << 4) & 0x3C) | (b[4] >> 4);
        packed[6] = ((b[4] << 2) & 0x3C) | (b[5] >> 6);
        packed[7] = b[5] & 0x3F;

        for (int value : packed)
        {
            if (value == ' ')
            {
                break;
            }

            name += (char) ((value > ' ') ? value : (value | 0x40));
        }

        context.symbols.push_back (std::move (name));
    }

    while (offset < end)
    {
        Byte    flag   = context.At (offset++);
        size_t  length = 1;

        if (flag == 0)
        {
            break;
        }

        if (flag < 0x80)
        {
            length = flag;
            DecodeVersion3Line (context, offset, flag - 1);
        }
        else if (flag == 0xFF)
        {
            context.Append (';');
        }
        else if (flag == 0xFE)
        {
            context.Append ('*');
        }
        else if (flag >= 0xF0 && flag < 0xFA)
        {
            context.Append (std::format ("^{}:", flag - 0xF0));
        }
        else if (flag == 0xFA || flag == 0xFB)
        {
            context.AppendSymbol (context.At (offset) | ((size_t) (flag & 0x01) << 8));
            context.Append (':');
            length = 2;
        }
        else if (flag == 0xFC || flag == 0xFD)
        {
            context.Tab (kOpcode);
            context.Append ('/');
            context.AppendSymbol (context.At (offset) | ((size_t) (flag & 0x01) << 8));
            length = 2;
        }
        else
        {
            context.Tab (kOpcode);

            if (!PrintMnemonic3 (context, flag))
            {
                context.Note (L"A mnemonic token isn't in LISA's table.");
            }
        }

        context.EndLine();
        offset += length - 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PrintMnemonic3
//
//  LISA.MNEMONICS's tables: $00 to $4D the instructions with operands, and
//  from $B0 the directives and single-byte instructions.
//
////////////////////////////////////////////////////////////////////////////////

bool LisaSource::PrintMnemonic3 (Context & context, Byte value)
{
    static constexpr char  kWithOperands[] =
        "addadcandcmpeorldaorasbcstasubxor" "asldecinclsrrolror"
        ".ifwhlbrabccbcsbeqbflbgebltbmibnebplbtrbvcbvsjsrobjorgphs"
        ".mdfzrinplclrls" "bitcpxcpyjmpldxldystxstytrbtsbstz" "=  conepzequset"
        ".daadrbytcspdbyhby" "anxsbtttlchnblkdciinvrvsmsgstrzro" "dfshexusrsav";
    static constexpr char  kSingles[] =
        ".el.fi.me.wedphif1if2endexpgenlstnlsnognoxpagpaunlccnd   "
        "asllsrrolrordecincbrkclccldcliclvdexdeyinxinynopphaphpplaplprti"
        "rtssecsedseitaxtaytsxtxatxstyaphxphyplxply";



    const char *  text = nullptr;

    if ((size_t) value * 3 + 3 <= sizeof (kWithOperands) - 1)
    {
        text = kWithOperands + (size_t) value * 3;
    }
    else if (value >= 0xB0 && (size_t) (value - 0xB0) * 3 + 3 <= sizeof (kSingles) - 1)
    {
        text = kSingles + (size_t) (value - 0xB0) * 3;
    }

    context.Append ((text != nullptr) ? std::string (text, 3) : std::string ("???"));

    return text != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion3Line
//
//  A line with a length: a comment; or a macro, a label or a local label,
//  then a mnemonic or a macro, then its operand.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion3Line (Context & context, size_t offset, int length)
{
    Byte  first    = context.At (offset);
    Byte  mnemonic = 0;



    if (first == 0xFE || first == 0xFF)
    {
        context.Append ((first == 0xFF) ? ';' : '*');

        while (--length > 0)
        {
            context.Append ((char) (context.At (++offset) & 0x7F));
        }

        return;
    }

    if (first == 0xFC || first == 0xFD)
    {
        context.Tab (kOpcode);
        context.Append ('/');
        context.AppendSymbol (context.At (offset + 1) | ((size_t) (first & 0x01) << 8));
        offset += 2;
        length -= 2;
        ConvertOperand3 (context, first, offset, length);
        return;
    }

    if (first == 0xFA || first == 0xFB)
    {
        context.AppendSymbol (context.At (offset + 1) | ((size_t) (first & 0x01) << 8));
        offset += 2;
        length -= 2;
    }
    else if (first >= 0xF0)
    {
        context.Append ('^');
        context.Append ((char) (first - 0xC0));
        offset++;
        length--;
    }

    mnemonic = context.At (offset++);
    length--;
    context.Tab (kOpcode);

    if (mnemonic >= 0xFC)
    {
        context.Append ('/');
        context.AppendSymbol (context.At (offset++) | ((size_t) (mnemonic & 0x01) << 8));
        length--;
    }
    else if (!PrintMnemonic3 (context, mnemonic))
    {
        context.Note (L"A mnemonic token isn't in LISA's table.");
    }

    ConvertOperand3 (context, mnemonic, offset, length);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::ConvertOperand3
//
//  Group 1 and 2 and group 5 instructions have an address mode byte; $10
//  to $7F open a parenthesis. The operand alternates operands and
//  operators: below $0E an operator, $20 to $2F a + with a digit, $FF a
//  comment, and anything else two operands in a row.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::ConvertOperand3 (Context & context, Byte mnemonic, size_t & offset, int & length)
{
    static constexpr char  kOperator1[] = "+-*/&|^=<>%<><";
    static constexpr char  kOperator2[] = "\0\0\0\0\0\0\0\0\0\0\0==>";



    int   mode         = 0;
    bool  wantOperator = false;



    if (mnemonic >= 0xFE)
    {
        PrintComment3 (context, 0, offset, length);
        return;
    }

    if (mnemonic < 0xB0 && (mnemonic < 0x11 || (mnemonic >= 0x29 && mnemonic < 0x34)))
    {
        mode = context.At (offset++);
        length--;
    }

    context.Tab (kOperand);

    if (mode >= 0x10 && mode < 0x80)
    {
        context.Append ('(');
    }

    while (length > 0)
    {
        Byte  value = 0;
        Next  next  = Next::Operator;

        if (!wantOperator)
        {
            value = context.At (offset++);
            length--;

            switch (value)
            {
                case 0x0E: context.Append ('~'); continue;
                case 0x0F: context.Append ('-'); continue;
                case 0x3A: context.Append ('#'); continue;
                case 0x3B: context.Append ('/'); continue;
                case 0x3D: context.Append ('@'); continue;
                default:                         break;
            }

            next = PrintNumber3 (context, mode, value, offset, length);

            if (next == Next::Failed)
            {
                return;
            }

            if (next == Next::Operand)
            {
                continue;
            }

            if (length <= 0)
            {
                break;
            }
        }

        wantOperator = false;
        value        = context.At (offset++);
        length--;

        if (value < 0x0E)
        {
            context.Append (' ');
            context.Append (kOperator1[value]);

            if (kOperator2[value] != '\0')
            {
                context.Append (kOperator2[value]);
            }

            context.Append (' ');
        }
        else if (value < 0x20 || value >= 0x30)
        {
            if (value == 0xFF)
            {
                PrintComment3 (context, mode, offset, length);
                return;
            }

            context.Append (',');
            offset--;
            length++;
        }
        else
        {
            context.Append ('+');
            next = PrintNumber3 (context, mode, (Byte) (value - 0x10), offset, length);

            if (next == Next::Failed)
            {
                return;
            }

            wantOperator = next == Next::Operator;
        }
    }

    PrintComment3 (context, mode, offset, length);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PrintNumber3
//
//  Below $1A a digit; $1A to $1F one or two bytes of decimal, hex or
//  binary; $36 to $39 an immediate symbol; $3C the location counter; $40
//  to $5F the < > and ? tokens; $60 to $7F a string, its length in the low
//  five bits or the next byte; $FA and $FB a symbol.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Next LisaSource::PrintNumber3 (Context & context, int mode, Byte value, size_t & offset, int & length)
{
    Next  next = Next::Operator;



    if (value < 0x1A)
    {
        context.Append ((char) ('0' | value));
    }
    else if (value == 0x1A || value == 0x1B)
    {
        context.Append (FormatValue (context, 'd', value - 0x19, offset, length));
    }
    else if (value == 0x1C || value == 0x1D)
    {
        context.Append (FormatValue (context, 'h', value - 0x1B, offset, length));
    }
    else if (value == 0x1E || value == 0x1F)
    {
        //  v3 writes a two-byte binary low byte first.
        context.Append ('%');

        for (int i = 0; i < value - 0x1D; i++)
        {
            Byte  b = context.At (offset++);

            for (int bit = 7; bit >= 0; bit--)
            {
                context.Append (((b >> bit) & 1) ? '1' : '0');
            }

            length--;
        }
    }
    else if (value >= 0x36 && value <= 0x39)
    {
        context.Append ((value <= 0x37) ? '#' : '/');
        context.AppendSymbol (context.At (offset++) | ((size_t) (value & 0x01) << 8));
        length--;
    }
    else if (value == 0x3C)
    {
        context.Append ('*');
    }
    else if (value < 0x4A)
    {
        context.Append ('<');
        context.Append ((char) (value - 0x10));
    }
    else if (value < 0x50)
    {
        context.Append ('?');
        context.Append ((char) (value - 0x1A));
    }
    else if (value < 0x5A)
    {
        context.Append ('>');
        context.Append ((char) (value - 0x20));
    }
    else if (value < 0x60)
    {
        int  shifted = value - 0x24;

        context.Append ('?');
        context.Append ((shifted == ';') ? '#' : (char) shifted);

        if (shifted == ':')
        {
            next = Next::Operand;
        }
    }
    else if (value < 0x80)
    {
        int  count = value & 0x1F;

        if (count == 0)
        {
            count = context.At (offset++);
            length--;
        }

        if (count > length)
        {
            context.Append ("!BAD STR!");
            context.Note (L"A string runs past the end of its line.");
            return Next::Failed;
        }

        context.line += ReadString (context, offset, length, count);
    }
    else if (value == 0xFA || value == 0xFB)
    {
        context.AppendSymbol (context.At (offset++) | ((size_t) (value & 0x01) << 8));
        length--;
    }
    else if (value == 0xFF)
    {
        PrintComment3 (context, mode, offset, length);
        offset += (size_t) std::max (length, 0);
        length  = 0;
    }

    return next;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PrintComment3
//
//  The address mode's close, then the rest of the line as a comment.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PrintComment3 (Context & context, int mode, size_t offset, int length)
{
    switch (mode)
    {
        case 0x04: context.Append (",X");  break;
        case 0x08: context.Append (",Y");  break;
        case 0x10: context.Append (")");   break;
        case 0x20: context.Append (",X)"); break;
        case 0x40: context.Append ("),Y"); break;
        default:                           break;
    }

    if (length > 0)
    {
        context.Tab (kComment);
        context.Append (';');

        while (length-- > 0)
        {
            context.Append ((char) (context.At (offset++) & 0x7F));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion4
//
//  The header is the version, the symbol table's end, the symbol count,
//  the three column stops and the CPU; the symbols start at 16, each a
//  length byte, its characters and a $00. A line's first byte below $80 is
//  its length; $F0 to $F9 are local labels, $FA a label, $FC a macro, $FD
//  a blank line, and $FE and $FF comments; any other byte is a mnemonic.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion4 (Context & context)
{
    size_t  end    = std::min<size_t> (AppleWorks::ReadWord (context.bytes, 2), context.bytes.size());
    size_t  count  = AppleWorks::ReadWord (context.bytes, 4);
    size_t  offset = 16;



    context.stops = { 0, context.At (6), context.At (7), context.At (8) };

    for (size_t symbol = 0; symbol < count; symbol++)
    {
        std::string  name;
        bool         first = true;
        Byte         b     = 0;

        if (offset >= end)
        {
            context.symbols.push_back ("!FAIL!");
            context.Note (L"The symbol table ends before its last symbol.");
            continue;
        }

        while (offset < end && (b = context.At (offset++)) != 0)
        {
            if (first)
            {
                first = false;
                continue;
            }

            name += (char) (((b < 0x80) ? (b | 0x20) : b) & 0x7F);
        }

        context.symbols.push_back (std::move (name));
    }

    offset = end;

    while (offset < context.bytes.size())
    {
        Byte    flag   = context.At (offset++);
        size_t  length = 1;

        if (flag == 0)
        {
            break;
        }

        if (flag < 0x80)
        {
            length = flag;
            DecodeVersion4Line (context, offset, flag - 1);
        }
        else if (flag == 0xFF)
        {
            context.Append (';');
        }
        else if (flag == 0xFE)
        {
            context.Append ('*');
        }
        else if (flag == 0xFD)
        {
        }
        else if (flag >= 0xF0 && flag < 0xFA)
        {
            context.Append (std::format ("^{}", flag - 0xF0));
        }
        else if (flag == 0xFA)
        {
            context.AppendSymbol (context.At (offset) | ((size_t) context.At (offset + 1) << 8));
            length = 3;
        }
        else if (flag == 0xFB)
        {
            context.Append ("??? ");
        }
        else if (flag == 0xFC)
        {
            context.Tab (kOpcode);
            context.Append ('_');
            context.AppendSymbol (context.At (offset) | ((size_t) context.At (offset + 1) << 8));
            length = 3;
        }
        else
        {
            context.Tab (kOpcode);
            PrintMnemonic4 (context, flag);
        }

        context.EndLine();
        offset += length - 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PrintMnemonic4
//
//  The v5.0a editor's table, with .assume at $6C and .table at $7F as
//  sample sources use them.
//
////////////////////////////////////////////////////////////////////////////////

bool LisaSource::PrintMnemonic4 (Context & context, Byte value)
{
    static constexpr const char *  kMnemonics[256] =
    {
        "???", "add", "adc", "and", "cmp", "eor", "lda", "ora", "sbc", "sta", "sub", "xor", "asl", "dec", "inc", "lsr",
        "rol", "ror", ".if", "whl", ".go", "bra", "bcc", "bcs", "beq", "bfl", "bge", "blt", "bmi", "bne", "bpl", "btr",
        "bvc", "bvs", "obj", "org", "phs", ".db", "pea", "per", "brl", ".md", "far", "fdr", "fzr", "inp", "lcl", "rls",
        "bit", "cpx", "cpy", "ldx", "ldy", "stx", "sty", "trb", "tsb", "stz", "pei", "rep", "sep", "jmp", "jsr", "jml",
        "jsl", "mvn", "mvp", "= ", "con", "epd", "epz", "eql", "equ", "set", ".da", "adr", "byt", "csp", "dby", "hby",
        "bby", "anx", "chn", "icl", "lib", "lnk", "msg", "psm", "rlb", "sbt", "ttl", "dci", "rvs", "str", "zro", "dfs",
        "hex", "usr", "sav", ".tf", "seg", "cpu", ".entry", ".ref", ".group", ".deref", "long", nullptr, ".assume", nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ".table",
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        ".el", ".fi", ".me", ".we", ".la", ".lx", ".sa", ".sx", "dph", "if1", "if2", "end", "exp", "gen", "lst", "nls",
        "nog", "nox", "pag", "pau", "nlc", "cnd", "asl", "lsr", "rol", "ror", "dec", "inc", "mvn", "mvp", "brk", "clc",
        "cld", "cli", "clv", "dex", "dey", "inx", "iny", "nop", "pha", "php", "pla", "plp", "rti", "rts", "sec", "sed",
        "sei", "tax", "tay", "tsx", "txa", "txs", "tya", "phx", "phy", "plx", "ply", "cop", "phb", "phd", "phk", "plb",
        "pld", "rtl", "stp", "swa", "tad", "tas", "tcd", "tcs", "tda", "tdc", "tsa", "tsc", "txy", "tyx", "wai", "xba",
        "xce", ".proc", ".endp", ".table", ".endt", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    };



    if (kMnemonics[value] == nullptr)
    {
        context.Append ("!BAD MNEMONIC!");
        context.Note (L"A mnemonic token isn't in LISA's table.");
        return false;
    }

    context.Append (kMnemonics[value]);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion4Line
//
//  A line with a length: a comment or an error line; or a macro, a label or
//  a local label, then a mnemonic or a macro, then its operand. A mnemonic
//  from $90 has no operand, only a comment.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion4Line (Context & context, size_t offset, int length)
{
    Byte  first    = context.At (offset);
    Byte  mnemonic = 0;



    if (first == 0xFF || first == 0xFE || first == 0xFD)
    {
        context.Append ((first == 0xFF) ? ';' : (first == 0xFE) ? '*' : '!');
        offset++;

        while (--length > 0)
        {
            context.Append ((char) (context.At (offset++) & 0x7F));
        }

        return;
    }

    if (first == 0xFC)
    {
        context.Tab (kOpcode);
        context.Append ('_');
        context.AppendSymbol (context.At (offset + 1) | ((size_t) context.At (offset + 2) << 8));
        offset += 3;
        length -= 3;
        ConvertOperand4 (context, first, offset, length);
        return;
    }

    if (first == 0xFA)
    {
        context.AppendSymbol (context.At (offset + 1) | ((size_t) context.At (offset + 2) << 8));
        offset += 3;
        length -= 3;
    }
    else if (first >= 0xF0)
    {
        context.Append ('^');
        context.Append ((char) (first - 0xC0));
        offset++;
        length--;
    }

    mnemonic = context.At (offset++);
    length--;
    context.Tab (kOpcode);

    if (mnemonic >= 0xFC)
    {
        context.Append ('_');
        context.AppendSymbol (context.At (offset) | ((size_t) context.At (offset + 1) << 8));
        offset += 2;
        length -= 2;
    }
    else
    {
        PrintMnemonic4 (context, mnemonic);

        if (mnemonic >= 0x90 && length > 0)
        {
            offset++;
            length--;
        }
    }

    ConvertOperand4 (context, mnemonic, offset, length);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::ConvertOperand4
//
//  Group 1 and 2, and group 5 but MVN and MVP, have an address mode byte,
//  which sets the operand's opening and closing characters. A token below
//  $80 with a first character in the operand table is a simple operand,
//  two characters or a unary operator; otherwise it's a number, a string or
//  a coerced symbol. Between operands, an operator in the operator table,
//  a + with a digit, or a comma before a second operand.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::ConvertOperand4 (Context & context, Byte mnemonic, size_t & offset, int & length)
{
    static constexpr char         kOperand1[] =
        "+-*/&|^=" "<>%<><~-" "01234567" "89\0\0\0\0\0\0" "++++++++" "++\0\0\0\0\0\0" "\0<>\0\0\0\0\0" "\0*@#/^|\\"
        "<<<<<<<<" "<<??????" ">>>>>>>>" ">>????\0?" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0";
    static constexpr char         kOperand2[] =
        "\0\0\0\0\0\0\0\0" "\0\0\0==>!!" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "01234567" "89\0\0\0\0\0\0" "\0<>\0\0\0\0\0" "\0\0!!!!!!"
        "01234567" "89012345" "01234567" "896789\0#" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0";
    static constexpr char         kOperator1[] =
        "+-*/&|^=" "<>%<><\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "!!!!!!!!" "!!!!!!!!" "\0<>\0\0\0\0\0" "\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0" "\0\0\0\0\0\0\0\0";
    static constexpr char         kOpen[16]   = { 0, 0, 0, 0, 0, 0, 0, 0, 0, '(', '(', '(', '(', '[', '[', 0 };
    static constexpr const char * kClose[16]  = { nullptr, nullptr, nullptr, ",X", ",X", ",X", nullptr, ",S", ",Y", "),Y", ",X)", ")", ",S),Y", "]", "],Y", nullptr };



    int   mode         = 0;
    bool  wantOperator = false;



    if (mnemonic == 0xFC || mnemonic < 0x90)
    {
        context.Tab (kOperand);

        if (mnemonic != 0xFC)
        {
            if (mnemonic < 0x12 || !(mnemonic < 0x30 || mnemonic == 0x41 || mnemonic == 0x42 || mnemonic >= 0x43))
            {
                mode = context.At (offset++);
                length--;
            }

            if (mode < 16 && kOpen[mode] != 0)
            {
                context.Append (kOpen[mode]);
            }
        }

        while (length > 0)
        {
            Byte  value = 0;

            if (!wantOperator)
            {
                value = context.At (offset++);
                length--;

                if (value >= 0x80)
                {
                    if (value == 0xFA)
                    {
                        context.AppendSymbol (context.At (offset) | ((size_t) context.At (offset + 1) << 8));
                        offset += 2;
                        length -= 2;
                    }
                    else if (value == 0xFF)
                    {
                        break;
                    }
                    else
                    {
                        context.Append ("!,");
                        continue;
                    }
                }
                else if (kOperand1[value] != '\0')
                {
                    context.Append (kOperand1[value]);

                    if (kOperand2[value] == '!')
                    {
                        continue;
                    }

                    if (kOperand2[value] != '\0')
                    {
                        context.Append (kOperand2[value]);
                    }
                }
                else
                {
                    Next  next = PrintComplex4 (context, value, offset, length);

                    if (next == Next::Failed)
                    {
                        break;
                    }

                    if (next == Next::Operand)
                    {
                        continue;
                    }
                }

                if (length <= 0)
                {
                    break;
                }
            }

            wantOperator = false;
            value        = context.At (offset++);
            length--;

            if (value >= 0x80 || kOperator1[value] == '\0')
            {
                if (value == 0xFF)
                {
                    break;
                }

                context.Append (',');
                offset--;
                length++;
            }
            else if (kOperator1[value] == '!')
            {
                context.Append ('+');

                if (kOperand2[value] != '\0')
                {
                    context.Append (kOperand2[value]);
                    wantOperator = true;
                }
                else
                {
                    wantOperator = PrintNumber4 (context, (Byte) (value - 0x10), offset, length) == Next::Operator;
                }
            }
            else
            {
                context.Append (' ');
                context.Append (kOperator1[value]);

                if (kOperand2[value] != '\0')
                {
                    context.Append (kOperand2[value]);
                }

                context.Append (' ');
            }
        }
    }

    if (mode != 0 && mode < 16 && kClose[mode] != nullptr)
    {
        context.Append (kClose[mode]);
    }

    if (length > 0)
    {
        context.Tab (kComment);
        context.Append (';');

        while (length-- > 0)
        {
            context.Append ((char) (context.At (offset++) & 0x7F));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PrintComplex4
//
//  $33 is followed by a class: a 4-byte decimal, hex or binary number, a
//  HEX directive's string of bytes, or an undelimited string. Any other
//  token is a number or a string.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Next LisaSource::PrintComplex4 (Context & context, Byte value, size_t & offset, int & length)
{
    Byte  kind  = 0;
    int   count = 0;



    if (value != 0x33)
    {
        return PrintNumber4 (context, value, offset, length);
    }

    kind = context.At (offset++);
    length--;

    switch (kind)
    {
        case 0x00: context.Append (FormatValue (context, 'd', 4, offset, length)); break;
        case 0x01: context.Append (FormatValue (context, 'h', 4, offset, length)); break;
        case 0x02: context.Append (FormatValue (context, 'b', 4, offset, length)); break;

        case 0x03:
        case 0x04:
            count = context.At (offset++);
            length--;

            if (count > length)
            {
                context.Append ("!BAD STR!");
                context.Note (L"A string runs past the end of its line.");
                return Next::Failed;
            }

            while (count-- > 0)
            {
                Byte  b = context.At (offset++);

                context.Append ((kind == 0x03) ? std::format ("{:02X}", b) : std::string (1, (char) (b & 0x7F)));
                length--;
            }

            break;

        default:
            context.Append ("!BAD CPLX OPRND!");
            context.Note (L"A complex operand's class isn't one LISA writes.");
            return Next::Failed;
    }

    return Next::Operator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PrintNumber4
//
//  $1A to $1F and $34 to $36 one to three bytes of decimal, hex or binary;
//  $37 and $38 a symbol coerced absolute or long; $5E a macro expression;
//  $60 to $7F a string.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Next LisaSource::PrintNumber4 (Context & context, Byte value, size_t & offset, int & length)
{
    int  count = 0;



    switch (value)
    {
        case 0x1A: context.Append (FormatValue (context, 'd', 1, offset, length)); break;
        case 0x1B: context.Append (FormatValue (context, 'd', 2, offset, length)); break;
        case 0x34: context.Append (FormatValue (context, 'd', 3, offset, length)); break;
        case 0x1C: context.Append (FormatValue (context, 'h', 1, offset, length)); break;
        case 0x1D: context.Append (FormatValue (context, 'h', 2, offset, length)); break;
        case 0x35: context.Append (FormatValue (context, 'h', 3, offset, length)); break;
        case 0x1E: context.Append (FormatValue (context, 'b', 1, offset, length)); break;
        case 0x1F: context.Append (FormatValue (context, 'b', 2, offset, length)); break;
        case 0x36: context.Append (FormatValue (context, 'b', 3, offset, length)); break;

        case 0x37:
        case 0x38:
            if (context.At (offset) == 0xFA)
            {
                offset++;
                length--;
            }

            context.AppendSymbol (context.At (offset) | ((size_t) context.At (offset + 1) << 8));
            context.Append ((value == 0x37) ? ":A" : ":L");
            offset += 2;
            length -= 2;
            break;

        case 0x5E:
            context.Append ("?:");
            return Next::Operand;

        default:
            if (value >= 0x80)
            {
                context.Append ("{CheckMoreOprnd}");
                break;
            }

            count = value & 0x1F;

            if (count == 0)
            {
                count = context.At (offset++);
                length--;
            }

            if (count > length)
            {
                context.Append ("!BAD STR!");
                context.Note (L"A string runs past the end of its line.");
                return Next::Failed;
            }

            context.line += ReadString (context, offset, length, count);
            break;
    }

    return Next::Operator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LisaSource::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    Context  context;



    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    context.bytes = bytes;
    context.file  = &outFile;

    if (facts.IsDos33())
    {
        DecodeVersion2 (context);
    }
    else if (facts.auxType < 0x4000)
    {
        DecodeVersion3 (context);
    }
    else
    {
        DecodeVersion4 (context);
    }

    if (!context.line.empty())
    {
        context.EndLine();
    }

    return S_OK;
}
