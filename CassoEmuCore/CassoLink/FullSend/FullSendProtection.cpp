#include "Pch.h"

#include "CassoLink/FullSend/FullSendProtection.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::Find
//
////////////////////////////////////////////////////////////////////////////////

ProtectionSigns FullSendProtection::Find (const std::vector<Byte> & image)
{
    ProtectionSigns  signs;
    VolumeKind       kind   = VolumeKind::Unknown;
    BootFamily       family = BootFamily::None;



    if (image.size() != (size_t) NibblizationLayer::kImageByteSize)
    {
        return signs;
    }

    kind   = VolumeImage::DetectFilesystem (image);
    family = GetBootFamily (image);

    //  A disk with CP/M's loader is a CP/M disk, though its directory can be
    //  empty and then isn't recognized.
    signs.isUnrecognized = kind == VolumeKind::Unknown && family != BootFamily::Cpm && !IsUniform (image);
    signs.hasOtherBoot   = family == BootFamily::Other;
    signs.hasDiskCode    = (kind == VolumeKind::Dos33 || kind == VolumeKind::ProDos || kind == VolumeKind::Pascal) && FindDiskCode (kind, image, signs.diskCodeFile);

    return signs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::GetBootFamily
//
//  The boot sector is physical sector 0 of track 0, the image's first 256
//  bytes in DOS logical order. Apple Pascal's messages are in the second
//  half of its boot block, which is logical sector 14.
//
////////////////////////////////////////////////////////////////////////////////

FullSendProtection::BootFamily FullSendProtection::GetBootFamily (const std::vector<Byte> & image)
{
    constexpr size_t      kSecondHalf = (size_t) 14 * NibblizationLayer::kSectorByteSize;
    std::span<const Byte> sector (image.data(), (std::min) (image.size(), s_kBootSectorBytes));
    std::vector<Byte>     block;
    BootFamily            family      = BootFamily::Other;



    if (image.size() >= kSecondHalf + NibblizationLayer::kSectorByteSize)
    {
        block.assign (image.begin(), image.begin() + (ptrdiff_t) s_kBootSectorBytes);
        block.insert (block.end(), image.begin() + (ptrdiff_t) kSecondHalf, image.begin() + (ptrdiff_t) (kSecondHalf + NibblizationLayer::kSectorByteSize));
    }

    if (std::all_of (sector.begin(), sector.end(), [] (Byte value) { return value == 0; }))
    {
        family = BootFamily::None;
    }
    else if (HasPrefix (sector, s_kDos33Boot))
    {
        family = BootFamily::Dos33;
    }
    else if (HasPrefix (sector, s_kCpmBoot))
    {
        family = BootFamily::Cpm;
    }
    else if (HasPrefix (sector, s_kProDosBoot) || HasPrefix (sector, s_kDataDiskBoot))
    {
        family = BootFamily::ProDos;
    }
    else if (std::any_of (std::begin (s_kPascalLoaders), std::end (s_kPascalLoaders), [&] (std::string_view name) { return HasText (block, name); }))
    {
        family = BootFamily::Pascal;
    }
    else if (HasText (block, s_kSosLoader))
    {
        family = BootFamily::Sos;
    }

    return family;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::CountDiskReads
//
//  An indexed read of $C08C, as LDA $C08C,X, or an absolute read of slot 6's
//  $C0EC, as LDA $C0EC.
//
////////////////////////////////////////////////////////////////////////////////

int FullSendProtection::CountDiskReads (std::span<const Byte> bytes)
{
    static constexpr Byte  kIndexed[]  = { 0xBD, 0xBC, 0xBE, 0xB9, 0xDD, 0xD9, 0x1D, 0x3D, 0x5D, 0x7D, 0xFD };
    static constexpr Byte  kAbsolute[] = { 0xAD, 0xAE, 0xAC, 0xCD, 0x2C };
    int                    reads       = 0;



    for (size_t at = 0; at + 2 < bytes.size(); at++)
    {
        Byte  opcode    = bytes[at];
        Byte  low       = bytes[at + 1];
        bool  isIo      = bytes[at + 2] == 0xC0;
        bool  isIndexed = isIo && low == 0x8C && std::find (std::begin (kIndexed), std::end (kIndexed), opcode) != std::end (kIndexed);
        bool  isSlot6   = isIo && low == 0xEC && std::find (std::begin (kAbsolute), std::end (kAbsolute), opcode) != std::end (kAbsolute);

        reads += (isIndexed || isSlot6) ? 1 : 0;
    }

    return reads;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::IsSystemFile
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendProtection::IsSystemFile (VolumeKind kind, const std::string & name)
{
    bool  isBios = kind == VolumeKind::Pascal && name.size() > s_kPascalBios.size()
                && _stricmp (name.c_str() + name.size() - s_kPascalBios.size(), std::string (s_kPascalBios).c_str()) == 0;



    return isBios || std::any_of (std::begin (s_kSystemFiles), std::end (s_kSystemFiles), [&] (std::string_view system)
    {
        return _stricmp (std::string (system).c_str(), name.c_str()) == 0;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::IsUniform
//
//  Every byte the same, as a disk formatted with no file system.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendProtection::IsUniform (const std::vector<Byte> & image)
{
    return !image.empty() && std::all_of (image.begin(), image.end(), [&] (Byte value) { return value == image.front(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::FindDiskCode
//
//  The volume directory's files only: a protected title's code is at the top
//  level of its disk.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendProtection::FindDiskCode (VolumeKind kind, const std::vector<Byte> & image, std::string & outFile)
{
    HRESULT                   hr     = S_OK;
    std::unique_ptr<IVolume>  volume = VolumeImage::CreateVolume (kind, image);
    VolumeListing             listing;
    bool                      found  = false;



    outFile.clear();

    BAIL_OUT_IF (volume == nullptr, S_OK);

    hr = volume->Enumerate (listing);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    for (size_t at = 0; at < listing.entries.size() && !found; at++)
    {
        const FileEntry  & entry       = listing.entries[at];
        FilePayload        payload;
        FilePath           path        = kind == VolumeKind::Dos33 ? FilePath::FromName (entry.name) : FilePath::Parse (entry.name);
        bool               isCandidate = !entry.isDirectory && IsReadableType (kind, entry.type) && !IsSystemFile (kind, entry.name);

        found   = isCandidate && SUCCEEDED (volume->Read (path, payload)) && CountDiskReads (payload.bytes) >= kDiskCodeReads;
        outFile = found ? entry.name : outFile;
    }

Error:
    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::HasPrefix
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendProtection::HasPrefix (std::span<const Byte> sector, std::span<const Byte> prefix)
{
    return sector.size() >= prefix.size() && std::equal (prefix.begin(), prefix.end(), sector.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::HasText
//
//  The text in the block with or without its characters' high bits.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendProtection::HasText (std::span<const Byte> block, std::string_view text)
{
    std::string  low (block.size(), ' ');



    std::transform (block.begin(), block.end(), low.begin(), [] (Byte value) { return (char) (value & 0x7F); });

    return low.find (text) != std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection::IsReadableType
//
//  Every type but text and BASIC, whose bytes aren't 6502 code.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendProtection::IsReadableType (VolumeKind kind, Byte type)
{
    static constexpr Byte  kDosText        = 0x00;
    static constexpr Byte  kDosInteger     = 0x01;
    static constexpr Byte  kDosApplesoft   = 0x02;
    static constexpr Byte  kProDosText     = 0x04;
    static constexpr Byte  kProDosInteger  = 0xFA;
    static constexpr Byte  kProDosBasic    = 0xFC;
    bool                   isReadable      = true;



    if (kind == VolumeKind::Dos33)
    {
        Byte  bare = (Byte) (type & 0x7F);

        isReadable = bare != kDosText && bare != kDosInteger && bare != kDosApplesoft;
    }
    else if (kind == VolumeKind::ProDos)
    {
        isReadable = type != kProDosText && type != kProDosInteger && type != kProDosBasic;
    }
    else if (kind == VolumeKind::Pascal)
    {
        isReadable = type != PascalVolume::kKindText;
    }

    return isReadable;
}
