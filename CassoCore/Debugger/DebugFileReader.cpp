#include "Pch.h"

#include "Debugger/DebugFileReader.h"
#include "Debugger/SymbolFileReader.h"
#include "Sha1.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugFileReader::Read (std::string_view text, DebugFile & out, std::string & error)
{
    HRESULT    hr   = S_OK;
    DebugFile  file;
    size_t     at   = 0;



    out = DebugFile();
    error.clear();

    while (at < text.size())
    {
        std::string_view  line  = GetNextLine (text, at);
        size_t            split = line.find_first_of ("\t ");
        Fields            fields;



        if (line.empty() || split == std::string_view::npos)
        {
            continue;
        }

        SplitFields (line.substr (split + 1), fields);
        ReadRecord  (line.substr (0, split), fields, file);
    }

    hr = Validate (file, error);
    CHR (hr);

    out = std::move (file);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::IsDebugFile
//
////////////////////////////////////////////////////////////////////////////////

bool DebugFileReader::IsDebugFile (std::string_view text)
{
    size_t            at   = 0;
    std::string_view  line;



    while (at < text.size() && line.empty())
    {
        line = GetNextLine (text, at);
    }

    return line.starts_with ("version") && line.find ("major=") != std::string_view::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::ReadMerlinListing
//
//  One segment spanning memory, so a span's start is its address. A line
//  that carries more bytes than fit beside its source text continues on
//  lines with an address and bytes but no line number; those extend the span
//  of the line before. The record's line is the listing's own line, since
//  the listing is the text shown.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugFileReader::ReadMerlinListing (std::string_view text, const std::string & name, DebugFile & out, std::string & error)
{
    static constexpr uint32_t     kAddressSpace = 0x10000;
    HRESULT                       hr            = S_OK;
    DebugFile                     file;
    std::vector<SymbolFileEntry>  symbols;
    SymbolFileFormat              format        = SymbolFileFormat::Unknown;
    std::string                   symbolError;
    HRESULT                       hrSymbols     = SymbolFileReader::Read (std::string (text), symbols, format, symbolError);
    size_t                        at            = 0;
    int                           lineNumber    = 0;
    bool                          hasLines      = false;



    out = DebugFile();
    error.clear();

    file.major = kSupportedMajor;
    file.files.push_back ({ 0, name, text.size(), 0, Sha1::ComputeTextHex (text), 0 });
    file.modules.push_back ({ 0, name, 0 });
    file.segments.push_back ({ 0, "LISTING", 0, kAddressSpace });
    file.scopes.push_back ({ 0, "", 0, -1 });

    while (at < text.size())
    {
        std::string_view  line          = GetNextLine (text, at);
        uint32_t          address       = 0;
        uint32_t          count         = 0;
        bool              hasLineNumber = false;

        lineNumber++;

        if (!TryParseListingLine (line, address, count, hasLineNumber) || count == 0)
        {
            continue;
        }

        if (!hasLineNumber && !file.spans.empty() &&
            file.spans.back().start + file.spans.back().size == address)
        {
            file.spans.back().size += count;
            continue;
        }

        file.spans.push_back ({ (int) file.spans.size(), 0, address, count });
        file.lines.push_back ({ (int) file.lines.size(), 0, lineNumber, DebugLineType::Asm, 0, { file.spans.back().id } });
    }

    hasLines = !file.lines.empty();

    if (SUCCEEDED (hrSymbols))
    {
        for (const SymbolFileEntry & symbol : symbols)
        {
            file.symbols.push_back ({ (int) file.symbols.size(), symbol.name, symbol.address, 0, 0, "lab" });
        }
    }

    CBREx (hasLines, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    out = std::move (file);

Error:
    if (FAILED (hr))
    {
        error = "This Merlin listing has no line that assembled bytes.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::TryParseListingLine
//
//  `AAAA: bb bb bb   nnn  source`: four hex digits and a colon, then bytes
//  one space apart, then, two or more spaces on, the line number, with `>`
//  in front of it on a line from a PUT file or a macro. A line of bytes alone
//  continues the one before.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugFileReader::TryParseListingLine (std::string_view line, uint32_t & address, uint32_t & byteCount, bool & hasLineNumber)
{
    static constexpr size_t  kFirstByte = 6;
    uint64_t                 value      = 0;
    size_t                   pos        = kFirstByte;
    auto                     isHex      = [] (char c) { return std::isxdigit ((unsigned char) c) != 0; };



    byteCount     = 0;
    hasLineNumber = false;

    if (line.size() < kFirstByte || line[4] != ':' || !TryParseNumber (std::string ("0x") + std::string (line.substr (0, 4)), value))
    {
        return false;
    }

    address = (uint32_t) value;

    while (pos + 2 <= line.size() && line[pos - 1] == ' ' && isHex (line[pos]) && isHex (line[pos + 1]) &&
           (pos + 2 == line.size() || line[pos + 2] == ' '))
    {
        byteCount++;
        pos += 3;
    }

    while (pos < line.size() && line[pos] == ' ')
    {
        pos++;
    }

    //  A line from a PUT file or a macro expansion carries `>` before its number.
    if (pos < line.size() && line[pos] == '>')
    {
        pos++;
    }

    hasLineNumber = pos < line.size() && std::isdigit ((unsigned char) line[pos]) != 0;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::GetNextLine
//
//  The line starting at `at`, without its CR or LF, and `at` moved past it.
//
////////////////////////////////////////////////////////////////////////////////

std::string_view DebugFileReader::GetNextLine (std::string_view text, size_t & at)
{
    size_t            end  = text.find ('\n', at);
    std::string_view  line;



    if (end == std::string_view::npos)
    {
        end = text.size();
    }

    line = text.substr (at, end - at);
    at   = end + 1;

    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
    {
        line.remove_suffix (1);
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::SplitFields
//
//  key=value pairs separated by commas. A quoted value may hold commas and a
//  backslash-escaped quote, so the split walks the text rather than cutting
//  at every comma. A quoted value is kept without its quotes.
//
////////////////////////////////////////////////////////////////////////////////

void DebugFileReader::SplitFields (std::string_view pairs, Fields & out)
{
    size_t  at = 0;



    while (at < pairs.size())
    {
        size_t            equals = pairs.find ('=', at);
        std::string_view  key;
        std::string       value;

        if (equals == std::string_view::npos)
        {
            return;
        }

        key = pairs.substr (at, equals - at);
        at  = equals + 1;

        if (at < pairs.size() && pairs[at] == '"')
        {
            for (at++; at < pairs.size() && pairs[at] != '"'; at++)
            {
                if (pairs[at] == '\\' && at + 1 < pairs.size())
                {
                    at++;
                }

                value += pairs[at];
            }

            at++;
        }
        else
        {
            while (at < pairs.size() && pairs[at] != ',')
            {
                value += pairs[at++];
            }
        }

        out.emplace_back (key, std::move (value));

        if (at < pairs.size() && pairs[at] == ',')
        {
            at++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::FindField
//
////////////////////////////////////////////////////////////////////////////////

const std::string * DebugFileReader::FindField (const Fields & fields, std::string_view key)
{
    for (const auto & field : fields)
    {
        if (field.first == key)
        {
            return &field.second;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::TryParseNumber
//
//  Decimal, or hex with 0x in front, as cc65 writes both.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugFileReader::TryParseNumber (std::string_view text, uint64_t & value)
{
    int                     base   = 10;
    std::from_chars_result  result;



    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        text.remove_prefix (2);
        base = 16;
    }

    result = std::from_chars (text.data(), text.data() + text.size(), value, base);

    return result.ec == std::errc() && result.ptr == text.data() + text.size() && !text.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::GetNumber
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DebugFileReader::GetNumber (const Fields & fields, std::string_view key, uint64_t fallback)
{
    const std::string  * text  = FindField (fields, key);
    uint64_t             value = fallback;



    if (text == nullptr || !TryParseNumber (*text, value))
    {
        value = fallback;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::GetInt
//
////////////////////////////////////////////////////////////////////////////////

int DebugFileReader::GetInt (const Fields & fields, std::string_view key, int fallback)
{
    return (int) GetNumber (fields, key, (uint64_t) (int64_t) fallback);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::GetString
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugFileReader::GetString (const Fields & fields, std::string_view key)
{
    const std::string  * text = FindField (fields, key);



    return (text != nullptr) ? *text : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::GetIdList
//
//  Ids joined by '+', as a line lists its spans.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> DebugFileReader::GetIdList (const Fields & fields, std::string_view key)
{
    const std::string  * text = FindField (fields, key);
    std::vector<int>     ids;
    size_t               at   = 0;



    while (text != nullptr && at < text->size())
    {
        size_t    plus  = text->find ('+', at);
        uint64_t  value = 0;

        if (plus == std::string::npos)
        {
            plus = text->size();
        }

        if (TryParseNumber (std::string_view (*text).substr (at, plus - at), value))
        {
            ids.push_back ((int) value);
        }

        at = plus + 1;
    }

    return ids;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::ReadRecord
//
//  A line record without a type key is an assembler line; its count is the
//  macro nesting depth, and only means something alongside a macro type.
//
////////////////////////////////////////////////////////////////////////////////

void DebugFileReader::ReadRecord (std::string_view keyword, const Fields & fields, DebugFile & file)
{
    if (keyword == "version")
    {
        file.major = GetInt (fields, "major", 0);
        file.minor = GetInt (fields, "minor", 0);
    }
    else if (keyword == "file")
    {
        file.files.push_back ({ GetInt (fields, "id", 0), GetString (fields, "name"), GetNumber (fields, "size", 0),
                                GetNumber (fields, "mtime", 0), GetString (fields, "sha1"), GetInt (fields, "mod", -1) });
    }
    else if (keyword == "seg")
    {
        file.segments.push_back ({ GetInt (fields, "id", 0), GetString (fields, "name"),
                                   (uint32_t) GetNumber (fields, "start", 0), (uint32_t) GetNumber (fields, "size", 0) });
    }
    else if (keyword == "span")
    {
        file.spans.push_back ({ GetInt (fields, "id", 0), GetInt (fields, "seg", -1),
                                (uint32_t) GetNumber (fields, "start", 0), (uint32_t) GetNumber (fields, "size", 0) });
    }
    else if (keyword == "line")
    {
        file.lines.push_back ({ GetInt (fields, "id", 0), GetInt (fields, "file", -1), GetInt (fields, "line", 0),
                                (DebugLineType) GetInt (fields, "type", 0), GetInt (fields, "count", 0),
                                GetIdList (fields, "span") });
    }
    else if (keyword == "sym")
    {
        file.symbols.push_back ({ GetInt (fields, "id", 0), GetString (fields, "name"), (uint32_t) GetNumber (fields, "val", 0),
                                  GetInt (fields, "seg", -1), GetInt (fields, "scope", -1), GetString (fields, "type") });
    }
    else if (keyword == "scope")
    {
        file.scopes.push_back ({ GetInt (fields, "id", 0), GetString (fields, "name"),
                                 GetInt (fields, "mod", -1), GetInt (fields, "parent", -1) });
    }
    else if (keyword == "mod")
    {
        file.modules.push_back ({ GetInt (fields, "id", 0), GetString (fields, "name"), GetInt (fields, "file", -1) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader::Validate
//
//  Every id a line or span names must be a record in the file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugFileReader::Validate (const DebugFile & file, std::string & error)
{
    HRESULT             hr = S_OK;
    std::set<int>       fileIds;
    std::set<int>       spanIds;
    std::set<int>       segIds;



    CBREx (file.major != 0,               HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (file.major == kSupportedMajor, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (const DebugSourceFile & each : file.files)    { fileIds.insert (each.id); }
    for (const DebugSpan & each : file.spans)          { spanIds.insert (each.id); }
    for (const DebugSegment & each : file.segments)    { segIds.insert (each.id);  }

    for (const DebugSpan & span : file.spans)
    {
        bool  known = segIds.contains (span.segment);

        CBREx (known, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    for (const DebugLine & line : file.lines)
    {
        bool  known = fileIds.contains (line.file);

        CBREx (known, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        for (int span : line.spans)
        {
            known = spanIds.contains (span);
            CBREx (known, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        }
    }

Error:
    if (FAILED (hr))
    {
        error = (file.major == 0)               ? std::string ("This is not a cc65 debug file: it has no version record.")
              : (file.major != kSupportedMajor) ? std::format ("This debug file is version {}. Only version 2 is supported.", file.major)
              :                                   std::string ("This debug file refers to a file, span or segment it does not hold.");
    }

    return hr;
}
