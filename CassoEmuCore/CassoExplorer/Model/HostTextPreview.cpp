#include "Pch.h"

#include "CassoExplorer/Model/HostTextPreview.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostTextPreview::GetHostCharset
//
//  A host file is text, and opens showing its characters rather than its
//  bytes, when it has no NUL and few other control codes besides tabs, line
//  breaks, form feeds and DOS's end-of-file mark. Code page 437 has symbols
//  for the rest, such as a pointer for 0x10, so a file with any is DOS text. Bytes
//  above 0x7F that are well-formed UTF-8 are UTF-8 text; otherwise they are
//  DOS text when most of them are DOS line-drawing characters, and Windows
//  text when not.
//
////////////////////////////////////////////////////////////////////////////////

HostCharset HostTextPreview::GetHostCharset (std::span<const Byte> sample)
{
    size_t  high     = 0;
    size_t  drawing  = 0;
    size_t  controls = 0;



    if (sample.empty())
    {
        return HostCharset::None;
    }

    for (Byte value : sample)
    {
        if (value == 0)
        {
            return HostCharset::None;
        }

        controls += ((value < 0x20 && value != '\t' && value != '\r' && value != '\n'
                      && value != kFormFeed && value != kDosEndOfFile) || value == 0x7F) ? 1 : 0;

        if (value >= 0x80)
        {
            high++;
            drawing += (value >= kDosDrawingFirst && value <= kDosDrawingLast) ? 1 : 0;
        }
    }

    if (controls * kTextControlRatio > sample.size())
    {
        return HostCharset::None;
    }

    if (high == 0 && controls == 0)
    {
        return HostCharset::Ascii;
    }

    if (high > 0 && IsUtf8 (sample))
    {
        return HostCharset::Utf8;
    }

    return (controls > 0 || drawing * 2 >= high) ? HostCharset::Dos : HostCharset::Windows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostTextPreview::IsUtf8
//
//  Whether every byte above 0x7F is part of a well-formed UTF-8 sequence. A
//  sequence cut off by the end of the sample counts as well formed.
//
////////////////////////////////////////////////////////////////////////////////

bool HostTextPreview::IsUtf8 (std::span<const Byte> sample)
{
    size_t  at = 0;



    while (at < sample.size())
    {
        Byte    lead  = sample[at];
        size_t  extra = (lead < 0x80)                  ? 0
                      : (lead >= 0xC2 && lead <= 0xDF) ? 1
                      : (lead >= 0xE0 && lead <= 0xEF) ? 2
                      : (lead >= 0xF0 && lead <= 0xF4) ? 3
                      :                                  SIZE_MAX;

        if (extra == SIZE_MAX)
        {
            return false;
        }

        for (size_t next = 1; next <= extra && at + next < sample.size(); next++)
        {
            if ((sample[at + next] & 0xC0) != 0x80)
            {
                return false;
            }
        }

        at += extra + 1;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostTextPreview::TryPreviewText
//
//  A file's bytes as lines of text: an assembler's source or an Apple file
//  Casso Explorer exported, in whatever character set it is, or any text in
//  UTF-8, or in UTF-16 when a byte order
//  mark leads it. Anything else, binary above all, stays as it was. A byte
//  order mark is dropped, and tabs are expanded to the next stop of eight.
//
////////////////////////////////////////////////////////////////////////////////

bool HostTextPreview::TryPreviewText (const std::wstring & name, std::span<const Byte> bytes, PreviewContent & outPreview)
{
    HostCharset   charset = GetHostCharset (bytes.subspan (0, (std::min) (bytes.size(), kTextSampleBytes)));
    bool          source  = IsSourceName (name);
    bool          apple   = IsAppleExportName (name);
    bool          wideLe  = bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE;
    bool          wideBe  = bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF;
    std::string   narrow ((const char *) bytes.data(), bytes.size());
    std::wstring  text;
    std::wstring  line;
    bool          isText  = ((source || apple) && charset != HostCharset::None) || charset == HostCharset::Utf8 || wideLe || wideBe;



    if (!isText)
    {
        return false;
    }

    if (wideLe || wideBe)
    {
        for (size_t at = 0; at + 1 < bytes.size(); at += 2)
        {
            text.push_back ((wchar_t) (wideLe ? (bytes[at] | (bytes[at + 1] << 8)) : ((bytes[at] << 8) | bytes[at + 1])));
        }
    }
    else
    {
        text = (charset == HostCharset::Utf8) ? TextEncoding::Utf8ToWide (narrow)
             : (charset == HostCharset::Dos)  ? TextEncoding::NarrowToWide (narrow, kDosCodePage)
                                              : TextEncoding::NarrowToWide (narrow, kWindowsCodePage);
    }

    if (!text.empty() && text[0] == kByteOrderMark)
    {
        text.erase (0, 1);
    }

    outPreview.lines.clear();

    for (size_t at = 0; at <= text.size(); at++)
    {
        wchar_t  ch = (at < text.size()) ? text[at] : L'\n';

        if (ch == L'\r' || ch == L'\n')
        {
            outPreview.lines.push_back (std::move (line));
            line.clear();
            at += (ch == L'\r' && at + 1 < text.size() && text[at + 1] == L'\n') ? 1 : 0;
        }
        else if (ch == L'\t')
        {
            line.append (kTabStop - line.size() % kTabStop, L' ');
        }
        else
        {
            line.push_back (ch);
        }
    }

    //  A last line break ends the last line rather than starting another.
    if (outPreview.lines.size() > 1 && outPreview.lines.back().empty())
    {
        outPreview.lines.pop_back();
    }

    outPreview.kind      = PreviewContent::Kind::Text;
    outPreview.source    = source;
    outPreview.bytes.clear();
    outPreview.hostPath.clear();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostTextPreview::IsAppleExportName
//
//  A BASIC listing or a text file Casso Explorer exported to the host, by the
//  suffix the export appends to its catalog name. A conversion of the file
//  format table counts when it is text: its pictures, documents and tables
//  have the previews Windows has for them.
//
////////////////////////////////////////////////////////////////////////////////

bool HostTextPreview::IsAppleExportName (const std::wstring & name)
{
    static constexpr const wchar_t  * kTextExtension = L".txt";
    ParsedHostName                    parsed;
    size_t                            dot            = name.rfind (L'.');



    if (!HostFileNaming::Parse (name, parsed) || parsed.converted == ParsedHostName::ConvertedKind::None)
    {
        return false;
    }

    return parsed.converted != ParsedHostName::ConvertedKind::Format || _wcsicmp (name.c_str() + dot, kTextExtension) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostTextPreview::IsSourceName
//
//  An assembler's source by its extension, as as65, ca65 and Merlin write
//  theirs.
//
////////////////////////////////////////////////////////////////////////////////

bool HostTextPreview::IsSourceName (const std::wstring & name)
{
    size_t        dot       = name.rfind (L'.');
    std::wstring  extension = (dot == std::wstring::npos) ? std::wstring() : name.substr (dot);



    return std::any_of (std::begin (kpszSourceExtensions), std::end (kpszSourceExtensions),
                        [&extension] (const wchar_t * known) { return _wcsicmp (known, extension.c_str()) == 0; });
}
