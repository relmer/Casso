#include "Pch.h"

#include "Debugger/SymbolFileReader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::Detect
//
//  A Merlin listing is known by its symbol table heading. Otherwise the
//  first line that fits one of the line forms decides: NAME=$ADDR is a Casso
//  debug file, `al` leads a VICE label, and an address first is AppleWin's.
//
////////////////////////////////////////////////////////////////////////////////

SymbolFileFormat SymbolFileReader::Detect (const std::string & content)
{
    std::vector<std::string>  lines;



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
    bool                      hasSymbols = false;



    symbols.clear();
    format = Detect (content);
    SplitLines (content, lines);

    switch (format)
    {
    case SymbolFileFormat::CassoDebug:    ReadCasso         (lines, symbols); break;
    case SymbolFileFormat::MerlinListing: ReadMerlinListing (lines, symbols); break;
    case SymbolFileFormat::AppleWinSym:   ReadAppleWin      (lines, symbols); break;
    case SymbolFileFormat::ViceLabels:    ReadVice          (lines, symbols); break;
    default:
        error = "The file is not a symbol file: it is not a Casso debug file, a Merlin listing, an AppleWin .SYM file or a VICE label file.";
        break;
    }

    hasSymbols = !symbols.empty();
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
    return al == "al" && TryParseHex (address, value) && name.size() > 1 && name[0] == '.';
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader::HasMerlinHeading
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolFileReader::HasMerlinHeading (const std::string & line)
{
    std::string  upper (line);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return upper.find ("SYMBOL TABLE") != std::string::npos;
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
////////////////////////////////////////////////////////////////////////////////

void SymbolFileReader::AddUnique (std::vector<SymbolFileEntry> & symbols, const std::string & name, Word address)
{
    for (const SymbolFileEntry & existing : symbols)
    {
        if (existing.name == name)
        {
            return;
        }
    }

    symbols.push_back ({ name, address });
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
//  SymbolFileReader::ReadMerlinListing
//
//  Only the lines after the symbol table heading count, and each may hold
//  several NAME=$ADDR entries.
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



        if (HasMerlinHeading (line))
        {
            isInTable = true;
            continue;
        }

        if (!isInTable)
        {
            continue;
        }

        // Tokens pair up as NAME =$ADDR, NAME= $ADDR or NAME=$ADDR.
        while (stream >> token)
        {
            size_t  equals = token.find ('=');
            Word    value  = 0;



            if (equals == std::string::npos)
            {
                pending = token;
            }
            else if (equals == 0 && !pending.empty() && TryParseHex (token.substr (1), value))
            {
                AddUnique (symbols, pending, value);
                pending.clear();
            }
            else if (equals == token.size() - 1)
            {
                pending = token.substr (0, equals);
            }
            else if (equals > 0 && TryParseHex (token.substr (equals + 1), value))
            {
                AddUnique (symbols, token.substr (0, equals), value);
                pending.clear();
            }
            else if (!pending.empty() && equals == 0 && token.size() == 1)
            {
                continue;
            }
            else if (!pending.empty() && TryParseHex (token, value))
            {
                AddUnique (symbols, pending, value);
                pending.clear();
            }
        }
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

        if (IsViceLine (line) && TryParseHex (address, value))
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
