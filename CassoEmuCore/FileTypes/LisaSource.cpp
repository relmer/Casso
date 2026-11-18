#include "Pch.h"

#include "FileTypes/LisaSource.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Listing::Tab
//
//  Spaces to the column, or one space when the text is already there.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Listing::Tab (int column)
{
    size_t  stop = (size_t) columns[column];



    if (text.size() < stop)
    {
        text.append (stop - text.size(), ' ');
    }
    else if (!text.empty() && text.back() != ' ')
    {
        text.push_back (' ');
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Listing::Put
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Listing::Put (char c)
{
    text.push_back (c);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Listing::Put
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Listing::Put (std::string_view value)
{
    text.append (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Listing::PutByte
//
//  A character of the file with bit 7 cleared; a control character is a
//  period.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Listing::PutByte (Byte value)
{
    char  c = (char) (value & 0x7F);



    text.push_back ((c < 0x20 || c == 0x7F) ? '.' : c);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Listing::EndLine
//
//  The line without its trailing spaces, as a line of the file.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Listing::EndLine()
{
    while (!text.empty() && text.back() == ' ')
    {
        text.pop_back();
    }

    file.lines.push_back (TextEncoding::NarrowToWide (text));
    text.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Listing::Note
//
//  A note is listed once, however many lines it applies to.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::Listing::Note (const std::wstring & note)
{
    if (std::find (file.notes.begin(), file.notes.end(), note) == file.notes.end())
    {
        file.notes.push_back (note);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Cursor::Take
//
//  The next byte, or 0 with `failed` set past the end.
//
////////////////////////////////////////////////////////////////////////////////

Byte LisaSource::Cursor::Take()
{
    if (!HasMore())
    {
        failed = true;
        return 0;
    }

    return bytes[at++];
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Cursor::TakeValue
//
//  A little-endian number of `count` bytes.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t LisaSource::Cursor::TakeValue (int count)
{
    uint32_t  value = 0;



    for (int i = 0; i < count; i++)
    {
        value |= (uint32_t) Take() << (8 * i);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word LisaSource::ReadWord (std::span<const Byte> bytes, size_t at)
{
    return (at + 1 < bytes.size()) ? (Word) (bytes[at] | (bytes[at + 1] << 8)) : (Word) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::GetVersion
//
//  The version a file's type, aux type and header fit, or None.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Version LisaSource::GetVersion (const FileFacts & facts, std::span<const Byte> bytes)
{
    size_t  size = bytes.size();



    if (facts.IsDos33() && facts.type == kDosAlternateB)
    {
        Word  end = ReadWord (bytes, 2);

        return (size >= 6 && end >= 6 && end <= size) ? Version::Two : Version::None;
    }

    if (!facts.HasProDosType (kProDosInt) || !facts.hasAuxType)
    {
        return Version::None;
    }

    if (facts.auxType >= 0x1000 && facts.auxType < 0x4000)
    {
        size_t  code    = ReadWord (bytes, 0);
        size_t  symbols = ReadWord (bytes, 2);
        bool    fits    = size >= 4 && symbols % 4 == 0 && symbols <= 512 * 8 && 4 + code + symbols <= size;

        return fits ? Version::Three : Version::None;
    }

    if (facts.auxType >= 0x4000 && facts.auxType < 0x6000 && size >= 16)
    {
        size_t  end   = ReadWord (bytes, 2);
        size_t  count = ReadWord (bytes, 4);
        bool    fits  = end >= 16 && end <= size && count * 2 <= end
                     && bytes[6] >= 1 && bytes[6] < 128
                     && bytes[7] >= 2 && bytes[7] < 128
                     && bytes[8] >= 3 && bytes[8] < 128;

        return fits ? Version::Four : Version::None;
    }

    return Version::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool LisaSource::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return GetVersion (facts, bytes) != Version::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion2
//
//  Lines from offset 4 to the data end at +2. A length of $FF ends them.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion2 (std::span<const Byte> bytes, Listing & out)
{
    size_t  end    = (std::min) ((size_t) ReadWord (bytes, 2), bytes.size());
    size_t  at     = 4;
    size_t  length = 0;



    out.columns[kMnemonic] = 9;
    out.columns[kOperand]  = 13;
    out.columns[kComment]  = 29;

    while (at < end)
    {
        length = bytes[at++];

        if (length == 0xFF)
        {
            if (end - at > 2)
            {
                out.Note (L"The file ends early, before its data length.");
            }

            break;
        }

        if (length == 0)
        {
            out.Note (L"A line has a length of 0; the lines after it aren't listed.");
            break;
        }

        length = (std::min) (length, end - at);
        DecodeLine2 (bytes.subspan (at, length), out);
        at += length;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeLine2
//
//  A comment line, or an optional label, a mnemonic token, an operand kind,
//  the operand's text and a comment after $BB. The line ends in $0D.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeLine2 (std::span<const Byte> line, Listing & out)
{
        static constexpr const char *  s_kMnemonics[128] =
        {
            "BGE", "BLT", "BMI", "BCC", "BCS", "BPL", "BNE", "BEQ",
            "BVS", "BVC", "BSB", "BNM", "BM1", "BNZ", "BIZ", "BIM",
            "BIP", "BIC", "BNC", "BRA", "BTR", "BFL", "BRK", "BKS",
            "CLV", "CLC", "CLD", "CLI", "DEX", "DEY", "INX", "INY",
            "NOP", "PHA", "PLA", "PHP", "PLP", "RTS", "RTI", "RSB",
            "RTN", "SEC", "SEI", "SED", "TAX", "TAY", "TSX", "TXA",
            "TXS", "TYA", "ADD", "CPR", "DCR", "INR", "SUB", "LDD",
            "POP", "PPD", "STD", "STP", "LDR", "STO", "SET", nullptr,
            "ADC", "AND", "ORA", "BIT", "CMP", "CPX", "CPY", "DEC",
            "EOR", "INC", "JMP", "JSR", nullptr, "LDA", "LDX", "LDY",
            "STA", "STX", "STY", "XOR", "LSR", "ROR", "ROL", "ASL",
            "ADR", "EQU", "ORG", "OBJ", "EPZ", "STR", "DCM", "ASC",
            "ICL", "END", "LST", "NLS", "HEX", "BYT", "HBY", "PAU",
            "DFS", "DCI", "...", "PAG", "INV", "BLK", "DBY", "TTL",
            "SBC", nullptr, "LET", ".IF", ".EL", ".FI", "=  ", "PHS",
            "DPH", ".DA", "GEN", "NOG", "USR", nullptr, nullptr, nullptr,
        };



    std::span<const Byte>  body;
    size_t                 at       = 0;
    Byte                   first    = 0;
    Byte                   token    = 0;
    Byte                   kind     = 0;
    const char           * mnemonic = nullptr;



    if (line.empty() || line.back() != kEndOfLine)
    {
        out.Note (L"A line doesn't end in a carriage return, and isn't listed.");
        return;
    }

    body  = line.first (line.size() - 1);
    first = body.empty() ? kEndOfLine : body[0];

    if (first == '*' || first == ';')
    {
        for (Byte value : body)
        {
            out.PutByte (value);
        }

        out.EndLine();
        return;
    }

    if ((first >= 'A' && first <= 'Z') || first == '^')
    {
        for (; at < body.size() && body[at] < 0x80; at++)
        {
            if (body[at] != ' ')
            {
                out.PutByte (body[at]);
            }
        }
    }
    else if (!body.empty() && first < 0x80)
    {
        out.Note (L"A line starts with a character that isn't a label, a comment or a mnemonic.");
    }

    if (at < body.size())
    {
        token    = body[at++];
        mnemonic = (token >= 0x80) ? s_kMnemonics[token - 0x80] : nullptr;

        out.Tab (kMnemonic);
        out.Put (mnemonic != nullptr ? mnemonic : "???");
        out.Put (' ');

        if (mnemonic == nullptr)
        {
            out.Note (L"A mnemonic token isn't in LISA's table.");
        }
    }

    if (at < body.size())
    {
        kind = body[at++];

        for (; kind != 0 && kind <= 0x20 && at < body.size() && body[at] < 0x80; at++)
        {
            out.PutByte (body[at]);
        }
    }

    if (at < body.size())
    {
        if (body[at] == kHighSemicolon)
        {
            out.Tab (kComment);
            out.Put (';');

            for (at++; at < body.size(); at++)
            {
                out.PutByte (body[at]);
            }
        }
        else
        {
            out.Note (L"A line has bytes after its operand that aren't a comment.");
        }
    }

    out.EndLine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutSymbol
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutSymbol (size_t index, const std::vector<std::string> & symbols, Listing & out)
{
    if (index >= symbols.size())
    {
        out.Put ('?');
        out.Note (L"A symbol's number is past the end of the symbol table.");
        return;
    }

    out.Put (symbols[index]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutNumber
//
//  A `count`-byte value in decimal ('d'), or with $ in hex ('$') or with %
//  in binary ('%'), the most significant byte first.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutNumber (Cursor & line, char radix, int count, Listing & out)
{
    uint32_t  value = line.TakeValue (count);



    if (radix == 'd')
    {
        out.Put (std::to_string (value));
        return;
    }

    out.Put (radix);

    for (int i = count - 1; i >= 0; i--)
    {
        if (radix == '$')
        {
            PutHex ((Byte) (value >> (8 * i)), out);
        }
        else
        {
            PutBits ((Byte) (value >> (8 * i)), out);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutHex
//
//  A byte as two uppercase hex digits.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutHex (Byte value, Listing & out)
{
    static constexpr const char *  kHexDigits = "0123456789ABCDEF";



    out.Put (kHexDigits[value >> 4]);
    out.Put (kHexDigits[value & 0xF]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutBits
//
//  A byte's eight bits, high bit first.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutBits (Byte value, Listing & out)
{
    for (int bit = 7; bit >= 0; bit--)
    {
        out.Put ((char) ('0' + ((value >> bit) & 1)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutString
//
//  A quoted string whose length is the token's low 5 bits, or the next byte
//  when they are 0. It is in double quotes when its first character has
//  bit 7 set, otherwise in single quotes, and a quote inside it is doubled.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Expect LisaSource::PutString (Cursor & line, Byte token, Listing & out)
{
    size_t  length = (token & 0x1F) != 0 ? (size_t) (token & 0x1F) : (size_t) line.Take();
    char    quote  = (line.Peek() & 0x80) ? '"' : '\'';
    char    c      = 0;



    if (length > line.GetLeft())
    {
        out.Note (L"A string is longer than its line.");
        return Expect::Failed;
    }

    out.Put (quote);

    for (size_t i = 0; i < length; i++)
    {
        c = (char) (line.Take() & 0x7F);

        if (c == quote)
        {
            out.Put (quote);
        }

        out.PutByte ((Byte) c);
    }

    out.Put (quote);
    return Expect::Operator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutRest
//
//  The rest of a line as a comment at the comment column.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutRest (Cursor & line, Listing & out)
{
    if (!line.HasMore())
    {
        return;
    }

    out.Tab (kComment);
    out.Put (';');

    while (line.HasMore())
    {
        out.PutByte (line.Take());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::ReadSymbols3
//
//  Eight-byte entries, each eight 6-bit characters packed into six bytes,
//  high bits first, and a value. A character of $20 ends a name; one below
//  $20 is a letter, its value plus $40.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::ReadSymbols3 (std::span<const Byte> table, std::vector<std::string> & outSymbols)
{
    for (size_t entry = 0; entry + 8 <= table.size(); entry += 8)
    {
        uint64_t     packed = 0;
        std::string  name;
        int          c      = 0;



        for (size_t i = 0; i < 6; i++)
        {
            packed = (packed << 8) | table[entry + i];
        }

        for (int shift = 42; shift >= 0; shift -= 6)
        {
            c = (int) ((packed >> shift) & 0x3F);

            if (c == 0x20)
            {
                break;
            }

            name.push_back ((char) ((c < 0x20) ? c + 0x40 : c));
        }

        outSymbols.push_back (std::move (name));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion3
//
//  The symbol table after the 4-byte header, then the code's lines, each a
//  flag byte that is a whole line by itself or the length of a long one.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion3 (std::span<const Byte> bytes, Listing & out)
{
    size_t                    codeLength  = ReadWord (bytes, 0);
    size_t                    tableLength = ReadWord (bytes, 2);
    std::vector<std::string>  symbols;
    Cursor                    code;
    Cursor                    line;
    Byte                      flag        = 0;



    ReadSymbols3 (bytes.subspan (4, tableLength), symbols);

    code.bytes = bytes.subspan (4 + tableLength, codeLength);

    while (code.HasMore())
    {
        flag = code.Take();

        if (flag == 0x00)
        {
            break;
        }

        if (flag < 0x80)
        {
            line.bytes  = code.bytes.subspan (code.at, (std::min) ((size_t) flag - 1, code.GetLeft()));
            line.at     = 0;
            line.failed = false;
            code.at    += line.bytes.size();

            DecodeLine3 (line, symbols, out);
            continue;
        }

        if (flag >= 0xFE)
        {
            out.Put ((flag == 0xFE) ? '*' : ';');
        }
        else if (flag >= 0xFC)
        {
            out.Tab (kMnemonic);
            out.Put ('/');
            PutSymbol (code.Take() | ((flag & 1) << 8), symbols, out);
        }
        else if (flag >= 0xFA)
        {
            PutSymbol (code.Take() | ((flag & 1) << 8), symbols, out);
            out.Put (':');
        }
        else if (flag >= 0xF0)
        {
            out.Put ('^');
            out.Put ((char) ('0' + flag - 0xF0));
            out.Put (':');
        }
        else
        {
            out.Tab (kMnemonic);
            PutMnemonic3 (flag, out);
        }

        out.EndLine();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutMnemonic3
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutMnemonic3 (Byte token, Listing & out)
{
        static constexpr const char *  s_kMnemonics[256] =
        {
            "add", "adc", "and", "cmp", "eor", "lda", "ora", "sbc",
            "sta", "sub", "xor", "asl", "dec", "inc", "lsr", "rol",
            "ror", ".if", "whl", "bra", "bcc", "bcs", "beq", "bfl",
            "bge", "blt", "bmi", "bne", "bpl", "btr", "bvc", "bvs",
            "jsr", "obj", "org", "phs", ".md", "fzr", "inp", "lcl",
            "rls", "bit", "cpx", "cpy", "jmp", "ldx", "ldy", "stx",
            "sty", "trb", "tsb", "stz", "=  ", "con", "epz", "equ",
            "set", ".da", "adr", "byt", "csp", "dby", "hby", "anx",
            "sbt", "ttl", "chn", "blk", "dci", "inv", "rvs", "msg",
            "str", "zro", "dfs", "hex", "usr", "sav", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            ".el", ".fi", ".me", ".we", "dph", "if1", "if2", "end",
            "exp", "gen", "lst", "nls", "nog", "nox", "pag", "pau",
            "nlc", "cnd", "   ", "asl", "lsr", "rol", "ror", "dec",
            "inc", "brk", "clc", "cld", "cli", "clv", "dex", "dey",
            "inx", "iny", "nop", "pha", "php", "pla", "plp", "rti",
            "rts", "sec", "sed", "sei", "tax", "tay", "tsx", "txa",
            "txs", "tya", "phx", "phy", "plx", "ply", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        };



    if (s_kMnemonics[token] == nullptr)
    {
        out.Put ("???");
        out.Note (L"A mnemonic token isn't in LISA's table.");
        return;
    }

    out.Put (s_kMnemonics[token]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeLine3
//
//  A long line: a comment, or an optional label, then a mnemonic or a
//  macro call, then the operand.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeLine3 (Cursor & line, const std::vector<std::string> & symbols, Listing & out)
{
    Byte  first    = line.Peek();
    Byte  mnemonic = 0;



    if (first >= 0xFE)
    {
        line.Take();
        out.Put ((first == 0xFE) ? '*' : ';');

        while (line.HasMore())
        {
            out.PutByte (line.Take());
        }

        out.EndLine();
        return;
    }

    if (first >= 0xFA && first < 0xFC)
    {
        line.Take();
        PutSymbol (line.Take() | ((first & 1) << 8), symbols, out);
    }
    else if (first >= 0xF0 && first < 0xFA)
    {
        line.Take();
        out.Put ('^');
        out.Put ((char) (first - 0xC0));
    }

    mnemonic = line.Take();
    out.Tab (kMnemonic);

    if (mnemonic == 0xFC || mnemonic == 0xFD)
    {
        out.Put ('/');
        PutSymbol (line.Take() | ((mnemonic & 1) << 8), symbols, out);
    }
    else if (mnemonic < 0xFE)
    {
        PutMnemonic3 (mnemonic, out);
    }

    PutOperand3 (line, mnemonic, symbols, out);
    out.EndLine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutOperand3
//
//  An address mode byte for the mnemonics that take one, then terms and
//  operators to the end of the line or a comment token, then the address
//  mode's closing text and the comment.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutOperand3 (Cursor & line, Byte mnemonic, const std::vector<std::string> & symbols, Listing & out)
{
    static constexpr const char *  kOperators = "+-*/&|^=<><%<><";
    static constexpr const char *  kSecond    = "           ==>";



    Byte    mode   = 0;
    Byte    token  = 0;
    Expect  expect = Expect::Term;



    if (mnemonic >= 0xFE)
    {
        PutRest (line, out);
        return;
    }

    if (mnemonic < 0xB0 && (mnemonic < 0x11 || (mnemonic >= 0x29 && mnemonic < 0x34)))
    {
        mode = line.Take();
    }

    out.Tab (kOperand);

    if (mode >= 0x10 && mode < 0x80)
    {
        out.Put ('(');
    }

    while (line.HasMore() && expect != Expect::End && expect != Expect::Failed)
    {
        token = line.Take();

        if (expect == Expect::Term)
        {
            expect = PutTerm3 (line, token, symbols, out);
        }
        else if (token < 0x0E)
        {
            out.Put (' ');
            out.Put (kOperators[token]);

            if (kSecond[token] != ' ')
            {
                out.Put (kSecond[token]);
            }

            out.Put (' ');
            expect = Expect::Term;
        }
        else if (token >= 0x20 && token < 0x30)
        {
            out.Put ('+');
            expect = PutTerm3 (line, (Byte) (token - 0x10), symbols, out);
        }
        else if (token == 0xFF)
        {
            expect = Expect::End;
        }
        else
        {
            out.Put (',');
            line.Back();
            expect = Expect::Term;
        }
    }

    if (expect == Expect::Failed)
    {
        return;
    }

    switch (mode)
    {
        case 0x04: out.Put (",X");  break;
        case 0x08: out.Put (",Y");  break;
        case 0x10: out.Put (")");   break;
        case 0x20: out.Put (",X)"); break;
        case 0x40: out.Put ("),Y"); break;
        default:                    break;
    }

    PutRest (line, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutTerm3
//
//  A v3 operand term: a prefix, a digit, a number, a symbol, a string or a
//  macro parameter. Returns whether a term or an operator is next.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Expect LisaSource::PutTerm3 (Cursor & line, Byte token, const std::vector<std::string> & symbols, Listing & out)
{
    switch (token)
    {
        case 0x0E: out.Put ('~'); return Expect::Term;
        case 0x0F: out.Put ('-'); return Expect::Term;
        case 0x3A: out.Put ('#'); return Expect::Term;
        case 0x3B: out.Put ('/'); return Expect::Term;
        case 0x3D: out.Put ('@'); return Expect::Term;
        case 0x1A: PutNumber (line, 'd', 1, out); return Expect::Operator;
        case 0x1B: PutNumber (line, 'd', 2, out); return Expect::Operator;
        case 0x1C: PutNumber (line, '$', 1, out); return Expect::Operator;
        case 0x1D: PutNumber (line, '$', 2, out); return Expect::Operator;
        case 0x1E: PutNumber (line, '%', 1, out); return Expect::Operator;
        case 0x3C: out.Put ('*'); return Expect::Operator;
        case 0xFF: return Expect::End;
        default:   break;
    }

    if (token == 0x1F)
    {
        //  Two bytes of binary in the order they are stored.
        out.Put ('%');
        PutBits (line.Take(), out);
        PutBits (line.Take(), out);
        return Expect::Operator;
    }

    if (token < 0x1A)
    {
        out.Put ((char) (0x30 + token));
    }
    else if (token >= 0x36 && token <= 0x39)
    {
        out.Put ((token <= 0x37) ? '#' : '/');
        PutSymbol (line.Take() | ((token & 1) << 8), symbols, out);
    }
    else if (token < 0x4A)
    {
        out.Put ('<');
        out.Put ((char) (token - 0x10));
    }
    else if (token < 0x50)
    {
        out.Put ('?');
        out.Put ((char) ('0' + token - 0x4A));
    }
    else if (token < 0x5A)
    {
        out.Put ('>');
        out.Put ((char) ('0' + token - 0x50));
    }
    else if (token < 0x60)
    {
        out.Put ('?');
        out.Put ((token == 0x5F) ? '#' : (char) (token - 0x24));

        return (token == 0x5E) ? Expect::Term : Expect::Operator;
    }
    else if (token < 0x80)
    {
        return PutString (line, token, out);
    }
    else if (token == 0xFA || token == 0xFB)
    {
        PutSymbol (line.Take() | ((token & 1) << 8), symbols, out);
    }

    return Expect::Operator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::ReadSymbols4
//
//  Each symbol is a length byte, skipped, then characters to a $00. A
//  character below $80 has bit 5 set.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::ReadSymbols4 (std::span<const Byte> table, size_t count, std::vector<std::string> & outSymbols, Listing & out)
{
    size_t  at = 0;



    for (size_t i = 0; i < count; i++)
    {
        std::string  name;



        if (at >= table.size())
        {
            out.Note (L"The symbol table ends before its last symbol.");
            break;
        }

        for (at++; at < table.size() && table[at] != 0x00; at++)
        {
            name.push_back ((char) (((table[at] < 0x80) ? (table[at] | 0x20) : table[at]) & 0x7F));
        }

        at++;
        outSymbols.push_back (std::move (name));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeVersion4
//
//  The columns and the symbol table from the header, then the code's lines
//  to the end of the file.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeVersion4 (std::span<const Byte> bytes, Listing & out)
{
    size_t                    tableEnd = ReadWord (bytes, 2);
    std::vector<std::string>  symbols;
    Cursor                    code;
    Cursor                    line;
    Byte                      flag     = 0;



    out.columns[kMnemonic] = bytes[6];
    out.columns[kOperand]  = bytes[7];
    out.columns[kComment]  = bytes[8];

    ReadSymbols4 (bytes.subspan (16, tableEnd - 16), ReadWord (bytes, 4), symbols, out);

    code.bytes = bytes.subspan (tableEnd);

    while (code.HasMore())
    {
        flag = code.Take();

        if (flag == 0x00)
        {
            break;
        }

        if (flag < 0x80)
        {
            line.bytes  = code.bytes.subspan (code.at, (std::min) ((size_t) flag - 1, code.GetLeft()));
            line.at     = 0;
            line.failed = false;
            code.at    += line.bytes.size();

            DecodeLine4 (line, symbols, out);
            continue;
        }

        if (flag == 0xFF || flag == 0xFE)
        {
            out.Put ((flag == 0xFE) ? '*' : ';');
        }
        else if (flag == 0xFC)
        {
            out.Tab (kMnemonic);
            out.Put ('_');
            PutSymbol (code.TakeValue (2), symbols, out);
        }
        else if (flag == 0xFA)
        {
            PutSymbol (code.TakeValue (2), symbols, out);
        }
        else if (flag == 0xFB)
        {
            out.Note (L"A line has the token $FB, which isn't known.");
        }
        else if (flag >= 0xF0 && flag < 0xFA)
        {
            out.Put ('^');
            out.Put ((char) ('0' + flag - 0xF0));
        }
        else if (flag < 0xF0)
        {
            out.Tab (kMnemonic);
            PutMnemonic4 (flag, out);
        }

        out.EndLine();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutMnemonic4
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutMnemonic4 (Byte token, Listing & out)
{
        static constexpr const char *  s_kMnemonics[256] =
        {
            "???", "add", "adc", "and", "cmp", "eor", "lda", "ora",
            "sbc", "sta", "sub", "xor", "asl", "dec", "inc", "lsr",
            "rol", "ror", ".if", "whl", ".go", "bra", "bcc", "bcs",
            "beq", "bfl", "bge", "blt", "bmi", "bne", "bpl", "btr",
            "bvc", "bvs", "obj", "org", "phs", ".db", "pea", "per",
            "brl", ".md", "far", "fdr", "fzr", "inp", "lcl", "rls",
            "bit", "cpx", "cpy", "ldx", "ldy", "stx", "sty", "trb",
            "tsb", "stz", "pei", "rep", "sep", "jmp", "jsr", "jml",
            "jsl", "mvn", "mvp", "= ", "con", "epd", "epz", "eql",
            "equ", "set", ".da", "adr", "byt", "csp", "dby", "hby",
            "bby", "anx", "chn", "icl", "lib", "lnk", "msg", "psm",
            "rlb", "sbt", "ttl", "dci", "rvs", "str", "zro", "dfs",
            "hex", "usr", "sav", ".tf", "seg", "cpu", ".entry", ".ref",
            ".group", ".deref", "long", nullptr, ".assume", nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ".table",
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            ".el", ".fi", ".me", ".we", ".la", ".lx", ".sa", ".sx",
            "dph", "if1", "if2", "end", "exp", "gen", "lst", "nls",
            "nog", "nox", "pag", "pau", "nlc", "cnd", "asl", "lsr",
            "rol", "ror", "dec", "inc", "mvn", "mvp", "brk", "clc",
            "cld", "cli", "clv", "dex", "dey", "inx", "iny", "nop",
            "pha", "php", "pla", "plp", "rti", "rts", "sec", "sed",
            "sei", "tax", "tay", "tsx", "txa", "txs", "tya", "phx",
            "phy", "plx", "ply", "cop", "phb", "phd", "phk", "plb",
            "pld", "rtl", "stp", "swa", "tad", "tas", "tcd", "tcs",
            "tda", "tdc", "tsa", "tsc", "txy", "tyx", "wai", "xba",
            "xce", ".proc", ".endp", ".table", ".endt", nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        };



    if (s_kMnemonics[token] == nullptr)
    {
        out.Put ("???");
        out.Note (L"A mnemonic token isn't in LISA's table.");
        return;
    }

    out.Put (s_kMnemonics[token]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::DecodeLine4
//
//  A long line: a comment, or an optional label, then a mnemonic or a
//  macro call, then the operand. A mnemonic of $90 and above has no
//  operand, only a comment after a comment token.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::DecodeLine4 (Cursor & line, const std::vector<std::string> & symbols, Listing & out)
{
    static constexpr const char *  kFullLine = "!*;";



    Byte  first    = line.Peek();
    Byte  mnemonic = 0;



    if (first >= 0xFD)
    {
        line.Take();
        out.Put (kFullLine[first - 0xFD]);

        while (line.HasMore())
        {
            out.PutByte (line.Take());
        }

        out.EndLine();
        return;
    }

    if (first == 0xFA)
    {
        line.Take();
        PutSymbol (line.TakeValue (2), symbols, out);
    }
    else if (first >= 0xF0 && first != 0xFC)
    {
        line.Take();
        out.Put ('^');
        out.Put ((char) (first - 0xC0));
    }

    mnemonic = line.Take();
    out.Tab (kMnemonic);

    if (mnemonic >= 0xFC)
    {
        out.Put ('_');
        PutSymbol (line.TakeValue (2), symbols, out);
        mnemonic = 0xFC;
    }
    else
    {
        PutMnemonic4 (mnemonic, out);
    }

    if (mnemonic >= 0x90 && mnemonic != 0xFC)
    {
        if (line.HasMore())
        {
            line.Take();
        }
    }
    else
    {
        PutOperand4 (line, mnemonic, symbols, out);
    }

    PutRest (line, out);
    out.EndLine();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutOperand4
//
//  An address mode byte for the mnemonics that take one, its opening
//  bracket, then terms and operators to the end of the line or a comment
//  token, then the address mode's closing text.
//
////////////////////////////////////////////////////////////////////////////////

void LisaSource::PutOperand4 (Cursor & line, Byte mnemonic, const std::vector<std::string> & symbols, Listing & out)
{
    static constexpr const char *  kClosing[15] = { "", "", "", ",X", ",X", ",X", "", ",S", ",Y", "),Y", ",X)", ")", ",S),Y", "]", "],Y" };



    Byte    mode   = 0;
    bool    moded  = mnemonic != 0xFC && (mnemonic < 0x12 || (mnemonic >= 0x30 && mnemonic <= 0x40));
    Expect  expect = Expect::Term;



    out.Tab (kOperand);

    if (moded)
    {
        mode = line.Take();

        if (mode >= 15)
        {
            out.Note (L"An operand's address mode isn't one of LISA's.");
            mode = 0;
        }
    }

    if (mode >= 9 && mode <= 12)
    {
        out.Put ('(');
    }
    else if (mode == 13 || mode == 14)
    {
        out.Put ('[');
    }

    while (line.HasMore() && expect != Expect::End && expect != Expect::Failed)
    {
        expect = (expect == Expect::Term) ? PutTerm4 (line, symbols, out) : PutOperator4 (line, symbols, out);
    }

    if (expect != Expect::Failed)
    {
        out.Put (kClosing[mode]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutTerm4
//
//  A v4 operand term: a symbol, one of the term table's characters, or a
//  value. Returns whether a term or an operator is next.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Expect LisaSource::PutTerm4 (Cursor & line, const std::vector<std::string> & symbols, Listing & out)
{
    Byte  token = line.Take();



    if (token == 0xFA)
    {
        PutSymbol (line.TakeValue (2), symbols, out);
        return Expect::Operator;
    }

    if (token == 0xFF)
    {
        return Expect::End;
    }

    if (token >= 0x80)
    {
        out.Put (',');
        out.Note (L"An operand has a token that isn't one of LISA's.");
        return Expect::Term;
    }

    if (s_kTermFirst[token] != ' ')
    {
        out.Put (s_kTermFirst[token]);

        if (s_kTermSecond[token] == '!')
        {
            return Expect::Term;
        }

        if (s_kTermSecond[token] != ' ')
        {
            out.Put (s_kTermSecond[token]);
        }

        return Expect::Operator;
    }

    return PutValue4 (line, token, symbols, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutOperator4
//
//  An operator between terms. A byte that isn't one starts the next term
//  after a comma.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Expect LisaSource::PutOperator4 (Cursor & line, const std::vector<std::string> & symbols, Listing & out)
{
    Byte  token = line.Take();
    char  first = (token < 0x80) ? s_kOperators[token] : ' ';



    if (token == 0xFF)
    {
        return Expect::End;
    }

    if (first == ' ')
    {
        out.Put (',');
        line.Back();
        return Expect::Term;
    }

    if (first == '!')
    {
        out.Put ('+');

        if (s_kTermSecond[token] != ' ')
        {
            out.Put (s_kTermSecond[token]);
            return Expect::Operator;
        }

        return PutValue4 (line, (Byte) (token - 0x10), symbols, out);
    }

    out.Put (' ');
    out.Put (first);

    if (s_kTermSecond[token] != ' ')
    {
        out.Put (s_kTermSecond[token]);
    }

    out.Put (' ');
    return Expect::Term;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::PutValue4
//
//  A number, a long value, a coerced symbol, a macro parameter or a string.
//
////////////////////////////////////////////////////////////////////////////////

LisaSource::Expect LisaSource::PutValue4 (Cursor & line, Byte token, const std::vector<std::string> & symbols, Listing & out)
{
    Byte    kind  = 0;
    size_t  count = 0;



    switch (token)
    {
        case 0x1A: PutNumber (line, 'd', 1, out); return Expect::Operator;
        case 0x1B: PutNumber (line, 'd', 2, out); return Expect::Operator;
        case 0x34: PutNumber (line, 'd', 3, out); return Expect::Operator;
        case 0x1C: PutNumber (line, '$', 1, out); return Expect::Operator;
        case 0x1D: PutNumber (line, '$', 2, out); return Expect::Operator;
        case 0x35: PutNumber (line, '$', 3, out); return Expect::Operator;
        case 0x1E: PutNumber (line, '%', 1, out); return Expect::Operator;
        case 0x1F: PutNumber (line, '%', 2, out); return Expect::Operator;
        case 0x36: PutNumber (line, '%', 3, out); return Expect::Operator;
        case 0x5E: out.Put ("?:");               return Expect::Term;
        default:   break;
    }

    if (token == 0x37 || token == 0x38)
    {
        if (line.Peek() == 0xFA)
        {
            line.Take();
        }

        PutSymbol (line.TakeValue (2), symbols, out);
        out.Put ((token == 0x37) ? ":A" : ":L");
        return Expect::Operator;
    }

    if (token != 0x33)
    {
        return PutString (line, token, out);
    }

    kind = line.Take();

    if (kind <= 2)
    {
        PutNumber (line, "d$%"[kind], 4, out);
        return Expect::Operator;
    }

    count = (kind <= 4) ? line.Take() : 0;

    if (kind > 4 || count > line.GetLeft())
    {
        out.Note (L"A long value in an operand isn't one of LISA's, or is longer than its line.");
        return Expect::Failed;
    }

    for (size_t i = 0; i < count; i++)
    {
        if (kind == 3)
        {
            PutHex (line.Take(), out);
        }
        else
        {
            out.PutByte (line.Take());
        }
    }

    return Expect::Operator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LisaSource::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LisaSource::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT  hr      = S_OK;
    Version  version = GetVersion (facts, bytes);
    bool     known   = version != Version::None;
    Listing  out     = { outFile };



    UNREFERENCED_PARAMETER (options);

    CBRFEx (known, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file isn't LISA source.");

    switch (version)
    {
        case Version::Two:   DecodeVersion2 (bytes, out); break;
        case Version::Three: DecodeVersion3 (bytes, out); break;
        default:             DecodeVersion4 (bytes, out); break;
    }

Error:
    return hr;
}
