#include "Pch.h"

#include "FileTypes/IigsFormats.h"
#include "Core/TextEncoding.h"
#include "FileTypes/AppleWorks.h"
#include "FileTypes/GlyphSheet.h"
#include "FileTypes/SuperHiRes.h"
#include "FileTypes/TextCharset.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IigsFont::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool IigsFont::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kFileType, 0x0000) && !bytes.empty() && bytes.size() > (size_t) bytes[0] + 1 + 12;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsFont::DescribeStyle
//
//  QuickDraw II's style bits: bold, italic, underline, outline and shadow.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring IigsFont::DescribeStyle (Word style)
{
    static constexpr const wchar_t *  kNames[] = { L"bold", L"italic", L"underline", L"outline", L"shadow" };



    std::wstring  text;



    for (size_t bit = 0; bit < std::size (kNames); bit++)
    {
        if (style & (1u << bit))
        {
            text += (text.empty() ? L"" : L", ") + std::wstring (kNames[bit]);
        }
    }

    return text.empty() ? L"plain" : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsFont::Decode
//
//  Each character's image is the strike's columns from its location to the
//  next one's; an offset/width entry of $FFFF is a missing character. The
//  sheet starts at character 0, so each glyph's cell is its code.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IigsFont::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT     hr       = S_OK;
    size_t      header   = (size_t) bytes[0] + 1;
    size_t      record   = 0;
    size_t      first    = 0;
    size_t      last     = 0;
    size_t      height   = 0;
    size_t      rowBytes = 0;
    size_t      strike   = 0;
    size_t      locAt    = 0;
    size_t      owAt     = 0;
    size_t      count    = 0;
    GlyphSheet  sheet;
    bool        inside   = false;



    UNREFERENCED_PARAMETER (options);

    record = header + 2 * (size_t) AppleWorks::ReadWord (bytes, header);
    inside = record + 2 * kRecordWords <= bytes.size();
    CBRFEx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The font record isn't in the file.");

    first    = AppleWorks::ReadWord (bytes, record + 2);
    last     = AppleWorks::ReadWord (bytes, record + 4);
    height   = AppleWorks::ReadWord (bytes, record + 14);
    rowBytes = 2 * (size_t) AppleWorks::ReadWord (bytes, record + 24);
    strike   = record + 2 * kRecordWords;
    count    = (last >= first) ? last - first + 3 : 0;
    locAt    = strike + rowBytes * height;
    owAt     = locAt + 2 * count;

    inside = count > 0 && last < 256 && owAt + 2 * count <= bytes.size();
    CBRFEx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The font's tables run past the end of the file.");

    sheet.name   = TextEncoding::NarrowToWide (facts.name);
    sheet.family = TextCharset::DecodeLine (bytes.subspan (1, bytes[0]), Charset::MacOsRoman);
    sheet.style  = DescribeStyle (AppleWorks::ReadWord (bytes, header + 4));
    sheet.size   = AppleWorks::ReadWord (bytes, header + 6);
    sheet.glyphs.resize (first);

    for (size_t character = first; character <= last; character++)
    {
        size_t  index  = character - first;
        size_t  left   = AppleWorks::ReadWord (bytes, locAt + 2 * index);
        size_t  right  = AppleWorks::ReadWord (bytes, locAt + 2 * index + 2);
        Glyph   glyph;

        if (AppleWorks::ReadWord (bytes, owAt + 2 * index) != kMissing && right > left && right <= rowBytes * 8)
        {
            glyph.width  = (int) (right - left);
            glyph.height = (int) height;

            for (size_t row = 0; row < height; row++)
            {
                for (size_t column = left; column < right; column++)
                {
                    Byte  b = bytes[strike + row * rowBytes + column / 8];

                    glyph.pixels.push_back (((b >> (7 - column % 8)) & 1) != 0);
                }
            }
        }

        sheet.glyphs.push_back (std::move (glyph));
    }

    sheet.Draw (outFile);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsIcons::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool IigsIcons::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kFileType) && bytes.size() >= kHeaderBytes + 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsIcons::ReadIcon
