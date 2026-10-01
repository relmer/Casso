#include "Pch.h"

#include "Ui/Debugger/SourceSyntax.h"

#include "Directive.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::Colors::Get
//
////////////////////////////////////////////////////////////////////////////////

uint32_t SourceSyntax::Colors::Get (Token token) const
{
    switch (token)
    {
    case Token::Mnemonic:   return mnemonic;
    case Token::Directive:  return directive;
    case Token::Symbol:     return symbol;
    case Token::Number:     return number;
    case Token::String:     return string;
    case Token::Comment:    return comment;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::IsMnemonic
//
//  The 65C02's mnemonics, the bit instructions with their bit number.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsMnemonic (const std::wstring & word)
{
    static const std::set<std::wstring>  s_kMnemonics =
    {
        L"ADC", L"AND", L"ASL", L"BCC", L"BCS", L"BEQ", L"BIT", L"BMI", L"BNE", L"BPL", L"BRA", L"BRK",
        L"BVC", L"BVS", L"CLC", L"CLD", L"CLI", L"CLV", L"CMP", L"CPX", L"CPY", L"DEC", L"DEX", L"DEY",
        L"EOR", L"INC", L"INX", L"INY", L"JMP", L"JSR", L"LDA", L"LDX", L"LDY", L"LSR", L"NOP", L"ORA",
        L"PHA", L"PHP", L"PHX", L"PHY", L"PLA", L"PLP", L"PLX", L"PLY", L"ROL", L"ROR", L"RTI", L"RTS",
        L"SBC", L"SEC", L"SED", L"SEI", L"STA", L"STP", L"STX", L"STY", L"STZ", L"TAX", L"TAY", L"TRB",
        L"TSB", L"TSX", L"TXA", L"TXS", L"TYA", L"WAI",
    };
    std::wstring  upper;



    for (wchar_t ch : word)
    {
        upper += (wchar_t) towupper (ch);
    }

    //  BBR0 to BBS7, RMB0 to SMB7.
    if (upper.size() == 4 && upper[3] >= L'0' && upper[3] <= L'7')
    {
        std::wstring  stem = upper.substr (0, 3);

        return stem == L"BBR" || stem == L"BBS" || stem == L"RMB" || stem == L"SMB";
    }

    return s_kMnemonics.contains (upper);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::GetSourceRuns
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SourceSyntax::Run> SourceSyntax::GetSourceRuns (const std::wstring & line, Assembler assembler)
{
    return (assembler == Assembler::Merlin) ? GetMerlinRuns (line) : GetFieldRuns (line, assembler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::GetFieldRuns
//
//  as65's and ca65's reading, and the one for an unknown assembler.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SourceSyntax::Run> SourceSyntax::GetFieldRuns (const std::wstring & line, Assembler assembler)
{
    std::vector<Run>  runs;
    size_t            n        = line.size();
    size_t            i        = 0;
    bool              isOpcode = true;



    if (n == 0)
    {
        return runs;
    }

    //  A star in the first column is a comment, unless it sets the origin.
    if (assembler == Assembler::Any && line[0] == L'*')
    {
        size_t  next = line.find_first_not_of (L' ', 1);

        if (next == std::wstring::npos || line[next] != L'=')
        {
            runs.push_back ({ 0, (int) n, Token::Comment });
            return runs;
        }
    }

    while (i < n)
    {
        size_t        end   = 0;
        std::wstring  word;

        if (line[i] == L' ')
        {
            i++;
            continue;
        }

        if (line[i] == L';')
        {
            runs.push_back ({ (int) i, (int) (n - i), Token::Comment });
            break;
        }

        if (!isOpcode)
        {
            AddOperandRuns (line, i, n, assembler, runs);
            break;
        }

        end  = line.find_first_of (L" ;", i);
        end  = (end == std::wstring::npos) ? n : end;
        word = line.substr (i, end - i);

        //  An assignment in place of an opcode -- *= or = -- leaves the rest
        //  an operand.
        if (word[0] == L'*' || word[0] == L'=')
        {
            isOpcode = false;
            i        = (word.size() > 1 && word[1] == L'=') ? i + 2 : i + 1;
            continue;
        }

        //  A label: in the first column, unless it is an opcode written
        //  there, or anywhere when it ends in a colon.
        if (word.back() == L':' || (i == 0 && word[0] != L'.' && !IsMnemonic (word)))
        {
            runs.push_back ({ (int) i, (int) (end - i), Token::Symbol });
        }
        else
        {
            runs.push_back ({ (int) i, (int) (end - i), IsMnemonic (word) ? Token::Mnemonic : Token::Directive });
            isOpcode = false;
        }

        i = end;
    }

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::GetInstructionRuns
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SourceSyntax::Run> SourceSyntax::GetInstructionRuns (const std::wstring & instruction)
{
    std::vector<Run>  runs;
    size_t            start = instruction.find_first_not_of (L' ');
    size_t            end   = 0;



    if (start == std::wstring::npos)
    {
        return runs;
    }

    end = instruction.find (L' ', start);
    end = (end == std::wstring::npos) ? instruction.size() : end;

    runs.push_back ({ (int) start, (int) (end - start), Token::Mnemonic });
    AddOperandRuns (instruction, end, instruction.size(), Assembler::Any, runs);

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::AddOperandRuns
//
//  From `from` to a comment or `to`: strings, numbers and symbols. A quote
//  with no closing one colors only itself and the character after it, as
//  as65's 'A does. ca65's unnamed label references, :+ and :-, are symbols.
//
////////////////////////////////////////////////////////////////////////////////

void SourceSyntax::AddOperandRuns (const std::wstring & line, size_t from, size_t to, Assembler assembler, std::vector<Run> & runs)
{
    size_t  n = (std::min) (to, line.size());
    size_t  i = from;



    while (i < n)
    {
        wchar_t  ch  = line[i];
        size_t   end = i + 1;

        if (ch == L';')
        {
            runs.push_back ({ (int) i, (int) (n - i), Token::Comment });
            return;
        }

        if (ch == L'"' || ch == L'\'')
        {
            size_t  close = line.find (ch, i + 1);

            end = (close == std::wstring::npos) ? (std::min) (n, i + 2) : close + 1;
            runs.push_back ({ (int) i, (int) (end - i), Token::String });
        }
        else if ((ch == L'$' && end < n && iswxdigit (line[end])) ||
                 (ch == L'%' && end < n && (line[end] == L'0' || line[end] == L'1')) ||
                 iswdigit (ch))
        {
            while (end < n && iswalnum (line[end]))
            {
                end++;
            }

            runs.push_back ({ (int) i, (int) (end - i), Token::Number });
        }
        else if (assembler == Assembler::Ca65 && ch == L':' && end < n && (line[end] == L'+' || line[end] == L'-'))
        {
            while (end < n && line[end] == line[i + 1])
            {
                end++;
            }

            runs.push_back ({ (int) i, (int) (end - i), Token::Symbol });
        }
        else if (IsSymbolStart (ch, assembler))
        {
            std::wstring  word;

            while (end < n && IsSymbolChar (line[end]))
            {
                end++;
            }

            word = line.substr (i, end - i);

            //  An index register is not a symbol.
            if (word.size() != 1 || (towupper (ch) != L'X' && towupper (ch) != L'Y' && towupper (ch) != L'A'))
            {
                runs.push_back ({ (int) i, (int) (end - i), Token::Symbol });
            }
        }

        i = end;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::IsSymbolStart
//
//  Merlin's local labels start with a colon and its variables with ], ca65's
//  local labels with @.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsSymbolStart (wchar_t ch, Assembler assembler)
{
    switch (assembler)
    {
    case Assembler::As65:    return iswalpha (ch) || ch == L'_' || ch == L'.';
    case Assembler::Merlin:  return iswalpha (ch) || ch == L'_' || ch == L':' || ch == L']';
    case Assembler::Ca65:    return iswalpha (ch) || ch == L'_' || ch == L'.' || ch == L'@';
    case Assembler::Any:     break;
    }

    return iswalpha (ch) || ch == L'_' || ch == L'.' || ch == L'@' || ch == L']';
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::IsSymbolChar
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsSymbolChar (wchar_t ch)
{
    return iswalnum (ch) || ch == L'_' || ch == L'.' || ch == L'@' || ch == L']';
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::GetMerlinRuns
//
//  A Merlin line, its fields where the assembler's Merlin profile finds them.
//  The first column is always a label, and everything after the operand is the
//  comment, with or without a semicolon. A string directive's operand runs to
//  the next of whatever character opens it, and a HEX operand is all digits.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SourceSyntax::Run> SourceSyntax::GetMerlinRuns (const std::wstring & line)
{
    const DialectProfile  & merlin  = DialectRegistry::Get (DialectId::Merlin);
    std::string             narrow  = ToNarrow (line);
    ParsedLine              parsed  = merlin.ParseLine (narrow, 0);
    std::vector<Run>        runs;
    size_t                  n       = line.size();
    size_t                  comment = (parsed.commentColumn > 0) ? (size_t) parsed.commentColumn - 1 : n;
    size_t                  start   = 0;
    size_t                  end     = 0;
    std::wstring            opcode;
    Directive               token   = Directive::None;



    if (parsed.labelColumn > 0)
    {
        end = line.find_first_of (L" \t");
        end = (end == std::wstring::npos) ? n : end;
        runs.push_back ({ 0, (int) end, Token::Symbol });
    }

    if (parsed.mnemonicColumn > 0)
    {
        start  = (size_t) parsed.mnemonicColumn - 1;
        end    = line.find_first_of (L" \t", start);
        end    = (end == std::wstring::npos) ? n : end;
        opcode = line.substr (start, end - start);
        token  = merlin.GetDirectiveForSpelling (Parser::ToUpper (ToNarrow (opcode)));

        runs.push_back ({ (int) start, (int) (end - start), IsMnemonic (opcode) ? Token::Mnemonic : Token::Directive });
    }

    if (parsed.operandColumn > 0)
    {
        start = (size_t) parsed.operandColumn - 1;
        end   = line.find_last_not_of (L" \t", comment - 1) + 1;

        if (token == Directive::HexData)
        {
            runs.push_back ({ (int) start, (int) (end - start), Token::Number });
        }
        else if (token == Directive::StringData || token == Directive::Include)
        {
            size_t  close = line.find (line[start], start + 1);

            close = (token == Directive::Include || close == std::wstring::npos || close >= end) ? end : close + 1;
            runs.push_back ({ (int) start, (int) (close - start), Token::String });
            AddOperandRuns (line, close, end, Assembler::Merlin, runs);
        }
        else
        {
            AddOperandRuns (line, start, end, Assembler::Merlin, runs);
        }
    }

    if (comment < n)
    {
        runs.push_back ({ (int) comment, (int) (n - comment), Token::Comment });
    }

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::DetectAssembler
//
//  Merlin's and ca65's own directives and label forms count for each; the one
//  with more decides. A tie goes by the extension: ca65's .s, .inc and .mac,
//  and as65 for everything else. Neither cc65's debug file nor a Merlin listing
//  records the assembler, so the file's text is all there is to go on.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Assembler SourceSyntax::DetectAssembler (const std::vector<std::wstring> & lines, const std::wstring & fileName)
{
    int           merlin    = 0;
    int           ca65      = 0;
    size_t        dot       = fileName.find_last_of (L'.');
    std::wstring  extension = (dot == std::wstring::npos) ? std::wstring() : fileName.substr (dot);



    for (const std::wstring & line : lines)
    {
        std::wistringstream  words (line);
        std::wstring         word;

        if (line.empty())
        {
            continue;
        }

        if ((line[0] == L'*' && line.find (L'=') == std::wstring::npos) || line[0] == L']' ||
            (line[0] == L':' && line.size() > 1 && iswalpha (line[1])))
        {
            merlin++;
        }

        if (line[0] == L'@')
        {
            ca65++;
        }

        words >> word;

        //  The opcode follows a label in the first column.
        if (!iswspace (line[0]) || (!word.empty() && word.back() == L':'))
        {
            words >> word;
        }

        merlin += IsMerlinOnlyOpcode (word) ? 1 : 0;
        ca65   += IsCa65OnlyOpcode (word) ? 1 : 0;
    }

    if (merlin != ca65)
    {
        return (merlin > ca65) ? Assembler::Merlin : Assembler::Ca65;
    }

    for (wchar_t & ch : extension)
    {
        ch = (wchar_t) towlower (ch);
    }

    return (extension == L".s" || extension == L".inc" || extension == L".mac") ? Assembler::Ca65 : Assembler::As65;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::GetAssemblerLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * SourceSyntax::GetAssemblerLabel (Assembler assembler)
{
    switch (assembler)
    {
    case Assembler::Any:     return L"Automatic";
    case Assembler::As65:    return L"as65";
    case Assembler::Merlin:  return L"Merlin";
    case Assembler::Ca65:    return L"ca65";
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::IsMerlinOnlyOpcode
//
//  A directive Merlin has and as65 does not, such as ASC, HEX or LUP.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsMerlinOnlyOpcode (const std::wstring & word)
{
    std::string  upper = Parser::ToUpper (ToNarrow (word));



    if (upper.empty() || upper[0] == '.')
    {
        return false;
    }

    return DialectRegistry::Get (DialectId::Merlin).GetDirectiveForSpelling (upper) != Directive::None &&
           DialectRegistry::Get (DialectId::As65).GetDirectiveForSpelling (upper)   == Directive::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::IsCa65OnlyOpcode
//
//  A dotted directive as65 does not have, such as .proc or .import.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsCa65OnlyOpcode (const std::wstring & word)
{
    std::string  upper = Parser::ToUpper (ToNarrow (word));



    if (upper.size() < 2 || upper[0] != '.')
    {
        return false;
    }

    return DialectRegistry::Get (DialectId::As65).GetDirectiveForSpelling (upper) == Directive::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::ToNarrow
//
//  One character for each, so columns stay where they were; anything outside
//  ASCII becomes a question mark, which no assembler's grammar gives a meaning.
//
////////////////////////////////////////////////////////////////////////////////

std::string SourceSyntax::ToNarrow (const std::wstring & text)
{
    constexpr wchar_t  kAsciiEnd = 0x80;
    std::string        narrow;



    for (wchar_t ch : text)
    {
        narrow += (ch < kAsciiEnd) ? (char) ch : '?';
    }

    return narrow;
}
