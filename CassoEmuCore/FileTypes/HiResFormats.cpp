#include "Pch.h"

#include "FileTypes/HiResFormats.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Lz4fh::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool Lz4fh::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (0x08, 0x8066) && !bytes.empty() && bytes[0] == kMagic;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Lz4fh::Unpack
//
//  A length of 15 continues in the next byte; a match's length is 4 more
//  than it is stored.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Lz4fh::Unpack (std::span<const Byte> bytes, std::vector<Byte> & outPage)
{
    HRESULT  hr    = S_OK;
    size_t   at    = 1;
    bool     ended = false;
    bool     ok    = !bytes.empty() && bytes[0] == kMagic;



    outPage.clear();

    CBREx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    while (!ended && at < bytes.size())
    {
        Byte    token    = bytes[at++];
        size_t  literals = token >> 4;
        size_t  match    = token & 0x0F;
        bool    inside   = false;

        if (literals == 15 && at < bytes.size())
        {
            literals += bytes[at++];
        }

        inside = at + literals <= bytes.size();
        CBREx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outPage.insert (outPage.end(), bytes.begin() + (ptrdiff_t) at, bytes.begin() + (ptrdiff_t) (at + literals));
        at += literals;

        if (match == 15)
        {
            Byte  more = (at < bytes.size()) ? bytes[at++] : kEnd;

            if (more == kEnd)
            {
                ended = true;
                continue;
            }

            if (more == kNoMatch)
            {
                continue;
            }

            match += more;
        }

        match += 4;

        inside = at + 2 <= bytes.size();
        CBREx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        {
            size_t  offset = (size_t) (bytes[at] | (bytes[at + 1] << 8));

            at += 2;

            inside = offset < outPage.size();
            CBREx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            for (size_t i = 0; i < match; i++)
            {
                outPage.push_back (outPage[offset + i]);
            }
        }
    }

    outPage.resize (PicturePreview::kHiResLength, 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Lz4fh::Decode
//
//  The page drawn as hi-res for the color monitor; the preview draws it for
//  the monitor chosen.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Lz4fh::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  page;
    MemoryBus          bus;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    hr = Unpack (bytes, page);
    CHRF (hr, outError = L"The LZ4FH picture doesn't unpack.");

    outFile.kind = DecodedFile::Kind::Picture;

    hr = PicturePreview::Render (PicturePreview::Mode::HiRes, page, PicturePreview::kHiResPage1, bus, outFile.bgra, outFile.width, outFile.height);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HiResFont::MatchesApple2
//
////////////////////////////////////////////////////////////////////////////////

bool HiResFont::MatchesApple2 (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  typed = facts.HasProDosType (kTypeFont, 0x0000) || facts.HasProDosType (kTypeBin) || (facts.IsDos33() && facts.type == 0x04);



    return typed && bytes.size() == kApple2Bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HiResFont::MatchesApple3
//
////////////////////////////////////////////////////////////////////////////////

bool HiResFont::MatchesApple3 (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  typed = facts.HasProDosType (kTypeFont, 0x0000) || facts.HasProDosType (kTypeBin);



    return typed && bytes.size() == kApple3Bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HiResFont::Decode
//
//  The glyphs start at $20 for an Apple II font and at $00 for an Apple ///
//  one; the high bit of each row isn't a pixel.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HiResFont::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT     hr     = S_OK;
    GlyphSheet  sheet;
    bool        sized  = bytes.size() == kApple2Bytes || bytes.size() == kApple3Bytes;



    UNREFERENCED_PARAMETER (options);

    CBRFEx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"A hi-res font is 768 or 1,024 bytes.");

    sheet.name   = std::wstring (facts.name.begin(), facts.name.end());
    sheet.family = (bytes.size() == kApple3Bytes) ? L"Apple /// font" : L"Apple II font";

    for (size_t glyphAt = 0; glyphAt + 8 <= bytes.size(); glyphAt += 8)
    {
        Glyph  glyph;

        glyph.width  = 7;
        glyph.height = 8;

        for (int row = 0; row < 8; row++)
        {
            for (int column = 0; column < 7; column++)
            {
                glyph.pixels.push_back ((bytes[glyphAt + (size_t) row] >> column) & 1);
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
//  PrintShopGraphic::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool PrintShopGraphic::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    Word  load  = facts.IsProDos() ? facts.auxType : facts.loadAddress;
    bool  typed = facts.HasProDosType (0x06) || (facts.IsDos33() && facts.type == 0x04);



    return typed && (load == 0x4800 || load == 0x5800 || load == 0x6800 || load == 0x7800)
        && (bytes.size() == kPlaneBytes || bytes.size() == kPlaneBytes + 4);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintShopGraphic::MatchesGs
//
////////////////////////////////////////////////////////////////////////////////

bool PrintShopGraphic::MatchesGs (const FileFacts & facts, std::span<const Byte> bytes)
{
    return (facts.HasProDosType (0xF8, 0xC313) && bytes.size() >= kPlaneBytes)
        || (facts.HasProDosType (0xF8, 0xC323) && bytes.size() >= 3 * kPlaneBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintShopGraphic::Decode
//
//  The planes are the file's last bytes; the colors are the ones Print Shop
//  GS draws for each mix of yellow, magenta and cyan.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintShopGraphic::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    static constexpr uint32_t  kColors[] = { 0xFFFFFFFF, 0xFF0000FF, 0xFFFF0000, 0xFFCC00CC, 0xFFFFFF00, 0xFF00FF00, 0xFFFF6600, 0xFF000000 };

    HRESULT  hr     = S_OK;
    bool     color  = facts.HasProDosType (0xF8, 0xC323);
    size_t   planes = color ? 3 : 1;
    size_t   start  = 0;
    bool     sized  = bytes.size() >= planes * kPlaneBytes;



    UNREFERENCED_PARAMETER (options);

    CBRFEx (sized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The Print Shop graphic is short.");

    start          = bytes.size() - planes * kPlaneBytes;
    outFile.kind   = DecodedFile::Kind::Picture;
    outFile.width  = kWidth * 2;
    outFile.height = kHeight * 3;
    outFile.bgra.assign ((size_t) outFile.width * (size_t) outFile.height, 0xFFFFFFFF);

    for (int y = 0; y < kHeight; y++)
    {
        for (int x = 0; x < kWidth; x++)
        {
            size_t    bit   = (size_t) (y * kWidth + x);
            int       mix   = 0;
            uint32_t  argb  = 0;

            for (size_t plane = 0; plane < planes; plane++)
            {
                int  set = (bytes[start + plane * kPlaneBytes + bit / 8] >> (7 - bit % 8)) & 1;

                mix = (mix << 1) | set;
            }

            argb = color ? kColors[mix] : (mix ? 0xFF000000 : 0xFFFFFFFF);

            for (int dy = 0; dy < 3; dy++)
            {
                for (int dx = 0; dx < 2; dx++)
                {
                    outFile.bgra[(size_t) (y * 3 + dy) * (size_t) outFile.width + (size_t) (x * 2 + dx)] = argb;
                }
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintShopFont::Matches
//
//  A binary file at $6000 or $5FF4, or Print Shop GS's $F8/$C316, whose
//  pointers land inside it.
//
////////////////////////////////////////////////////////////////////////////////

bool PrintShopFont::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    GlyphSheet  sheet;
    Word        load  = facts.IsProDos() ? facts.auxType : facts.loadAddress;
    bool        typed = facts.HasProDosType (0xF8, 0xC316)
                     || ((facts.HasProDosType (0x06) || (facts.IsDos33() && facts.type == 0x04)) && (load == 0x6000 || load == 0x5FF4));



    return typed && bytes.size() > 576 && (TryRead (bytes, 0, sheet) || TryRead (bytes, 12, sheet));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintShopFont::TryRead
//
//  The glyphs, with the table at `header`; false when a pointer of a glyph
//  other than the space and the graphic lands outside the file.
//
////////////////////////////////////////////////////////////////////////////////

bool PrintShopFont::TryRead (std::span<const Byte> bytes, size_t header, GlyphSheet & outSheet)
{
    if (bytes.size() < header + kGlyphs * 4)
    {
        return false;
    }

    outSheet.glyphs.clear();

    for (size_t glyph = 0; glyph < kGlyphs; glyph++)
    {
        int     width   = bytes[header + glyph] & 0x7F;
        int     height  = bytes[header + kGlyphs + glyph];
        size_t  pointer = (size_t) (bytes[header + 2 * kGlyphs + glyph] | (bytes[header + 3 * kGlyphs + glyph] << 8));
        size_t  at      = pointer - kBase + header;
        size_t  stride  = (size_t) (width + 7) / 8;
        Glyph   out;

        if (glyph == 0 || glyph == 32 || width == 0 || height == 0)
        {
            outSheet.glyphs.push_back (out);
            continue;
        }

        if (pointer < kBase || at + stride * (size_t) height > bytes.size())
        {
            return false;
        }

        out.width  = width;
        out.height = height;

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                out.pixels.push_back ((bytes[at + (size_t) y * stride + (size_t) x / 8] >> (x % 8)) & 1);
            }
        }

        outSheet.glyphs.push_back (std::move (out));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintShopFont::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintShopFont::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT     hr   = S_OK;
    GlyphSheet  sheet;
    bool        read = TryRead (bytes, 0, sheet) || TryRead (bytes, 12, sheet);



    UNREFERENCED_PARAMETER (options);

    CBRFEx (read, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The Print Shop font's glyphs aren't in the file.");

    sheet.name   = std::wstring (facts.name.begin(), facts.name.end());
    sheet.family = L"Print Shop font";
    sheet.Draw (outFile);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShapeTable::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool ShapeTable::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return (facts.HasProDosType (0x06) || (facts.IsDos33() && facts.type == 0x04)) && Check (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShapeTable::Check
//
//  At least one shape, every offset past the table and inside the file,
//  and every shape ended by a zero byte. The spare byte is zero, or the
//  first shape starts right after the table, as the tools that make shape
//  tables write them; code rarely does either.
//
////////////////////////////////////////////////////////////////////////////////

bool ShapeTable::Check (std::span<const Byte> bytes)
{
    size_t  count  = bytes.empty() ? 0 : bytes[0];
    size_t  lowest = SIZE_MAX;



    if (count == 0 || bytes.size() < 2 + count * 2 + 1)
    {
        return false;
    }

    for (size_t shape = 0; shape < count; shape++)
    {
        lowest = std::min (lowest, (size_t) (bytes[2 + shape * 2] | (bytes[3 + shape * 2] << 8)));
    }

    if (bytes[1] != 0 && lowest != 2 + count * 2)
    {
        return false;
    }

    for (size_t shape = 0; shape < count; shape++)
    {
        size_t  offset = (size_t) (bytes[2 + shape * 2] | (bytes[3 + shape * 2] << 8));
        size_t  end    = offset;

        if (offset < 2 + count * 2 || offset >= bytes.size())
        {
            return false;
        }

        while (end < bytes.size() && bytes[end] != 0)
        {
            end++;
        }

        if (end >= bytes.size())
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShapeTable::DrawNumber
//
//  A shape's number in a 3-by-5 digit face, centered under its cell.
//
////////////////////////////////////////////////////////////////////////////////

void ShapeTable::DrawNumber (DecodedFile & file, int left, int top, int number)
{
    static constexpr uint16_t  kDigits[] = { 0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF };

    std::string  text  = std::to_string (number);
    int          x     = left + (kCell - (int) text.size() * 4) / 2;



    for (char c : text)
    {
        uint16_t  bits = kDigits[c - '0'];

        for (int row = 0; row < 5; row++)
        {
            for (int column = 0; column < 3; column++)
            {
                if ((bits >> (14 - row * 3 - column)) & 1)
                {
                    file.bgra[(size_t) (top + 1 + row) * (size_t) file.width + (size_t) (x + column)] = 0xFF808080;
                }
            }
        }

        x += 4;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShapeTable::Decode
//
//  Each shape's moves are walked once for their extent and again to plot,
//  centered in the cell.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShapeTable::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    static constexpr int  kDx[] = { 0, 1, 0, -1 };
    static constexpr int  kDy[] = { -1, 0, 1, 0 };

    HRESULT  hr    = S_OK;
    bool     ok    = Check (bytes);
    int      count = 0;
    int      rows  = 0;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    CBRFEx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file isn't a shape table.");

    count          = bytes[0];
    rows           = (count + kPerRow - 1) / kPerRow;
    outFile.kind   = DecodedFile::Kind::Picture;
    outFile.width  = kPerRow * kCell;
    outFile.height = rows * (kCell + kLabel);
    outFile.bgra.assign ((size_t) outFile.width * (size_t) outFile.height, 0xFF000000);

    for (int shape = 0; shape < count; shape++)
    {
        size_t  offset = (size_t) (bytes[2 + (size_t) shape * 2] | (bytes[3 + (size_t) shape * 2] << 8));
        int     left   = (shape % kPerRow) * kCell;
        int     top    = (shape / kPerRow) * (kCell + kLabel);
        int     minX   = 0;
        int     minY   = 0;
        int     maxX   = 0;
        int     maxY   = 0;

        for (int pass = 0; pass < 2; pass++)
        {
            int  x     = (pass == 0) ? 0 : left + kCell / 2 - (minX + maxX) / 2;
            int  y     = (pass == 0) ? 0 : top  + kCell / 2 - (minY + maxY) / 2;
            int  moves = 0;

            for (size_t at = offset; at < bytes.size() && bytes[at] != 0 && moves < kMaxMoves; at++)
            {
                Byte  b          = bytes[at];
                int   vectors[3] = { b & 0x07, (b >> 3) & 0x07, (b >> 6) & 0x03 };
                int   used       = (vectors[2] != 0) ? 3 : (vectors[1] != 0) ? 2 : 1;

                for (int v = 0; v < used; v++, moves++)
                {
                    bool  plot = (v < 2) && (vectors[v] & 0x04);
                    int   dir  = vectors[v] & 0x03;

                    if (plot && pass == 0)
                    {
                        minX = std::min (minX, x);
                        minY = std::min (minY, y);
                        maxX = std::max (maxX, x);
                        maxY = std::max (maxY, y);
                    }
                    else if (plot && x >= left && x < left + kCell && y >= top && y < top + kCell)
                    {
                        outFile.bgra[(size_t) y * (size_t) outFile.width + (size_t) x] = 0xFFFFFFFF;
                    }

                    x += kDx[dir];
                    y += kDy[dir];
                }
            }
        }

        DrawNumber (outFile, left, top + kCell, shape + 1);
    }

Error:
    return hr;
}
