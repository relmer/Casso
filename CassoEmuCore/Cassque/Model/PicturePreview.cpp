#include "Pch.h"

#include "Cassque/Model/PicturePreview.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PicturePreview::Choose
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Mode PicturePreview::Choose (Word address, size_t length)
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
        return Mode::DoubleHiRes;
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
    int                    & outHeight)
{
    HRESULT            hr     = S_OK;
    bool               fits   = (size_t) address + bytes.size() <= kAddressSpace;
    bool               known  = mode != Mode::None;
    std::vector<Byte>  main (kAddressSpace, 0);
    std::vector<Byte>  aux;



    CBREx (known, E_INVALIDARG);
    CBREx (fits,  E_INVALIDARG);

    outWidth  = kWidth;
    outHeight = kHeight;
    outBgra.assign ((size_t) kWidth * kHeight, 0xFF000000u);

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

            video.SetPage2 (address == kHiResPage2);
            video.Render (main.data(), outBgra.data(), kWidth, kHeight);
            break;
        }

        case Mode::DoubleHiRes:
        {
            AppleDoubleHiResMode  video (bus);

            video.SetAuxMemory  (aux.data());
            video.SetMainMemory (main.data());
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

Error:
    return hr;
}
