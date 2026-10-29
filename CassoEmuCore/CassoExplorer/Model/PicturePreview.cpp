#include "Pch.h"

#include "CassoExplorer/Model/PicturePreview.h"
#include "Core/MemoryBus.h"
#include "Devices/Printer/PngCodec.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Video/MonochromeTint.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::Choose
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Mode PicturePreview::Choose (Word address, size_t length, std::string_view name, std::span<const Byte> bytes)
{
    bool  hiResAddress = address == kHiResPage1 || address == kHiResPage2;
    bool  hiResLength  = length == kHiResLength || length == kHiResShort;
    bool  loResAddress = address == kLoResPage1 || address == kLoResPage2;



    if (hiResAddress && hiResLength)
    {
        return Mode::HiRes;
    }

    if (address == kHiResPage1 && length == kDoubleHiResLength)
    {
        if (IsMonochromeName (name))
        {
            return Mode::DoubleHiResMono;
        }

        if (IsColorName (name) || bytes.size() != kDoubleHiResLength)
        {
            return Mode::DoubleHiRes;
        }

        return (MeasurePeriodFour (bytes) >= kColorPeriodicity) ? Mode::DoubleHiRes : Mode::DoubleHiResMono;
    }

    if (loResAddress && length == kLoResLength)
    {
        return Mode::LoRes;
    }

    return Mode::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::Render
//
//  The bytes are placed at their load address inside a scratch address space
//  and the mode renders from that span, page 2 selected when the address is
//  the second page's. A double hi-res file is the auxiliary half followed by
//  the main half, which is the order the common 16 KB format stores them in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PicturePreview::Render (
    Mode                     mode,
    std::span<const Byte>    bytes,
    Word                     address,
    MemoryBus              & bus,
    std::vector<uint32_t>  & outBgra,
    int                    & outWidth,
    int                    & outHeight,
    Monitor                  monitor,
    uint32_t                 customArgb)
{
    HRESULT            hr     = S_OK;
    bool               fits   = (size_t) address + bytes.size() <= kAddressSpace;
    bool               known  = mode != Mode::None;
    bool               mono   = monitor != Monitor::Color;
    std::vector<Byte>  main (kAddressSpace, 0);
    std::vector<Byte>  aux;



    CBREx (known, E_INVALIDARG);
    CBREx (fits,  E_INVALIDARG);

    outWidth  = kWidth;
    outHeight = kHeight;
    outBgra.assign ((size_t) kWidth * kHeight, 0xFF000000u);

    if (mode == Mode::DoubleHiResMono)
    {
        RenderMonochrome (bytes, address, outBgra);
        Tint (outBgra, monitor, customArgb);
        return S_OK;
    }

    if (mode == Mode::DoubleHiRes)
    {
        size_t  half = bytes.size() / 2;

        aux.assign (kAddressSpace, 0);

        std::copy (bytes.begin(), bytes.begin() + (ptrdiff_t) half, aux.begin() + address);
        std::copy (bytes.begin() + (ptrdiff_t) half, bytes.end(), main.begin() + address);
    }
    else
    {
        std::copy (bytes.begin(), bytes.end(), main.begin() + address);
    }

    switch (mode)
    {
        case Mode::HiRes:
        {
            AppleHiResMode  video (bus);

            video.SetPage2      (address == kHiResPage2);
            video.SetMonochrome (mono);
            video.Render (main.data(), outBgra.data(), kWidth, kHeight);
            break;
        }

        case Mode::DoubleHiRes:
        {
            AppleDoubleHiResMode  video (bus);

            video.SetAuxMemory  (aux.data());
            video.SetMainMemory (main.data());
            video.SetMonochrome (mono);
            video.Render (main.data(), outBgra.data(), kWidth, kHeight);
            break;
        }

        case Mode::LoRes:
        {
            AppleLoResMode  video (bus);

            video.SetPage2 (address == kLoResPage2);
            video.Render (main.data(), outBgra.data(), kWidth, kHeight);
            break;
        }

        default:
            break;
    }

    Tint (outBgra, monitor, customArgb);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::Tint
//
//  Casso's own tints, as its screen applies them after drawing a frame. The
//  custom monitor scales its color by each pixel's brightness, as amber's
//  tint scales its phosphor's.
//
////////////////////////////////////////////////////////////////////////////////

void PicturePreview::Tint (std::vector<uint32_t> & bgra, Monitor monitor, uint32_t customArgb)
{
    float  red   = (float) ((customArgb >> 16) & 0xFF) / 255.0f;
    float  green = (float) ((customArgb >>  8) & 0xFF) / 255.0f;
    float  blue  = (float) ( customArgb        & 0xFF) / 255.0f;



    for (uint32_t & pixel : bgra)
    {
        switch (monitor)
        {
            case Monitor::Green:  pixel = Casso::Video::TintGreenMono (pixel); break;
            case Monitor::Amber:  pixel = Casso::Video::TintAmberMono (pixel); break;
            case Monitor::White:  pixel = Casso::Video::TintWhiteMono (pixel); break;

            case Monitor::Custom:
            {
                uint8_t  level = Casso::Video::ComputeLuminance (pixel);

                pixel = Casso::Video::MakePixel ((uint8_t) (level * red), (uint8_t) (level * green), (uint8_t) (level * blue));
                break;
            }

            default:              break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::IsMonochromeName
//
//  The A2FC/A2FM naming: a double hi-res file ending .A2FM is monochrome.
//
////////////////////////////////////////////////////////////////////////////////

bool PicturePreview::IsMonochromeName (std::string_view name)
{
    return HasSuffix (name, ".A2FM");
}





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::IsColorName
//
////////////////////////////////////////////////////////////////////////////////

bool PicturePreview::IsColorName (std::string_view name)
{
    return HasSuffix (name, ".A2FC");
}





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::HasSuffix
//
//  In any case, as names from a ProDOS catalog are upper case and a host's
//  are as typed.
//
////////////////////////////////////////////////////////////////////////////////

bool PicturePreview::HasSuffix (std::string_view name, std::string_view suffix)
{
    bool  matches = name.size() >= suffix.size();



    for (size_t i = 0; matches && i < suffix.size(); i++)
    {
        matches = toupper ((unsigned char) name[name.size() - suffix.size() + i]) == toupper ((unsigned char) suffix[i]);
    }

    return matches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::MeasurePeriodFour
//
//  Every other row read as 560 dots, the power of its spectrum at period
//  four and its harmonic at period 4/3, against the average power at the
//  periods around them (about 3.1 to 5.6 dots, away from 4). A color
//  picture's cells are four-dot patterns, so its rows ring at period four;
//  monochrome art has no reason to favor four over its neighbors.
//
////////////////////////////////////////////////////////////////////////////////

double PicturePreview::MeasurePeriodFour (std::span<const Byte> bytes)
{
    static constexpr int     s_kDots   = 560;
    static constexpr int     s_kFirst  = 100;
    static constexpr int     s_kLast   = 180;
    static constexpr double  s_kTwoPi  = 6.283185307179586;
    size_t                   half      = bytes.size() / 2;
    double                   at        = 0.0;
    double                   around    = 0.0;
    int                      aroundN   = 0;
    std::vector<double>      dots (s_kDots, 0.0);



    for (int y = 0; y < kHeight / 2; y += 2)
    {
        size_t  row  = (size_t) (((y & 7) << 10) | (((y >> 3) & 7) << 7)) + (size_t) ((y >> 6) * 40);
        int     x    = 0;
        double  mean = 0.0;

        for (int col = 0; col < 40; col++)
        {
            for (size_t bank : { (size_t) 0, half })
            {
                Byte  value = (row + (size_t) col + bank < bytes.size()) ? bytes[row + (size_t) col + bank] : (Byte) 0;

                for (int bit = 0; bit < 7; bit++, x++)
                {
                    dots[(size_t) x] = (double) ((value >> bit) & 1);
                    mean            += dots[(size_t) x];
                }
            }
        }

        mean /= s_kDots;

        for (int k = s_kFirst; k <= s_kLast + (3 * s_kDots / 4 - s_kLast); k++)
        {
            bool    isPeak   = k == s_kDots / 4 || k == 3 * s_kDots / 4;
            bool    isAround = k <= s_kLast && std::abs (k - s_kDots / 4) > 3;
            double  re       = 0.0;
            double  im       = 0.0;

            if (!isPeak && !isAround)
            {
                continue;
            }

            for (int i = 0; i < s_kDots; i++)
            {
                double  phase = s_kTwoPi * (double) k * (double) i / (double) s_kDots;

                re += (dots[(size_t) i] - mean) * std::cos (phase);
                im -= (dots[(size_t) i] - mean) * std::sin (phase);
            }

            if (isPeak)
            {
                at += re * re + im * im;
            }
            else
            {
                around  += re * re + im * im;
                aroundN += (y == 0) ? 1 : 0;
            }
        }
    }

    return (around > 0.0 && aroundN > 0) ? at / (around / (double) aroundN * 2.0) : 0.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::RenderMonochrome
//
//  Each row interleaves the halves a byte at a time, auxiliary first, seven
//  pixels to a byte, low bit leftmost: 560 dots, each row drawn twice.
//
////////////////////////////////////////////////////////////////////////////////

void PicturePreview::RenderMonochrome (std::span<const Byte> bytes, Word address, std::vector<uint32_t> & outBgra)
{
    size_t  half = bytes.size() / 2;



    UNREFERENCED_PARAMETER (address);

    for (int y = 0; y < kHeight / 2; y++)
    {
        size_t  row = (size_t) (((y & 7) << 10) | (((y >> 3) & 7) << 7)) + (size_t) ((y >> 6) * 40);
        int     x   = 0;

        for (int col = 0; col < 40; col++)
        {
            for (size_t bank : { (size_t) 0, half })
            {
                Byte  value = (row + (size_t) col + bank < bytes.size()) ? bytes[row + (size_t) col + bank] : (Byte) 0;

                for (int bit = 0; bit < 7 && x < kWidth; bit++, x++)
                {
                    uint32_t  argb = ((value >> bit) & 1) ? 0xFFFFFFFFu : 0xFF000000u;

                    outBgra[(size_t) (y * 2)     * kWidth + (size_t) x] = argb;
                    outBgra[(size_t) (y * 2 + 1) * kWidth + (size_t) x] = argb;
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::EncodePng
//
//  Each 0xAARRGGBB pixel becomes R, G, B and an opaque A.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PicturePreview::EncodePng (const std::vector<uint32_t> & bgra, int width, int height, std::vector<Byte> & outPng)
{
    HRESULT    hr    = S_OK;
    size_t     count = bgra.size();
    bool       sized = width > 0 && height > 0 && count == (size_t) width * height;
    RgbaImage  image;



    CBREx (sized, E_INVALIDARG);

    image.width  = width;
    image.height = height;
    image.rgba.resize (count * 4);

    for (size_t i = 0; i < count; i++)
    {
        image.rgba[i * 4 + 0] = (Byte) (bgra[i] >> 16);
        image.rgba[i * 4 + 1] = (Byte) (bgra[i] >> 8);
        image.rgba[i * 4 + 2] = (Byte) bgra[i];
        image.rgba[i * 4 + 3] = 0xFF;
    }

    hr = PngCodec::EncodeRgba (image, 96, outPng);
    CHR (hr);

Error:
    return hr;
}
