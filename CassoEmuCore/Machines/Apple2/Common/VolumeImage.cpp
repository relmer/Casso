#include "Pch.h"

#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/GutenbergVolume.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/RdosVolume.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Machines/Apple2/Common/TrackWritability.h"
#include "Devices/Disk/SectorFieldWriter.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Disk/Archives/ArchiveVolume.h"
#include "Devices/Disk/Archives/GzipFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::ProDosFileToDosLogical
//
//  The position each DOS logical sector occupies in the file comes from the
//  track layer, which owns both interleaves. Restating it here as a table of
//  its own is the mistake this function used to carry: the wrong table read
//  back perfectly through itself, so nothing that wrote a file and read it
//  again could tell.
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
                         + (size_t) NibblizationLayer::GetPoFileIndexForDosLogicalSector (logical)) * kSectorBytes;
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
                         + (size_t) NibblizationLayer::GetPoFileIndexForDosLogicalSector (logical)) * kSectorBytes;

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
    MountDiagnosis  ignored;



    return Load (fileBytes, path, outSectors, outReport, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::Load
//
//  The same load, saying why it refused.
//
//  Each guard names its own reason, which is the point: the console used to
//  answer every one of them with "is not a disk image this tool can read", a
//  sentence that covers a renamed file, a truncated download and a damaged WOZ
//  alike and helps with none of them.
//
//  The Denibblize refusal is the one that stays generic, and honestly so. It
//  means a track decoded partly and the gaps would be lost data, which is a
//  statement about the surface rather than about the container; the survey the
//  caller prints afterwards says far more about it than a one-line reason
//  could.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::Load (
    const vector<Byte>   & fileBytes,
    const std::string    & path,
    vector<Byte>         & outSectors,
    SectorDecodeReport   & outReport,
    MountDiagnosis       & outDiagnosis)
{
    HRESULT             hr       = S_OK;
    DiskFormat          format   = DiskFormat::Dsk;
    size_t              size     = fileBytes.size();
    bool                sized    = false;
    bool                hasData  = size != 0;
    bool                thirteen = IsThirteenSectorFile (path);
    ImageContainerKind  kind     = ImageContainer::GetKind (path, size);
    DiskImage           image;



    outReport.Reset (0);

    outDiagnosis              = MountDiagnosis();
    outDiagnosis.fileByteSize = size;

    if (kind != ImageContainerKind::None)
    {
        hr = LoadContainer (kind, fileBytes, std::filesystem::path (path).replace_extension().string(), outSectors, outReport, outDiagnosis);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    //  A .d13 file is the 13-sector buffer itself.
    if (thirteen)
    {
        sized = size == ThirteenSectorCodec::kImageByteSize;

        CBRFEx (hasData, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH),
                outDiagnosis.failure = MountFailure::EmptyFile);
        CBRFEx (sized, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH),
                outDiagnosis.failure = MountFailure::WrongSizeForFormat);

        outSectors = fileBytes;

        BAIL_OUT_IF (true, S_OK);
    }

    hr = DiskImageStore::GetSourceFormatByExtension (path, format);
    CHRF (hr, outDiagnosis.failure = MountFailure::UnknownExtension);

    outDiagnosis.format = format;

    CBRFEx (hasData, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH),
            outDiagnosis.failure = MountFailure::EmptyFile);

    if (format == DiskFormat::Woz || format == DiskFormat::Nib)
    {
        hr = LoadBitStream (format, fileBytes, image);
        CHRF (hr, outDiagnosis = DiskImageStore::ClassifyLoadFailure (format, fileBytes));

        //  The sector decode DOES happen here, unlike on the emulator's flush
        //  path: these callers address files and sectors, so a track that will
        //  not decode is a track they cannot act on. Its refusal is the strict
        //  one, and correctly so.
        hr = DenibblizeBitStream (image, outSectors, outReport);
        CHRF (hr, outDiagnosis.failure = MountFailure::Unrecognized);

        BAIL_OUT_IF (true, S_OK);
    }

    sized = size == (size_t) NibblizationLayer::kImageByteSize;

    // ERROR_BAD_LENGTH rather than E_INVALIDARG, which in this tree marks a
    // coding error and asserts. A file the user named is not a coding error
    // however wrong its size is.
    CBRFEx (sized, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH),
            outDiagnosis.failure = MountFailure::WrongSizeForFormat);

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
//  VolumeImage::CollectChangedTracks
//
//  A whole track is the unit because a bit stream is written a track at a time:
//  one altered byte costs that track's encoding and nothing else.
//
////////////////////////////////////////////////////////////////////////////////

