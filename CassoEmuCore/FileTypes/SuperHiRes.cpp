#include "Pch.h"

#include "FileTypes/SuperHiRes.h"
#include "FileTypes/PackBytes.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::MatchesScreen
//
//  $C1/$0000, or a 32 KB binary file whose name ends .PIC or .SHR.
//
////////////////////////////////////////////////////////////////////////////////

bool SuperHiRes::MatchesScreen (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  typed = facts.HasProDosType (kTypePic, 0x0000)
               || (facts.HasProDosType (kTypeBin) && (facts.IsNameEnding (".PIC") || facts.IsNameEnding (".SHR")));



    return typed && bytes.size() >= kPixelBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::MatchesPacked
//
////////////////////////////////////////////////////////////////////////////////

bool SuperHiRes::MatchesPacked (const FileFacts & facts, std::span<const Byte> bytes)
{
    return (facts.HasProDosType (kTypePnt, 0x0001) || facts.HasProDosType (kTypePnt, 0x0002)) && !bytes.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::MatchesPaintworks
//
//  A palette, a background color and 16 patterns ahead of the pixels.
//
////////////////////////////////////////////////////////////////////////////////

bool SuperHiRes::MatchesPaintworks (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kTypePnt, 0x0000) && bytes.size() > 0x222;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::ToArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t SuperHiRes::ToArgb (Byte low, Byte high)
{
    uint32_t  red   = (uint32_t) (high & 0x0F) * 17;
    uint32_t  green = (uint32_t) (low >> 4)    * 17;
    uint32_t  blue  = (uint32_t) (low & 0x0F)  * 17;



    return 0xFF000000 | (red << 16) | (green << 8) | blue;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::ReadPalette
//
//  16 colors of two bytes each; reversed, color 15 first, as Brooks stores
//  them.
//
////////////////////////////////////////////////////////////////////////////////

void SuperHiRes::ReadPalette (std::span<const Byte> bytes, std::array<uint32_t, 16> & outPalette, bool reversed)
{
    for (size_t color = 0; color < 16; color++)
    {
        size_t  at = (reversed ? 15 - color : color) * 2;

        outPalette[color] = (at + 1 < bytes.size()) ? ToArgb (bytes[at], bytes[at + 1]) : 0xFF000000;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::FromScreenBytes
//
////////////////////////////////////////////////////////////////////////////////

void SuperHiRes::FromScreenBytes (std::span<const Byte> bytes, Screen & outScreen)
{
    outScreen = Screen();
    outScreen.pixels.assign (bytes.begin(), bytes.begin() + (ptrdiff_t) std::min (bytes.size(), kPixelBytes));
    outScreen.lines = outScreen.pixels.size() / outScreen.bytesPerLine;

    for (size_t line = 0; line < 200; line++)
    {
        outScreen.scbs.push_back ((kScbOffset + line < bytes.size()) ? bytes[kScbOffset + line] : 0);
    }

    outScreen.palettes.resize (16);

    for (size_t palette = 0; palette < 16; palette++)
    {
        size_t  start = kPaletteStart + palette * 32;

        ReadPalette ((start < bytes.size()) ? bytes.subspan (start) : std::span<const Byte>(), outScreen.palettes[palette]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::Render
//
//  A 640-mode byte is four pixels, each of whose two bits pick a color
//  from the quarter of the palette its place in the byte has. A 320-mode
//  byte is two pixels; with fill mode on, color 0 repeats the last color.
//  A screen of fewer than 200 rows is drawn black below its last row.
//
////////////////////////////////////////////////////////////////////////////////

void SuperHiRes::Render (const Screen & screen, DecodedFile & outFile)
{
    static constexpr Byte  kQuarter[] = { 8, 12, 0, 4 };

    bool    wide   = false;
    size_t  height = std::max<size_t> (screen.lines, 1);
    size_t  drawn  = std::min (screen.lines, screen.pixels.size() / screen.bytesPerLine);



    for (size_t line = 0; line < drawn; line++)
    {
        wide = wide || (line < screen.scbs.size() && (screen.scbs[line] & kMode640) != 0);
    }

    outFile.kind   = DecodedFile::Kind::Picture;
    outFile.width  = (int) (screen.bytesPerLine * (wide ? 4 : 2));
    outFile.height = (int) height;
    outFile.bgra.assign ((size_t) outFile.width * height, 0xFF000000);

    for (size_t line = 0; line < drawn; line++)
    {
        Byte                              scb     = (line < screen.scbs.size()) ? screen.scbs[line] : 0;
        const std::array<uint32_t, 16>  & palette = !screen.linePalettes.empty() ? screen.linePalettes[std::min (line, screen.linePalettes.size() - 1)]
                                                                                  : screen.palettes[(size_t) (scb & 0x0F) % std::max<size_t> (screen.palettes.size(), 1)];
        uint32_t                        * row     = outFile.bgra.data() + line * (size_t) outFile.width;
        uint32_t                          last    = palette[0];
        size_t                            x       = 0;

        for (size_t b = 0; b < screen.bytesPerLine; b++)
        {
            Byte  value = screen.pixels[line * screen.bytesPerLine + b];

            if (scb & kMode640)
            {
                for (int pixel = 0; pixel < 4; pixel++)
                {
                    row[x++] = palette[kQuarter[pixel] + ((value >> (6 - pixel * 2)) & 0x03)];
                }

                continue;
            }

            for (int pixel = 0; pixel < 2; pixel++)
            {
                Byte      index = (Byte) ((value >> (4 - pixel * 4)) & 0x0F);
                uint32_t  color = (index == 0 && (scb & kFill)) ? last : palette[index];

                last     = color;
                row[x++] = color;

                if (wide)
                {
                    row[x++] = color;
                }
            }
        }
    }

    if (drawn < height)
    {
        outFile.notes.push_back (std::format (L"The picture is {} lines short.", height - drawn));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::DecodeScreen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperHiRes::DecodeScreen (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    Screen  screen;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    FromScreenBytes (bytes, screen);
    screen.lines = 200;
    Render (screen, outFile);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::DecodePacked
//
//  $C0/$0001 is a packed screen; $C0/$0002 is Apple Preferred.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperHiRes::DecodePacked (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT            hr       = S_OK;
    std::vector<Byte>  unpacked;
    Screen             screen;
    bool               complete = false;



    UNREFERENCED_PARAMETER (options);

    if (facts.auxType == 0x0002)
    {
        return DecodePreferred (bytes, outFile, outError);
    }

    PackBytes::Unpack (bytes, kScreenBytes, unpacked);

    complete = unpacked.size() >= kPixelBytes;
    CBRFEx (complete || !unpacked.empty(), HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The packed picture doesn't unpack.");

    FromScreenBytes (unpacked, screen);
    screen.lines = 200;
    Render (screen, outFile);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::DecodePaintworks
//
//  The pixels unpack to as many lines as there are, 396 on the Paintworks
//  disks and 200 elsewhere, all 320 mode in the one palette.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperHiRes::DecodePaintworks (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    constexpr size_t  kPixelsAt = 0x222;
    constexpr size_t  kMaxLines = 400;

    HRESULT  hr     = S_OK;
    Screen   screen;
    bool     pixels = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    screen.palettes.resize (1);
    ReadPalette (bytes, screen.palettes[0]);
    PackBytes::Unpack (bytes.subspan (kPixelsAt), kMaxLines * screen.bytesPerLine, screen.pixels);

    pixels = screen.pixels.size() >= screen.bytesPerLine;
    CBRFEx (pixels, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The Paintworks picture doesn't unpack.");

    screen.lines = screen.pixels.size() / screen.bytesPerLine;
    screen.scbs.assign (screen.lines, 0);
    Render (screen, outFile);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes::DecodePreferred
//
//  Blocks of a length, a name and data. MAIN has the master mode, the
//  pixels in a line, the palettes, and each line's packed length and mode;
//  MULTIPAL has a palette for each line.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperHiRes::DecodePreferred (std::span<const Byte> bytes, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT  hr       = S_OK;
    Screen   screen;
    size_t   at       = 0;
    bool     haveMain = false;



    auto  read16 = [&bytes] (size_t offset) { return (offset + 1 < bytes.size()) ? (size_t) (bytes[offset] | (bytes[offset + 1] << 8)) : 0; };

    while (at + 5 < bytes.size())
    {
        size_t       length  = read16 (at) | (read16 (at + 2) << 16);
        size_t       nameLen = bytes[at + 4];
        std::string  name ((const char *) bytes.data() + at + 5, std::min (nameLen, bytes.size() - at - 5));
        size_t       data    = at + 5 + nameLen;

        if (length < 5 || at + length > bytes.size())
        {
            break;
        }

        if (name == "MAIN" && data + 6 < at + length)
        {
            size_t  master    = read16 (data);
            size_t  perLine   = read16 (data + 2);
            size_t  tables    = read16 (data + 4);
            size_t  cursor    = data + 6;
            size_t  lines     = 0;
            size_t  packedAt  = 0;
            bool    is640     = (master & kMode640) != 0;

            screen.palettes.resize (std::max<size_t> (tables, 1));

            for (size_t table = 0; table < tables; table++, cursor += 32)
            {
                ReadPalette (bytes.subspan (std::min (cursor, bytes.size())), screen.palettes[table]);
            }

            lines               = read16 (cursor);
            screen.bytesPerLine = std::max<size_t> (1, (perLine + (is640 ? 3 : 1)) / (is640 ? 4 : 2));
            screen.lines        = lines;
            packedAt            = cursor + 2 + lines * 4;

            for (size_t line = 0; line < lines && packedAt <= at + length; line++)
            {
                size_t             packed = read16 (cursor + 2 + line * 4);
                size_t             mode   = read16 (cursor + 4 + line * 4);
                std::vector<Byte>  row;

                PackBytes::Unpack (bytes.subspan (packedAt, std::min (packed, at + length - packedAt)), screen.bytesPerLine, row);
                row.resize (screen.bytesPerLine, 0);
                screen.pixels.insert (screen.pixels.end(), row.begin(), row.end());
                screen.scbs.push_back ((Byte) mode);
                packedAt += packed;
            }

            haveMain = true;
        }
        else if (name == "MULTIPAL" && data + 2 <= at + length)
        {
            size_t  tables = read16 (data);

            screen.linePalettes.resize (tables);

            for (size_t table = 0; table < tables; table++)
            {
                ReadPalette (bytes.subspan (std::min (data + 2 + table * 32, bytes.size())), screen.linePalettes[table]);
            }
        }

        at += length;
    }

    CBRFEx (haveMain, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The Apple Preferred file has no picture.");

    Render (screen, outFile);

Error:
    return hr;
}
