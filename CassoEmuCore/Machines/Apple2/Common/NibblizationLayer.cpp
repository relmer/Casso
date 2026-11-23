#include "Pch.h"

#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/FluxSectorWriter.h"
#include "Machines/Apple2/Common/NibbleFieldWriter.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
#include "Machines/Apple2/Common/TrackBitReader.h"
#include "Machines/Apple2/Common/TrackSectorDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetAddressVolume
//
//  The volume a disk's address fields have: a DOS 3.3 VTOC's, as INIT writes
//  it, or 254 for any other disk. The VTOC is DOS sector 0 of track 17, which
//  is physical sector 0 in either order.
//
////////////////////////////////////////////////////////////////////////////////

static Byte GetAddressVolume (const vector<Byte> & raw, const int * interleave)
{
    size_t  vtoc   = static_cast<size_t> (Dos33Skeleton::kVtocTrack * NibbleFormat::kSectorsPerTrack + interleave[0]) * NibbleFormat::kSectorByteSize;
    Byte    volume = raw[vtoc + Dos33Skeleton::kVtocOffVolumeNumber];
    bool    isVtoc = raw[vtoc + Dos33Skeleton::kVtocOffCatalogTrack] == Dos33Skeleton::kVtocTrack
                  && raw[vtoc + Dos33Skeleton::kVtocOffMaxTsPairs]   == Dos33Skeleton::kMaxTsPairs
                  && raw[vtoc + Dos33Skeleton::kVtocOffTrackCount]   == NibbleFormat::kTrackCount
                  && raw[vtoc + Dos33Skeleton::kVtocOffSectorCount]  == NibbleFormat::kSectorsPerTrack;



    return (isVtoc && volume != 0 && volume != 0xFF) ? volume : NibbleFormat::kDefaultVolume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizeWithMap
//
//  Common nibblization driver. The address fields go down in sequential
//  order, so the loop variable is the physical sector; `interleave` names the
//  file offset whose 256 bytes belong under that address mark.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT NibblizeWithMap (
    const vector<Byte>  &  raw,
    const int           *  interleave,
    DiskImage           &  out)
{
    HRESULT   hr        = S_OK;
    int       track     = 0;
    int       physical  = 0;
    size_t    offset    = 0;
    size_t    rawSize   = 0;
    Byte      volume    = 0;



    rawSize = raw.size();

    // A WRONG-SIZED BUFFER IS USER INPUT HERE, not a caller bug. This guard
    // asserted for as long as the only callers were trusted; then the live
    // mount path started handing it whatever file was dropped on a drive, and
    // a 4 KB .dsk raised an assertion dialog in a Debug build before anything
    // could report it. E_INVALIDARG marks a coding error in this tree and
    // always asserts, so it is the wrong verdict for a file somebody
    // downloaded.
    //
    // ERROR_BAD_LENGTH rather than a bare failure: the caller has to be able
    // to tell a wrong size from every other way a load can go wrong, or the
    // message it writes cannot say which one happened.
    CBREx (rawSize == NibbleFormat::kImageByteSize, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    volume = GetAddressVolume (raw, interleave);

    for (track = 0; track < NibbleFormat::kTrackCount; track++)
    {
        size_t   bitOffset = 0;

        out.ResizeTrack (track, NibbleFormat::kTrackBitCapacity);

        for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
        {
            offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical])
                   * NibbleFormat::kSectorByteSize;

            NibbleFieldWriter::AppendAddressField (out.GetTrackBitsForWrite (track), bitOffset,
                                                   volume,
                                                   static_cast<Byte> (track),
                                                   static_cast<Byte> (physical));
            NibbleFieldWriter::AppendDataField    (out.GetTrackBitsForWrite (track), bitOffset, &raw[offset]);
        }

        // Trim the track to what we actually wrote so the engine wraps
        // back to the address-field sync gap (real Disk II behavior),
        // not into a run of zero bits left over from the resize.
        out.SetTrackBitCount (track, bitOffset);
    }

    out.ClearDirty();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::RenibblizeTracks
