#include "Pch.h"

#include "FileTypes/TeachDocument.h"
#include "FileTypes/AppleWorks.h"
#include "FileTypes/IigsResources.h"
#include "FileTypes/TextCharset.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TeachDocument::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool TeachDocument::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    UNREFERENCED_PARAMETER (bytes);

    return facts.HasProDosType (kFileType, kAuxType);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TeachDocument::ReadLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t TeachDocument::ReadLong (std::span<const Byte> bytes, size_t at)
{
    return (uint32_t) bytes[at] | ((uint32_t) bytes[at + 1] << 8) | ((uint32_t) bytes[at + 2] << 16) | ((uint32_t) bytes[at + 3] << 24);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TeachDocument::GetAlignment
//
//  TextEdit's justification: 0 left, -1 right, 1 center and 2 full.
//
////////////////////////////////////////////////////////////////////////////////

Alignment TeachDocument::GetAlignment (Word justification)
{
    switch (justification)
    {
        case 0xFFFF: return Alignment::Right;
        case 1:      return Alignment::Center;
        case 2:      return Alignment::Justified;
        default:     return Alignment::Left;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TeachDocument::ReadFormat
//
//  A TEFormat is a version word, the rulers' length and rulers, the style
//  list's length and 12-byte styles, then the count of runs and each run's
//  length and offset into the style list. A style's third byte is its
//  QuickDraw II style: bit 0 bold, bit 1 italic, bit 2 underline. The first
//  ruler's justification, at its sixth byte, applies to the whole text.
//
////////////////////////////////////////////////////////////////////////////////

bool TeachDocument::ReadFormat (std::span<const Byte> format, Alignment & outAlignment, std::vector<StyleRun> & outRuns)
{
    size_t  rulers     = 0;
    size_t  stylesAt   = 0;
    size_t  styleBytes = 0;
    size_t  itemsAt    = 0;
    size_t  items      = 0;



    if (format.size() < 6)
    {
        return false;
    }

    rulers   = ReadLong (format, 2);
    stylesAt = 6 + rulers + 4;

    if (rulers > format.size() || stylesAt > format.size())
    {
        return false;
    }

    if (rulers >= 8)
    {
        outAlignment = GetAlignment (AppleWorks::ReadWord (format, 6 + 6));
    }

    styleBytes = ReadLong (format, stylesAt - 4);
    itemsAt    = stylesAt + styleBytes + 4;

    if (styleBytes > format.size() || itemsAt > format.size())
    {
        return false;
    }

    items = ReadLong (format, itemsAt - 4);

    if (items > (format.size() - itemsAt) / kItemBytes)
    {
        return false;
    }

    for (size_t item = 0; item < items; item++)
    {
        size_t  base   = itemsAt + item * kItemBytes;
        size_t  offset = ReadLong (format, base + 4);
        Byte    style  = (offset + kStyleBytes <= styleBytes) ? format[stylesAt + offset + 2] : 0;

        outRuns.push_back (StyleRun { ReadLong (format, base), style });
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TeachDocument::Decode
//
//  Without the resource fork, or with a style block that doesn't read, the
//  text is decoded plain.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TeachDocument::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    Charset                charset   = TextCharset::Resolve (options.charset, true);
    Alignment              alignment = Alignment::Left;
    std::vector<StyleRun>  runs;
    std::span<const Byte>  format;
    size_t                 run       = 0;
    size_t                 left      = 0;
    Byte                   style     = 0;
    std::vector<Byte>      pending;
    Paragraph              paragraph;
    auto                   flush     = [&]()
    {
        if (!pending.empty())
        {
            AppleWorks::AddText (paragraph, TextCharset::DecodeLine (pending, charset), (style & 0x01) != 0, (style & 0x02) != 0, (style & 0x04) != 0);
            pending.clear();
        }
    };



    UNREFERENCED_PARAMETER (outError);

    if (!IigsResources::Find (facts.resourceFork, IigsResources::kStyleBlock, 1, format) || !ReadFormat (format, alignment, runs))
    {
        outFile.notes.push_back (L"The resource fork's styles aren't available, so the text is plain.");
        runs.clear();
    }

    outFile.kind        = DecodedFile::Kind::Document;
    paragraph.alignment = alignment;
    left                = runs.empty() ? SIZE_MAX : runs[0].length;
    style               = runs.empty() ? 0 : runs[0].style;

    for (Byte b : bytes)
    {
        while (left == 0 && run + 1 < runs.size())
        {
            flush();
            run++;
            left  = runs[run].length;
            style = runs[run].style;
        }

        if (left != SIZE_MAX && left > 0)
        {
            left--;
        }

        if (b == kReturn)
        {
            flush();
            outFile.paragraphs.push_back (std::move (paragraph));
            paragraph = Paragraph { alignment, {} };
            continue;
        }

        pending.push_back (b);
    }

    flush();

    if (!paragraph.runs.empty())
    {
        outFile.paragraphs.push_back (std::move (paragraph));
    }

    return S_OK;
}
