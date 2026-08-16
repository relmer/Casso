#include "Pch.h"

#include "VolumeImage.h"
#include "NibblizationLayer.h"
#include "Dos33Skeleton.h"
#include "ProDosSkeleton.h"
#include "DiskImageStore.h"
#include "WozLoader.h"



//
//  ProDOS-ordered file position for each DOS logical sector. A .po file stores
//  its sixteen sectors per track in ProDOS block order, so DOS logical sector L
//  lives at file index kProDosFileIndex[L] within the track.
//
static constexpr int  s_kProDosFileIndex[16] =
{
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::ProDosFileToDosLogical
//
////////////////////////////////////////////////////////////////////////////////

void VolumeImage::ProDosFileToDosLogical (const vector<Byte> & fileBytes, vector<Byte> & outSectors)
{
    const size_t  kSectorBytes = (size_t) NibblizationLayer::kSectorByteSize;
    int           track        = 0;
    int           logical      = 0;



    outSectors.assign ((size_t) NibblizationLayer::kImageByteSize, 0);

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        for (logical = 0; logical < NibblizationLayer::kSectorsPerTrack; logical++)
        {
            size_t  from = ((size_t) track * NibblizationLayer::kSectorsPerTrack
                         + (size_t) s_kProDosFileIndex[logical]) * kSectorBytes;
            size_t  to   = ((size_t) track * NibblizationLayer::kSectorsPerTrack
                         + (size_t) logical) * kSectorBytes;

            std::copy (fileBytes.begin() + (ptrdiff_t) from,
                       fileBytes.begin() + (ptrdiff_t) (from + kSectorBytes),
                       outSectors.begin() + (ptrdiff_t) to);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::DosLogicalToProDosFile
//
////////////////////////////////////////////////////////////////////////////////

void VolumeImage::DosLogicalToProDosFile (const vector<Byte> & sectors, vector<Byte> & outFileBytes)
{
    const size_t  kSectorBytes = (size_t) NibblizationLayer::kSectorByteSize;
    int           track        = 0;
    int           logical      = 0;



    outFileBytes.assign ((size_t) NibblizationLayer::kImageByteSize, 0);

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        for (logical = 0; logical < NibblizationLayer::kSectorsPerTrack; logical++)
        {
            size_t  from = ((size_t) track * NibblizationLayer::kSectorsPerTrack
                         + (size_t) logical) * kSectorBytes;
            size_t  to   = ((size_t) track * NibblizationLayer::kSectorsPerTrack
                         + (size_t) s_kProDosFileIndex[logical]) * kSectorBytes;

            std::copy (sectors.begin() + (ptrdiff_t) from,
                       sectors.begin() + (ptrdiff_t) (from + kSectorBytes),
                       outFileBytes.begin() + (ptrdiff_t) to);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::Load
//
//  Sector-order files need only a reorder, if that. A bit-stream image is
//  decoded with the DOS interleave regardless of which filesystem it carries,
//  because the physical sectors on the disk are the same either way and the
//  interleave only decides where each lands in the buffer -- DOS logical order
//  being the layout every consumer here expects.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::Load (
    const vector<Byte>   & fileBytes,
    const std::string    & path,
    vector<Byte>         & outSectors,
    SectorDecodeReport   & outReport)
{
    HRESULT     hr     = S_OK;
    DiskFormat  format = DiskFormat::Dsk;
    size_t      size   = fileBytes.size();
    bool        sized  = false;
    DiskImage   image;



    outReport.Reset (0);

    hr = DiskImageStore::DetectFormatByExtension (path, format);
    CHR (hr);

    if (format == DiskFormat::Woz)
    {
        hr = WozLoader::Load (fileBytes, image);
        CHR (hr);

        hr = NibblizationLayer::Denibblize (image, DiskFormat::Dsk, outSectors, outReport);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    sized = size == (size_t) NibblizationLayer::kImageByteSize;
    CBREx (sized, E_INVALIDARG);

    if (format == DiskFormat::Po)
    {
        ProDosFileToDosLogical (fileBytes, outSectors);
    }
    else
    {
        outSectors = fileBytes;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::IsPlausibleVolumeName
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeImage::IsPlausibleVolumeName (const vector<Byte> & sectors, size_t nameLength)
{
    size_t  i     = 0;
    bool    valid = nameLength >= 1 && nameLength <= 15;



    for (i = 0; valid && i < nameLength; i++)
    {
        Byte  c = sectors[ProDosSkeleton::BlockByteOffset (2, 0x04 + 1 + i)];

        if (i == 0)
        {
            valid = isalpha ((unsigned char) c) != 0;
        }
        else
        {
            valid = isalnum ((unsigned char) c) != 0 || c == '.';
        }
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::LooksLikeDos33
//
//  Five corroborating fields from the VTOC. Any one alone would be weak; a
//  buffer that satisfies all five is describing itself as DOS 3.3 in a way
//  stray data does not.
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeImage::LooksLikeDos33 (const vector<Byte> & sectors)
{
    size_t  vtoc = Dos33Skeleton::SectorOffset (17, 0);



    if (vtoc + 0x38 >= sectors.size())
    {
        return false;
    }

    return sectors[vtoc + 0x01] == 17                                              // catalog track
        && sectors[vtoc + 0x02] >= 1  && sectors[vtoc + 0x02] <= 15                 // catalog sector
        && sectors[vtoc + 0x34] == (Byte) NibblizationLayer::kTrackCount            // 35 tracks
        && sectors[vtoc + 0x35] == (Byte) NibblizationLayer::kSectorsPerTrack       // 16 sectors
        && sectors[vtoc + 0x36] == 0x00 && sectors[vtoc + 0x37] == 0x01;            // 256 bytes
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::LooksLikeProDos
//
//  The storage-type nibble alone is NOT sufficient, and that is the whole point
//  of this function's shape. Real 6502 boot code on a DOS 3.3 disk presents a
//  high nibble of $F at exactly the offset a volume header would occupy, so a
//  check that stopped there would misidentify a disk this project ships as a
//  fixture. The name, the entry geometry, and the block count all have to agree.
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeImage::LooksLikeProDos (const vector<Byte> & sectors)
{
    size_t  header  = ProDosSkeleton::BlockByteOffset (2, 0x04);
    Byte    typeLen = 0;
    size_t  nameLen = 0;
    Word    total   = 0;
    Word    bitmap  = 0;



    if (header >= sectors.size())
    {
        return false;
    }

    typeLen = sectors[header];
    nameLen = (size_t) (typeLen & 0x0F);

    if ((typeLen & 0xF0) != 0xF0)
    {
        return false;
    }

    if (!IsPlausibleVolumeName (sectors, nameLen))
    {
        return false;
    }

    total  = (Word) (sectors[ProDosSkeleton::BlockByteOffset (2, 0x04 + 0x25)]
                  | (sectors[ProDosSkeleton::BlockByteOffset (2, 0x04 + 0x26)] << 8));
    bitmap = (Word) (sectors[ProDosSkeleton::BlockByteOffset (2, 0x04 + 0x23)]
                  | (sectors[ProDosSkeleton::BlockByteOffset (2, 0x04 + 0x24)] << 8));

    return sectors[ProDosSkeleton::BlockByteOffset (2, 0x04 + 0x1F)] == 0x27   // entry length
        && sectors[ProDosSkeleton::BlockByteOffset (2, 0x04 + 0x20)] == 0x0D   // entries per block
        && total  > 0 && total  <= (Word) ProDosSkeleton::kTotalBlocks
        && bitmap > 0 && bitmap <  (Word) ProDosSkeleton::kTotalBlocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::DetectFilesystem
//
//  DOS 3.3 is tested first because its VTOC offers more independent fields to
//  agree on, so a match there is the harder one to reach by accident.
//
////////////////////////////////////////////////////////////////////////////////

VolumeKind VolumeImage::DetectFilesystem (const vector<Byte> & sectors)
{
    bool  sized = sectors.size() == (size_t) NibblizationLayer::kImageByteSize;



    if (!sized)
    {
        return VolumeKind::Unknown;
    }

    if (LooksLikeDos33 (sectors))
    {
        return VolumeKind::Dos33;
    }

    if (LooksLikeProDos (sectors))
    {
        return VolumeKind::ProDos;
    }

    return VolumeKind::Unknown;
}