//
//  Re-encodes only the named tracks, leaving every other track's packed bits
//  exactly as they are.
//
//  This is what makes a write to a bit-stream image safe. Re-encoding the whole
//  image would replace all 35 tracks with freshly synthesized standard nibbles,
//  discarding timing, sync patterns, and weak bits everywhere -- including on
//  tracks the operation never touched. A 512-byte file has no business
//  rewriting a disk.
//
//  Tracks outside the image are skipped rather than treated as an error: the
//  caller names the tracks its edit landed on, and an image shorter than the
//  nominal geometry simply has nothing at those positions.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::RenibblizeTracks (
    const vector<Byte>    &  sectors,
    DiskFormat               fmt,
    std::span<const int>     tracks,
    DiskImage             &  inOutImage)
{
    HRESULT       hr         = S_OK;
    const int *   interleave = nullptr;
    size_t        rawSize    = sectors.size();
    size_t        i          = 0;
    int           track      = 0;
    int           physical   = 0;
    int           trackCount = inOutImage.GetTrackCount();
    Byte          volume     = 0;



    CBRAEx (rawSize == (size_t) NibbleFormat::kImageByteSize, E_INVALIDARG);

    switch (fmt)
    {
        case DiskFormat::Dsk: interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        case DiskFormat::Do:  interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        case DiskFormat::Po:  interleave = SectorInterleave::kPo_PhysicalToFile;   break;
        case DiskFormat::Woz: interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        default:              hr         = E_INVALIDARG;         break;
    }

    CHR (hr);

    volume = GetAddressVolume (sectors, interleave);

    for (i = 0; i < tracks.size(); i++)
    {
        size_t  bitOffset = 0;

        track = tracks[i];

        if (track < 0 || track >= NibbleFormat::kTrackCount || track >= trackCount)
        {
            continue;
        }

        //  A flux track is never regenerated: only the data fields that
        //  changed are spliced in, and everything else keeps its timing.
        if (inOutImage.GetTrackKind (track) == TrackKind::Flux)
        {
            hr = FluxSectorWriter::WriteFluxTrackSectors (sectors, interleave, track, inOutImage);
            CHR (hr);
            continue;
        }

        inOutImage.ResizeTrack (track, NibbleFormat::kTrackBitCapacity);

        for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
        {
            size_t  offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[physical])
                           * NibbleFormat::kSectorByteSize;

            NibbleFieldWriter::AppendAddressField (inOutImage.GetTrackBitsForWrite (track), bitOffset,
                                                   volume,
                                                   static_cast<Byte> (track),
                                                   static_cast<Byte> (physical));
            NibbleFieldWriter::AppendDataField    (inOutImage.GetTrackBitsForWrite (track), bitOffset, &sectors[offset]);
        }

        inOutImage.SetTrackBitCount (track, bitOffset);

        //  This track's bits were just replaced. Saying so is what lets a
        //  writer that copies untouched tracks tell them from rewritten ones.
        inOutImage.MarkTrackDirty (track);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::GetImageVolume
//
////////////////////////////////////////////////////////////////////////////////

Byte NibblizationLayer::GetImageVolume (const vector<Byte> & sectors)
{
    bool  isWhole = sectors.size() >= (size_t) NibbleFormat::kImageByteSize;



    return isWhole ? GetAddressVolume (sectors, SectorInterleave::kDsk_PhysicalToFile) : NibbleFormat::kDefaultVolume;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer::NibblizeDsk / NibblizeDo / NibblizePo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizeDsk (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, SectorInterleave::kDsk_PhysicalToFile, out);



    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Dsk);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizeDo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizeDo (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, SectorInterleave::kDsk_PhysicalToFile, out);



    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Do);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizePo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::NibblizePo (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr = NibblizeWithMap (raw, SectorInterleave::kPo_PhysicalToFile, out);



    if (SUCCEEDED (hr))
    {
        out.SetSourceFormat (DiskFormat::Po);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Nibblize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Nibblize (const vector<Byte> & raw, DiskFormat fmt, DiskImage & out)
{
    HRESULT  hr = S_OK;



    switch (fmt)
    {
        case DiskFormat::Dsk: hr = NibblizeDsk (raw, out); break;
        case DiskFormat::Do:  hr = NibblizeDo  (raw, out); break;
        case DiskFormat::Po:  hr = NibblizePo  (raw, out); break;
        default:              hr = E_INVALIDARG;           break;
    }

    return hr;
}




