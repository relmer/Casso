#include "Pch.h"

#include "Devices/Disk/DiskThumbnailCommand.h"
#include "CommandLineOptions.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "Devices/Disk/DiskHelpPage.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/RgbaImage.h"
#include "DiskCheck/ThumbnailStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailCommand::Run
//
//  One of the three actions: a picture to set, --remove, or --out.
//
////////////////////////////////////////////////////////////////////////////////

void DiskThumbnailCommand::Run (const CommandLineOptions & options, ThumbnailStore & store, DiskCommandResult & result)
{
    HRESULT  hr      = S_OK;
    bool     sets    = !options.disk.picturePath.empty();
    bool     writes  = !options.disk.hostFile.empty();
    int      actions = (sets ? 1 : 0) + (options.disk.remove ? 1 : 0) + (writes ? 1 : 0);
    bool     isOne   = actions == 1;



    CBRF (isOne, (result.diagnostics += DiskHelpPage::ApplyPrefixes (
                      "Error: wrong number of thumbnail actions\n"
                      "       Use one of these: a picture file to set the thumbnail, %Lremove to remove the one the user set,"
                      " or %Lout <file> to write it to a file.\n", options.flagPrefix),
                  result.exitStatus = DiskCommandResult::kNoOutput));

    if (sets)
    {
        Set (options, store, result);
    }
    else if (options.disk.remove)
    {
        Remove (options, store, result);
    }
    else
    {
        WriteOut (options, store, result);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailCommand::ConvertToPng
//
//  Decoded and encoded again, so a file that isn't a picture fails and
//  the store has a PNG whatever the file's format.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskThumbnailCommand::ConvertToPng (const std::vector<Byte> & picture, std::vector<Byte> & outPng)
{
    HRESULT    hr      = S_OK;
    HRESULT    hrCom   = ::CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    RgbaImage  image;



    outPng.clear();

    hr = PngCodec::DecodeRgba (picture, image);
    CHR (hr);

    hr = PngCodec::EncodeRgba (image, kPngDpi, outPng);
    CHR (hr);

Error:
    if (SUCCEEDED (hrCom))
    {
        ::CoUninitialize();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailCommand::SetFromScreen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskThumbnailCommand::SetFromScreen (ThumbnailStore & store, const std::filesystem::path & image, std::span<const uint32_t> bgra, int width, int height)
{
    HRESULT            hr    = S_OK;
    HRESULT            hrCom = ::CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
    std::vector<Byte>  png;



    hr = PicturePreview::EncodePng (std::vector<uint32_t> (bgra.begin(), bgra.end()), width, height, png);
    CHR (hr);

    hr = store.SetByUser (image, png);
    CHR (hr);

Error:
    if (SUCCEEDED (hrCom))
    {
        ::CoUninitialize();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailCommand::Set
//
////////////////////////////////////////////////////////////////////////////////

void DiskThumbnailCommand::Set (const CommandLineOptions & options, ThumbnailStore & store, DiskCommandResult & result)
{
    HRESULT              hr     = S_OK;
    const std::string  & image  = options.disk.imagePath;
    const std::string  & file   = options.disk.picturePath;
    std::ifstream        in (file, std::ios::binary);
    bool                 isOpen = in.is_open();
    std::vector<Byte>    picture;
    std::vector<Byte>    png;



    CBRF (isOpen, result.FailAction (image, "set the thumbnail", file + " couldn't be read"));

    picture.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());

    hr = ConvertToPng (picture, png);
    CHRF (hr, result.FailAction (image, "set the thumbnail", file + " isn't a PNG, JPEG, BMP, GIF, or TIFF picture"));

    hr = store.SetByUser (std::filesystem::path (image), png);
    CHRF (hr, result.FailAction (image, "set the thumbnail", "the image file couldn't be read"));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailCommand::Remove
//
//  A thumbnail made by booting is not removed: the image would only be
//  booted again for a new one.
//
////////////////////////////////////////////////////////////////////////////////

void DiskThumbnailCommand::Remove (const CommandLineOptions & options, ThumbnailStore & store, DiskCommandResult & result)
{
    HRESULT              hr        = S_OK;
    const std::string  & image     = options.disk.imagePath;
    std::vector<Byte>    png;
    ThumbnailRecord      record;
    bool                 isUserSet = false;



    hr = store.Find (std::filesystem::path (image), png, record, isUserSet);
    CHRF (hr, result.FailAction (image, "remove the thumbnail", "the image file couldn't be read"));

    CBRF (isUserSet, result.FailAction (image, "remove the thumbnail", "the image has no thumbnail set by the user"));

    hr = store.Remove (std::filesystem::path (image));
    CHRF (hr, result.FailAction (image, "remove the thumbnail", "the image file couldn't be read"));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailCommand::WriteOut
//
////////////////////////////////////////////////////////////////////////////////

void DiskThumbnailCommand::WriteOut (const CommandLineOptions & options, ThumbnailStore & store, DiskCommandResult & result)
{
    HRESULT              hr        = S_OK;
    const std::string  & image     = options.disk.imagePath;
    const std::string  & file      = options.disk.hostFile;
    std::vector<Byte>    png;
    ThumbnailRecord      record;
    bool                 isUserSet = false;
    bool                 hasPng    = false;
    std::ofstream        out;
    bool                 isGood    = false;



    hr = store.Find (std::filesystem::path (image), png, record, isUserSet);
    CHRF (hr, result.FailAction (image, "write the thumbnail", "the image file couldn't be read"));

    hasPng = !png.empty();
    CBRF (hasPng, result.FailAction (image, "write the thumbnail", "the image has no thumbnail"));

    out.open (file, std::ios::binary | std::ios::trunc);
    out.write ((const char *) png.data(), (std::streamsize) png.size());
    out.close();

    isGood = !out.fail();
    CBRF (isGood, result.FailAction (image, "write the thumbnail", file + " couldn't be written"));

Error:
    return;
}
