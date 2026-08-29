#include "Pch.h"

#include "VolumeImage.h"
#include "NibbleImageCodec.h"
#include "NibblizationLayer.h"
#include "Dos33Skeleton.h"
#include "ProDosSkeleton.h"
#include "DiskImageStore.h"
#include "WozLoader.h"
#include "TrackWritability.h"





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
                         + (size_t) NibblizationLayer::PoFileIndexForDosLogicalSector (logical)) * kSectorBytes;
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
                         + (size_t) NibblizationLayer::PoFileIndexForDosLogicalSector (logical)) * kSectorBytes;

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
    HRESULT     hr      = S_OK;
    DiskFormat  format  = DiskFormat::Dsk;
    size_t      size    = fileBytes.size();
    bool        sized   = false;
    bool        hasData = size != 0;
    DiskImage   image;



    outReport.Reset (0);

    outDiagnosis              = MountDiagnosis();
    outDiagnosis.fileByteSize = size;

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
        hr = NibblizationLayer::Denibblize (image, DiskFormat::Dsk, outSectors, outReport);
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
//  VolumeImage::ChangedTracks
//
//  A whole track is the unit because a bit stream is written a track at a time:
//  one altered byte costs that track's encoding and nothing else.
//
////////////////////////////////////////////////////////////////////////////////

void VolumeImage::ChangedTracks (
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
    HRESULT     hr     = S_OK;
    DiskFormat  format = DiskFormat::Dsk;
    bool        sized  = editedSectors.size() == (size_t) NibblizationLayer::kImageByteSize;



    outFileBytes.clear();
    outRefusalReason.clear();

    CBRAEx (sized, E_INVALIDARG);

    hr = DiskImageStore::GetSourceFormatByExtension (path, format);
    CHR (hr);

    if (format == DiskFormat::Woz || format == DiskFormat::Nib)
    {
        hr = SaveBitStream (format, originalFileBytes, editedSectors,
                            outFileBytes, outRefusalReason);
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    sized = originalFileBytes.size() == (size_t) NibblizationLayer::kImageByteSize;
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
//  EVERY track the edit needs is judged BEFORE any track is re-encoded, and a
//  single refusal abandons the whole operation. Refusing only the offending
//  track and re-encoding the others would hand back an image carrying part of
//  an edit, indistinguishable from a complete one.
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
    HRESULT             hr         = S_OK;
    DiskImage           image;
    vector<Byte>        prior;
    vector<Byte>        serialized;
    SectorDecodeReport  report;
    TrackWritability    writability;
    vector<int>         changed;
    bool                imageOk    = false;
    bool                tracksOk   = false;
    size_t              i          = 0;



    hr = LoadBitStream (format, originalFileBytes, image);
    CHR (hr);

    hr = NibblizationLayer::Denibblize (image, DiskFormat::Dsk, prior, report);
    CHR (hr);

    writability = TrackWritability::Evaluate (image, report);
    ChangedTracks (prior, editedSectors, changed);

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

    hr = NibblizationLayer::RenibblizeTracks (editedSectors, DiskFormat::Dsk, changed, image);
    CHR (hr);

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