//
//  An icon's rows are its image size over its height in bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool IigsIcons::ReadIcon (std::span<const Byte> bytes, size_t at, Icon & outIcon, size_t & outEnd)
{
    size_t  size = 0;



    if (at + 8 > bytes.size())
    {
        return false;
    }

    size           = AppleWorks::ReadWord (bytes, at + 2);
    outIcon.height = AppleWorks::ReadWord (bytes, at + 4);
    outIcon.width  = AppleWorks::ReadWord (bytes, at + 6);
    outEnd         = at + 8 + 2 * size;

    if (outEnd > bytes.size() || outIcon.height <= 0 || outIcon.width <= 0 || size < (size_t) outIcon.height * (size_t) ((outIcon.width + 1) / 2))
    {
        return false;
    }

    outIcon.image = bytes.subspan (at + 8, size);
    outIcon.mask  = bytes.subspan (at + 8 + size, size);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsIcons::GetPixel
//
//  The high nibble of each byte is the left pixel.
//
////////////////////////////////////////////////////////////////////////////////

Byte IigsIcons::GetPixel (std::span<const Byte> plane, const Icon & icon, int x, int y)
{
    size_t  rowBytes = plane.size() / (size_t) icon.height;
    Byte    b        = plane[(size_t) y * rowBytes + (size_t) x / 2];



    return (x % 2 == 0) ? (Byte) (b >> 4) : (Byte) (b & 0x0F);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsIcons::DrawIcon
//
//  An icon in the super hi-res standard palette where its mask is set; or
//  its mask, black where set on white.
//
////////////////////////////////////////////////////////////////////////////////

void IigsIcons::DrawIcon (const Icon & icon, bool drawMask, int left, int top, DecodedFile & outFile)
{
    static constexpr Word  kStandard[16] = { 0x000, 0x777, 0x841, 0x72C, 0x00F, 0x080, 0xF70, 0xD00, 0xFA9, 0xFF0, 0x0E0, 0x4DF, 0xDAF, 0x78F, 0xCCC, 0xFFF };



    for (int y = 0; y < icon.height; y++)
    {
        for (int x = 0; x < icon.width; x++)
        {
            Byte      mask  = GetPixel (icon.mask, icon, x, y);
            uint32_t  color = kBackground;

            if (drawMask)
            {
                color = (mask != 0) ? kMaskInk : kMaskPaper;
            }
            else if (mask != 0)
            {
                Word  rgb = kStandard[GetPixel (icon.image, icon, x, y)];

                color = SuperHiRes::ToArgb ((Byte) (rgb & 0xFF), (Byte) (rgb >> 8));
            }

            for (int line = 0; line < 2; line++)
            {
                outFile.bgra[(size_t) (top + y * 2 + line) * (size_t) outFile.width + (size_t) (left + x)] = color;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsIcons::Decode
//
//  The entries are read once for the picture's size and again to draw.
//  Each entry's name pattern, type and aux type go in the notes, in the
//  rows' order.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IigsIcons::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT                            hr     = S_OK;
    std::vector<std::array<Icon, 2>>   rows;
    size_t                             at     = kHeaderBytes;
    int                                width  = 0;
    int                                height = 0;
    int                                top    = kGap;
    bool                               any    = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    while (at + 2 <= bytes.size())
    {
        size_t               length = AppleWorks::ReadWord (bytes, at);
        size_t               end    = 0;
        std::array<Icon, 2>  icons;
        size_t               name   = 0;

        if (length == 0 || at + length > bytes.size() || at + kIconAt > bytes.size())
        {
            break;
        }

        if (!ReadIcon (bytes, at + kIconAt, icons[0], end) || !ReadIcon (bytes, end, icons[1], end))
        {
            outFile.notes.push_back (L"An icon entry doesn't read; the icons after it are dropped.");
            break;
        }

        name = std::min<size_t> (bytes[at + kNameAt], 15);
        outFile.notes.push_back (std::format (L"{}: {} (type ${:02X}, aux ${:04X})",
                                              rows.size() + 1,
                                              TextCharset::DecodeLine (bytes.subspan (at + kNameAt + 1, name), Charset::MacOsRoman),
                                              AppleWorks::ReadWord (bytes, at + kTypeAt),
                                              AppleWorks::ReadWord (bytes, at + kTypeAt + 2)));

        width   = std::max (width, kGap + 2 * (icons[0].width + kGap) + 2 * (icons[1].width + kGap));
        height += 2 * std::max (icons[0].height, icons[1].height) + kGap;
        rows.push_back (icons);
        at += length;
    }

    any = !rows.empty();
    CBRFEx (any, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file doesn't have any icons.");

    outFile.kind   = DecodedFile::Kind::Picture;
    outFile.width  = width;
    outFile.height = height + kGap;
    outFile.bgra.assign ((size_t) outFile.width * (size_t) outFile.height, kBackground);

    for (const std::array<Icon, 2> & icons : rows)
    {
        int  left = kGap;

        for (const Icon & icon : icons)
        {
            DrawIcon (icon, false, left, top, outFile);
            left += icon.width + kGap;
            DrawIcon (icon, true, left, top, outFile);
            left += icon.width + kGap;
        }

        top += 2 * std::max (icons[0].height, icons[1].height) + kGap;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsAnimation::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool IigsAnimation::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kFileType, 0x0000) && bytes.size() >= kRecordsAt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsAnimation::Decode
//
//  The entries run for the animation's length, less the length word
//  itself; a length of 4 or less falls back to the data's length.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IigsAnimation::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    SuperHiRes::Screen  screen;
    size_t              length = AppleWorks::ReadLong (bytes, kScreenBytes + 8);
    size_t              end    = 0;
    size_t              frames = 0;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    if (length <= 4)
    {
        length = AppleWorks::ReadLong (bytes, kScreenBytes);
    }

    end = std::min (bytes.size(), kRecordsAt + ((length > 4) ? length - 4 : 0));

    for (size_t at = kRecordsAt; at + 4 <= end; at += 4)
    {
        if (AppleWorks::ReadWord (bytes, at) == 0)
        {
            frames++;
        }
    }

    SuperHiRes::FromScreenBytes (bytes.first (kScreenBytes), screen);
    screen.lines = 200;
    SuperHiRes::Render (screen, outFile);

    outFile.notes.push_back (std::format (L"The first of {} frames, {} ticks apart.", frames, AppleWorks::ReadWord (bytes, kScreenBytes + 4)));

    return S_OK;
}
