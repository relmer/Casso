#include "Pch.h"

#include "Machines/Apple2/Common/Denibblizer.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/FluxBitView.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
#include "Machines/Apple2/Common/TrackBitReader.h"
#include "Machines/Apple2/Common/TrackSectorDecoder.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/StampedTrackMarks.h"
#include "Machines/Apple2/Common/TrackMarks.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblize
//
//  Walk every track's bit stream and recover its 16 sectors into the
//  flat output buffer. The output layout is the inverse of the matching
//  Nibblize call: DSK/DO use the DOS interleave; PO uses the ProDOS map.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Denibblizer::Denibblize (const DiskImage & img, DiskFormat fmt, vector<Byte> & out)
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

HRESULT Denibblizer::SalvageSectors (
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

HRESULT Denibblizer::Denibblize (
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

HRESULT Denibblizer::Denibblize (
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
//  Denibblizer::TrackScan
//
//  The result of one scan of a track, by the sector number in its address
//  fields: the bytes of each sector that decoded, and the sectors that
//  verified, that decoded without verifying, that were lost, and that
//  verified twice in one revolution.
//
////////////////////////////////////////////////////////////////////////////////

struct Denibblizer::TrackScan
{
    std::array<std::array<Byte, NibbleFormat::kSectorByteSize>, NibbleFormat::kSectorsPerTrack>  data       = {};
    uint16_t                                                                                     mask       = 0;
    uint16_t                                                                                     recovered  = 0;
    uint16_t                                                                                     lost       = 0;
    uint16_t                                                                                     duplicated = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeTracks
//
//  The shared walk. keepRecovered decides what happens to a sector that
//  decoded but did not verify: the strict path drops it (so the track reads
//  as partial and the save is refused), salvage keeps its bytes.
//
//  A track whose sectors do not all verify is scanned again with the marks
//  the image's format stamp lists for it, and each sector takes the better
//  of the two results. The second scan can only add sectors the first did
//  not verify, so a stale or wrong stamp cannot change a sector the first
//  scan read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Denibblizer::DecodeTracks (
    const DiskImage     &  img,
    DiskFormat             fmt,
    bool                   keepRecovered,
    vector<Byte>        &  out,
    DenibblizeReport    &  report,
    SectorDecodeReport  *  outCoverage)
{
    HRESULT                      hr         = S_OK;
    const int                  * interleave = nullptr;
    int                          trackLimit = img.GetTrackCount();
    FormatStamp                  stamp;
    bool                         hasStamp   = StampedTrackMarks::TryLoadStamp (img.GetWozMetadata(), stamp);
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

    for (int track = 0; track < trackLimit; track++)
    {
        TrackScan   scan;
        TrackMarks  marks;
        bool        hasMarks = hasStamp && StampedTrackMarks::TryGetTrackMarks (stamp, track, marks);

        src = TrackBitReader::GetTrackBits (img, img.ResolveWholeTrack (track), fluxView);

        ScanTrack (src, nullptr, scan);

        if (hasMarks && scan.mask != kAllSectorsMask)
        {
            TrackScan  stamped;

            ScanTrack  (src, &marks, stamped);
            MergeScans (stamped, scan);
        }

        WriteTrack  (scan, track, interleave, keepRecovered, out);
        ReportTrack (scan, track, src, hasMarks ? &marks : nullptr, interleave, report, outCoverage);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblizer::ScanTrack
//
//  Decodes sectors from the track until all sixteen verify, a revolution
//  turns up nothing new, or the attempts run out. With marks, only their
//  prologs are matched.
//
////////////////////////////////////////////////////////////////////////////////

void Denibblizer::ScanTrack (const TrackBitReader::TrackBits & src, const TrackMarks * marks, TrackScan & outScan)
{
    //  Enough attempts to still find all sixteen good sectors when several
    //  reads fail or repeat, without letting a hostile track spin forever.
    constexpr int  kMaxAttemptsPerTrack                = NibbleFormat::kSectorsPerTrack * 2;
    Byte           data[NibbleFormat::kSectorByteSize] = {};
    Byte           outSector                           = 0;
    Byte           sectorBase                          = TrackSectorDecoder::FindSectorBase (src, marks);
    size_t         bitPos                              = 0;
    size_t         trackBits                           = src.bitCount;
    size_t         revolutionEnd                       = trackBits;
    uint16_t       seenAtRevStart                      = 0;



    //  A revolution that turns up nothing new means there is nothing left to
    //  find, and the scan can stop. Without this an EMPTY track is the worst
    //  case in the whole decoder: the mask never fills, so the loop below
    //  runs its full count, and each attempt scans a whole revolution looking
    //  for a prologue that is not there.
    //
    //  It costs the damaged-media recovery nothing. That recovery works by
    //  arriving at a skipped sector from a different offset on the next time
    //  round, so it pays off within a revolution or two; a revolution that
    //  adds no sector, recovers none and loses none has met only headers it
    //  has already read.
    for (int attempt = 0; attempt < kMaxAttemptsPerTrack && outScan.mask != kAllSectorsMask; attempt++)
    {
        size_t         fieldStart = SIZE_MAX;
        size_t         dataStart  = SIZE_MAX;
        SectorOutcome  outcome    = SectorOutcome::Lost;
        HRESULT        hrSector   = S_OK;
        uint16_t       bit        = 0;
        bool           firstPass  = false;

        //  AT THE TOP, because every failure path below continues, and a
        //  track with nothing on it takes all of them.
        if (bitPos >= revolutionEnd)
        {
            uint16_t  seen = static_cast<uint16_t> (outScan.mask | outScan.recovered | outScan.lost);

            if (seen == seenAtRevStart)
            {
                break;
            }

            seenAtRevStart = seen;
            revolutionEnd  = bitPos + trackBits;
        }

        //  A hard failure means no address field could be found before the
        //  cursor wrapped. Keep scanning anyway: the loop is bounded, and
        //  abandoning it here would cost the sectors after a damaged one.
        hrSector  = TrackSectorDecoder::DecodeOneSector (src, bitPos, outSector, data, outcome, fieldStart, dataStart, marks);
        firstPass = fieldStart < trackBits;

        if (FAILED (hrSector))
        {
            continue;
        }

        outSector = (outSector >= sectorBase) ? static_cast<Byte> (outSector - sectorBase) : outSector;

        if (outSector >= NibbleFormat::kSectorsPerTrack)
        {
            continue;
        }

        bit = static_cast<uint16_t> (1 << outSector);

        if (outcome == SectorOutcome::Lost)
        {
            outScan.lost = static_cast<uint16_t> (outScan.lost | bit);
            continue;
        }

        if (outcome == SectorOutcome::Recovered)
        {
            outScan.recovered = static_cast<uint16_t> (outScan.recovered | bit);

            if ((outScan.mask & bit) == 0)
            {
                memcpy (outScan.data[outSector].data(), data, NibbleFormat::kSectorByteSize);
            }

            continue;
        }

        //  A SECOND COPY OF A SECTOR IS DAMAGE, AND SETTING THE BIT AGAIN
        //  HIDES IT. Duplicates are only meaningful within one revolution: the
        //  loop keeps scanning after the cursor has been all the way round,
        //  which is how it retries the sectors a damaged header cost it, and a
        //  second pass necessarily meets headers it has already read.
        if (firstPass && (outScan.mask & bit) != 0)
        {
            outScan.duplicated = static_cast<uint16_t> (outScan.duplicated | bit);
        }

        outScan.mask = static_cast<uint16_t> (outScan.mask | bit);
        memcpy (outScan.data[outSector].data(), data, NibbleFormat::kSectorByteSize);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblizer::MergeScans
//
//  Each sector of the stamped scan that is better than the detection
//  scan's: verified where the detection scan did not verify it, or
//  recovered where the detection scan had no bytes for it.
//
////////////////////////////////////////////////////////////////////////////////

void Denibblizer::MergeScans (const TrackScan & stamped, TrackScan & ioScan)
{
    for (int sector = 0; sector < NibbleFormat::kSectorsPerTrack; sector++)
    {
        uint16_t  bit        = static_cast<uint16_t> (1 << sector);
        bool      isVerified = (stamped.mask & bit) != 0 && (ioScan.mask & bit) == 0;
        bool      isBetter   = (stamped.recovered & bit) != 0 && ((ioScan.mask | ioScan.recovered) & bit) == 0;

        if (isVerified)
        {
            ioScan.data[sector] = stamped.data[sector];
            ioScan.mask         = static_cast<uint16_t> (ioScan.mask       |  bit);
            ioScan.recovered    = static_cast<uint16_t> (ioScan.recovered  & ~bit);
            ioScan.lost         = static_cast<uint16_t> (ioScan.lost       & ~bit);
            ioScan.duplicated   = static_cast<uint16_t> (ioScan.duplicated | (stamped.duplicated & bit));
        }
        else if (isBetter)
        {
            ioScan.data[sector] = stamped.data[sector];
            ioScan.recovered    = static_cast<uint16_t> (ioScan.recovered | bit);
            ioScan.lost         = static_cast<uint16_t> (ioScan.lost      & ~bit);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblizer::WriteTrack
//
//  Each verified sector's bytes into its place in the sector image, and
//  with keepRecovered, each recovered one's.
//
////////////////////////////////////////////////////////////////////////////////

void Denibblizer::WriteTrack (const TrackScan & scan, int track, const int * interleave, bool keepRecovered, vector<Byte> & out)
{
    uint16_t  written = static_cast<uint16_t> (scan.mask | (keepRecovered ? scan.recovered : 0));



    for (int sector = 0; sector < NibbleFormat::kSectorsPerTrack; sector++)
    {
        size_t  offset = static_cast<size_t> (track * NibbleFormat::kSectorsPerTrack + interleave[sector]) * NibbleFormat::kSectorByteSize;

        if ((written & (1 << sector)) != 0)
        {
            memcpy (&out[offset], scan.data[(size_t) sector].data(), NibbleFormat::kSectorByteSize);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Denibblizer::ReportTrack
//
//  The track's counts in the sector report, and its outcome in the coverage
//  report.
//
//  COVERAGE IS INDEXED BY OUTPUT SLOT, and the scan's mask is indexed by the
//  number the address field gave. The two differ by the interleave, so a
//  consumer holding the buffer and reading the mask directly would test the
//  wrong sector and find zeros in a slot the report calls present.
//
//  UNFORMATTED MEANS NO ADDRESS FIELD ANYWHERE, not "no bits". A wiped track
//  still has bits, and a track whose headers are all destroyed yields
//  nothing at all. A track with data fields and no address field the
//  decoder accepts was written: its sectors are lost, not blank.
//
////////////////////////////////////////////////////////////////////////////////

void Denibblizer::ReportTrack (
    const TrackScan                  &  scan,
    int                                 track,
    const TrackBitReader::TrackBits  &  src,
    const TrackMarks                 *  marks,
    const int                        *  interleave,
    DenibblizeReport                 &  report,
    SectorDecodeReport               *  outCoverage)
{
    uint16_t  slotMask = 0;
    int       decoded  = TrackSectorDecoder::PopCount (scan.mask);
    bool      hasData  = false;



    for (int sector = 0; sector < NibbleFormat::kSectorsPerTrack; sector++)
    {
        if ((scan.mask & (1 << sector)) != 0)
        {
            slotMask = static_cast<uint16_t> (slotMask | (1 << interleave[sector]));
        }
    }

    report.decodedSectorMask[static_cast<size_t> (track)] = scan.mask;

    if (outCoverage != nullptr)
    {
        TrackDecodeOutcome  outcome = TrackDecodeOutcome::Partial;

        hasData = TrackSectorDecoder::HasDataField (src) || (marks != nullptr && TrackSectorDecoder::HasDataField (src, marks));

        if (src.bitCount == 0 || (slotMask == 0 && scan.recovered == 0 && scan.lost == 0 && !hasData))
        {
            outcome = TrackDecodeOutcome::Unformatted;
        }
        else if (slotMask == kAllSectorsMask && scan.duplicated == 0)
        {
            outcome = TrackDecodeOutcome::Complete;
        }

        outCoverage->SetOutcome (track, outcome, slotMask, scan.duplicated != 0);
    }

    if (src.bitCount == 0)
    {
        return;
    }

    report.tracksPresent++;
    report.sectorsVerified  += decoded;
    report.sectorsRecovered += TrackSectorDecoder::PopCount (scan.recovered);
    report.sectorsLost      += TrackSectorDecoder::PopCount (scan.lost);

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




