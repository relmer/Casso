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
    case Token::Address:    return address;
    case Token::Bytes:      return bytes;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::IsDirectiveLine
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsDirectiveLine (const std::wstring & line, Assembler assembler)
{
    std::string  upper;



    for (const Run & run : GetSourceRuns (line, assembler))
    {
        if (run.token == Token::Mnemonic)
        {
            return false;
        }

        if (run.token != Token::Directive)
        {
            continue;
        }

        upper = Parser::ToUpper (ToNarrow (line.substr ((size_t) run.start, (size_t) run.length)));

        if (upper[0] == '.')
        {
            return true;
        }

        return (assembler != Assembler::As65 && assembler != Assembler::Ca65 &&
                DialectRegistry::Get (DialectId::Merlin).GetDirectiveForSpelling (upper) != Directive::None) ||
               (assembler != Assembler::Merlin &&
                DialectRegistry::Get (DialectId::As65).GetDirectiveForSpelling (upper) != Directive::None);
    }

    return false;
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
//  Each assembler's own directives, label forms and origin count for it, and
//  the one with the most decides. Text with no clue, or with equal clues for
//  the most, is as65's, Casso's own assembler; the file's extension plays no
//  part, so a file with no clues reads the same whatever it is called.
//  Neither cc65's debug file nor a Merlin listing records the assembler, so
//  the file's text is all there is to go on.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Assembler SourceSyntax::DetectAssembler (const std::vector<std::wstring> & lines)
{
    int                        merlin  = 0;
    int                        ca65    = 0;
    int                        as65    = 0;
    int                        most    = 0;
    int                        leaders = 0;
    Listing                    listing = DetectListing (lines);
    std::vector<std::wstring>  sources;



    //  A listing's assembler is its source's, past the address and bytes.
    if (listing != Listing::None)
    {
        for (const std::wstring & line : lines)
        {
            sources.push_back (GetListingSource (line, listing));
        }

        return DetectAssembler (sources);

    }

    for (const std::wstring & line : lines)
    {
        std::wistringstream  words (line);
        std::wstring         word;
        size_t               first = line.find_first_not_of (L" \t");

        if (first == std::wstring::npos)
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

        //  as65's origin, *= or * =, which neither of the others has.
        if (line[first] == L'*' && line.find_first_not_of (L" \t", first + 1) != std::wstring::npos &&
            line[line.find_first_not_of (L" \t", first + 1)] == L'=')
        {
            as65++;
            continue;
        }

        words >> word;

        //  The opcode follows a label in the first column.
        if (!iswspace (line[0]) || (!word.empty() && word.back() == L':'))
        {
            words >> word;
        }

        merlin += IsMerlinOnlyOpcode (word) ? 1 : 0;
        ca65   += IsCa65OnlyOpcode (word) ? 1 : 0;
        as65   += IsAs65OnlyOpcode (word) ? 1 : 0;
    }

    most    = (std::max) ({ merlin, ca65, as65 });
    leaders = (merlin == most ? 1 : 0) + (ca65 == most ? 1 : 0) + (as65 == most ? 1 : 0);

    if (most == 0 || leaders > 1)
    {
        return Assembler::As65;
    }

    if (merlin == most)
    {
        return Assembler::Merlin;
    }

    return (ca65 == most) ? Assembler::Ca65 : Assembler::As65;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::GetLineRuns
//
//  A listing row's address and bytes, then the runs of the source past them,
//  moved to where that source starts. A line of a listing that is not one of
//  its rows, such as ca65's heading, is left uncolored.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SourceSyntax::Run> SourceSyntax::GetLineRuns (const std::wstring & line, Assembler assembler, Listing listing)
{
    std::vector<Run>  runs;
    bool              hasAddress = false;
    size_t            column     = GetListingSourceColumn (listing);
    size_t            bytesFrom  = 0;
    size_t            bytesTo    = 0;
    size_t            first      = 0;
    size_t            last       = 0;



    if (listing == Listing::None)
    {
        return GetSourceRuns (line, assembler);
    }

    if (listing == Listing::Merlin && IsMerlinListingRow (line, hasAddress))
    {
        constexpr size_t  kAddressLength = 4;
        constexpr size_t  kBytesFrom     = 5;
        constexpr size_t  kBytesTo       = 15;

        if (hasAddress)
        {
            runs.push_back ({ 0, (int) kAddressLength, Token::Address });
        }

        bytesFrom = kBytesFrom;
        bytesTo   = kBytesTo;
    }
    else if (listing == Listing::Ca65 && IsCa65ListingRow (line))
    {
        constexpr size_t  kAddressLength = 6;
        constexpr size_t  kBytesFrom     = 10;
        constexpr size_t  kBytesTo       = 23;

        runs.push_back ({ 0, (int) kAddressLength, Token::Address });
        bytesFrom = kBytesFrom;
        bytesTo   = kBytesTo;
    }
    else
    {
        return runs;
    }

    bytesTo = (std::min) (bytesTo, line.size());
    first   = line.find_first_not_of (L' ', bytesFrom);

    if (first != std::wstring::npos && first < bytesTo)
    {
        last = line.find_last_not_of (L' ', bytesTo - 1);
        runs.push_back ({ (int) first, (int) (last + 1 - first), Token::Bytes });
    }

    if (line.size() > column)
    {
        for (Run run : GetSourceRuns (line.substr (column), assembler))
        {
            run.start += (int) column;
            runs.push_back (run);
        }
    }

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::DetectListing
//
//  A listing when at least half of the lines that are not blank are one
//  listing's rows and at least one of them carries an address.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Listing SourceSyntax::DetectListing (const std::vector<std::wstring> & lines)
{
    int   text            = 0;
    int   merlinRows      = 0;
    int   merlinAddresses = 0;
    int   ca65Rows        = 0;
    bool  hasAddress      = false;



    for (const std::wstring & line : lines)
    {
        if (line.find_first_not_of (L" \t") == std::wstring::npos)
        {
            continue;
        }

        text++;

        if (IsMerlinListingRow (line, hasAddress))
        {
            merlinRows++;
            merlinAddresses += hasAddress ? 1 : 0;
        }

        ca65Rows += IsCa65ListingRow (line) ? 1 : 0;
    }

    if (ca65Rows > 0 && ca65Rows * 2 >= text)
    {
        return Listing::Ca65;
    }

    return (merlinAddresses > 0 && merlinRows * 2 >= text) ? Listing::Merlin : Listing::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::GetListingSource
//
//  Empty for a line that is not one of the listing's rows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceSyntax::GetListingSource (const std::wstring & line, Listing listing)
{
    bool    hasAddress = false;
    size_t  column     = GetListingSourceColumn (listing);
    bool    isRow      = (listing == Listing::Merlin) ? IsMerlinListingRow (line, hasAddress) :
                         (listing == Listing::Ca65)   ? IsCa65ListingRow (line) : false;



    if (listing == Listing::None)
    {
        return line;
    }

    return (isRow && line.size() > column) ? line.substr (column) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::GetListingSourceColumn
//
//  Merlin's source starts in column 22, past the address, its colon, up to
//  three bytes and the line number; ca65's in column 25, past the address,
//  the include depth and up to four bytes.
//
////////////////////////////////////////////////////////////////////////////////

size_t SourceSyntax::GetListingSourceColumn (Listing listing)
{
    constexpr size_t  kMerlinColumn = 21;
    constexpr size_t  kCa65Column   = 24;



    switch (listing)
    {
    case Listing::Merlin:  return kMerlinColumn;
    case Listing::Ca65:    return kCa65Column;
    case Listing::None:    break;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::IsMerlinListingRow
//
//  An address, a colon and its bytes, or blanks where they would be, then
//  the line number ending before the source's column; a line from a PUT or
//  USE file has a > ahead of its number.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsMerlinListingRow (const std::wstring & line, bool & hasAddress)
{
    constexpr size_t  kAddressLength = 4;
    constexpr size_t  kNumberFrom    = 15;
    constexpr size_t  kSourceColumn  = 21;
    size_t            i              = 0;
    size_t            digits         = 0;



    hasAddress = line.size() > kAddressLength && line[kAddressLength] == L':' &&
                 std::all_of (line.begin(), line.begin() + kAddressLength, [] (wchar_t ch) { return iswxdigit (ch) && !iswlower (ch); });

    if (line.size() <= kNumberFrom)
    {
        return false;
    }

    for (i = hasAddress ? kAddressLength + 1 : 0; i < kNumberFrom; i++)
    {
        if (line[i] != L' ' && !(hasAddress && iswxdigit (line[i])))
        {
            return false;
        }
    }

    while (i < kSourceColumn && i < line.size() && (line[i] == L' ' || line[i] == L'>'))
    {
        i++;
    }

    while (i < kSourceColumn && i < line.size() && iswdigit (line[i]))
    {
        i++;
        digits++;
    }

    return digits > 0 && (i == line.size() || line[i] == L' ');
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceSyntax::IsCa65ListingRow
//
//  Six hex digits of address, an r when it is relocatable, then the include
//  depth.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsCa65ListingRow (const std::wstring & line)
{
    constexpr size_t  kAddressLength = 6;
    size_t            i              = kAddressLength + 2;
    size_t            digits         = 0;



    if (line.size() <= i || (line[kAddressLength] != L'r' && line[kAddressLength] != L' ') || line[kAddressLength + 1] != L' ')
    {
        return false;
    }

    if (!std::all_of (line.begin(), line.begin() + kAddressLength, [] (wchar_t ch) { return iswxdigit (ch) && !iswlower (ch); }))
    {
        return false;
    }

    while (i < line.size() && iswdigit (line[i]))
    {
        i++;
        digits++;
    }

    return digits > 0 && (i == line.size() || line[i] == L' ');
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
//  SourceSyntax::IsAs65OnlyOpcode
//
//  A directive without a dot that as65 has and Merlin does not, such as
//  MACRO or ENDM. ca65 has no directive without a dot.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceSyntax::IsAs65OnlyOpcode (const std::wstring & word)
{
    std::string  upper = Parser::ToUpper (ToNarrow (word));



    if (upper.empty() || upper[0] == '.')
    {
        return false;
    }

    return DialectRegistry::Get (DialectId::As65).GetDirectiveForSpelling (upper)   != Directive::None &&
           DialectRegistry::Get (DialectId::Merlin).GetDirectiveForSpelling (upper) == Directive::None;
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
