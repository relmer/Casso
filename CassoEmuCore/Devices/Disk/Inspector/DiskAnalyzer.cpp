#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Devices/Disk/FluxTrack.h"
#include "Machines/Apple2/Common/Disk2Controller.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::Analyze
//
//  Every record some quarter track plays, or would play were it not damaged,
//  is analyzed once; records nothing reads are left out.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::Analyze (std::shared_ptr<const DiskCopy> copy, const DecodeSettings & settings, DiskAnalysis & out)
{
    vector<int>  slots;
    int          slot  = 0;



    out           = DiskAnalysis();
    out.mediaId   = copy->mediaId;
    out.settings  = settings;
    out.headLimit = Disk2Controller::kMaxQuarterTrack;

    for (slot = 0; slot < static_cast<int> (copy->tracks.size()); slot++)
    {
        slots.push_back (slot);
    }

    Reanalyze (copy, slots, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::Reanalyze
//
//  The given records analyzed again from a newer copy, every other record's
//  analysis kept, then everything built from the whole disk built again.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::Reanalyze (std::shared_ptr<const DiskCopy> copy, std::span<const int> slots, DiskAnalysis & inOut)
{
    for (int slot : slots)
    {
        Accept (copy, slot, AnalyzeRecord (*copy, slot, inOut.settings), inOut);
    }

    Assemble (inOut);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::AnalyzeRecord
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const TrackAnalysis> DiskAnalyzer::AnalyzeRecord (const DiskCopy & copy, int slot, const DecodeSettings & settings)
{
    std::shared_ptr<TrackAnalysis>  analysis;



    if (slot >= 0 && slot < static_cast<int> (copy.tracks.size()) && !GetQuarterTracksOfSlot (copy, slot).empty())
    {
        analysis = std::make_shared<TrackAnalysis>();
        TrackAnalyzer::Analyze (*copy.tracks[slot], GetContext (copy, slot), settings, *analysis);
    }

    return analysis;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::Accept
//
//  The copy a result was made from becomes the disk's copy: copies of one
//  disk differ only in the records the guest wrote since.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::Accept (std::shared_ptr<const DiskCopy> copy, int slot, std::shared_ptr<const TrackAnalysis> track, DiskAnalysis & inOut)
{
    inOut.copy    = copy;
    inOut.mediaId = copy->mediaId;
    inOut.tracks.resize (copy->tracks.size());

    if (slot >= 0 && slot < static_cast<int> (inOut.tracks.size()))
    {
        inOut.tracks[slot] = std::move (track);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::GetQuarterTracksOfSlot
//
//  The quarter tracks that play a record, in order, or for a damaged record
//  the ones the map gives it.
//
////////////////////////////////////////////////////////////////////////////////

vector<int> DiskAnalyzer::GetQuarterTracksOfSlot (const DiskCopy & copy, int slot)
{
    vector<int>  qts;
    bool         isDamaged = copy.IsSlotDamaged (slot);
    int          qt        = 0;



    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        if (copy.playedSlot[qt] == slot || (isDamaged && copy.mappedSlot[qt] == slot))
        {
            qts.push_back (qt);
        }
    }

    return qts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::GetContext
//
//  A record's physical track is the whole track nearest the middle of the
//  quarter tracks that play it. When that middle is a half track N.5, an
//  address field may give N or N+1.
//
////////////////////////////////////////////////////////////////////////////////

TrackContext DiskAnalyzer::GetContext (const DiskCopy & copy, int slot)
{
    static constexpr double  kPerTrack = DiskImage::kQuarterTracksPerWholeTrack;
    static constexpr double  kHalf     = 0.5;



    vector<int>   qts    = GetQuarterTracksOfSlot (copy, slot);
    TrackContext  context;
    double        middle = 0;



    if (!qts.empty())
    {
        middle                = (qts.front() + qts.back()) / 2.0 / kPerTrack;
        context.physicalTrack = static_cast<int> (std::floor (middle + kHalf));
        context.alsoTrack     = (middle - std::floor (middle) == kHalf) ? context.physicalTrack - 1 : -1;
    }

    context.isNibbleImage = copy.format == DiskFormat::Nib;
    context.isDamaged     = copy.IsSlotDamaged (slot);

    return context;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::GetHomeQuarterTrack
//
//  Where a record's findings are listed: its whole track when a whole track
//  plays it, else the middle of the quarter tracks that do.
//
////////////////////////////////////////////////////////////////////////////////

int DiskAnalyzer::GetHomeQuarterTrack (const DiskCopy & copy, int slot)
{
    vector<int>  qts  = GetQuarterTracksOfSlot (copy, slot);
    int          home = -1;



    for (int qt : qts)
    {
        if (qt % DiskImage::kQuarterTracksPerWholeTrack == 0 && home < 0)
        {
            home = qt;
        }
    }

    if (home < 0 && !qts.empty())
    {
        home = qts[(qts.size() - 1) / 2];
    }

    return home;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::Assemble
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::Assemble (DiskAnalysis & inOut)
{
    inOut.image = ImageDetails::MakeFromCopy (*inOut.copy);
    inOut.findings.clear();

    Summarize         (inOut);
    BuildEntries      (inOut);
    AddTrackFindings  (inOut);
    AddVolumeFindings (inOut);
    AddLayoutFindings (inOut);
    AddLengthFindings (inOut);
    AddDamageFindings (inOut);
    AddImageFindings  (inOut);
    AddFileSystemFindings (inOut);

    std::stable_sort (inOut.findings.begin(), inOut.findings.end(), [] (const Finding & a, const Finding & b)
    {
        return (a.quarterTrack != b.quarterTrack) ? a.quarterTrack < b.quarterTrack : a.cell < b.cell;
    });

    for (const Finding & f : inOut.findings)
    {
        if (f.quarterTrack >= 0)
        {
            inOut.entries[f.quarterTrack].findingCount++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::BuildEntries (DiskAnalysis & inOut)
{
    const DiskCopy &  copy = *inOut.copy;
    int               qt   = 0;
    int               slot = 0;
    int               peer = 0;



    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        QuarterTrackEntry &  entry = inOut.entries[qt];

        entry                   = QuarterTrackEntry();
        entry.isBeyondHeadReach = qt > inOut.headLimit;
        slot                    = copy.playedSlot[qt];

        for (const DamagedQuarterTrack & d : copy.damagedQuarterTracks)
        {
            if (d.quarterTrack == qt)
            {
                entry.content         = QuarterTrackContent::Damaged;
                entry.hasDamageReason = true;
                entry.damageReason    = DamageReason::MapEntryOutOfRange;
            }
        }

        if (copy.mappedSlot[qt] >= 0 && copy.IsSlotDamaged (copy.mappedSlot[qt]))
        {
            slot                  = copy.mappedSlot[qt];
            entry.content         = QuarterTrackContent::Damaged;
            entry.hasDamageReason = true;

            for (const DamagedTrack & d : copy.damagedTracks)
            {
                entry.damageReason = (d.trkIndex == slot) ? d.reason : entry.damageReason;
            }
        }
        else if (entry.content != QuarterTrackContent::Damaged && slot >= 0)
        {
            entry.content = (copy.tracks[slot]->kind == TrackKind::Flux) ? QuarterTrackContent::FluxTrack : QuarterTrackContent::BitTrack;
        }

        entry.slot          = (entry.content == QuarterTrackContent::Nothing) ? -1 : slot;
        entry.isFromFluxMap = entry.slot >= 0 && copy.woz.layout.hasFluxMap && copy.woz.layout.flux[qt] == entry.slot;
        entry.trackClass    = (entry.content == QuarterTrackContent::Damaged) ? TrackClass::Damaged : TrackClass::NothingRecorded;

        if (entry.slot >= 0 && entry.slot < static_cast<int> (inOut.tracks.size()) && inOut.tracks[entry.slot] != nullptr
            && entry.content != QuarterTrackContent::Damaged)
        {
            entry.trackClass        = inOut.tracks[entry.slot]->trackClass;
            entry.sectorsGood       = inOut.tracks[entry.slot]->sectorsGood;
            entry.sectorsFound      = inOut.tracks[entry.slot]->sectorsFound;
            entry.sectorsNotChecked = inOut.tracks[entry.slot]->sectorsNotChecked;
        }

        for (peer = 0; peer < DiskImage::kQuarterTrackCount && entry.slot >= 0; peer++)
        {
            bool  isPeer = peer != qt && (copy.playedSlot[peer] == entry.slot || copy.mappedSlot[peer] == entry.slot);

            if (isPeer)
            {
                entry.sharesWith.push_back (peer);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::AddTrackFindings
//
//  Each record's own findings, placed at its home quarter track, and 13-sector
//  fields on a disk that also has 16-sector ones.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::AddTrackFindings (DiskAnalysis & inOut)
{
    bool     has16   = false;
    Finding  finding;



    for (const auto & track : inOut.tracks)
    {
        has16 = has16 || (track != nullptr && track->has16);
    }

    for (const auto & track : inOut.tracks)
    {
        if (track == nullptr)
        {
            continue;
        }

        for (Finding f : track->findings)
        {
            f.quarterTrack = GetHomeQuarterTrack (*inOut.copy, track->slot);
            inOut.findings.push_back (f);
        }

        if (has16 && track->has13)
        {
            finding              = Finding();
            finding.category     = FindingCategory::Track;
            finding.kind         = FindingKind::ThirteenSectorFields;
            finding.slot         = track->slot;
            finding.quarterTrack = GetHomeQuarterTrack (*inOut.copy, track->slot);
            inOut.findings.push_back (finding);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::AddVolumeFindings
//
//  An address field whose volume differs from the disk's most common one.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::AddVolumeFindings (DiskAnalysis & inOut)
{
    Finding  finding;
    int      f       = 0;



    for (const auto & track : inOut.tracks)
    {
        if (track == nullptr || inOut.summary.commonVolume < 0)
        {
            continue;
        }

        for (f = 0; f < static_cast<int> (track->fields.size()); f++)
        {
            const LocatedField &  field = track->fields[f];

            if (field.role != FieldRole::Address || !field.isAddressChecksumGood || field.volume == inOut.summary.commonVolume)
            {
                continue;
            }

            finding              = Finding();
            finding.category     = FindingCategory::Field;
            finding.kind         = FindingKind::VolumeDiffers;
            finding.slot         = track->slot;
            finding.field        = f;
            finding.sector       = field.sector;
            finding.cell         = field.startCell;
            finding.hasCell      = true;
            finding.value        = field.volume;
            finding.value2       = inOut.summary.commonVolume;
            finding.quarterTrack = GetHomeQuarterTrack (*inOut.copy, track->slot);
            inOut.findings.push_back (finding);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::AddLayoutFindings
//
//  A record played strictly between whole tracks N and N+1 that is neither
//  track N's record nor track N+1's, a record no whole track plays, or one
//  more than one whole track plays: one finding each, with the quarter tracks
//  that play it. The standard WOZ layout, the one that also maps N+0.5 to a
//  neighbor, and Casso's own layout all pass, so a standard disk in any of
//  them has none.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::AddLayoutFindings (DiskAnalysis & inOut)
{
    static constexpr int  kPerTrack = DiskImage::kQuarterTracksPerWholeTrack;



    const DiskCopy &  copy    = *inOut.copy;
    vector<int>       qts;
    int               slot    = 0;
    int               wholes  = 0;
    bool              between = false;
    int               below   = 0;
    int               above   = 0;
    Finding           finding;



    for (slot = 0; slot < static_cast<int> (copy.tracks.size()); slot++)
    {
        qts     = GetQuarterTracksOfSlot (copy, slot);
        wholes  = 0;
        between = false;

        if (qts.empty() || copy.IsSlotDamaged (slot))
        {
            continue;
        }

        for (int qt : qts)
        {
            below   = copy.playedSlot[(qt / kPerTrack) * kPerTrack];
            above   = (qt / kPerTrack + 1) * kPerTrack < DiskImage::kQuarterTrackCount ? copy.playedSlot[(qt / kPerTrack + 1) * kPerTrack] : -1;
            wholes += (qt % kPerTrack == 0) ? 1 : 0;
            between = between || (qt % kPerTrack != 0 && slot != below && slot != above);
        }

        finding               = Finding();
        finding.category      = FindingCategory::QuarterTrack;
        finding.slot          = slot;
        finding.quarterTracks = qts;
        finding.quarterTrack  = GetHomeQuarterTrack (copy, slot);

        if (between)
        {
            finding.kind = FindingKind::RecordBetweenTracks;
        }
        else if (wholes == 0)
        {
            finding.kind = FindingKind::RecordWithoutWholeTrack;
        }
        else if (wholes > 1)
        {
            finding.kind = FindingKind::RecordAcrossWholeTracks;
        }

        if (between || wholes != 1)
        {
            inOut.findings.push_back (finding);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::AddLengthFindings
//
//  On a WOZ, a bit track more than 2% from 51,200 cells, or a flux track
//  whose turn is more than 2% from 200 ms. Sector and nibble images take
//  their track lengths from the format, so they get none.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::AddLengthFindings (DiskAnalysis & inOut)
{
    static constexpr double  kNominalCells = FluxTrack::kNominalRevolutionCells;
    static constexpr double  kNominalTurn  = kNominalCells * FluxTrack::kCellNumerator / FluxTrack::kCellDenominator;
    static constexpr double  kPermille     = 1000.0;



    double   ratio   = 0;
    Finding  finding;



    for (const auto & track : inOut.tracks)
    {
        if (track == nullptr || inOut.copy->format != DiskFormat::Woz || track->framed.cellCount == 0)
        {
            continue;
        }

        ratio = track->framed.isFlux ? track->framed.turnTicks / kNominalTurn - 1.0
                                     : track->framed.cellCount / kNominalCells - 1.0;

        if (std::abs (ratio) <= kTrackLengthTolerance)
        {
            continue;
        }

        finding              = Finding();
        finding.category     = FindingCategory::Track;
        finding.kind         = FindingKind::TrackLengthDiffers;
        finding.slot         = track->slot;
        finding.value        = static_cast<int> (track->framed.cellCount);
        finding.value2       = static_cast<int> (std::lround (ratio * kPermille));
        finding.quarterTrack = GetHomeQuarterTrack (*inOut.copy, track->slot);
        inOut.findings.push_back (finding);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::AddDamageFindings
//
//  Each damaged record, with every quarter track the map gives it, and each
//  map entry that points at no record.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::AddDamageFindings (DiskAnalysis & inOut)
{
    Finding  finding;



    for (const DamagedTrack & d : inOut.copy->damagedTracks)
    {
        finding               = Finding();
        finding.category      = FindingCategory::ImageFile;
        finding.kind          = FindingKind::DamagedRecord;
        finding.slot          = d.trkIndex;
        finding.value         = static_cast<int> (d.reason);
        finding.quarterTracks = GetQuarterTracksOfSlot (*inOut.copy, d.trkIndex);
        finding.quarterTrack  = GetHomeQuarterTrack (*inOut.copy, d.trkIndex);
        inOut.findings.push_back (finding);
    }

    for (const DamagedQuarterTrack & d : inOut.copy->damagedQuarterTracks)
    {
        finding              = Finding();
        finding.category     = FindingCategory::ImageFile;
        finding.kind         = FindingKind::DamagedMapEntry;
        finding.quarterTrack = d.quarterTrack;
        finding.value        = d.entry;
        finding.value2       = d.isFlux ? 1 : 0;
        inOut.findings.push_back (finding);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::AddImageFindings
//
//  The image file's problems, and on a nibble image a track with no sync.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::AddImageFindings (DiskAnalysis & inOut)
{
    Finding  finding;



    inOut.findings.insert (inOut.findings.end(), inOut.image.problems.begin(), inOut.image.problems.end());

    for (const auto & track : inOut.tracks)
    {
        if (track == nullptr || inOut.copy->format != DiskFormat::Nib)
        {
            continue;
        }

        if (std::find (track->nibbleKinds.begin(), track->nibbleKinds.end(), NibbleKind::Sync) == track->nibbleKinds.end())
        {
            finding              = Finding();
            finding.category     = FindingCategory::ImageFile;
            finding.kind         = FindingKind::NibTrackWithoutSync;
            finding.slot         = track->slot;
            finding.quarterTrack = GetHomeQuarterTrack (*inOut.copy, track->slot);
            inOut.findings.push_back (finding);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::Summarize
//
//  Each record counted once. The format is the first that applies: nothing
//  recorded, 13 and 16 sector, 16 sector, 13 sector, nonstandard,
//  unformatted, otherwise damaged.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::Summarize (DiskAnalysis & inOut)
{
    static constexpr int  kVolumes = 256;



    DiskSummary                summary;
    std::array<int, kVolumes>  volumes      = {};
    bool                       has16        = false;
    bool                       has13        = false;
    bool                       hasAny       = false;
    int                        best         = 0;
    int                        v            = 0;



    for (const auto & track : inOut.tracks)
    {
        if (track == nullptr)
        {
            continue;
        }

        hasAny = true;
        has16  = has16 || track->has16;
        has13  = has13 || track->has13;

        summary.tracksWithData    += (track->trackClass != TrackClass::NothingRecorded && track->trackClass != TrackClass::Damaged) ? 1 : 0;
        summary.sectorsGood       += track->sectorsGood;
        summary.sectorsFound      += track->sectorsFound;
        summary.sectorsNotChecked += track->sectorsNotChecked;
        summary.badSectors        += track->sectorsBad;
        summary.nonstandardTracks += (track->trackClass == TrackClass::Nonstandard) ? 1 : 0;
        summary.unformattedTracks += (track->trackClass == TrackClass::Unformatted) ? 1 : 0;
        summary.fluxTracks        += (track->kind == TrackKind::Flux && track->framed.cellCount > 0) ? 1 : 0;

        for (const LocatedField & f : track->fields)
        {
            if (f.role == FieldRole::Address && f.isAddressChecksumGood)
            {
                volumes[f.volume]++;
            }
        }
    }

    summary.damagedTracks = static_cast<int> (inOut.copy->damagedTracks.size() + inOut.copy->damagedQuarterTracks.size());
    hasAny                = hasAny || summary.damagedTracks > 0;

    for (v = 0; v < kVolumes; v++)
    {
        if (volumes[v] > best)
        {
            best                 = volumes[v];
            summary.commonVolume = v;
        }
    }

    if (!hasAny)
    {
        summary.format = DiskFormatClass::NothingRecorded;
    }
    else if (has16 && has13)
    {
        summary.format = DiskFormatClass::ThirteenAndSixteen;
    }
    else if (has16)
    {
        summary.format = DiskFormatClass::Sixteen;
    }
    else if (has13)
    {
        summary.format = DiskFormatClass::Thirteen;
    }
    else if (summary.nonstandardTracks > 0)
    {
        summary.format = DiskFormatClass::Nonstandard;
    }
    else if (summary.unformattedTracks > 0)
    {
        summary.format = DiskFormatClass::Unformatted;
    }
    else if (summary.tracksWithData == 0 && summary.damagedTracks == 0)
    {
        summary.format = DiskFormatClass::NothingRecorded;
    }
    else
    {
        summary.format = DiskFormatClass::Damaged;
    }

    inOut.summary = summary;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer::AddFileSystemFindings
//
//  The file and sector map of every volume found, built again from the
//  analysis whenever it is assembled (FR-083), and its findings (FR-093).
//  Until every whole track's record is analyzed, a map would read sectors
//  still being analyzed as missing, so none is built.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAnalyzer::AddFileSystemFindings (DiskAnalysis & inOut)
{
    bool  isReady = true;
    int   qt      = 0;
    int   slot    = -1;



    inOut.fileMaps.clear();

    for (qt = 0; isReady && qt < DiskImage::kQuarterTrackCount; qt += DiskImage::kQuarterTracksPerWholeTrack)
    {
        slot    = inOut.entries[qt].slot;
        isReady = slot < 0 || (slot < static_cast<int> (inOut.tracks.size()) && inOut.tracks[slot] != nullptr);
    }

    if (isReady)
    {
        inOut.fileMaps = FileMapBuilder::Build (inOut);

        for (const FileMap & map : inOut.fileMaps)
        {
            inOut.findings.insert (inOut.findings.end(), map.findings.begin(), map.findings.end());
        }
    }
}
