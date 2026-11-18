#include "Pch.h"

#include "FileTypes/BusinessBasic.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool BusinessBasic::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kFileType) && bytes.size() >= 4 && bytes.size() <= kMaxBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::GetKeyword
//
//  A one-byte token's keyword, or null for a blank entry.
//
////////////////////////////////////////////////////////////////////////////////

const char * BusinessBasic::GetKeyword (Byte token)
{
    static constexpr const char *  s_kKeywords[128] =
    {
        "END", "FOR", "NEXT", "INPUT", "OUTPUT", "DIM", "READ", "WRITE",
        "OPEN", "CLOSE", nullptr, "TEXT", nullptr, "BYE", nullptr, nullptr,
        nullptr, nullptr, nullptr, "WINDOW", "INVOKE", "PERFORM", nullptr, nullptr,
        "FRE", "HPOS", "VPOS", "ERRLIN", "ERR", "KBD", "EOF", "TIME$",
        "DATE$", "PREFIX$", "EXFN.", "EXFN%.", "OUTREC", "INDENT", nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, "POP", "HOME", nullptr,
        "SUB$(", "OFF", "TRACE", "NOTRACE", "NORMAL", "INVERSE", "SCALE(", "RESUME",
        nullptr, "LET", "GOTO", "IF", "RESTORE", "SWAP", "GOSUB", "RETURN",
        "REM", "STOP", "ON", nullptr, "LOAD", "SAVE", "DELETE", "RUN",
        "RENAME", "LOCK", "UNLOCK", "CREATE", "EXEC", "CHAIN", nullptr, nullptr,
        nullptr, "CATALOG", nullptr, nullptr, "DATA", "IMAGE", "CAT", "DEF",
        nullptr, "PRINT", "DEL", "ELSE", "CONT", "LIST", "CLEAR", "GET",
        "NEW", "TAB", "TO", "SPC(", "USING", "THEN", nullptr, "MOD",
        "STEP", "AND", "OR", "EXTENSION", "DIV", nullptr, "FN", "NOT",
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    };



    return (token >= 0x80) ? s_kKeywords[token - 0x80] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::GetExtended
//
//  The keyword of the byte after $FF, by its low 7 bits, or null.
//
////////////////////////////////////////////////////////////////////////////////

const char * BusinessBasic::GetExtended (Byte token)
{
    static constexpr const char *  s_kKeywords[128] =
    {
        "TAB(", "TO", "SPC(", "USING", "THEN", nullptr, "MOD", "STEP",
        "AND", "OR", "EXTENSION", "DIV", nullptr, "FN", "NOT", nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, "AS", "SGN(", "INT(", "ABS(",
        nullptr, "TYP(", "REC(", nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, "PDL(", "BUTTON(", "SQR(",
        "RND(", "LOG(", "EXP(", "COS(", "SIN(", "TAN(", "ATN(", nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, "STR$(", "HEX$(", "CHR$(", "LEN(", "VAL(",
        "ASC(", "TEN(", nullptr, nullptr, "CONV(", "CONV&(", "CONV$(", "CONV%(",
        "LEFT$(", "RIGHT$(", "MID$(", "INSTR(", nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    };



    return s_kKeywords[token & 0x7F];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::IsWordEnd
//
//  Whether a space goes between this character and a keyword after it:
//  after a letter, a digit, a quote or a variable's type mark.
//
////////////////////////////////////////////////////////////////////////////////

bool BusinessBasic::IsWordEnd (char c)
{
    return isalnum ((unsigned char) c) || c == '"' || c == '$' || c == '%' || c == '&' || c == '.' || c == ')';
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::ListLine
//
//  A line's statements as text. A keyword has a space before it after a
//  word and a space after it before a word, unless it ends in an open
//  parenthesis. A REM's text and a string's are as stored. A token with a
//  blank entry is its value in braces.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BusinessBasic::ListLine (std::span<const Byte> content)
{
    std::string   text;
    const char  * keyword = nullptr;
    bool          spaced  = false;
    bool          quoted  = false;
    bool          remark  = false;
    Byte          value   = 0;



    for (size_t at = 0; at < content.size(); at++)
    {
        value = content[at];

        if (remark || quoted || value < 0x80)
        {
            char  c = (char) value;

            if (!remark && !quoted && spaced && c != ' ' && c != ':' && c != ',' && c != ';' && c != ')')
            {
                text.push_back (' ');
            }

            text.push_back ((c < 0x20 || c == 0x7F) ? '.' : c);

            quoted = (c == '"' && !remark) ? !quoted : quoted;
            spaced = false;
            continue;
        }

        if (value == kExtended && at + 1 < content.size())
        {
            at++;
            keyword = GetExtended (content[at]);
        }
        else
        {
            keyword = (value == kExtended) ? nullptr : GetKeyword (value);
        }

        if (!text.empty() && IsWordEnd (text.back()))
        {
            text.push_back (' ');
        }

        if (keyword == nullptr)
        {
            text += (value == kExtended && at < content.size()) ? std::format ("{{$FF ${:02X}}}", content[at]) : std::format ("{{${:02X}}}", value);
            spaced = false;
            continue;
        }

        text  += keyword;
        spaced = keyword[strlen (keyword) - 1] != '(';
        remark = value == kRem;
    }

    return TextEncoding::NarrowToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::Decode
//
//  Lines from offset 2, each its number and its statements, to a line
//  whose offset byte is 0. A line without its $00 ends the listing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BusinessBasic::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT       hr     = S_OK;
    size_t        at     = 2;
    size_t        next   = 0;
    size_t        end    = 0;
    Word          number = 0;
    std::wstring  line;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    while (at < bytes.size() && bytes[at] != 0)
    {
        next = at + bytes[at];

        if (at + 3 > bytes.size())
        {
            outFile.notes.push_back (L"The file ends in the middle of a line number.");
            break;
        }

        number = (Word) (bytes[at + 1] | (bytes[at + 2] << 8));
        end    = at + 3;

        while (end < bytes.size() && bytes[end] != 0)
        {
            end++;
        }

        if (end >= bytes.size())
        {
            outFile.notes.push_back (std::format (L"Line {} doesn't end before the end of the file.", number));
            break;
        }

        line = ListLine (bytes.subspan (at + 3, end - (at + 3)));
        outFile.lines.push_back (line.empty() ? std::to_wstring (number) : std::format (L"{} {}", number, line));

        if (next != end + 1)
        {
            outFile.notes.push_back (std::format (L"Line {}'s length byte doesn't match where the line ends.", number));
        }

        at = end + 1;
    }

    return hr;
}
