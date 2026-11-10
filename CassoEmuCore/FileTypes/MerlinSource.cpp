#include "Pch.h"

#include "FileTypes/MerlinSource.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource::Matches
//
//  A ProDOS text file ending .S, or a DOS 3.3 binary file starting T.,
//  either with Merlin's bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinSource::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  typed = (facts.HasProDosType (kProDosText) && facts.IsNameEnding (".S"))
               || (facts.IsDos33() && facts.type == kDosBinary && facts.IsNameStarting ("T."));



    return typed && Check (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource::Check
//
//  High-ASCII text and the low spaces Merlin stores, ending in $8D.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinSource::Check (std::span<const Byte> bytes)
{
    if (bytes.empty() || bytes.back() != kLineEnd)
    {
        return false;
    }

    for (Byte b : bytes)
    {
        if (b < 0x80 && b != 0x20 && b != 0x09)
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource::Decode
//
//  Each column padded out to where the next starts, at least one space
//  apart; a comment line as it is.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinSource::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT      hr = S_OK;
    std::string  line;
    bool         ok = Check (bytes);



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    CBRFEx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file isn't Merlin source.");

    outFile.kind = DecodedFile::Kind::Lines;

    for (Byte b : bytes)
    {
        if (b == kLineEnd)
        {
            std::string  text;
            size_t       column = 0;
            size_t       start  = 0;

            for (size_t at = 0; at <= line.size(); at++)
            {
                if (at < line.size() && line[at] != '\x01')
                {
                    continue;
                }

                if (column > 0)
                {
                    PadToColumn (column, text);
                }

                text  += line.substr (start, at - start);
                start  = at + 1;
                column++;
            }

            outFile.lines.push_back (TextEncoding::Utf8ToWide (text));
            line.clear();
            continue;
        }

        line += (b == kColumnBreak) ? '\x01' : (char) (b & 0x7F);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource::PadToColumn
//
//  Spaces out to where a column starts, at least one.
//
////////////////////////////////////////////////////////////////////////////////

void MerlinSource::PadToColumn (size_t column, std::string & inOutText)
{
    size_t  target = (column < _countof (kColumns)) ? kColumns[column] : inOutText.size() + 1;



    inOutText.append ((inOutText.size() < target) ? target - inOutText.size() : 1, ' ');
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource::CountCommentBreaks
//
//  The column breaks ahead of a comment at a position in its line: as many
//  as put it there when the line is decoded, or one when no count does.
//
////////////////////////////////////////////////////////////////////////////////

size_t MerlinSource::CountCommentBreaks (const std::vector<std::string> & columns, size_t position)
{
    static constexpr size_t  kMostBreaks = 8;
    std::string              text;



    for (size_t at = 0; at < columns.size(); at++)
    {
        if (at > 0)
        {
            PadToColumn (at, text);
        }

        text += columns[at];
    }

    if (columns.empty() && position == 0)
    {
        return 0;
    }

    for (size_t breaks = 1; breaks <= kMostBreaks; breaks++)
    {
        PadToColumn (columns.empty() ? breaks : columns.size() - 1 + breaks, text);

        if (text.size() == position)
        {
            return breaks;
        }
    }

    return columns.empty() ? 0 : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource::SplitColumns
//
//  A line's columns, split on spaces outside quotes; everything from a
//  semicolon that starts a column on is the comment. An operand such as
//  ]1=" has a quote that never closes, so two or more spaces and then a
//  semicolon end a quote too.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinSource::SplitColumns (const std::string & line, std::vector<std::string> & outColumns)
{
    std::string  current;
    char         quote   = 0;
    size_t       at      = 0;



    outColumns.clear();

    for (at = 0; at < line.size(); at++)
    {
        char  c = line[at];

        if (quote == 0 && c == ';' && current.empty())
        {
            break;
        }

        if (quote != 0 && c == ' ')
        {
            size_t  next = line.find_first_not_of (' ', at);

            if (next != std::string::npos && next - at >= 2 && line[next] == ';')
            {
                quote = 0;
            }
        }

        if (quote == 0 && (c == ' ' || c == '\t'))
        {
            if (!current.empty() || outColumns.empty())
            {
                outColumns.push_back (current);
                current.clear();
            }

            while (at + 1 < line.size() && (line[at + 1] == ' ' || line[at + 1] == '\t'))
            {
                at++;
            }

            continue;
        }

        if (quote == 0 && (c == '"' || c == '\'') && outColumns.size() >= 2)
        {
            quote = c;
        }
        else if (c == quote)
        {
            quote = 0;
        }

        current += c;
    }

    if (!current.empty())
    {
        outColumns.push_back (current);
    }

    return (at < line.size()) ? line.substr (at) : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource::Encode
//
//  A line starting with * is encoded whole; any other is split into its
//  columns, each joined by $A0, with spaces inside a quote or a comment as
//  $20.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinSource::Encode (const std::vector<std::string> & lines, std::vector<Byte> & outBytes, std::wstring & outError)
{
    HRESULT  hr     = S_OK;
    size_t   number = 0;



    outBytes.clear();

    for (const std::string & raw : lines)
    {
        std::vector<std::string>  columns;
        std::string               comment;
        std::vector<Byte>         coded;
        std::string               line   = raw;
        bool                      fits   = false;

        number++;

        while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
        {
            line.pop_back();
        }

        if (!line.empty() && line[0] == '*')
        {
            for (char c : line)
            {
                coded.push_back ((c == ' ') ? (Byte) 0x20 : (Byte) (c | 0x80));
            }
        }
        else
        {
            comment = SplitColumns (line, columns);

            while (!columns.empty() && columns.back().empty())
            {
                columns.pop_back();
            }

            for (size_t at = 0; at < columns.size(); at++)
            {
                if (at > 0)
                {
                    coded.push_back (kColumnBreak);
                }

                for (char c : columns[at])
                {
                    coded.push_back ((c == ' ') ? (Byte) 0x20 : (Byte) (c | 0x80));
                }
            }

            if (!comment.empty())
            {
                coded.insert (coded.end(), CountCommentBreaks (columns, line.size() - comment.size()), kColumnBreak);

                for (char c : comment)
                {
                    coded.push_back ((c == ' ') ? (Byte) 0x20 : (Byte) (c | 0x80));
                }
            }
        }

        fits = coded.size() < kMaxLine;
        CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = std::format (L"Line {} is longer than Merlin accepts.", number));

        coded.push_back (kLineEnd);
        outBytes.insert (outBytes.end(), coded.begin(), coded.end());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinSource::Import
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinSource::Import (std::span<const Byte> hostBytes, VolumeKind volume, FilePayload & outPayload, std::wstring & outError)
{
    HRESULT                   hr    = S_OK;
    std::vector<std::string>  lines;
    std::string               line;
    size_t                    start = (hostBytes.size() >= 3 && hostBytes[0] == 0xEF && hostBytes[1] == 0xBB && hostBytes[2] == 0xBF) ? 3 : 0;



    for (size_t at = start; at < hostBytes.size(); at++)
    {
        char  c = (char) hostBytes[at];

        if (c == '\r' || c == '\n')
        {
            lines.push_back (line);
            line.clear();

            if (c == '\r' && at + 1 < hostBytes.size() && hostBytes[at + 1] == '\n')
            {
                at++;
            }

            continue;
        }

        line += c;
    }

    if (!line.empty())
    {
        lines.push_back (line);
    }

    hr = Encode (lines, outPayload.bytes, outError);
    CHR (hr);

    outPayload.type     = (volume == VolumeKind::Dos33) ? kDosBinary : kProDosText;
    outPayload.encoding = PayloadEncoding::Verbatim;

    if (volume == VolumeKind::Dos33)
    {
        outPayload.loadAddress    = 0x8000;
        outPayload.hasLoadAddress = true;
    }

Error:
    return hr;
}
