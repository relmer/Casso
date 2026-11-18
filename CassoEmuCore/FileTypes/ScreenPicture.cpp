#include "Pch.h"

#include "FileTypes/ScreenPicture.h"
#include "Core/MemoryBus.h"
#include "FileTypes/PackBytes.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::MatchesHiRes
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenPicture::MatchesHiRes (const FileFacts & facts, std::span<const Byte> bytes)
{
    Word  address = 0;



    if (GetBinaryAddress (facts, address))
    {
        return PicturePreview::Choose (address, bytes.size()) == PicturePreview::Mode::HiRes;
    }

    if (IsScreenFile (facts))
    {
        return bytes.size() >= kHiResMin && bytes.size() <= PicturePreview::kHiResLength;
    }

    return IsPacked (facts, kAuxPacked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::MatchesDoubleHiRes
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenPicture::MatchesDoubleHiRes (const FileFacts & facts, std::span<const Byte> bytes)
{
    Word  address = 0;



    if (GetBinaryAddress (facts, address))
    {
        return PicturePreview::IsDoubleHiRes (PicturePreview::Choose (address, bytes.size()));
    }

    if (IsScreenFile (facts))
    {
        return bytes.size() >= kDoubleHiResMin && bytes.size() <= PicturePreview::kDoubleHiResLength;
    }

    return IsPacked (facts, kAuxPackedDouble);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::MatchesLoRes
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenPicture::MatchesLoRes (const FileFacts & facts, std::span<const Byte> bytes)
{
    Word  address = 0;



    return GetBinaryAddress (facts, address) && PicturePreview::Choose (address, bytes.size()) == PicturePreview::Mode::LoRes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::DecodeHiRes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenPicture::DecodeHiRes (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    return Decode (FileFormat::HiResScreen, bytes, facts, options, outFile, outError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::DecodeDoubleHiRes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenPicture::DecodeDoubleHiRes (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    return Decode (FileFormat::DoubleHiResScreen, bytes, facts, options, outFile, outError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::DecodeLoRes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenPicture::DecodeLoRes (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    return Decode (FileFormat::LoResScreen, bytes, facts, options, outFile, outError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::Unpack
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenPicture::Unpack (FileFormat format, std::span<const Byte> bytes, const FileFacts & facts, std::vector<Byte> & outScreen, Word & outAddress, std::wstring & outError)
{
    HRESULT  hr     = S_OK;
    size_t   length = (format == FileFormat::DoubleHiResScreen) ? PicturePreview::kDoubleHiResLength : PicturePreview::kHiResLength;



    outScreen.clear();
    outAddress = PicturePreview::kHiResPage1;

    if (GetBinaryAddress (facts, outAddress))
    {
        outScreen.assign (bytes.begin(), bytes.end());
        return S_OK;
    }

    if (IsPacked (facts, (format == FileFormat::DoubleHiResScreen) ? kAuxPackedDouble : kAuxPacked))
    {
        PackBytes::Unpack (bytes, length, outScreen);
        CBREx (outScreen.size() == length, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        return S_OK;
    }

    outScreen.assign (bytes.begin(), bytes.end());

Error:
    if (FAILED (hr))
    {
        outError = L"The packed picture doesn't unpack.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::ChooseMode
//
//  A double hi-res picture left to Auto is chosen as the preview chooses
//  one, by its name and its dots.
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Mode ScreenPicture::ChooseMode (FileFormat format, const FileFacts & facts, std::span<const Byte> screen, const DecodeOptions & options)
{
    if (format == FileFormat::HiResScreen)
    {
        return PicturePreview::Mode::HiRes;
    }

    if (format == FileFormat::LoResScreen)
    {
        return PicturePreview::Mode::LoRes;
    }

    switch (options.doubleHiRes)
    {
        case DoubleHiResForm::BlackAndWhite:  return PicturePreview::Mode::DoubleHiResMono;
        case DoubleHiResForm::SimpleColor:    return PicturePreview::Mode::DoubleHiRes;
        case DoubleHiResForm::WindowedColor:  return PicturePreview::Mode::DoubleHiResWindowed;
        default:                              break;
    }

    return PicturePreview::Choose (PicturePreview::kHiResPage1, PicturePreview::kDoubleHiResLength, facts.name, screen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::ChooseMonitor
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Monitor ScreenPicture::ChooseMonitor (FileFormat format, const DecodeOptions & options)
{
    return (format == FileFormat::HiResScreen && options.hiResMono) ? PicturePreview::Monitor::White : PicturePreview::Monitor::Color;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::IsBinary
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenPicture::IsBinary (const FileFacts & facts)
{
    return (facts.IsDos33() && facts.type == kTypeDos33Binary) || facts.HasProDosType (kTypeProDosBinary);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::GetBinaryAddress
//
//  A DOS 3.3 binary's load address, or a ProDOS binary's aux type.
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenPicture::GetBinaryAddress (const FileFacts & facts, Word & outAddress)
{
    if (!IsBinary (facts))
    {
        return false;
    }

    if (facts.hasLoadAddress)
    {
        outAddress = facts.loadAddress;
        return true;
    }

    if (facts.IsProDos() && facts.hasAuxType)
    {
        outAddress = facts.auxType;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::IsScreenFile
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenPicture::IsScreenFile (const FileFacts & facts)
{
    return facts.HasProDosType (kTypePicture) && facts.hasAuxType && facts.auxType < kAuxPacked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::IsPacked
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenPicture::IsPacked (const FileFacts & facts, Word auxType)
{
    return facts.HasProDosType (kTypePicture, auxType);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture::Decode
//
//  The screen is drawn as the preview draws it, from a bus that is never
//  read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenPicture::Decode (FileFormat format, std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT            hr      = S_OK;
    std::vector<Byte>  screen;
    Word               address = 0;
    MemoryBus          bus;



    hr = Unpack (format, bytes, facts, screen, address, outError);
    CHR (hr);

    outFile.kind = DecodedFile::Kind::Picture;

    hr = PicturePreview::Render (ChooseMode (format, facts, screen, options), screen, address, bus,
                                 outFile.bgra, outFile.width, outFile.height, ChooseMonitor (format, options));
    CHR (hr);

Error:
    return hr;
}