void VolumeImage::CollectChangedTracks (
    const vector<Byte>  & priorSectors,
    const vector<Byte>  & editedSectors,
    vector<int>         & outTracks)
{
    const size_t  kTrackBytes = (size_t) NibblizationLayer::kSectorsPerTrack
                              * (size_t) NibblizationLayer::kSectorByteSize;
    int           track       = 0;
    size_t        needed      = (size_t) NibblizationLayer::kImageByteSize;



    outTracks.clear();

    if (priorSectors.size() != needed || editedSectors.size() != needed)
    {
        return;
    }

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        size_t  base = (size_t) track * kTrackBytes;
        bool    same = std::equal (priorSectors.begin()  + (ptrdiff_t) base,
                                   priorSectors.begin()  + (ptrdiff_t) (base + kTrackBytes),
                                   editedSectors.begin() + (ptrdiff_t) base);

        if (!same)
        {
            outTracks.push_back (track);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::CollectSectorWrites
//
//  One write per changed sector, by whole track and physical sector. The
//  buffers are in DOS logical order, so a changed logical sector is written
//  under the address field the drive presents it at.
//
////////////////////////////////////////////////////////////////////////////////

void VolumeImage::CollectSectorWrites (
    const vector<Byte>   & priorSectors,
    const vector<Byte>   & editedSectors,
    vector<SectorWrite>  & outWrites)
{
    size_t       needed   = static_cast<size_t> (NibblizationLayer::kImageByteSize);
    int          track    = 0;
    int          physical = 0;
    size_t       base     = 0;
    SectorWrite  write;



    outWrites.clear();

    if (priorSectors.size() != needed || editedSectors.size() != needed)
    {
        return;
    }

    for (track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        for (physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
        {
            base = (static_cast<size_t> (track) * NibblizationLayer::kSectorsPerTrack
                 + static_cast<size_t> (NibblizationLayer::GetDosFileIndexForPhysicalSector (physical)))
                 * NibblizationLayer::kSectorByteSize;

            if (std::equal (priorSectors.begin() + base, priorSectors.begin() + base + NibblizationLayer::kSectorByteSize, editedSectors.begin() + base))
            {
                continue;
            }

            write        = SectorWrite();
            write.track  = track;
            write.sector = static_cast<Byte> (physical);
            std::copy (editedSectors.begin() + base, editedSectors.begin() + base + NibblizationLayer::kSectorByteSize, write.bytes.begin());

            outWrites.push_back (write);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::DescribeUnwritableTrack
//
////////////////////////////////////////////////////////////////////////////////

std::string VolumeImage::DescribeUnwritableTrack (int track)
{
    return "track " + std::to_string (track)
         + " does not decode to a complete set of standard sectors, so rewriting"
           " it would destroy what could not be read";
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::Save
//
//  The inverse of Load, and the only place an edited buffer becomes a file.
//
//  Sector-order containers hold nothing but sectors, so there is nothing to
//  preserve and nothing to judge -- the buffer is written out in the order the
//  extension names. A .po's reorder comes from the track layer for the reason
//  recorded on ProDosFileToDosLogical.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::Save (
    const vector<Byte>  & originalFileBytes,
    const std::string   & path,
    const vector<Byte>  & editedSectors,
    vector<Byte>        & outFileBytes,
    std::string         & outRefusalReason)
{
    HRESULT             hr       = S_OK;
    DiskFormat          format   = DiskFormat::Dsk;
    bool                thirteen = editedSectors.size() == ThirteenSectorCodec::kImageByteSize;
    bool                sized    = thirteen || editedSectors.size() == (size_t) NibblizationLayer::kImageByteSize;
    ImageContainerKind  kind     = ImageContainer::GetKind (path, originalFileBytes.size());



    outFileBytes.clear();
    outRefusalReason.clear();

    if (kind != ImageContainerKind::None)
    {
        hr = SaveContainer (kind, originalFileBytes, std::filesystem::path (path).replace_extension().string(), editedSectors, outFileBytes, outRefusalReason);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    CBRAEx (sized, E_INVALIDARG);

    if (IsThirteenSectorFile (path))
    {
        CBREx (thirteen, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

        outFileBytes = editedSectors;

        BAIL_OUT_IF (true, S_OK);
    }

    hr = DiskImageStore::GetSourceFormatByExtension (path, format);
    CHR (hr);

    if (format == DiskFormat::Woz || format == DiskFormat::Nib)
    {
        hr = SaveBitStream (format, originalFileBytes, editedSectors,
                            outFileBytes, outRefusalReason);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    sized = !thirteen && originalFileBytes.size() == (size_t) NibblizationLayer::kImageByteSize;
    CBRAEx (sized, E_INVALIDARG);

    if (format == DiskFormat::Po)
    {
        DosLogicalToProDosFile (editedSectors, outFileBytes);
    }
    else
    {
        outFileBytes = editedSectors;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::LoadBitStream
//
//  One bit-stream container's file into a DiskImage.
//
//  Two containers store tracks rather than sectors, and everything downstream
//  of this point treats them identically -- decode, judge, re-encode what
//  changed. Keeping the choice in one place is what stops the four sites that
//  need it from each growing their own branch, which is how one of them comes
//  to know about a container the others do not.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::LoadBitStream (DiskFormat format, const vector<Byte> & fileBytes,
                                    DiskImage & outImage)
{
    HRESULT  hr = S_OK;



    switch (format)
    {
        case DiskFormat::Woz: hr = WozLoader::Load (fileBytes, outImage);       break;
        case DiskFormat::Nib: hr = NibbleImageCodec::Load (fileBytes, outImage); break;
        default:              hr = E_INVALIDARG;                                break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::SerializeBitStream
//
//  The image back into the container it came from.
//
//  The original bytes reach the nibble writer and not the WOZ one, which is
//  not an oversight. A nibble image has no header or metadata to preserve, so
//  its writer uses the source bytes for a different purpose entirely: copying
//  the tracks the edit did not touch, byte for byte, instead of re-deriving
//  them. The WOZ writer keeps what it needs on the image itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::SerializeBitStream (DiskFormat format, const DiskImage & image,
                                         const vector<Byte> & originalFileBytes,
                                         vector<Byte> & outFileBytes)
{
    HRESULT  hr = S_OK;



    switch (format)
    {
        case DiskFormat::Woz:
            hr = WozLoader::Serialize (image, outFileBytes);
            break;

        case DiskFormat::Nib:
            hr = NibbleImageCodec::Serialize (image, originalFileBytes, outFileBytes);
            break;

        default:
            hr = E_INVALIDARG;
            break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::SaveBitStream
//
//  EVERY track the edit needs is judged BEFORE any sector is written, and a
//  single refusal abandons the whole operation. Refusing only the offending
//  track and writing the others would hand back an image carrying part of an
//  edit, indistinguishable from a complete one.
//
//  Each changed sector goes through the shared sector writer, which rewrites
//  only its data field inside the existing track. A track is never rebuilt:
//  its volume, sync runs, length, the place of every sector and, on a flux
//  track, its recorded timing all stay as they were.
//
//  The prior buffer is decoded here rather than taken from the caller so the
//  two sides of the comparison cannot disagree about what the image held. A
//  track that decoded Partial contributes its zeros to both sides, so it counts
//  as changed only when the edit genuinely landed on it -- at which point it is
//  refused.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::SaveBitStream (
    DiskFormat            format,
    const vector<Byte>  & originalFileBytes,
    const vector<Byte>  & editedSectors,
    vector<Byte>        & outFileBytes,
    std::string         & outRefusalReason)
{
    HRESULT                   hr         = S_OK;
    DiskImage                 image;
    vector<Byte>              prior;
    vector<Byte>              serialized;
    SectorDecodeReport        report;
    TrackWritability          writability;
    vector<int>               changed;
    vector<SectorWrite>       writes;
    vector<SectorWriteError>  errors;
    bool                      imageOk    = false;
    bool                      tracksOk   = false;
    bool                      sameSize   = false;
    bool                      thirteen   = false;
    size_t                    i          = 0;



    hr = LoadBitStream (format, originalFileBytes, image);
    CHR (hr);

    hr = DenibblizeBitStream (image, prior, report);
    CHR (hr);

    sameSize = prior.size() == editedSectors.size();
    thirteen = prior.size() == ThirteenSectorCodec::kImageByteSize;
    CBREx (sameSize, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    writability = TrackWritability::Evaluate (image, report);

    if (thirteen)
    {
        ThirteenSectorCodec::CollectChangedTracks (prior, editedSectors, changed);
    }
    else
    {
        CollectChangedTracks (prior, editedSectors, changed);
    }

    imageOk = writability.IsImageWritable();
    CBRFEx (imageOk, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
            outRefusalReason = writability.GetImageRefusalReason());

    tracksOk = writability.AreTracksWritable (changed);

    for (i = 0; !tracksOk && outRefusalReason.empty() && i < changed.size(); i++)
    {
        bool  writable = writability.IsTrackWritable (changed[i]);

        if (writable)
        {
            continue;
        }

        outRefusalReason = DescribeUnwritableTrack (changed[i]);
    }

    CBREx (tracksOk, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    if (thirteen)
    {
        ThirteenSectorCodec::CollectSectorWrites (prior, editedSectors, writes);
    }
    else
    {
        CollectSectorWrites (prior, editedSectors, writes);
    }

    hr = SectorFieldWriter::Write (image, writes, SectorWritePolicy::Strict, errors);
    CHRF (hr, outRefusalReason = errors.empty() ? std::string() : SectorFieldWriter::Describe (errors.front()));

    //  Serialized into a local so a failure part-way through cannot leave the
    //  caller holding a fragment that looks like an image.
    hr = SerializeBitStream (format, image, originalFileBytes, serialized);
    CHR (hr);

    outFileBytes = serialized;

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
        Byte  c = sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 2, 0x04 + 1 + i)];

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
//  stray data does not. The track and sector counts are those of any DOS 3.x
//  geometry whose size is the buffer's (Dos33Geometry::TryDetect).
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeImage::LooksLikeDos33 (const vector<Byte> & sectors)
{
    Dos33Geometry  geometry;



    return Dos33Geometry::TryDetect (sectors, geometry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::DenibblizeBitStream
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::DenibblizeBitStream (const DiskImage & image, vector<Byte> & outSectors, SectorDecodeReport & outReport)
{
    HRESULT  hr = S_OK;



    if (ThirteenSectorCodec::IsThirteenSector (image))
    {
        ThirteenSectorCodec::Denibblize (image, outSectors, outReport);
    }
    else
    {
        hr = NibblizationLayer::Denibblize (image, DiskFormat::Dsk, outSectors, outReport);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::IsThirteenSectorFile
//
////////////////////////////////////////////////////////////////////////////////

bool VolumeImage::IsThirteenSectorFile (const std::string & path)
{
    std::string  extension = std::filesystem::path (path).extension().string();



    return _stricmp (extension.c_str(), ".d13") == 0;
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
    size_t  header  = ProDosSkeleton::GetBlockByteOffset (sectors.size(), 2, 0x04);
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

    total  = (Word) (sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 2, 0x04 + 0x25)]
                  | (sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 2, 0x04 + 0x26)] << 8));
    bitmap = (Word) (sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 2, 0x04 + 0x23)]
                  | (sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 2, 0x04 + 0x24)] << 8));

    return sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 2, 0x04 + 0x1F)] == 0x27   // entry length
        && sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 2, 0x04 + 0x20)] == 0x0D   // entries per block
        && total  > 0 && total  <= (Word) ProDosSkeleton::GetBlockCount (sectors.size())
        && bitmap > 0 && bitmap <  (Word) ProDosSkeleton::GetBlockCount (sectors.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::GetSystemName
//
////////////////////////////////////////////////////////////////////////////////

const char * VolumeImage::GetSystemName (VolumeKind kind)
{
    switch (kind)
    {
        case VolumeKind::Dos33:  return "DOS 3.3";
        case VolumeKind::ProDos: return "ProDOS";
        case VolumeKind::Pascal: return "Apple Pascal";
        case VolumeKind::Cpm:    return "CP/M";
        case VolumeKind::Archive: return "Archive";
        case VolumeKind::Gutenberg: return "Gutenberg";
        case VolumeKind::Rdos:   return "RDOS";
        default:                 return "";
    }
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



    //  An archive's buffer is the archive itself.
    if (ArchiveVolume::IsArchive (sectors))
    {
        return VolumeKind::Archive;
    }

    //  A buffer of another size is a DOS 3.x volume of that size, a 13-sector
    //  disk, or a run of ProDOS blocks.
    if (!sized && LooksLikeDos33 (sectors))
    {
        return VolumeKind::Dos33;
    }

    if (sectors.size() == ThirteenSectorCodec::kImageByteSize)
    {
        return RdosVolume::IsFound (sectors) ? VolumeKind::Rdos : VolumeKind::Unknown;
    }

    if (!sized)
    {
        return ProDosSkeleton::IsValidVolumeSize (sectors.size()) && LooksLikeProDos (sectors) ? VolumeKind::ProDos : VolumeKind::Unknown;
    }

    if (LooksLikeDos33 (sectors))
    {
        return VolumeKind::Dos33;
    }

    if (LooksLikeProDos (sectors))
    {
        return VolumeKind::ProDos;
    }

    if (PascalVolume::IsFound (sectors))
    {
        return VolumeKind::Pascal;
    }

    if (GutenbergVolume::IsFound (sectors))
    {
        return VolumeKind::Gutenberg;
    }

    if (RdosVolume::IsFound (sectors))
    {
        return VolumeKind::Rdos;
    }

    //  Last, as the only one with no signature.
    if (CpmVolume::IsFound (sectors))
    {
        return VolumeKind::Cpm;
    }

    return VolumeKind::Unknown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::CreateVolume
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<IVolume> VolumeImage::CreateVolume (VolumeKind kind, const vector<Byte> & sectors, ISectorReadObserver * observer)
{
    std::unique_ptr<IVolume>  volume;



    switch (kind)
    {
        case VolumeKind::Dos33:
        {
            auto  dos = std::make_unique<Dos33Volume> (sectors);

            dos->SetReadObserver (observer);
            volume = std::move (dos);
            break;
        }

        case VolumeKind::ProDos:
        {
            auto  pro = std::make_unique<ProDosVolume> (sectors);

            pro->SetReadObserver (observer);
            volume = std::move (pro);
            break;
        }

        case VolumeKind::Pascal:
        {
            auto  block = std::make_unique<PascalVolume> (sectors);

            block->SetReadObserver (observer);
            volume = std::move (block);
            break;
        }

        case VolumeKind::Archive:
        {
            volume = std::make_unique<ArchiveVolume> (sectors);
            break;
        }

        case VolumeKind::Cpm:
        {
            auto  cpm = std::make_unique<CpmVolume> (sectors);

            cpm->SetReadObserver (observer);
            volume = std::move (cpm);
            break;
        }

        case VolumeKind::Gutenberg:
        {
            auto  gutenberg = std::make_unique<GutenbergVolume> (sectors);

            gutenberg->SetReadObserver (observer);
            volume = std::move (gutenberg);
            break;
        }

        case VolumeKind::Rdos:
        {
            auto  rdos = std::make_unique<RdosVolume> (sectors);

            rdos->SetReadObserver (observer);
            volume = std::move (rdos);
            break;
        }

        case VolumeKind::Unknown:
        default:
            volume = std::make_unique<NullVolume>();
            break;
    }

    return volume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::LoadContainer
//
//  A raw block image is its own payload. A 2IMG payload is read by its
//  header's format, nibbles through the nibble layer as a `.nib` is. A Disk
//  Copy 4.2 image's data is blocks, read whatever its checksum; Save is where
//  a bad checksum stops a write.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::LoadContainer (
    ImageContainerKind     kind,
    const vector<Byte>   & fileBytes,
    const std::string    & innerPath,
    vector<Byte>         & outSectors,
    SectorDecodeReport   & outReport,
    MountDiagnosis       & outDiagnosis)
{
    HRESULT           hr        = S_OK;
    bool              hasData   = !fileBytes.empty();
    bool              isDos     = false;
    bool              dosSized  = false;
    bool              isArchive = false;
    std::string       innerName;
    TwoImgHeader      twoImg;
    DiskCopy42Header  diskCopy;
    vector<Byte>      payload;
    std::string       error;
    DiskImage         image;



    CBRFEx (hasData, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH),
            outDiagnosis.failure = MountFailure::EmptyFile);

    if (kind == ImageContainerKind::Blocks)
    {
        hr = LoadBlockPayload (fileBytes, outSectors);
        CHRF (hr, outDiagnosis.failure = MountFailure::WrongSizeForFormat);

        BAIL_OUT_IF (true, S_OK);
    }

    if (kind == ImageContainerKind::Archive)
    {
        isArchive = ArchiveVolume::IsArchive (fileBytes);
        CBRFEx (isArchive, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outDiagnosis.failure = MountFailure::Unrecognized);

        outSectors = fileBytes;

        BAIL_OUT_IF (true, S_OK);
    }

    if (kind == ImageContainerKind::ShrinkItDisk)
    {
        hr = ExtractShrinkItDisk (fileBytes, payload, error);
        CHRF (hr, outDiagnosis.failure = MountFailure::Unrecognized);

        hr = LoadBlockPayload (payload, outSectors);
        CHRF (hr, outDiagnosis.failure = MountFailure::WrongSizeForFormat);

        BAIL_OUT_IF (true, S_OK);
    }

    if (kind == ImageContainerKind::Gzip)
    {
        hr = GzipFile::Expand (fileBytes, payload, innerName, error);
        CHRF (hr, outDiagnosis.failure = MountFailure::Unrecognized);

        hr = Load (payload, innerPath, outSectors, outReport, outDiagnosis);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    if (kind == ImageContainerKind::DiskCopy42)
    {
        hr = DiskCopy42Container::Parse (fileBytes, diskCopy, payload, error);
        CHRF (hr, outDiagnosis.failure = MountFailure::Unrecognized);

        hr = LoadBlockPayload (payload, outSectors);
        CHRF (hr, outDiagnosis.failure = MountFailure::WrongSizeForFormat);

        BAIL_OUT_IF (true, S_OK);
    }

    hr = TwoImgContainer::Parse (fileBytes, twoImg, payload, error);
    CHRF (hr, outDiagnosis.failure = MountFailure::Unrecognized);

    if (twoImg.format == TwoImgHeader::kFormatNibbles)
    {
        hr = LoadBitStream (DiskFormat::Nib, payload, image);
        CHRF (hr, outDiagnosis.failure = MountFailure::Unrecognized);

        hr = DenibblizeBitStream (image, outSectors, outReport);
        CHRF (hr, outDiagnosis.failure = MountFailure::Unrecognized);

        BAIL_OUT_IF (true, S_OK);
    }

    isDos    = twoImg.format == TwoImgHeader::kFormatDosOrder;
    dosSized = payload.size() == (size_t) NibblizationLayer::kImageByteSize;

    if (isDos)
    {
        CBRFEx (dosSized, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH),
                outDiagnosis.failure = MountFailure::WrongSizeForFormat);

        outSectors = payload;

        BAIL_OUT_IF (true, S_OK);
    }

    hr = LoadBlockPayload (payload, outSectors);
    CHRF (hr, outDiagnosis.failure = MountFailure::WrongSizeForFormat);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::SaveContainer
//
//  The inverse of LoadContainer, with each container's header written back as
//  it was read. A Disk Copy 4.2 image whose data checksum didn't match isn't
//  written, since a rewrite would compute a checksum over the damage and hide
//  it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::SaveContainer (
    ImageContainerKind     kind,
    const vector<Byte>   & originalFileBytes,
    const std::string    & innerPath,
    const vector<Byte>   & editedSectors,
    vector<Byte>         & outFileBytes,
    std::string          & outRefusalReason)
{
    HRESULT           hr        = S_OK;
    TwoImgHeader      twoImg;
    DiskCopy42Header  diskCopy;
    vector<Byte>      payload;
    vector<Byte>      newPayload;
    std::string       error;
    std::string       innerName;



    if (kind == ImageContainerKind::Blocks)
    {
        SaveBlockPayload (editedSectors, outFileBytes);

        BAIL_OUT_IF (true, S_OK);
    }

    if (kind == ImageContainerKind::Archive)
    {
        outFileBytes = editedSectors;

        BAIL_OUT_IF (true, S_OK);
    }

    if (kind == ImageContainerKind::ShrinkItDisk)
    {
        SaveBlockPayload (editedSectors, newPayload);

        hr = ReplaceShrinkItDisk (originalFileBytes, newPayload, outFileBytes, outRefusalReason);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    if (kind == ImageContainerKind::Gzip)
    {
        hr = GzipFile::Expand (originalFileBytes, payload, innerName, error);
        CHRF (hr, outRefusalReason = error);

        hr = Save (payload, innerPath, editedSectors, newPayload, outRefusalReason);
        CHR (hr);

        GzipFile::Build (newPayload, innerName, outFileBytes);

        BAIL_OUT_IF (true, S_OK);
    }

    if (kind == ImageContainerKind::DiskCopy42)
    {
        hr = DiskCopy42Container::Parse (originalFileBytes, diskCopy, payload, error);
        CHRF (hr, outRefusalReason = error);

        CBRFEx (!diskCopy.isDataChecksumBad, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
                outRefusalReason = "The Disk Copy 4.2 image's data doesn't match its checksum, so it isn't written.");

        SaveBlockPayload (editedSectors, newPayload);
        DiskCopy42Container::Build (diskCopy, newPayload, outFileBytes);

        BAIL_OUT_IF (true, S_OK);
    }

    hr = TwoImgContainer::Parse (originalFileBytes, twoImg, payload, error);
    CHRF (hr, outRefusalReason = error);

    if (twoImg.format == TwoImgHeader::kFormatNibbles)
    {
        hr = SaveBitStream (DiskFormat::Nib, payload, editedSectors, newPayload, outRefusalReason);
        CHR (hr);
    }
    else if (twoImg.format == TwoImgHeader::kFormatDosOrder)
    {
        newPayload = editedSectors;
    }
    else
    {
        SaveBlockPayload (editedSectors, newPayload);
    }

    TwoImgContainer::Build (twoImg, newPayload, outFileBytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::LoadBlockPayload
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::LoadBlockPayload (const vector<Byte> & payload, vector<Byte> & outSectors)
{
    HRESULT  hr       = S_OK;
    bool     isFloppy = payload.size() == (size_t) NibblizationLayer::kImageByteSize;
    bool     sizeOk   = ProDosSkeleton::IsValidVolumeSize (payload.size());



    CBREx (sizeOk, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    if (isFloppy)
    {
        ProDosFileToDosLogical (payload, outSectors);
    }
    else
    {
        outSectors = payload;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::SaveBlockPayload
//
////////////////////////////////////////////////////////////////////////////////

void VolumeImage::SaveBlockPayload (const vector<Byte> & sectors, vector<Byte> & outPayload)
{
    if (sectors.size() == (size_t) NibblizationLayer::kImageByteSize)
    {
        DosLogicalToProDosFile (sectors, outPayload);
    }
    else
    {
        outPayload = sectors;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::ExtractShrinkItDisk
//
//  The disk in a ShrinkIt disk archive's first record that has one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::ExtractShrinkItDisk (const vector<Byte> & fileBytes, vector<Byte> & outDisk, std::string & outError)
{
    HRESULT              hr      = S_OK;
    NuFxArchive          archive;
    const NuFxThread   * disk    = nullptr;
    const NuFxRecord   * holder  = nullptr;



    hr = NuFxArchive::Parse (fileBytes, archive, outError);
    CHR (hr);

    for (const NuFxRecord & record : archive.records)
    {
        disk = NuFxArchive::FindThread (record, NuFxThread::kClassData, NuFxThread::kKindDiskImage);

        if (disk != nullptr)
        {
            holder = &record;
            break;
        }
    }

    CBRFEx (holder, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The archive has no disk in it.");

    hr = NuFxArchive::ExpandThread (*holder, *disk, outDisk, outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeImage::ReplaceShrinkItDisk
//
//  The archive again with its disk record's disk replaced, LZW/2-compressed,
//  and every other record as it was.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VolumeImage::ReplaceShrinkItDisk (const vector<Byte> & originalFileBytes, const vector<Byte> & disk, vector<Byte> & outFileBytes, std::string & outRefusalReason)
{
    HRESULT      hr       = S_OK;
    NuFxArchive  archive;
    bool         replaced = false;



    hr = NuFxArchive::Parse (originalFileBytes, archive, outRefusalReason);
    CHR (hr);

    for (NuFxRecord & record : archive.records)
    {
        for (NuFxThread & thread : record.threads)
        {
            if (!replaced && thread.threadClass == NuFxThread::kClassData && thread.kind == NuFxThread::kKindDiskImage)
            {
                thread           = NuFxArchive::MakeDataThread (NuFxThread::kKindDiskImage, disk);
                record.extraType = (uint32_t) (disk.size() / ProDosSkeleton::kBlockByteSize);
                record.version   = std::max<Word> (record.version, 3);
                replaced         = true;
            }
        }
    }

    CBRFEx (replaced, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outRefusalReason = "The archive has no disk in it.");

    archive.Build (outFileBytes);

Error:
    return hr;
}
