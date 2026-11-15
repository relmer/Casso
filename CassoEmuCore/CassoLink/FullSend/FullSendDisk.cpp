#include "Pch.h"

#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendManifest.h"
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
    VolumeKind         kind = VolumeKind::Unknown;



    if (image.size() != (size_t) NibblizationLayer::kImageByteSize)
    {
        return false;
    }

    std::copy_n (image.begin(), kTrackBytes, zero.begin());

    kind = VolumeImage::DetectFilesystem (zero);

    return kind == VolumeKind::ProDos || kind == VolumeKind::Pascal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::NeedsDirectoryTrack
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendDisk::NeedsDirectoryTrack (const std::vector<Byte> & image)
{
    constexpr size_t   kTrackBytes = (size_t) NibblizationLayer::kSectorsPerTrack * NibblizationLayer::kSectorByteSize;
    std::vector<Byte>  tracks (image.size(), 0);



    if (image.size() != (size_t) NibblizationLayer::kImageByteSize)
    {
        return false;
    }

    std::copy_n (image.begin(), kTrackBytes, tracks.begin());
    std::copy_n (image.begin() + (ptrdiff_t) (kDosCatalogTrack * kTrackBytes), kTrackBytes, tracks.begin() + (ptrdiff_t) (kDosCatalogTrack * kTrackBytes));

    return VolumeImage::DetectFilesystem (tracks) == VolumeKind::Unknown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::AssessDamage
//
////////////////////////////////////////////////////////////////////////////////

const char * FullSendDisk::AssessDamage (const std::vector<Byte> & image, int emptyTracks, int scatteredSectors)
{
    bool  recognized = VolumeImage::DetectFilesystem (image) != VolumeKind::Unknown;



    UNREFERENCED_PARAMETER (scatteredSectors);

    if (emptyTracks != 0)
    {
        return recognized ? FullSendManifest::kStatusUnclear : FullSendManifest::kStatusProtected;
    }

    return recognized ? FullSendManifest::kStatusDamaged : FullSendManifest::kStatusUnclear;
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

        if (NeedsDirectoryTrack (image))
        {
            tracks.insert (tracks.end(), image.begin() + (ptrdiff_t) (kCpmDirectoryTrack * kTrackBytes), image.begin() + (ptrdiff_t) ((kCpmDirectoryTrack + 1) * kTrackBytes));
        }
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
            //  A name with no letter or digit, such as a name of control
            //  characters on a protected disk, is passed over.
            auto  program = std::find_if (listing.entries.begin(), listing.entries.end(), [] (const FileEntry & entry)
            {
                return (entry.type & kDosProgramTypes) != 0 && std::any_of (entry.name.begin(), entry.name.end(), [] (char ch) { return std::isalnum ((unsigned char) ch) != 0; });
            });

            title = (program == listing.entries.end()) ? std::string() : program->name;

            //  Its control characters are dropped, not made legal for a file
            //  name, and the spaces around what is left.
            std::erase_if (title, [] (char ch) { return (unsigned char) ch < ' ' || ch == '\x7F'; });
            title.erase (0, title.find_first_not_of (' '));
            title.erase (title.find_last_not_of (' ') + 1);
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
//  FullSendDisk::GetDosSectorOffset
//
////////////////////////////////////////////////////////////////////////////////

size_t FullSendDisk::GetDosSectorOffset (int track, int sector)
{
    return ((size_t) track * NibblizationLayer::kSectorsPerTrack + (size_t) sector) * NibblizationLayer::kSectorByteSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::FindDosEntries
//
//  The count of a DOS 3.3 catalog's files, and the first one's track/sector
//  list, type without its lock bit, and length in sectors. A catalog entry
//  begins with its list's track: 0 for an entry never used, $FF for a
//  deleted file.
//
////////////////////////////////////////////////////////////////////////////////

int FullSendDisk::FindDosEntries (const std::vector<Byte> & image, Byte & outListTrack, Byte & outListSector, Byte & outType, int & outSectors)
{
    constexpr size_t  kFirstEntry   = 0x0B;
    constexpr size_t  kEntryBytes   = 35;
    constexpr int     kEntries      = 7;
    constexpr size_t  kTypeAt       = 2;
    constexpr size_t  kLengthAt     = 33;
    constexpr Byte    kDeleted      = 0xFF;
    constexpr Byte    kLockBit      = 0x80;
    constexpr int     kMostSectors  = 32;
    int               count         = 0;
    size_t            vtoc          = GetDosSectorOffset (kDosCatalogTrack, 0);
    int               track         = vtoc + 2 < image.size() ? image[vtoc + 1] : 0;
    int               sector        = vtoc + 2 < image.size() ? image[vtoc + 2] : 0;



    for (int hop = 0; hop < kMostSectors && track != 0 && GetDosSectorOffset (track, sector) + NibblizationLayer::kSectorByteSize <= image.size(); hop++)
    {
        size_t  at = GetDosSectorOffset (track, sector);

        for (int entry = 0; entry < kEntries; entry++)
        {
            size_t  from = at + kFirstEntry + (size_t) entry * kEntryBytes;

            if (image[from] == 0 || image[from] == kDeleted)
            {
                continue;
            }

            if (count == 0)
            {
                outListTrack  = image[from];
                outListSector = image[from + 1];
                outType       = (Byte) (image[from + kTypeAt] & ~kLockBit);
                outSectors    = image[from + kLengthAt] | (image[from + kLengthAt + 1] << 8);
            }

            count++;
        }

        track  = image[at + 1];
        sector = image[at + 2];
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::FindLoneProgramTrack
//
////////////////////////////////////////////////////////////////////////////////

int FullSendDisk::FindLoneProgramTrack (const std::vector<Byte> & image)
{
    constexpr Byte  kInteger   = 0x01;
    constexpr Byte  kApplesoft = 0x02;
    Byte            listTrack  = 0;
    Byte            listSector = 0;
    Byte            type       = 0;
    int             sectors    = 0;
    bool            lone       = VolumeImage::DetectFilesystem (image) == VolumeKind::Dos33 && FindDosEntries (image, listTrack, listSector, type, sectors) == 1;



    return (lone && (type == kInteger || type == kApplesoft) && sectors <= kGreetingSectors && listTrack < NibblizationLayer::kTrackCount) ? listTrack : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::IsBlank
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendDisk::IsBlank (const std::vector<Byte> & image, std::span<const int> haveTracks, std::string & outSystem)
{
    constexpr size_t          kFirstPair = 0x0C;
    constexpr size_t          kProgramAt = 2;
    constexpr int             kNoLines   = 3;
    VolumeKind                kind       = VolumeImage::DetectFilesystem (image);
    bool                      known      = kind == VolumeKind::Dos33 || kind == VolumeKind::ProDos || kind == VolumeKind::Pascal || kind == VolumeKind::Cpm;
    std::unique_ptr<IVolume>  volume     = known ? VolumeImage::CreateVolume (kind, image) : nullptr;
    VolumeListing             listing;
    HRESULT                   hrList     = volume != nullptr ? volume->Enumerate (listing) : E_FAIL;
    bool                      listed     = SUCCEEDED (hrList);
    bool                      mostlyFree = listed && (listing.totalUnits == 0 || (uint64_t) listing.freeUnits * 10 >= (uint64_t) listing.totalUnits * kBlankFreeTenths);
    int                       listTrack  = (listed && kind == VolumeKind::Dos33) ? FindLoneProgramTrack (image) : -1;
    Byte                      track      = 0;
    Byte                      sector     = 0;
    Byte                      type       = 0;
    int                       sectors    = 0;
    size_t                    list       = 0;
    size_t                    data       = 0;
    int                       length     = 0;
    bool                      blank      = false;



    auto  have = [&haveTracks] (int number) { return std::find (haveTracks.begin(), haveTracks.end(), number) != haveTracks.end(); };

    outSystem = known ? std::string (VolumeImage::GetSystemName (kind)) : std::string();

    if (!listed || !mostlyFree)
    {
        return false;
    }

    blank = listing.entries.empty() && listing.directoryFiles == 0;

    //  A lone BASIC program with no lines: its first sector has the program's
    //  length, then the program, which ends with a zero link to the next line.
    if (!blank && listTrack >= 0 && have (listTrack))
    {
        FindDosEntries (image, track, sector, type, sectors);

        list = GetDosSectorOffset (track, sector);

        if (list + NibblizationLayer::kSectorByteSize <= image.size() && image[list + kFirstPair] != 0 && image[list + kFirstPair] < NibblizationLayer::kTrackCount && have (image[list + kFirstPair]))
        {
            data   = GetDosSectorOffset (image[list + kFirstPair], image[list + kFirstPair + 1]);
            length = (data + kProgramAt + 2 <= image.size()) ? (image[data] | (image[data + 1] << 8)) : INT_MAX;
            blank  = length <= kNoLines || (length != INT_MAX && image[data + kProgramAt] == 0 && image[data + kProgramAt + 1] == 0);
        }
    }

    return blank;
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

std::string FullSendDisk::MakeImageTitle (int sequence, const std::string & catalogTitle, const std::string & prefix)
{
    bool         substituted = false;
    std::string  title       = TextEncoding::WideToNarrow (HostFileNaming::MakeHostLegal (prefix, substituted)) + std::format ("{:04}", sequence);



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
    VolumeKind  kind = VolumeKind::Unknown;



    if (!chosen.empty())
    {
        return chosen;
    }

    kind = VolumeImage::DetectFilesystem (image);

    return kind == VolumeKind::ProDos || kind == VolumeKind::Pascal ? ".po" : ".dsk";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDisk::GetSystemName
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendDisk::GetSystemName (const std::vector<Byte> & image)
{
    return VolumeImage::GetSystemName (VolumeImage::DetectFilesystem (image));
}
