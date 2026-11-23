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





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  Walk every track's bit stream and recover its 16 sectors into the
//  flat output buffer. The output layout is the inverse of the matching
//  Nibblize call: DSK/DO use the DOS interleave; PO uses the ProDOS map.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (const DiskImage & img, DiskFormat fmt, vector<Byte> & out)
{
    HRESULT             hr          = S_OK;
    SectorDecodeReport  coverage;
    bool                lostSectors = false;



    hr = Denibblize (img, fmt, out, coverage);
    CHR (hr);

    //  THE REPORTLESS SIGNATURE IS NOT AN ESCAPE HATCH. It exists for callers
    //  that do not want the detail, not for callers that want to ignore it, and
    //  DiskImage::Serialize -- the flush path, where silent truncation cost data
    //  in the first place -- is the one production caller.
    //
    //  IT ASKS COVERAGE RATHER THAN THE SECTOR COUNTS, and the difference is a
    //  track whose sixteen slots are all filled because one sector arrived
    //  twice. The counts read that as complete; coverage reads it as a slot
    //  claimed by a sector that cannot be the one belonging to it.
    lostSectors = coverage.HasDataLoss();
    CBR (!lostSectors);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  Decode every track's bit stream back to a plain 143,360-byte sector image.
//
//  What used to lose data was the SILENCE, not the early stop. This returned
//  S_OK over a track it had only partly decoded, and the flush path wrote that
//  buffer over the user's file -- so a damaged track was saved as a mixture of
//  real sectors, zeroed sectors and wrong sectors, reported as a clean save.
//
//  Measured, on an image with one sector's data field destroyed: that sector
//  comes back as zeros, AND a second sector comes back holding the wrong data,
//  because the scan for the missing data field runs on and finds the NEXT
//  sector's, storing it under the sector number the address field gave. Two
//  sectors wrong from one point of damage, and nothing said so.
//
//  So the fix that matters is the report, and a partial track failing the whole
//  operation: the zeros and the misfiled sectors are lost data rather than
//  blank media, and refusing to save is the only answer that destroys nothing.
//  The caller keeps the image dirty and the existing file intact.
//
//  A track that decodes nothing at all is a different case and still succeeds:
//  an unformatted track in a sector image legitimately IS zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::SalvageSectors (
    const DiskImage   &  img,
    DiskFormat           fmt,
    vector<Byte>      &  out,
    DenibblizeReport  &  report)
{
    HRESULT   hr = S_OK;



    hr = DecodeTracks (img, fmt, true, out, report, nullptr);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  The same decode, reported as COVERAGE rather than as sector counts.
//
//  IT SUCCEEDS OVER DAMAGE ON PURPOSE. The strict overload refuses, because its
//  caller is about to write the buffer back over the user's file. This one is
//  for callers that can present a partial result -- listing a damaged disk,
//  extracting the files that survived -- and refusing would leave them with
//  nothing to show for sectors that read perfectly well.
//
//  Both reports come off ONE pass, so they cannot disagree about the same disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (
    const DiskImage     &  img,
    DiskFormat             fmt,
    vector<Byte>        &  out,
    SectorDecodeReport  &  outReport)
{
    HRESULT           hr      = S_OK;
    DenibblizeReport  sectors;



    hr = DecodeTracks (img, fmt, false, out, sectors, &outReport);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::Denibblize (
    const DiskImage   &  img,
    DiskFormat           fmt,
    vector<Byte>      &  out,
    DenibblizeReport  &  report)
{
    HRESULT   hr          = S_OK;
    bool      lostSectors = false;



    hr = DecodeTracks (img, fmt, false, out, report, nullptr);
    CHR (hr);

    // A half-decoded track means the output holds zeros where the user's data
    // should be. Saying so is the whole point: the caller writes this buffer
    // over their file, and once it lands nothing distinguishes it from a
    // clean save. Salvage takes the other entry point and does not come here.
    lostSectors = report.HasPartialTrack();
    CBR (!lostSectors);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeTracks
//
//  The shared walk. keepRecovered decides what happens to a sector that
//  decoded but did not verify: the strict path drops it (so the track reads
//  as partial and the save is refused), salvage keeps its bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibblizationLayer::DecodeTracks (
    const DiskImage     &  img,
    DiskFormat             fmt,
    bool                   keepRecovered,
    vector<Byte>        &  out,
    DenibblizeReport    &  report,
    SectorDecodeReport  *  outCoverage)
{
    // Enough attempts to still find all sixteen good sectors when several
    // reads fail or repeat, without letting a hostile track spin forever.
    constexpr int  kMaxAttemptsPerTrack = NibbleFormat::kSectorsPerTrack * 2;
    constexpr int  kAllSectorsMask      = (1 << NibbleFormat::kSectorsPerTrack) - 1;



    HRESULT                      hr                                  = S_OK;
    const int                  * interleave                          = nullptr;
    int                          track                               = 0;
    int                          attempt                             = 0;
    int                          decoded                             = 0;
    uint16_t                     mask                                = 0;
    uint16_t                     recovered                           = 0;
    uint16_t                     lost                                = 0;
    uint16_t                     duplicated                          = 0;
    uint16_t                     slotMask                            = 0;
    uint16_t                     seenAtRevStart                      = 0;
    size_t                       revolutionEnd                       = 0;
    Byte                         outSector                           = 0;
    Byte                         sectorBase                          = 0;
    Byte                         data[NibbleFormat::kSectorByteSize] = {};
    size_t                       bitPos                              = 0;
    size_t                       offset                              = 0;
    size_t                       trackBits                           = 0;
    int                          trackLimit                          = img.GetTrackCount();
    FluxBitView                  fluxView;
    TrackBitReader::TrackBits    src;



    out.assign (NibbleFormat::kImageByteSize, 0);

    report = DenibblizeReport();

    switch (fmt)
    {
        case DiskFormat::Dsk: interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        case DiskFormat::Do:  interleave = SectorInterleave::kDsk_PhysicalToFile;  break;
        case DiskFormat::Po:  interleave = SectorInterleave::kPo_PhysicalToFile;   break;
        default:              hr         = E_INVALIDARG;         break;
    }

    CHR (hr);

    if (trackLimit > NibbleFormat::kTrackCount)
    {
        trackLimit = NibbleFormat::kTrackCount;
    }

    report.decodedSectorMask.assign (static_cast<size_t> (trackLimit), 0);

    if (outCoverage != nullptr)
    {
        outCoverage->Reset (trackLimit);
    }

    for (track = 0; track < trackLimit; track++)
    {
        bitPos     = 0;
        mask       = 0;
        recovered  = 0;
        lost       = 0;
        duplicated = 0;
        slotMask   = 0;
        src        = TrackBitReader::GetTrackBits (img, img.ResolveWholeTrack (track), fluxView);
        trackBits  = src.bitCount;
        sectorBase = TrackSectorDecoder::FindSectorBase (src);

        //  A revolution that turns up nothing new means there is nothing left
        //  to find, and the scan can stop. Without this an EMPTY track is the
        //  worst case in the whole decoder: the mask never fills, so the loop
        //  below runs its full count, and each attempt scans a whole
        //  revolution looking for a prologue that is not there -- thirty-two
        //  passes over the same bits to conclude what the first one did.
        //
        //  It costs the damaged-media recovery nothing. That recovery works by
        //  arriving at a skipped sector from a different offset on the next
        //  time round, so it pays off within a revolution or two; a revolution
        //  that adds no sector, recovers none and loses none has met only
        //  headers it has already read.
        revolutionEnd  = trackBits;
        seenAtRevStart = 0;

        for (attempt = 0; attempt < kMaxAttemptsPerTrack && mask != kAllSectorsMask; attempt++)
        {
            //  AT THE TOP, because every failure path below continues, and a
            //  track with nothing on it takes all of them -- which is the one
            //  case this exists to cut short.
            if (bitPos >= revolutionEnd)
            {
                uint16_t  seen = static_cast<uint16_t> (mask | recovered | lost);

                if (seen == seenAtRevStart)
                {
                    break;
                }

                seenAtRevStart = seen;
                revolutionEnd  = bitPos + trackBits;
            }

            //  DUPLICATES ARE ONLY MEANINGFUL WITHIN ONE REVOLUTION. This loop
            //  deliberately keeps scanning after the cursor has been all the way
            //  round, because that is how it retries the sectors a damaged header
            //  cost it -- and a second pass necessarily meets headers it has
            //  already read. Counting those would invent damage the scan caused
            //  rather than report damage the disk carries.
            size_t         fieldStart = SIZE_MAX;
            size_t         dataStart  = SIZE_MAX;
            SectorOutcome  outcome    = SectorOutcome::Lost;
            HRESULT        hrSector   = TrackSectorDecoder::DecodeOneSector (src, bitPos, outSector,
                                                         data, outcome, fieldStart, dataStart);
            bool           firstPass  = fieldStart < trackBits;

            // A hard failure means no address field could be found before the
            // cursor wrapped -- there is nothing further on this track. Keep
            // scanning anyway: the loop is bounded, and abandoning it here is
            // what would cost the sectors after a damaged one the moment the
            // decoder grows a per-sector failure mode. It has one now.
            if (FAILED (hrSector))
            {
                continue;
            }

            outSector = (outSector >= sectorBase) ? static_cast<Byte> (outSector - sectorBase) : outSector;

            if (outSector >= NibbleFormat::kSectorsPerTrack)
            {
                continue;
            }

            if (outcome == SectorOutcome::Lost)
            {
                lost = static_cast<uint16_t> (lost | (1 << outSector));
                continue;
            }

            if (outcome == SectorOutcome::Recovered)
            {
                recovered = static_cast<uint16_t> (recovered | (1 << outSector));

                if (!keepRecovered)
                {
                    continue;
                }
            }

            offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[outSector])
                   * NibbleFormat::kSectorByteSize;

            memcpy (&out[offset], data, NibbleFormat::kSectorByteSize);

            if (outcome == SectorOutcome::Verified)
            {
                //  A SECOND COPY OF A SECTOR IS DAMAGE, AND SETTING THE BIT
                //  AGAIN HIDES IT. The buffer keeps whichever copy landed last,
                //  the mask is full either way, and every count below reads the
                //  track as complete -- so coverage has to be asked whether each
                //  slot was filled EXACTLY once, not merely whether it is full.
                if (firstPass && (mask & (1 << outSector)) != 0)
                {
                    duplicated = static_cast<uint16_t> (duplicated | (1 << outSector));
                }

                mask = static_cast<uint16_t> (mask | (1 << outSector));

                //  COVERAGE IS INDEXED BY OUTPUT SLOT, and the mask above is
                //  indexed by the number the address field carried. The two
                //  differ by the interleave, so a consumer holding the buffer
                //  and reading the mask directly would test the wrong sector
                //  and find zeros in a slot the report calls present.
                slotMask = static_cast<uint16_t> (slotMask | (1 << interleave[outSector]));
            }
        }

        report.decodedSectorMask[static_cast<size_t> (track)] = mask;

        //  UNFORMATTED IS DECIDED BY THE BITS, NOT BY THE MASK. A track that
        //  holds no bits at all was never written; a track that holds bits but
        //  yielded no sector is damaged, and calling that one blank is what
        //  licenses writing zeros over it.
        if (outCoverage != nullptr)
        {
            TrackDecodeOutcome  outcome = TrackDecodeOutcome::Partial;

            //  UNFORMATTED MEANS NO ADDRESS FIELD ANYWHERE, not "no bits". A
            //  wiped track still carries bits, and a track whose headers are all
            //  destroyed yields nothing at all -- while a track that yielded
            //  something and then failed is damaged, which is a different thing
            //  and must not license writing zeros over it.
            //  A track with data fields and no address field the decoder
            //  accepts was written: its sectors are lost, not blank.
            if (trackBits == 0 || (slotMask == 0 && recovered == 0 && lost == 0 && !TrackSectorDecoder::HasDataField (src)))
            {
                outcome = TrackDecodeOutcome::Unformatted;
            }
            else if (slotMask == kAllSectorsMask && duplicated == 0)
            {
                outcome = TrackDecodeOutcome::Complete;
            }

            outCoverage->SetOutcome (track, outcome, slotMask, duplicated != 0);
        }

        if (trackBits == 0)
        {
            continue;
        }

        decoded = TrackSectorDecoder::PopCount (mask);

        report.tracksPresent++;
        report.sectorsVerified  += decoded;
        report.sectorsRecovered += TrackSectorDecoder::PopCount (recovered);
        report.sectorsLost      += TrackSectorDecoder::PopCount (lost);

        if (decoded == 0)
        {
            report.tracksUnformatted++;
        }
        else if (decoded < NibbleFormat::kSectorsPerTrack)
        {
            report.tracksPartial++;
            report.sectorsMissing += NibbleFormat::kSectorsPerTrack - decoded;
        }
        else
        {
            report.tracksComplete++;
        }
    }

Error:
    return hr;
}




