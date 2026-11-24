#include "Pch.h"

#include "Devices/Disk/Stamp/FormatStampBuilder.h"

#include "Version.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Inspector/TrackAnalysis.h"
#include "Devices/Disk/Inspector/TrackCopy.h"
#include "Devices/Disk/Stamp/MarkDiscovery.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::Build
//
//  An entry for each position GetStampPositions lists, built with the
//  decode settings. The stamp lists the settings when they are
//  not standard. With a capture report, the stamp has its facts.
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampBuilder::Build (
    const DiskCopy        &  copy,
    const DecodeSettings  &  settings,
    StampSource              source,
    const CaptureReport   *  capture,
    const std::string     &  written,
    FormatStamp           &  outStamp)
{
    outStamp         = FormatStamp();
    outStamp.writer  = GetWriter();
    outStamp.written = written;
    outStamp.source  = source;

    if (!settings.IsStandard())
    {
        outStamp.decodeRanges = settings.GetRanges();
    }

    for (int position : GetStampPositions (copy))
    {
        StampTrack  track;

        BuildTrack (copy, position, settings, track);
        outStamp.tracks.push_back (std::move (track));
    }

    if (capture != nullptr)
    {
        FillCapture (*capture, outStamp);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::GetWriter
//
////////////////////////////////////////////////////////////////////////////////

std::string FormatStampBuilder::GetWriter()
{
    return "Casso " VERSION_STRING;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::GetStampPositions
//
//  Every whole track with a record, and each position between two whole
//  tracks whose record is neither of theirs: a half or quarter track with a
//  separate stream.
//
////////////////////////////////////////////////////////////////////////////////

vector<int> FormatStampBuilder::GetStampPositions (const DiskCopy & copy)
{
    constexpr int  kPerTrack = DiskImage::kQuarterTracksPerWholeTrack;
    vector<int>    positions;
    int            count     = (int) copy.playedSlot.size();



    for (int position = 0; position < count; position++)
    {
        int   slot  = copy.playedSlot[(size_t) position];
        int   phase = position % kPerTrack;
        int   below = position - phase;
        int   above = below + kPerTrack;
        bool  isOwn = slot >= 0;

        if (isOwn && phase != 0)
        {
            isOwn = slot != copy.playedSlot[(size_t) below] && (above >= count || slot != copy.playedSlot[(size_t) above]);
        }

        if (isOwn)
        {
            positions.push_back (position);
        }
    }

    return positions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::BuildTrack
//
//  One position's entry. When sectors are unverified under the settings, the
//  track is analyzed again with the marks found on it added to the
//  settings' marks for its track, and that analysis is used when it has
//  more good sectors.
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampBuilder::BuildTrack (const DiskCopy & copy, int position, const DecodeSettings & settings, StampTrack & outTrack)
{
    int                                   slot     = copy.playedSlot[(size_t) position];
    TrackContext                          context  = DiskAnalyzer::GetContext (copy, slot);
    std::shared_ptr<const TrackAnalysis>  analysis = DiskAnalyzer::AnalyzeRecord (copy, slot, settings);
    std::shared_ptr<const TrackAnalysis>  retry;
    DecodeSettings                        widened  = settings;
    DecodeRange                           range;
    FieldMarks                            found;



    outTrack          = StampTrack();
    outTrack.position = position;

    if (slot >= 0 && slot < (int) copy.tracks.size() && copy.tracks[(size_t) slot] != nullptr)
    {
        outTrack.bitsCrc    = ComputeTrackCrc (*copy.tracks[(size_t) slot]);
        outTrack.hasBitsCrc = true;
    }

    if (analysis == nullptr)
    {
        return;
    }

    if (IsUnverified (*analysis))
    {
        MarkDiscovery::Discover (analysis->framed, context.physicalTrack, found);

        range.firstTrack       = context.physicalTrack;
        range.lastTrack        = context.physicalTrack;
        range.customMarks      = settings.GetMarksForTrack (context.physicalTrack);
        range.checks           = settings.GetChecksForTrack (context.physicalTrack);
        range.matchStandardToo = false;

        range.customMarks.address16.insert (range.customMarks.address16.begin(), found.address16.begin(), found.address16.end());
        range.customMarks.address13.insert (range.customMarks.address13.begin(), found.address13.begin(), found.address13.end());
        range.customMarks.data.insert      (range.customMarks.data.begin(),      found.data.begin(),      found.data.end());

        widened.AddRange (range);
        retry = DiskAnalyzer::AnalyzeRecord (copy, slot, widened);

        if (retry != nullptr && retry->sectorsGood > analysis->sectorsGood)
        {
            analysis = retry;
        }
    }

    FillTrack (*analysis, outTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::ComputeTrackCrc
//
//  The CRC-32 of a track record's bits, or of its flux bytes for a flux
//  track. A save compares it with the entry's to find the tracks rewritten
//  since the stamp was built.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FormatStampBuilder::ComputeTrackCrc (const TrackCopy & track)
{
    constexpr size_t  kByteBits = 8;
    size_t            bytes     = std::min (track.bits.size(), (track.bitCount + kByteBits - 1) / kByteBits);



    return track.kind == TrackKind::Flux ? ZipArchive::ComputeCrc32 (track.fluxBytes)
                                         : ZipArchive::ComputeCrc32 (std::span<const Byte> (track.bits.data(), bytes));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::IsUnverified
//
//  Whether the analysis has a data field without an address field, a
//  sector without good checksums, or a nonstandard or unformatted class.
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampBuilder::IsUnverified (const TrackAnalysis & analysis)
{
    bool  hasOrphan = std::ranges::any_of (analysis.fields, [] (const LocatedField & field) { return field.role == FieldRole::Data && field.pairedField < 0; });



    return hasOrphan
        || analysis.trackClass == TrackClass::Nonstandard
        || analysis.trackClass == TrackClass::Unformatted
        || analysis.sectorsGood < analysis.sectorsFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::FillTrack
//
//  The entry for one analyzed track: its encoding, the marks its fields
//  have, in the order first found, the sector numbers of its good address
//  fields in disk order, its volumes, its checksum results and its counts.
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampBuilder::FillTrack (const TrackAnalysis & analysis, StampTrack & outTrack)
{
    outTrack.encoding     = GetEncoding (analysis);
    outTrack.sectorsFound = analysis.sectorsFound;
    outTrack.sectorsGood  = analysis.sectorsGood;
    outTrack.isVerified   = analysis.sectorsFound > 0 && analysis.sectorsGood == analysis.sectorsFound;

    for (const LocatedField & field : analysis.fields)
    {
        int           length  = std::clamp (field.bodyNibble - field.firstNibble, 0, DiskMarkPattern::kMaxNibbles);
        vector<Byte>  prolog (field.prologueFound.begin(), field.prologueFound.begin() + length);

        if (field.role == FieldRole::Data)
        {
            AddDistinct (outTrack.dataPrologs, std::move (prolog));
            AddEpilog   (field, outTrack.dataEpilogs);
            continue;
        }

        AddDistinct (outTrack.addressPrologs, std::move (prolog));
        AddEpilog   (field, outTrack.addressEpilogs);

        if (field.isAddressChecksumGood)
        {
            outTrack.sectors.push_back (field.sector);

            if (std::ranges::find (outTrack.volumes, field.volume) == outTrack.volumes.end())
            {
                outTrack.volumes.push_back (field.volume);
            }
        }
    }

    FillChecksums (analysis, outTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::GetEncoding
//
////////////////////////////////////////////////////////////////////////////////

StampEncoding FormatStampBuilder::GetEncoding (const TrackAnalysis & analysis)
{
    StampEncoding  encoding = StampEncoding::Nonstandard;



    switch (analysis.trackClass)
    {
        case TrackClass::NothingRecorded:    encoding = StampEncoding::Empty;       break;
        case TrackClass::Sixteen:            encoding = StampEncoding::Sixteen;     break;
        case TrackClass::Thirteen:           encoding = StampEncoding::Thirteen;    break;
        case TrackClass::ThirteenAndSixteen: encoding = StampEncoding::Both;        break;
        case TrackClass::Unformatted:        encoding = StampEncoding::Unformatted; break;
        case TrackClass::Damaged:            encoding = StampEncoding::Nonstandard; break;
        case TrackClass::Nonstandard:        encoding = StampEncoding::Nonstandard; break;
    }

    //  A track whose every sector decodes with marks the analysis was given
    //  is that encoding, whatever its class under the standard marks.
    if (encoding == StampEncoding::Nonstandard && analysis.sectorsFound > 0 && analysis.sectorsGood == analysis.sectorsFound)
    {
        encoding = analysis.has13 && analysis.has16 ? StampEncoding::Both
                 : analysis.has13                   ? StampEncoding::Thirteen
                 :                                    StampEncoding::Sixteen;
    }

    return encoding;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::AddDistinct
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampBuilder::AddDistinct (vector<vector<Byte>> & ioLists, vector<Byte> && nibbles)
{
    if (!nibbles.empty() && std::ranges::find (ioLists, nibbles) == ioLists.end())
    {
        ioLists.push_back (std::move (nibbles));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::AddEpilog
//
//  The epilog a field ends with: the matched mark and its tail nibble when
//  it has one, or the three nibbles after the body when no epilog mark
//  matched.
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampBuilder::AddEpilog (const LocatedField & field, vector<vector<Byte>> & ioLists)
{
    constexpr size_t  kStandardLength = 2;
    size_t            length          = DiskMarkPattern::kMaxNibbles;



    if (field.isEpilogueStandard)
    {
        length = kStandardLength + (field.hasEpilogueTail ? 1 : 0);
    }

    AddDistinct (ioLists, vector<Byte> (field.epilogueFound.begin(), field.epilogueFound.begin() + (ptrdiff_t) length));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::FillChecksums
//
//  Not checked when the settings turned a check off for a sector, failing
//  when a checked sector's checksum didn't match, and standard otherwise.
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampBuilder::FillChecksums (const TrackAnalysis & analysis, StampTrack & outTrack)
{
    for (const AnalyzedSector & sector : analysis.sectors)
    {
        if (!sector.isAddressCheck)
        {
            outTrack.addressChecksum = StampChecksum::NotChecked;
        }
        else if (!sector.isAddressGood && outTrack.addressChecksum == StampChecksum::Standard)
        {
            outTrack.addressChecksum = StampChecksum::Failing;
        }

        if (sector.dataField < 0)
        {
            continue;
        }

        if (!sector.isDataCheck)
        {
            outTrack.dataChecksum = StampChecksum::NotChecked;
        }
        else if (!sector.isDataGood && outTrack.dataChecksum == StampChecksum::Standard)
        {
            outTrack.dataChecksum = StampChecksum::Failing;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder::FillCapture
//
//  The capture's facts for the disk, and for each entry the facts of its
//  position.
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampBuilder::FillCapture (const CaptureReport & capture, FormatStamp & ioStamp)
{
    ioStamp.hasCapture = true;
    ioStamp.capture    = capture.facts;

    for (StampTrack & track : ioStamp.tracks)
    {
        track.hasCapture                = true;
        track.capture.isWeak            = std::ranges::find (capture.weak,     track.position) != capture.weak.end();
        track.capture.isUnstable        = std::ranges::find (capture.unstable, track.position) != capture.unstable.end();
        track.capture.hasSeparateStream = track.position % DiskImage::kQuarterTracksPerWholeTrack != 0
                                       && std::ranges::find (capture.streams, track.position) != capture.streams.end();

        for (const auto & [position, sector] : capture.replaced)
        {
            if (position == track.position)
            {
                track.capture.replacedSectors.push_back (sector);
            }
        }
    }
}
