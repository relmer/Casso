#include "Pch.h"

#include "FileTypes/SuperHiRes.h"
#include "FileTypes/PackBytes.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200::MatchesBrooks
//
//  $C1/$0002, or a binary file of its size whose name ends .3200.
//
////////////////////////////////////////////////////////////////////////////////

bool SuperHiRes3200::MatchesBrooks (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  typed = facts.HasProDosType (0xC1, 0x0002) || (facts.HasProDosType (0x06) && facts.IsNameEnding (".3200"));



    return typed && bytes.size() >= kBrooksBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200::Matches3201
//
//  A binary file ending .3201 that starts with "APP" in high ASCII and a
//  NUL.
//
////////////////////////////////////////////////////////////////////////////////

bool SuperHiRes3200::Matches3201 (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.IsNameEnding (".3201") && bytes.size() > 4 + kPaletteBytes
        && bytes[0] == 0xC1 && bytes[1] == 0xD0 && bytes[2] == 0xD0 && bytes[3] == 0x00;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200::MatchesDreamGrafix
//
//  $C0/$8005 with DreamGrafix's footer.
//
////////////////////////////////////////////////////////////////////////////////

bool SuperHiRes3200::MatchesDreamGrafix (const FileFacts & facts, std::span<const Byte> bytes)
{
    static constexpr char  kSignature[] = "\x0A" "DreamWorld";



    return facts.HasProDosType (0xC0, 0x8005) && bytes.size() > kFooterBytes
        && memcmp (bytes.data() + bytes.size() - 11, kSignature, 11) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200::DrawWithLinePalettes
//
//  Each line's palette is in reverse order, color 15 first.
//
////////////////////////////////////////////////////////////////////////////////

void SuperHiRes3200::DrawWithLinePalettes (std::span<const Byte> pixels, std::span<const Byte> palettes, DecodedFile & outFile)
{
    SuperHiRes::Screen  screen;



    screen.pixels.assign (pixels.begin(), pixels.begin() + (ptrdiff_t) std::min (pixels.size(), SuperHiRes::kPixelBytes));
    screen.scbs.assign (200, 0);
    screen.linePalettes.resize (200);
    screen.lines = 200;

    for (size_t line = 0; line < 200; line++)
    {
        size_t  start = line * 32;

        SuperHiRes::ReadPalette ((start < palettes.size()) ? palettes.subspan (start) : std::span<const Byte>(), screen.linePalettes[line], true);
    }

    SuperHiRes::Render (screen, outFile);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200::DecodeBrooks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperHiRes3200::DecodeBrooks (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    DrawWithLinePalettes (bytes.first (SuperHiRes::kPixelBytes), bytes.subspan (SuperHiRes::kPixelBytes), outFile);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200::Decode3201
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperHiRes3200::Decode3201 (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  pixels;
    bool               ok     = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    PackBytes::Unpack (bytes.subspan (4 + kPaletteBytes), SuperHiRes::kPixelBytes, pixels);

    ok = !pixels.empty();
    CBRFEx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The .3201 picture doesn't unpack.");

    DrawWithLinePalettes (pixels, bytes.subspan (4, kPaletteBytes), outFile);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200::DecodeDreamGrafix
//
//  A 256-color picture unpacks to the screen's layout; a 3200-color one to
//  the pixels and Brooks's palettes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperHiRes3200::DecodeDreamGrafix (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT             hr       = S_OK;
    size_t              footer   = bytes.size() - kFooterBytes;
    bool                many     = (bytes[footer] | (bytes[footer + 1] << 8)) == 1;
    size_t              full     = many ? SuperHiRes::kPixelBytes + kPaletteBytes : SuperHiRes::kScreenBytes;
    std::vector<Byte>   unpacked;
    SuperHiRes::Screen  screen;
    bool                whole    = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    hr = UnpackLzw (bytes.first (footer), full, unpacked);
    CHRF (hr, outError = L"The DreamGrafix picture doesn't unpack.");

    whole = unpacked.size() >= full;
    CBRFEx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = std::format (L"The DreamGrafix picture unpacks to {} bytes, not {}.", unpacked.size(), full));

    if (many)
    {
        DrawWithLinePalettes (std::span<const Byte> (unpacked).first (SuperHiRes::kPixelBytes), std::span<const Byte> (unpacked).subspan (SuperHiRes::kPixelBytes), outFile);
    }
    else
    {
        SuperHiRes::FromScreenBytes (unpacked, screen);
        screen.lines = 200;
        SuperHiRes::Render (screen, outFile);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SuperHiRes3200::UnpackLzw
//
//  The width grows by a bit each time the next free code equals the
//  width's limit, up to 12 bits. A code equal to the next free one is the
//  previous string followed by its first byte.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SuperHiRes3200::UnpackLzw (std::span<const Byte> in, size_t limit, std::vector<Byte> & outBytes)
{
    HRESULT             hr       = S_OK;
    std::vector<int>    prefix (4096, 0);
    std::vector<Byte>   suffix (4096, 0);
    std::vector<Byte>   stack;
    int                 width    = 9;
    int                 freeCode = kFirstFree;
    int                 oldCode  = -1;
    Byte                first    = 0;
    uint64_t            bit      = 0;



    outBytes.clear();

    while (outBytes.size() < limit && bit + (uint64_t) width <= (uint64_t) in.size() * 8)
    {
        int  code   = 0;
        int  inCode = 0;
        bool known  = false;

        for (int b = 0; b < width; b++, bit++)
        {
            code |= ((in[(size_t) (bit / 8)] >> (bit % 8)) & 1) << b;
        }

        if (code == kEnd)
        {
            break;
        }

        if (code == kClear)
        {
            width    = 9;
            freeCode = kFirstFree;
            oldCode  = -1;
            continue;
        }

        if (oldCode < 0)
        {
            known = code < 256;
            CBREx (known, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            first   = (Byte) code;
            oldCode = code;
            outBytes.push_back (first);
            continue;
        }

        inCode = code;
        stack.clear();

        known = code <= freeCode;
        CBREx (known, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        if (code == freeCode)
        {
            stack.push_back (first);
            code = oldCode;
        }

        while (code >= 256)
        {
            stack.push_back (suffix[(size_t) code]);
            code = prefix[(size_t) code];
        }

        first = (Byte) code;
        stack.push_back (first);
        outBytes.insert (outBytes.end(), stack.rbegin(), stack.rend());

        if (freeCode < 4096)
        {
            prefix[(size_t) freeCode] = oldCode;
            suffix[(size_t) freeCode] = first;
            freeCode++;
        }

        oldCode = inCode;

        if (freeCode >= (1 << width) && width < 12)
        {
            width++;
        }
    }

    if (outBytes.size() > limit)
    {
        outBytes.resize (limit);
    }

Error:
    return hr;
}
