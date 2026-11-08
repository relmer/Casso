#include "Pch.h"

#include "Debugger/SymbolFileReader.h"
#include "Debugger/DebugFileReader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::Detect
//
//  A cc65 debug file is known by its version record, and a Merlin listing by
//  its symbol table heading. Otherwise the
//  first line that fits one of the line forms decides: NAME=$ADDR is a Casso
//  debug file, `al` leads a VICE label, and an address first is AppleWin's.
//
////////////////////////////////////////////////////////////////////////////////

SymbolFileFormat SymbolFileReader::Detect (const std::string & content)
{
    std::vector<std::string>  lines;



    if (DebugFileReader::IsDebugFile (content))
    {
        return SymbolFileFormat::Cc65Debug;
    }

    SplitLines (content, lines);

    for (const std::string & line : lines)
    {
        if (HasMerlinHeading (line))
        {
            return SymbolFileFormat::MerlinListing;
        }
    }

    for (const std::string & line : lines)
    {
        if (IsCassoLine (line))    { return SymbolFileFormat::CassoDebug; }
        if (IsViceLine (line))     { return SymbolFileFormat::ViceLabels; }
        if (IsAppleWinLine (line)) { return SymbolFileFormat::AppleWinSym; }
    }

    return SymbolFileFormat::Unknown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SymbolFileReader::Read (const std::string & content, std::vector<SymbolFileEntry> & symbols, SymbolFileFormat & format, std::string & error)
{
    HRESULT                   hr         = S_OK;
    std::vector<std::string>  lines;
    DebugFile                 debugFile;
    bool                      hasSymbols = false;



    symbols.clear();
    format = Detect (content);
    SplitLines (content, lines);

    switch (format)
    {
    case SymbolFileFormat::CassoDebug:    ReadCasso         (lines, symbols); break;

    case SymbolFileFormat::Cc65Debug:
        hr = DebugFileReader::Read (content, debugFile, error);
        CHR (hr);
        ReadCc65 (content, debugFile, symbols);
        break;

    case SymbolFileFormat::MerlinListing: ReadMerlinListing (lines, symbols); break;
    case SymbolFileFormat::AppleWinSym:   ReadAppleWin      (lines, symbols); break;
    case SymbolFileFormat::ViceLabels:    ReadVice          (lines, symbols); break;
    default:
        error = "The file is not a symbol file: it is not a Casso or cc65 debug file, a Merlin listing, an AppleWin .SYM file or a VICE label file.";
        break;
    }

    hasSymbols = !symbols.empty();

    if (!hasSymbols && error.empty())
    {
        error = "The file holds no symbols.";
    }

    CBREx (hasSymbols, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::SplitLines
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::SplitLines (const std::string & content, std::vector<std::string> & lines)
{
    size_t  start = 0;



    while (start < content.size())
    {
        size_t  end = content.find ('\n', start);



        lines.push_back (Trim (content.substr (start, end == std::string::npos ? std::string::npos : end - start)));

        if (end == std::string::npos)
        {
            break;
        }

        start = end + 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::IsCassoLine
//
//  NAME=$ADDR, or NAME =$ADDR as a Merlin listing writes it.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolFileReader::IsCassoLine (const std::string & line)
{
    size_t  equals = line.find ('=');
    Word    value  = 0;



    if (equals == std::string::npos || equals == 0 || line[0] == ';')
    {
        return false;
    }

    return TryParseHex (Trim (line.substr (equals + 1)), value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::IsAppleWinLine
//
//  ADDR NAME, four hex digits then a name.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolFileReader::IsAppleWinLine (const std::string & line)
{
    size_t  space = line.find_first_of (" \t");
    Word    value = 0;



    if (space == std::string::npos || line[0] == ';')
    {
        return false;
    }

    return TryParseHex (line.substr (0, space), value) && !Trim (line.substr (space)).empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::IsViceLine
//
//  al ADDR .NAME
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolFileReader::IsViceLine (const std::string & line)
{
    std::istringstream  stream (line);
    std::string         al;
    std::string         address;
    std::string         name;
    Word                value = 0;



    stream >> al >> address >> name;
    return al == "al" && TryParseViceAddress (address, value) && name.size() > 1 && name[0] == '.';
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::TryParseViceAddress
//
//  VICE writes a label's address after the memory it is in, `C:0300` for the
//  computer's own; ld65's -Ln writes six digits, `000300`. Either form, or
//  bare hex, is taken when the address fits in 16 bits.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolFileReader::TryParseViceAddress (const std::string & text, Word & value)
{
    static constexpr size_t    kMaxDigits  = 6;
    static constexpr uint32_t  kMaxAddress = 0xFFFF;
    bool                       hasSpace    = text.size() > 2 && isalpha ((unsigned char) text[0]) && text[1] == ':';
    std::string                digits      = hasSpace ? text.substr (2) : text;
    uint32_t                   parsed      = 0;



    if (digits.empty() || digits.size() > kMaxDigits || digits.find_first_not_of ("0123456789ABCDEFabcdef") != std::string::npos)
    {
        return false;
    }

    parsed = (uint32_t) std::stoul (digits, nullptr, 16);

    if (parsed > kMaxAddress)
    {
        return false;
    }

    value = (Word) parsed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::HasMerlinHeading
//
//  Merlin heads its tables `Symbol table - alphabetical order:` and `Symbol
//  table - numerical order:`. The words alone are not enough: a comment in
//  any other format may mention a symbol table.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolFileReader::HasMerlinHeading (const std::string & line)
{
    std::string  upper (line);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return upper.starts_with ("SYMBOL TABLE -");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::TryParseHex
//
//  Up to four hex digits, with or without a leading $.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolFileReader::TryParseHex (const std::string & text, Word & value)
{
    std::string  digits = text.starts_with ('$') ? text.substr (1) : text;



    if (digits.empty() || digits.size() > kAddressDigits || digits.find_first_not_of ("0123456789ABCDEFabcdef") != std::string::npos)
    {
        return false;
    }

    value = (Word) std::stoul (digits, nullptr, 16);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::AddUnique
//
//  Names are compared without regard to case, as the symbol table compares
//  them, so `Loop` and `LOOP` are one symbol and the count the reader gives
//  is the count the table ends up holding.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::AddUnique (std::vector<SymbolFileEntry> & symbols, const std::string & name, Word address, bool isRelocatable)
{
    for (const SymbolFileEntry & existing : symbols)
    {
        if (_stricmp (existing.name.c_str(), name.c_str()) == 0)
        {
            return;
        }
    }

    symbols.push_back ({ name, address, isRelocatable });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::ReadCasso
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::ReadCasso (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols)
{
    for (const std::string & line : lines)
    {
        size_t  equals = line.find ('=');
        Word    value  = 0;



        if (IsCassoLine (line) && TryParseHex (Trim (line.substr (equals + 1)), value))
        {
            AddUnique (symbols, Trim (line.substr (0, equals)), value);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::ReadCc65
//
//  The symbols in a scope with no parent, which is the module's own; a
//  symbol with no scope at all is taken too. Local labels and the labels
//  macro expansions made sit in scopes below it.
//
//  THE ASSEMBLER'S OWN SYMBOLS STAY OUT. ca65 predefines some thirty CPU_
//  constants in every file and records them as defined on line 0, which no
//  source has; taking them put CPU_ISET_4510 on every STA $0400. An import
//  carries no value of its own and would sit at $0000. A label moves with a
//  load offset and an equate, a value, does not.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::ReadCc65 (const std::string & content, const DebugFile & file, std::vector<SymbolFileEntry> & symbols)
{
    std::set<int>  topLevel;
    std::set<int>  predefined;



    FindPredefinedSymbols (content, predefined);

    for (const DebugScope & scope : file.scopes)
    {
        if (scope.parent < 0)
        {
            topLevel.insert (scope.id);
        }
    }

    for (const DebugSymbol & symbol : file.symbols)
    {
        bool  isTopLevel = symbol.scope < 0 || topLevel.contains (symbol.scope);



        if (!isTopLevel || symbol.type == "imp" || predefined.contains (symbol.id))
        {
            continue;
        }

        AddUnique (symbols, symbol.name, (Word) symbol.value, symbol.type == "lab");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::FindPredefinedSymbols
//
//  The ids of the sym records whose definition is a line record on line 0,
//  where ca65 places the symbols it defines itself. A record's def may list
//  several lines, joined by +; the first is the definition.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::FindPredefinedSymbols (const std::string & content, std::set<int> & predefined)
{
    std::vector<std::string>  lines;
    std::set<int>             lineZero;



    SplitLines (content, lines);

    for (const std::string & line : lines)
    {
        int  id     = 0;
        int  number = 0;



        if (line.starts_with ("line\t") && TryGetRecordField (line, "id", id) && TryGetRecordField (line, "line", number) && number == 0)
        {
            lineZero.insert (id);
        }
    }

    for (const std::string & line : lines)
    {
        int  id  = 0;
        int  def = 0;



        if (line.starts_with ("sym\t") && TryGetRecordField (line, "id", id) && TryGetRecordField (line, "def", def) && lineZero.contains (def))
        {
            predefined.insert (id);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::TryGetRecordField
//
//  The decimal value of `key=` in a debug file record, up to the first
//  character that is not a digit.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolFileReader::TryGetRecordField (const std::string & record, const std::string & key, int & value)
{
    std::string  wanted = key + "=";
    size_t       at     = record.find ("\t" + wanted);
    size_t       end    = 0;



    if (at == std::string::npos)
    {
        at = record.find ("," + wanted);
    }

    if (at == std::string::npos)
    {
        return false;
    }

    at += wanted.size() + 1;
    end = record.find_first_not_of ("0123456789", at);

    if (end == at)
    {
        return false;
    }

    value = std::stoi (record.substr (at, end == std::string::npos ? std::string::npos : end - at));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::ReadMerlinListing
//
//  Only the lines after the symbol table heading count, and each may hold
//  several NAME=$ADDR entries.
//
//  Each entry starts with Merlin's flag column: E for an entry point, ? for
//  a symbol nothing refers to, M for a label a macro defined, and MD for a
//  macro's own name. The column is three wide and a name follows it
//  directly, so `MD?MOV` is the macro MOV flagged unreferenced, not a symbol
//  of that spelling.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::ReadMerlinListing (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols)
{
    bool  isInTable = false;



    for (const std::string & line : lines)
    {
        std::istringstream  stream (line);
        std::string         token;
        std::string         pending;
        bool                isAwaitingValue = false;



        if (HasMerlinHeading (line))
        {
            isInTable = true;
            continue;
        }

        if (!isInTable)
        {
            continue;
        }

        // Tokens pair up as NAME =$ADDR, NAME= $ADDR, NAME = $ADDR or
        // NAME=$ADDR. A flag token before a name is replaced by the name.
        while (stream >> token)
        {
            size_t  equals = 0;
            Word    value  = 0;



            StripMerlinFlags (token);
            equals = token.find ('=');

            if (isAwaitingValue && TryParseHex (token, value))
            {
                AddUnique (symbols, pending, value);
                pending.clear();
                isAwaitingValue = false;
                continue;
            }

            isAwaitingValue = false;

            if (equals == std::string::npos)
            {
                pending = token;
                continue;
            }

            if (equals > 0)
            {
                pending = token.substr (0, equals);
            }

            if (equals == token.size() - 1)
            {
                isAwaitingValue = !pending.empty();
                continue;
            }

            if (!pending.empty() && TryParseHex (token.substr (equals + 1), value))
            {
                AddUnique (symbols, pending, value);
                pending.clear();
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::StripMerlinFlags
//
//  A full flag column runs into the name: `MD?MOV` is the flags MD? and the
//  name MOV, and the token becomes MOV. A name whose third character is not
//  `?` never had flags joined to it, since the column's last flag is `?` or a
//  space.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::StripMerlinFlags (std::string & token)
{
    static constexpr size_t  kFlagColumn = 3;
    bool                     isJoined    = token.size() > kFlagColumn && token[kFlagColumn - 1] == '?' &&
                                           token[kFlagColumn] != '=' &&
                                           token.find_first_not_of ("EMDX", 0) == kFlagColumn - 1;



    if (isJoined)
    {
        token = token.substr (kFlagColumn);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::ReadAppleWin
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::ReadAppleWin (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols)
{
    for (const std::string & line : lines)
    {
        std::istringstream  stream (line);
        std::string         address;
        std::string         name;
        Word                value = 0;



        stream >> address >> name;

        if (IsAppleWinLine (line) && TryParseHex (address, value))
        {
            AddUnique (symbols, name, value);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::ReadVice
//
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::ReadVice (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols)
{
    for (const std::string & line : lines)
    {
        std::istringstream  stream (line);
        std::string         al;
        std::string         address;
        std::string         name;
        Word                value = 0;



        stream >> al >> address >> name;

        if (IsViceLine (line) && TryParseViceAddress (address, value))
        {
            AddUnique (symbols, name.substr (1), value);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::Trim
//
////////////////////////////////////////////////////////////////////////////////

std::string SymbolFileReader::Trim (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t\r");
    size_t  last  = text.find_last_not_of  (" \t\r");



    return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
}
