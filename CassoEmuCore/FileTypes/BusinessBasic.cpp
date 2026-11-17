#include "Pch.h"

#include "FileTypes/BusinessBasic.h"
#include "FileTypes/AppleWorks.h"
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
//  Tokens $80 to $FE; a gap in the table is not a keyword.
//
////////////////////////////////////////////////////////////////////////////////

const char * BusinessBasic::GetKeyword (Byte token)
{
    static constexpr const char *  kKeywords[0x7F] =
    {
        "END",     "FOR",     "NEXT",    "INPUT",   "OUTPUT",  "DIM",     "READ",    "WRITE",       // $80
        "OPEN",    "CLOSE",   nullptr,   "TEXT",    nullptr,   "BYE",     nullptr,   nullptr,       // $88
        nullptr,   nullptr,   nullptr,   "WINDOW",  "INVOKE",  "PERFORM", nullptr,   nullptr,       // $90
        "FRE",     "HPOS",    "VPOS",    "ERRLIN",  "ERR",     "KBD",     "EOF",     "TIME$",       // $98
        "DATE$",   "PREFIX$", "EXFN.",   "EXFN%.",  "OUTREC",  "INDENT",  nullptr,   nullptr,       // $A0
        nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   "POP",     "HOME",    nullptr,       // $A8
        "SUB$(",   "OFF",     "TRACE",   "NOTRACE", "NORMAL",  "INVERSE", "SCALE(",  "RESUME",      // $B0
        nullptr,   "LET",     "GOTO",    "IF",      "RESTORE", "SWAP",    "GOSUB",   "RETURN",      // $B8
        "REM",     "STOP",    "ON",      nullptr,   "LOAD",    "SAVE",    "DELETE",  "RUN",         // $C0
        "RENAME",  "LOCK",    "UNLOCK",  "CREATE",  "EXEC",    "CHAIN",   nullptr,   nullptr,       // $C8
        nullptr,   "CATALOG", nullptr,   nullptr,   "DATA",    "IMAGE",   "CAT",     "DEF",         // $D0
        nullptr,   "PRINT",   "DEL",     "ELSE",    "CONT",    "LIST",    "CLEAR",   "GET",         // $D8
        "NEW",     "TAB",     "TO",      "SPC(",    "USING",   "THEN",    nullptr,   "MOD",         // $E0
        "STEP",    "AND",     "OR",      "EXTENSION", "DIV",   nullptr,   "FN",      "NOT",         // $E8
        nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,       // $F0
        nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,                  // $F8
    };



    return (token >= 0x80 && token < 0xFF) ? kKeywords[token - 0x80] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::GetExtended
//
//  The byte after $FF, its high bit masked off.
//
////////////////////////////////////////////////////////////////////////////////

const char * BusinessBasic::GetExtended (Byte token)
{
    static constexpr std::pair<Byte, const char *>  kExtendedWords[] =
    {
        { 0x00, "TAB(" },    { 0x01, "TO" },       { 0x02, "SPC(" },     { 0x03, "USING" },
        { 0x04, "THEN" },    { 0x06, "MOD" },      { 0x07, "STEP" },     { 0x08, "AND" },
        { 0x09, "OR" },      { 0x0A, "EXTENSION" },{ 0x0B, "DIV" },      { 0x0D, "FN" },
        { 0x0E, "NOT" },     { 0x1C, "AS" },       { 0x1D, "SGN(" },     { 0x1E, "INT(" },
        { 0x1F, "ABS(" },    { 0x21, "TYP(" },     { 0x22, "REC(" },     { 0x2D, "PDL(" },
        { 0x2E, "BUTTON(" }, { 0x2F, "SQR(" },     { 0x30, "RND(" },     { 0x31, "LOG(" },
        { 0x32, "EXP(" },    { 0x33, "COS(" },     { 0x34, "SIN(" },     { 0x35, "TAN(" },
        { 0x36, "ATN(" },    { 0x43, "STR$(" },    { 0x44, "HEX$(" },    { 0x45, "CHR$(" },
        { 0x46, "LEN(" },    { 0x47, "VAL(" },     { 0x48, "ASC(" },     { 0x49, "TEN(" },
        { 0x4C, "CONV(" },   { 0x4D, "CONV&(" },   { 0x4E, "CONV$(" },   { 0x4F, "CONV%(" },
        { 0x50, "LEFT$(" },  { 0x51, "RIGHT$(" },  { 0x52, "MID$(" },    { 0x53, "INSTR(" },
    };



    for (const auto & [code, word] : kExtendedWords)
    {
        if (code == (token & 0x7F))
        {
            return word;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::AppendWord
//
//  A keyword is set off by a space from the text before it, except at the
//  start or after punctuation or an operator.
//
////////////////////////////////////////////////////////////////////////////////

void BusinessBasic::AppendWord (std::string & ioLine, const char * keyword)
{
    char  last = ioLine.empty() ? ' ' : ioLine.back();



    if (strchr (" :(,;=+-*/<>^#", last) == nullptr)
    {
        ioLine += ' ';
    }

    ioLine += keyword;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::ListLine
//
//  A name, number, string or # after a keyword that ends in a letter is set
//  off by a space. A quoted string, and the rest of the line after REM, are
//  copied as they are. A token that isn't a keyword is written as its value
//  in braces.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BusinessBasic::ListLine (std::span<const Byte> content)
{
    std::string  line;
    bool         quoted  = false;
    bool         remark  = false;
    bool         spaced  = false;



    for (size_t at = 0; at < content.size(); at++)
    {
        Byte          b       = content[at];
        const char *  keyword = nullptr;

        if (quoted || remark || b < 0x80)
        {
            if (spaced && !quoted && !remark && (isalnum (b) || b == '"' || b == '#' || b == '.'))
            {
                line += ' ';
            }

            spaced  = false;
            quoted ^= (b == '"');
            line   += (char) (b & 0x7F);
            continue;
        }

        if (b == kExtended && at + 1 < content.size())
        {
            keyword = GetExtended (content[++at]);
        }
        else
        {
            keyword = GetKeyword (b);
        }

        if (keyword == nullptr)
        {
            line  += std::format ("{{${:02X}}}", b);
            spaced = false;
            continue;
        }

        AppendWord (line, keyword);
        remark = b == kRem;
        spaced = isalpha ((unsigned char) line.back()) != 0;
    }

    return TextEncoding::NarrowToWide (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BusinessBasic::Decode
//
//  A line's length counts its length byte, its number and its $00.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BusinessBasic::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    size_t  at    = 2;
    bool    ended = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    while (at < bytes.size())
    {
        size_t  length = bytes[at];

        if (length == 0)
        {
            ended = true;
            break;
        }

        if (length < 4 || at + length > bytes.size())
        {
            break;
        }

        {
            std::wstring  text = ListLine (bytes.subspan (at + 3, length - 4));

            outFile.lines.push_back (std::to_wstring (AppleWorks::ReadWord (bytes, at + 1)) + (text.empty() ? L"" : L" " + text));
        }

        at += length;
    }

    if (!ended)
    {
        outFile.notes.push_back (L"The program ends before its last line.");
    }

    return S_OK;
}
