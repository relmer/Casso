#include "Pch.h"

#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Update/Sha256Digest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::IsCatalogOnTrackZero
//
//  Judged from track 0 alone, so the result for a full image and for the track
//  read first is the same.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendDisk::IsCatalogOnTrackZero (const std::vector<Byte> & image)
{
    constexpr size_t   kTrackBytes = (size_t) NibblizationLayer::kSectorsPerTrack * NibblizationLayer::kSectorByteSize;
    std::vector<Byte>  zero (image.size(), 0);



    if (image.size() != (size_t) NibblizationLayer::kImageByteSize)
    {
        return false;
    }

    std::copy_n (image.begin(), kTrackBytes, zero.begin());

    return VolumeImage::DetectFilesystem (zero) == VolumeKind::ProDos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::ComputeFingerprint
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDisk::ComputeFingerprint (const std::vector<Byte> & image, std::string & outHex)
{
    constexpr size_t   kTrackBytes = (size_t) NibblizationLayer::kSectorsPerTrack * NibblizationLayer::kSectorByteSize;
    HRESULT            hr          = S_OK;
    bool               sized       = image.size() == (size_t) NibblizationLayer::kImageByteSize;
    std::vector<Byte>  tracks;



    CBRAEx (sized, E_INVALIDARG);

    tracks.assign (image.begin(), image.begin() + (ptrdiff_t) kTrackBytes);

    if (!IsCatalogOnTrackZero (image))
    {
        tracks.insert (tracks.end(), image.begin() + (ptrdiff_t) (kDosCatalogTrack * kTrackBytes), image.begin() + (ptrdiff_t) ((kDosCatalogTrack + 1) * kTrackBytes));
    }

    hr = ComputeHash (tracks, outHex);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::ComputeHash
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendDisk::ComputeHash (std::span<const Byte> image, std::string & outHex)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  digest;



    hr = Sha256Digest::Compute (image, digest);
    CHR (hr);

    outHex = ToHex (digest);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::ToHex
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendDisk::ToHex (const std::vector<Byte> & digest)
{
    std::string  text;



    for (Byte value : digest)
    {
        text += std::format ("{:02x}", value);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::GetCatalogTitle
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendDisk::GetCatalogTitle (const std::vector<Byte> & image)
{
    constexpr Byte  kDosProgramTypes = 0x01 | 0x02 | 0x04 | 0x20 | 0x40;
    HRESULT         listed           = E_FAIL;
    VolumeKind      kind             = VolumeImage::DetectFilesystem (image);
    VolumeListing   listing;
    std::string     title;



    if (kind == VolumeKind::ProDos)
    {
        listed = ProDosVolume (image).Enumerate (listing);

        if (SUCCEEDED (listed) && listing.hasVolumeName)
        {
            title = listing.volumeName;
        }
    }
    else if (kind == VolumeKind::Dos33)
    {
        listed = Dos33Volume (image).Enumerate (listing);

        if (SUCCEEDED (listed))
        {
            auto  program = std::find_if (listing.entries.begin(), listing.entries.end(), [] (const FileEntry & entry) { return (entry.type & kDosProgramTypes) != 0; });

            title = (program == listing.entries.end()) ? std::string() : program->name;
        }
    }
    else if (!TryGetPascalVolume (image, title))
    {
        title.clear();
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::TryGetPascalVolume
//
//  Apple Pascal's directory starts at block 2 with the volume's entry: first
//  block 0, next block 6, type 0, then the name's length, 1 to 7, and the name.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendDisk::TryGetPascalVolume (const std::vector<Byte> & image, std::string & outName)
{
    constexpr int     kDirectoryBlock = 2;
    constexpr size_t  kNextBlock      = 2;
    constexpr size_t  kType           = 4;
    constexpr size_t  kNameLength     = 6;
    constexpr Byte    kAfterDirectory = 6;
    constexpr Byte    kLongestName    = 7;
    size_t            at              = ProDosSkeleton::GetBlockByteOffset (kDirectoryBlock, 0);
    size_t            length          = 0;
    bool              valid           = at + kNameLength + kLongestName < image.size();



    outName.clear();

    valid = valid && image[at] == 0 && image[at + 1] == 0 && image[at + kNextBlock] == kAfterDirectory && image[at + kNextBlock + 1] == 0
                  && image[at + kType] == 0 && image[at + kType + 1] == 0
                  && image[at + kNameLength] >= 1 && image[at + kNameLength] <= kLongestName;

    if (valid)
    {
        length = image[at + kNameLength];
        outName.assign (image.begin() + (ptrdiff_t) (at + kNameLength + 1), image.begin() + (ptrdiff_t) (at + kNameLength + 1 + length));
        valid = std::all_of (outName.begin(), outName.end(), [] (char ch) { return ch > ' ' && ch <= '~'; });
    }

    if (!valid)
    {
        outName.clear();
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::MakeImageTitle
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendDisk::MakeImageTitle (int sequence, const std::string & catalogTitle)
{
    bool         substituted = false;
    std::string  title       = std::format ("{:04}", sequence);



    if (!catalogTitle.empty())
    {
        title += " " + TextEncoding::WideToNarrow (HostFileNaming::MakeHostFolderName (catalogTitle, substituted));
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::GetImageExtension
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendDisk::GetImageExtension (const std::vector<Byte> & image, const std::string & chosen)
{
    if (!chosen.empty())
    {
        return chosen;
    }

    return VolumeImage::DetectFilesystem (image) == VolumeKind::ProDos ? ".po" : ".dsk";
}
