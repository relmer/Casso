#include "Pch.h"

#include "ApplesoftTokenizer.h"
#include "Core/AppleSingleCodec.h"
#include "Debugger/SymbolFileReader.h"
#include "Ui/Debugger/DroppedFiles.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::GetKind
//
//  Merlin's sources end in .S, which the case-blind match takes with as65's
//  and ca65's .s.
//
////////////////////////////////////////////////////////////////////////////////

DroppedFiles::Kind DroppedFiles::GetKind (const std::wstring & path)
{
    if (HasExtension (path, { L".dbg", L".sym", L".lbl", L".vs" }))
    {
        return Kind::Symbols;
    }

    if (HasExtension (path, { L".a65", L".s", L".asm", L".inc", L".a", L".src", L".mac", L".65s", L".s65" }))
    {
        return Kind::Source;
    }

    if (IsBasicProgramFile (path))
    {
        return Kind::Basic;
    }

    return Kind::Other;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::GetOpening
//
////////////////////////////////////////////////////////////////////////////////

DroppedFiles::Opening DroppedFiles::GetOpening (const std::string & content)
{
    if (IsBinary (content))
    {
        return Opening::Hex;
    }

    if (IsBasicListing (content))
    {
        return Opening::Basic;
    }

    if (SymbolFileReader::Detect (content) != SymbolFileFormat::Unknown)
    {
        return Opening::Symbols;
    }

    return Opening::Text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::IsBasicListing
//
//  Each line that isn't blank starts with a line number above the line
//  before's, and some line has one of the statements a program can't do
//  without after its number.
//
////////////////////////////////////////////////////////////////////////////////

bool DroppedFiles::IsBasicListing (const std::string & content)
{
    static constexpr const char *  s_kpszStatements[] = { "REM", "PRINT", "GOTO", "GOSUB", "IF", "FOR", "NEXT", "HOME", "TEXT", "INPUT",
                                                          "POKE", "DIM", "END", "RETURN", "LET", "HGR", "GR", "DATA", "READ", "CALL" };
    std::istringstream             stream (content);
    std::string                    line;
    long                           last         = -1;
    int                            numbered     = 0;
    bool                           hasStatement = false;



    while (std::getline (stream, line))
    {
        size_t       at     = line.find_first_not_of (" \t");
        size_t       digits = 0;
        long         number = 0;
        std::string  word;

        if (at == std::string::npos || line[at] == '\r')
        {
            continue;
        }

        while (at + digits < line.size() && isdigit ((unsigned char) line[at + digits]) && digits < 6)
        {
            number = number * 10 + (line[at + digits] - '0');
            digits++;
        }

        if (digits == 0 || number <= last || at + digits >= line.size() || !isspace ((unsigned char) line[at + digits]))
        {
            return false;
        }

        last = number;
        numbered++;

        std::istringstream (line.substr (at + digits)) >> word;

        for (const char * statement : s_kpszStatements)
        {
            hasStatement = hasStatement || _stricmp (word.c_str(), statement) == 0;
        }
    }

    return numbered > 0 && hasStatement;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::IsBasicProgramFile
//
////////////////////////////////////////////////////////////////////////////////

bool DroppedFiles::IsBasicProgramFile (const std::wstring & path)
{
    std::wstring  name = std::filesystem::path (path).filename().wstring();



    std::ranges::transform (name, name.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });

    return name.ends_with (L".bas") || name.ends_with (L".bas.txt") || name.ends_with (L".as") || name.find (L"#fc") != std::wstring::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::TryGetBasicProgram
//
//  An AppleSingle file by its magic, whatever its name; a CiderPress #fc
//  file's bytes as they are; anything else is a listing. A ProDOS TXT file's
//  text has the high bit clear and lines ending in a return, which the
//  tokenizer takes.
//
////////////////////////////////////////////////////////////////////////////////

bool DroppedFiles::TryGetBasicProgram (const std::wstring & path, const std::vector<Byte> & bytes, std::vector<Byte> & tokenized, std::string & error)
{
    static constexpr Word  kBasType = 0x00FC;
    static constexpr Word  kTxtType = 0x0004;
    AppleSingleFile        single;
    ApplesoftListingError  listingError;
    std::string            text;
    std::wstring           name     = std::filesystem::path (path).filename().wstring();
    HRESULT                hr       = S_OK;



    std::ranges::transform (name, name.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });

    if (AppleSingleCodec::IsAppleSingle (bytes))
    {
        hr = AppleSingleCodec::Decode (bytes, single, error);

        if (FAILED (hr))
        {
            return false;
        }

        if (single.fileType == kBasType)
        {
            tokenized = single.data;
            return true;
        }

        if (single.fileType != kTxtType)
        {
            error = std::format ("The AppleSingle file is of ProDOS type ${:02X}, not a BAS or TXT file.", single.fileType);
            return false;
        }

        text.assign (single.data.begin(), single.data.end());
    }
    else if (name.find (L"#fc") != std::wstring::npos)
    {
        tokenized = bytes;
        return true;
    }
    else
    {
        text.assign (bytes.begin(), bytes.end());
    }

    std::ranges::transform (text, text.begin(), [] (char ch) { return (char) (ch & 0x7F); });

    hr = ApplesoftTokenizer::Tokenize (text, tokenized, listingError);

    if (FAILED (hr))
    {
        error = listingError.hasLineNumber ? std::format ("Line {}: {}", listingError.lineNumber, listingError.reason) : listingError.reason;
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::IsBinary
//
//  A NUL, or any control character a text file does not hold, in the first
//  few kilobytes.
//
////////////////////////////////////////////////////////////////////////////////

bool DroppedFiles::IsBinary (const std::string & content)
{
    static constexpr unsigned char  kFirstPrintable = 0x20;
    static constexpr unsigned char  kEscape         = 0x1B;
    size_t                          count           = std::min (content.size(), kBinarySample);



    for (size_t i = 0; i < count; i++)
    {
        unsigned char  ch = (unsigned char) content[i];

        if (ch < kFirstPrintable && ch != '\t' && ch != '\r' && ch != '\n' && ch != '\f' && ch != kEscape)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::FormatHexDump
//
////////////////////////////////////////////////////////////////////////////////

std::string DroppedFiles::FormatHexDump (const std::string & content)
{
    static constexpr unsigned char  kFirstPrintable = 0x20;
    static constexpr unsigned char  kLastPrintable  = 0x7E;
    std::string                     text;
    std::string                     chars;
    unsigned char                   ch              = 0;



    for (size_t line = 0; line < content.size(); line += kHexBytesPerLine)
    {
        chars.clear();
        text += std::format ("{:04X} ", line);

        for (size_t i = 0; i < kHexBytesPerLine; i++)
        {
            if (line + i >= content.size())
            {
                text += "   ";
                continue;
            }

            ch     = (unsigned char) content[line + i];
            text  += std::format (" {:02X}", ch);
            chars += (ch >= kFirstPrintable && ch <= kLastPrintable) ? (char) ch : '.';
        }

        text += "  " + chars + "\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::GetSymbolFilesBeside
//
//  The source's own name with a debug file's extension, then a symbol
//  table's.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DroppedFiles::GetSymbolFilesBeside (const std::wstring & sourcePath)
{
    std::filesystem::path  path = sourcePath;



    return { std::filesystem::path (path).replace_extension (L".dbg").wstring(),
             std::filesystem::path (path).replace_extension (L".sym").wstring() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DroppedFiles::HasExtension
//
////////////////////////////////////////////////////////////////////////////////

bool DroppedFiles::HasExtension (const std::wstring & path, std::initializer_list<const wchar_t *> extensions)
{
    std::wstring  extension = std::filesystem::path (path).extension().wstring();



    for (const wchar_t * known : extensions)
    {
        if (_wcsicmp (known, extension.c_str()) == 0)
        {
            return true;
        }
    }

    return false;
}
