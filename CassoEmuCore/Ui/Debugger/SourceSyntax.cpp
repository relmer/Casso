#include "Pch.h"

#include "Ui/Debugger/SourceSyntax.h"





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

std::vector<SourceSyntax::Run> SourceSyntax::GetSourceRuns (const std::wstring & line)
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
    if (line[0] == L'*')
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
            AddOperandRuns (line, i, runs);
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
    AddOperandRuns (instruction, end, runs);

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::AddOperandRuns
//
//  From `from` to a comment or the end: strings, numbers and symbols. A quote
//  with no closing one colors only itself and the character after it, as
//  as65's 'A does.
//
////////////////////////////////////////////////////////////////////////////////

void SourceSyntax::AddOperandRuns (const std::wstring & line, size_t from, std::vector<Run> & runs)
{
    size_t  n = line.size();
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
        else if (IsSymbolStart (ch))
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
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsSymbolStart (wchar_t ch)
{
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
