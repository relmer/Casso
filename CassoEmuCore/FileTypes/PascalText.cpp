#include "Pch.h"

#include "FileTypes/PascalText.h"
#include "FileTypes/TextCharset.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PascalText::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool PascalText::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  typed = (facts.IsPascal() && facts.type == kPascalText) || facts.HasProDosType (kProDosText);



    return typed && Check (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalText::Check
//
//  At least the header and one page, in whole pages.
//
////////////////////////////////////////////////////////////////////////////////

bool PascalText::Check (std::span<const Byte> bytes)
{
    return bytes.size() >= 2 * kPageBytes && bytes.size() % kPageBytes == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalText::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalText::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT            hr        = S_OK;
    bool               fits      = Check (bytes);
    std::vector<Byte>  line;
    bool               lineStart = true;



    UNREFERENCED_PARAMETER (facts);

    CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"A Pascal textfile is a header and pages of 1,024 bytes.");

    outFile.kind = DecodedFile::Kind::Lines;

    for (size_t at = kPageBytes; at < bytes.size(); at++)
    {
        Byte  b = bytes[at];

        if (b == 0)
        {
            continue;
        }

        if (lineStart && b == kDle && at + 1 < bytes.size())
        {
            size_t  spaces = (bytes[at + 1] > kIndentBase) ? (size_t) (bytes[at + 1] - kIndentBase) : 0;

            line.insert (line.end(), spaces, ' ');
            at++;
            lineStart = false;
            continue;
        }

        if (b == '\r')
        {
            outFile.lines.push_back (TextCharset::DecodeLine (line, options.charset));
            line.clear();
            lineStart = true;
            continue;
        }

        line.push_back (b);
        lineStart = false;
    }

    if (!line.empty())
    {
        outFile.lines.push_back (TextCharset::DecodeLine (line, options.charset));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalText::Encode
//
//  A line's leading spaces become a DLE code, and a line that doesn't fit
//  in what is left of a page starts the next.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalText::Encode (const std::vector<std::string> & lines, std::vector<Byte> & outBytes, std::wstring & outError)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  page;
    size_t             number = 0;



    outBytes.assign (kPageBytes, 0);

    for (const std::string & text : lines)
    {
        std::vector<Byte>  coded;
        size_t             indent = text.find_first_not_of (' ');
        bool               fits   = false;

        number++;
        indent = (indent == std::string::npos) ? text.size() : indent;

        if (indent > 0 && indent <= kMaxIndent)
        {
            coded.push_back (kDle);
            coded.push_back ((Byte) (indent + kIndentBase));
            coded.insert (coded.end(), text.begin() + (ptrdiff_t) indent, text.end());
        }
        else
        {
            coded.insert (coded.end(), text.begin(), text.end());
        }

        coded.push_back ('\r');

        fits = coded.size() <= kPageBytes;
        CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = std::format (L"Line {} is longer than a Pascal textfile page.", number));

        if (page.size() + coded.size() > kPageBytes)
        {
            page.resize (kPageBytes, 0);
            outBytes.insert (outBytes.end(), page.begin(), page.end());
            page.clear();
        }

        page.insert (page.end(), coded.begin(), coded.end());
    }

    page.resize (kPageBytes, 0);
    outBytes.insert (outBytes.end(), page.begin(), page.end());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PascalText::Import
//
//  A host text file, its lines ending in CR, LF or CR LF, as a textfile.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PascalText::Import (std::span<const Byte> hostBytes, VolumeKind volume, FilePayload & outPayload, std::wstring & outError)
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

        line += (c == '\t') ? ' ' : c;
    }

    if (!line.empty())
    {
        lines.push_back (line);
    }

    hr = Encode (lines, outPayload.bytes, outError);
    CHR (hr);

    outPayload.type     = (volume == VolumeKind::Pascal) ? kPascalText : kProDosText;
    outPayload.encoding = PayloadEncoding::Verbatim;

Error:
    return hr;
}
