#include "Pch.h"

#include "Machines/Apple2/Common/DiskImageConverter.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::GetTargets
//
//  A 140K disk is offered every sector order, a 2IMG, a WOZ and both nibble
//  formats; a larger volume is offered the block-order formats. A
//  container's file is larger than its disk, so any 2IMG or Disk Copy file
//  under two floppies' size is taken to be a floppy. A .nib, .nb2 or .woz is
//  offered the sector orders and the nibble formats. The source's format is
//  left out.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageConverter::GetTargets (const std::string & path, uint64_t fileBytes, std::vector<DiskImageFormat> & outTargets)
{
    static constexpr DiskImageFormat  kFloppy[]   = { DiskImageFormat::Dsk, DiskImageFormat::Po, DiskImageFormat::TwoImg, DiskImageFormat::Woz,
                                                    DiskImageFormat::Nib, DiskImageFormat::Nb2 };
    static constexpr DiskImageFormat  kVolume[]   = { DiskImageFormat::Po, DiskImageFormat::TwoImg, DiskImageFormat::Hdv };
    std::string                       extension   = GetLowerExtension (path);
    bool                              isTwoImg    = extension == ".2mg" || extension == ".2img";
    bool                              isContainer = isTwoImg || extension == ".dc" || extension == ".dc42";
    bool                              isRaw       = extension == ".dsk" || extension == ".do" || extension == ".po" || extension == ".hdv";
    bool                              isFloppy    = isContainer ? fileBytes < 2 * (uint64_t) NibbleFormat::kImageByteSize : fileBytes == (uint64_t) NibbleFormat::kImageByteSize;



    outTargets.clear();

    if (extension == ".d13")
    {
        if (fileBytes == (uint64_t) ThirteenSectorCodec::kImageByteSize)
        {
            outTargets.push_back (DiskImageFormat::Woz);
        }

        return;
    }

    if (IsBitStream (extension))
    {
        outTargets.push_back (DiskImageFormat::Dsk);
        outTargets.push_back (DiskImageFormat::Po);

        if (extension != ".nib")
        {
            outTargets.push_back (DiskImageFormat::Nib);
        }

        if (extension != ".nb2")
        {
            outTargets.push_back (DiskImageFormat::Nb2);
        }

        return;
    }

    if (!isContainer && !isRaw)
    {
        return;
    }

    for (DiskImageFormat format : isFloppy ? std::span<const DiskImageFormat> (kFloppy) : std::span<const DiskImageFormat> (kVolume))
    {
        bool  isSame = (format == DiskImageFormat::Dsk    && (extension == ".dsk" || extension == ".do"))
                    || (format == DiskImageFormat::Po     && extension == ".po")
                    || (format == DiskImageFormat::TwoImg && isTwoImg)
                    || (format == DiskImageFormat::Hdv    && extension == ".hdv");

        if (!isSame)
        {
            outTargets.push_back (format);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::GetExtension
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DiskImageConverter::GetExtension (DiskImageFormat format)
{
    switch (format)
    {
        case DiskImageFormat::Dsk:    return L".dsk";
        case DiskImageFormat::Po:     return L".po";
        case DiskImageFormat::TwoImg: return L".2mg";
        case DiskImageFormat::Hdv:    return L".hdv";
        case DiskImageFormat::Woz:    return L".woz";
        case DiskImageFormat::Nib:    return L".nib";
        case DiskImageFormat::Nb2:    return L".nb2";
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::Convert
//
//  The image is loaded into the volume layer's buffer, which is in DOS
//  logical order for a 140K disk, in block order for a larger volume and
//  in the layout of a .d13 file for a 13-sector disk. A .nib or .woz must
//  decode whole: a sector image of a track with missing sectors would have
//  zeros for them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageConverter::Convert (const std::vector<Byte> & fileBytes, const std::string & path, DiskImageFormat target, std::vector<Byte> & outBytes, std::string & outReason)
{
    HRESULT                       hr         = S_OK;
    std::vector<DiskImageFormat>  targets;
    std::vector<Byte>             sectors;
    SectorDecodeReport            report;
    bool                          isOffered  = false;
    bool                          isFloppy   = false;
    bool                          isThirteen = false;
    bool                          isBits     = IsBitStream (GetLowerExtension (path));



    outBytes.clear();
    outReason.clear();

    GetTargets (path, fileBytes.size(), targets);

    isOffered = std::find (targets.begin(), targets.end(), target) != targets.end();
    CBRFEx (isOffered, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), outReason = "The image can't be converted to that format.");

    hr = VolumeImage::Load (fileBytes, path, sectors, report);
    CHRF (hr, outReason = "The file doesn't open as a disk image.");

    if (isBits)
    {
        hr = CheckEveryTrack (report, outReason);
        CHR (hr);
    }

    isFloppy   = sectors.size() == (size_t) NibbleFormat::kImageByteSize;
    isThirteen = sectors.size() == ThirteenSectorCodec::kImageByteSize;

    if (target == DiskImageFormat::Woz)
    {
        hr = BuildWoz (sectors, outBytes, outReason);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    if (target == DiskImageFormat::Nib || target == DiskImageFormat::Nb2)
    {
        hr = BuildNibbles (sectors, (target == DiskImageFormat::Nib) ? NibbleImageCodec::kNibTrackSize : NibbleImageCodec::kNb2TrackSize, outBytes, outReason);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    CBRFEx (!isThirteen, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outReason = "A 13-sector disk can't be written as a 16-sector image.");

    if (target == DiskImageFormat::Dsk)
    {
        CBRFEx (isFloppy, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outReason = "Only a 140K disk can be written in DOS order.");

        outBytes = sectors;
    }
    else if (target == DiskImageFormat::TwoImg)
    {
        BuildTwoImg (sectors, outBytes);
    }
    else
    {
        VolumeImage::SaveBlockPayload (sectors, outBytes);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::GetLowerExtension
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskImageConverter::GetLowerExtension (const std::string & path)
{
    std::string  extension = std::filesystem::path (path).extension().string();



    std::transform (extension.begin(), extension.end(), extension.begin(),
                    [] (char c) { return (char) std::tolower ((unsigned char) c); });

    return extension;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::IsBitStream
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageConverter::IsBitStream (const std::string & extension)
{
    return extension == ".nib" || extension == ".nb2" || extension == ".woz";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::CheckEveryTrack
//
//  Every track must have each of its sectors once. An unformatted track
//  fails too, since a copy-protected disk's tracks can be unreadable as
//  sectors without a single address field.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageConverter::CheckEveryTrack (const SectorDecodeReport & report, std::string & outReason)
{
    HRESULT  hr        = S_OK;
    int      count     = report.GetTrackCount();
    bool     hasTracks = count > 0;



    CBRFEx (hasTracks, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outReason = "The disk has no tracks to convert.");

    for (int track = 0; track < count; track++)
    {
        bool  isWhole = report.GetOutcome (track) == TrackDecodeOutcome::Complete;

        CBRFEx (isWhole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
                outReason = std::format ("Track {} has sectors that can't be decoded. The disk may be copy-protected or damaged.", track));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::BuildWoz
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageConverter::BuildWoz (const std::vector<Byte> & sectors, std::vector<Byte> & outBytes, std::string & outReason)
{
    HRESULT    hr         = S_OK;
    bool       isFloppy   = sectors.size() == (size_t) NibbleFormat::kImageByteSize;
    bool       isThirteen = sectors.size() == ThirteenSectorCodec::kImageByteSize;
    DiskImage  image;



    CBRFEx (isFloppy || isThirteen, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outReason = "Only a 5.25-inch disk can be written as a WOZ.");

    if (isThirteen)
    {
        hr = ThirteenSectorCodec::Nibblize (sectors, NibbleFormat::kDefaultVolume, image);
    }
    else
    {
        hr = Nibblizer::NibblizeDsk (sectors, image);
    }

    CHRA (hr);

    hr = WozLoader::Serialize (image, outBytes);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::BuildNibbles
//
//  A 140K disk nibblized as a drive would read it, each track padded or
//  cut to the format's track size.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageConverter::BuildNibbles (const std::vector<Byte> & sectors, size_t trackSize, std::vector<Byte> & outBytes, std::string & outReason)
{
    HRESULT    hr       = S_OK;
    bool       isFloppy = sectors.size() == (size_t) NibbleFormat::kImageByteSize;
    DiskImage  image;



    CBRFEx (isFloppy, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outReason = "Only a 140K disk can be written as nibbles.");

    hr = Nibblizer::NibblizeDsk (sectors, image);
    CHRA (hr);

    hr = NibbleImageCodec::Build (image, trackSize, outBytes);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter::BuildTwoImg
//
//  The blocks in ProDOS order, the order most 2IMG files use.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageConverter::BuildTwoImg (const std::vector<Byte> & sectors, std::vector<Byte> & outBytes)
{
    TwoImgHeader       header;
    std::vector<Byte>  payload;



    VolumeImage::SaveBlockPayload (sectors, payload);

    header.format     = TwoImgHeader::kFormatProDosOrder;
    header.blockCount = (uint32_t) (payload.size() / ProDosSkeleton::kBlockByteSize);

    TwoImgContainer::Build (header, payload, outBytes);
}
