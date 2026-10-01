#include "Pch.h"

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

    if (SymbolFileReader::Detect (content) != SymbolFileFormat::Unknown)
    {
        return Opening::Symbols;
    }

    return Opening::Text;
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
