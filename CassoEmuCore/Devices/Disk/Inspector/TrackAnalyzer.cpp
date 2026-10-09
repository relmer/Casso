#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Devices/Disk/Inspector/StandardTrackDecoder.h"
#include "Devices/Disk/FluxTrack.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::Analyze
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::Analyze (const TrackCopy & copy, const TrackContext & context, const DecodeSettings & settings, TrackAnalysis & out)
{
    DecodeChecks          checks = settings.GetChecksForTrack (context.physicalTrack);
    StandardTrackDecoder  decoder;



    out                 = TrackAnalysis();
    out.slot            = copy.slot;
    out.kind            = copy.kind;
    out.guestWriteCount = copy.guestWriteCount;
    out.context         = context;

    LatchFramer::Frame (copy, out.framed);

    if (out.framed.cellCount == 0)
    {
        out.trackClass = context.isDamaged ? TrackClass::Damaged : TrackClass::NothingRecorded;
        return;
    }

    decoder.Decode (out.framed, settings.GetMarksForTrack (context.physicalTrack), out.fields);

    KindNibbles      (out, checks);
    BuildSectors     (out, checks);
    Classify         (out);
    CountSectors     (out);
    Measure          (out);
    MeasureGaps      (out);
    FindFieldIssues  (out, checks);
    FindSectorIssues (out);
    FindStrayMarks   (out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::GetAngle
//
//  Where a cell is around the turn, as a fraction of it from the index: its
//  place among the cells on a bit track, its time over the time of one turn on
//  a flux track. This is how Casso's drive places the head on each.
//
////////////////////////////////////////////////////////////////////////////////

double TrackAnalyzer::GetAngle (const TrackAnalysis & analysis, uint32_t cell)
{
    const FramedTrack &  track   = analysis.framed;
    double               elapsed = 0;
    uint32_t             k       = 0;
    double               angle   = 0;



    if (track.cellCount == 0)
    {
        return 0;
    }

    if (track.isFlux && track.turnTicks > 0)
    {
        for (k = 0; k < cell && k < track.cellCount; k++)
        {
            elapsed += track.cellTicks[k];
        }

        angle = elapsed / track.turnTicks;
    }
    else
    {
        angle = static_cast<double> (cell % track.cellCount) / static_cast<double> (track.cellCount);
    }

    return angle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::KindNibbles
//
//  Noise first, then sync, then every nibble inside a field takes the field's
//  kind. A sync nibble is an FF followed by one or two zero cells; a nibble
//  image stores no zero cells, so there an FF beside another FF outside a
//  field counts as sync.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::KindNibbles (TrackAnalysis & inOut, const DecodeChecks & checks)
{
    static constexpr Byte  kSync         = 0xFF;
    static constexpr int   kMaxSyncZeros = 2;



    const vector<FramedNibble> &  nibbles = inOut.framed.nibbles;
    size_t                        n       = nibbles.size();
    size_t                        i       = 0;
    bool                          isSync  = false;
    bool                          hasPair = false;
    size_t                        f       = 0;



    inOut.nibbleKinds.assign      (n, NibbleKind::Other);
    inOut.isFailedChecksum.assign (n, 0);
    inOut.fieldOfNibble.assign    (n, -1);

    for (i = 0; i < n; i++)
    {
        hasPair = nibbles[(i + n - 1) % n].value == kSync || nibbles[(i + 1) % n].value == kSync;
        isSync  = nibbles[i].value == kSync
               && (inOut.context.isNibbleImage ? hasPair
                                               : nibbles[i].extraZeroCells >= 1 && nibbles[i].extraZeroCells <= kMaxSyncZeros);

        if (nibbles[i].isNoise)
        {
            inOut.nibbleKinds[i] = NibbleKind::Noise;
        }
        else if (isSync)
        {
            inOut.nibbleKinds[i] = NibbleKind::Sync;
        }
    }

    for (f = 0; f < inOut.fields.size(); f++)
    {
        KindField (inOut, static_cast<int> (f), checks);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::KindField
//
//  A field's prologue, body (with a data field's checksum nibble) and
//  epilogue. A field whose checksum failed while its check is on marks its
//  prologue and body as failed.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::KindField (TrackAnalysis & inOut, int fieldIndex, const DecodeChecks & checks)
{
    const LocatedField &  field     = inOut.fields[fieldIndex];
    bool                  isAddress = field.role == FieldRole::Address;
    size_t                n         = inOut.framed.nibbles.size();
    int                   prologue  = field.bodyNibble - field.firstNibble;
    int                   body      = isAddress ? DiskFieldFormat::kAddressNibbles : DiskFieldFormat::GetBodyLength (field.kind) + 1;
    bool                  isFailed  = isAddress ? (checks.isAddressChecksumOn && !field.isAddressChecksumGood)
                                                : (checks.isDataChecksumOn && !field.data.isChecksumGood);
    int                   j         = 0;
    size_t                idx       = 0;
    NibbleKind            kind      = NibbleKind::Other;



    for (j = 0; j < field.nibbleCount; j++)
    {
        idx = (static_cast<size_t> (field.firstNibble) + j) % n;

        if (j < prologue)
        {
            kind = isAddress ? NibbleKind::AddressPrologue : NibbleKind::DataPrologue;
        }
        else if (j < prologue + body)
        {
            kind = isAddress ? NibbleKind::AddressField : NibbleKind::DataField;
        }
        else
        {
            kind = isAddress ? NibbleKind::AddressEpilogue : NibbleKind::DataEpilogue;
        }

        inOut.nibbleKinds[idx]      = kind;
        inOut.fieldOfNibble[idx]    = fieldIndex;
        inOut.isFailedChecksum[idx] = (isFailed && j < prologue + body) ? 1 : 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::BuildSectors
//
//  One sector per address field, in passing order, with its state: a failed
//  address checksum outranks a missing data field, which outranks a check
//  being off, which outranks a failed data checksum.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::BuildSectors (TrackAnalysis & inOut, const DecodeChecks & checks)
{
    AnalyzedSector  sector;
    size_t          f       = 0;
    int             passing = 0;



    inOut.sectors.clear();

    for (f = 0; f < inOut.fields.size(); f++)
    {
        const LocatedField &  address = inOut.fields[f];

        if (address.role != FieldRole::Address)
        {
            continue;
        }

        sector                = AnalyzedSector();
        sector.addressField   = static_cast<int> (f);
        sector.dataField      = address.pairedField;
        sector.sector         = address.sector;
        sector.kind           = address.kind;
        sector.isAddressGood  = address.isAddressChecksumGood;
        sector.isAddressCheck = checks.isAddressChecksumOn;
        sector.isDataCheck    = checks.isDataChecksumOn;
        sector.isDataGood     = sector.dataField >= 0 && inOut.fields[sector.dataField].data.isChecksumGood;
        sector.passingIndex   = passing++;

        if (sector.isAddressCheck && !sector.isAddressGood)
        {
            sector.state = SectorState::BadAddress;
        }
        else if (sector.dataField < 0)
        {
            sector.state = SectorState::NoDataField;
        }
        else if (!sector.isAddressCheck || !sector.isDataCheck)
        {
            sector.state = SectorState::NotChecked;
        }
        else if (!sector.isDataGood)
        {
            sector.state = SectorState::BadData;
        }

        if (inOut.context.isDamaged)
        {
            sector.editBlock = EditBlock::DamagedRecord;
        }
        else if (sector.dataField < 0)
        {
            sector.editBlock = EditBlock::NoDataField;
        }
        else if (sector.isAddressCheck && !sector.isAddressGood)
        {
            sector.editBlock = EditBlock::AddressChecksumFailed;
        }
        else if (inOut.fields[sector.dataField].hasNoise)
        {
            sector.editBlock = EditBlock::NoiseInDataField;
        }
        else if (!inOut.fields[sector.dataField].data.invalidNibbles.empty())
        {
            sector.editBlock = EditBlock::NibblesOutsideTable;
        }

        MapLogical (inOut.context.physicalTrack, sector);
        inOut.sectors.push_back (sector);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::MapLogical
//
//  On a 16-sector track, the DOS 3.3 logical sector and the ProDOS block and
//  half the physical sector holds under the standard skew.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::MapLogical (int physicalTrack, AnalyzedSector & inOut)
{
    static constexpr int  kHalvesPerBlock = 2;
    static constexpr int  kBlocksPerTrack = NibblizationLayer::kSectorsPerTrack / kHalvesPerBlock;



    int  po = 0;



    if (inOut.kind != DiskFieldKind::Sixteen || inOut.sector >= NibblizationLayer::kSectorsPerTrack)
    {
        return;
    }

    inOut.dos33Logical = NibblizationLayer::GetDosFileIndexForPhysicalSector (inOut.sector);
    po                 = NibblizationLayer::GetPoFileIndexForDosLogicalSector (inOut.dos33Logical);
    inOut.prodosBlock  = physicalTrack * kBlocksPerTrack + po / kHalvesPerBlock;
    inOut.prodosHalf   = po % kHalvesPerBlock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::Classify
//
//  The first class that applies: a standard kind when at least one of its
//  address fields passes its checksum or is not checked; unformatted when
//  more than half the cells are noise or random bits; otherwise nonstandard.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::Classify (TrackAnalysis & inOut)
{
    const FramedTrack &  track      = inOut.framed;
    uint64_t             noiseCells = 0;
    size_t               i          = 0;
    double               share      = 0;



    for (const AnalyzedSector & s : inOut.sectors)
    {
        bool  isCounted = s.isAddressGood || !s.isAddressCheck;

        inOut.has16 = inOut.has16 || (isCounted && s.kind == DiskFieldKind::Sixteen);
        inOut.has13 = inOut.has13 || (isCounted && s.kind == DiskFieldKind::Thirteen);
    }

    for (const RandomRegion & r : track.randomRegions)
    {
        noiseCells += r.cellCount;
    }

    for (i = 0; i < track.nibbles.size(); i++)
    {
        if (inOut.nibbleKinds[i] == NibbleKind::Noise)
        {
            noiseCells += LatchFramer::kNibbleCells + track.nibbles[i].extraZeroCells;
        }
    }

    share = static_cast<double> (noiseCells) / static_cast<double> (track.cellCount);

    if (inOut.has16 && inOut.has13)
    {
        inOut.trackClass = TrackClass::ThirteenAndSixteen;
    }
    else if (inOut.has16)
    {
        inOut.trackClass = TrackClass::Sixteen;
    }
    else if (inOut.has13)
    {
        inOut.trackClass = TrackClass::Thirteen;
    }
    else if (share > kUnformattedShare)
    {
        inOut.trackClass = TrackClass::Unformatted;
    }
    else
    {
        inOut.trackClass = TrackClass::Nonstandard;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::CountSectors
//
//  Only a track of a standard class counts sectors; the candidate fields on a
//  nonstandard track count toward neither found nor good.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::CountSectors (TrackAnalysis & inOut)
{
    bool  isStandard = inOut.has16 || inOut.has13;



    for (const AnalyzedSector & s : inOut.sectors)
    {
        if (!isStandard)
        {
            continue;
        }

        inOut.sectorsFound++;
        inOut.sectorsGood       += (s.state == SectorState::Good)       ? 1 : 0;
        inOut.sectorsNotChecked += (s.state == SectorState::NotChecked) ? 1 : 0;
        inOut.sectorsBad        += (s.state == SectorState::BadAddress || s.state == SectorState::BadData) ? 1 : 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::Measure
//
//  Length, nibble count, every sync run and the longest (the likely write
//  seam), the angles of sector 0 and the first field, and on a flux track the
//  time of one turn, the RPM it means and the mean cell against nominal.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::Measure (TrackAnalysis & inOut)
{
    static constexpr double  kSecondsPerTick   = 125e-9;
    static constexpr double  kSecondsPerMinute = 60.0;



    TrackMeasurements &  m     = inOut.measurements;
    const FramedTrack &  track = inOut.framed;
    size_t               n     = track.nibbles.size();
    size_t               start = 0;
    size_t               k     = 0;
    size_t               idx   = 0;
    SyncRun              run;



    m.lengthCells = track.cellCount;
    m.nibbleCount = static_cast<int> (n);

    //  Start on a nibble that is not sync, so a run across the index is whole.
    while (start < n && inOut.nibbleKinds[start] == NibbleKind::Sync)
    {
        start++;
    }

    for (k = 0; k < n && start < n; k++)
    {
        idx = (start + k) % n;

        if (inOut.nibbleKinds[idx] != NibbleKind::Sync)
        {
            continue;
        }

        if (run.count > 0 && static_cast<size_t> (run.firstNibble + run.count) % n == idx)
        {
            m.syncRuns.back().count++;
            run = m.syncRuns.back();
            continue;
        }

        run             = SyncRun();
        run.firstNibble = static_cast<int> (idx);
        run.count       = 1;
        run.startCell   = track.nibbles[idx].startCell;
        run.widthCells  = LatchFramer::kNibbleCells + track.nibbles[idx].extraZeroCells;
        m.syncRuns.push_back (run);
    }

    for (const SyncRun & r : m.syncRuns)
    {
        if (r.count > m.longestSync.count)
        {
            m.longestSync = r;
        }
    }

    for (const AnalyzedSector & s : inOut.sectors)
    {
        if (s.sector == 0 && m.sector0Angle < 0)
        {
            m.sector0Angle = GetAngle (inOut, inOut.fields[s.addressField].startCell);
        }
    }

    if (!inOut.fields.empty())
    {
        m.firstFieldAngle = GetAngle (inOut, inOut.fields.front().startCell);
    }

    if (track.isFlux && track.turnTicks > 0)
    {
        m.turnTicks     = track.turnTicks;
        m.rpm           = kSecondsPerMinute / (track.turnTicks * kSecondsPerTick);
        m.meanCellTicks = track.turnTicks / static_cast<double> (track.cellCount);
        m.deviation     = m.meanCellTicks / kNominalCellTicks - 1.0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::MeasureGaps
//
//  For each field, the nibbles between it and the field before it, and the
//  sync run that ends just before it.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::MeasureGaps (TrackAnalysis & inOut)
{
    size_t    n     = inOut.framed.nibbles.size();
    size_t    count = inOut.fields.size();
    size_t    f     = 0;
    int       end   = 0;
    size_t    idx   = 0;
    FieldGap  gap;



    inOut.fieldGaps.assign (count, FieldGap());

    for (f = 0; f < count && n > 0; f++)
    {
        const LocatedField &  previous = inOut.fields[(f + count - 1) % count];

        gap               = FieldGap();
        end               = previous.firstNibble + previous.nibbleCount;
        gap.nibblesBefore = static_cast<int> ((static_cast<size_t> (inOut.fields[f].firstNibble) + n - static_cast<size_t> (end) % n) % n);
        idx               = (static_cast<size_t> (inOut.fields[f].firstNibble) + n - 1) % n;

        while (inOut.nibbleKinds[idx] == NibbleKind::Sync && static_cast<size_t> (gap.syncBefore.count) < n)
        {
            gap.syncBefore.firstNibble = static_cast<int> (idx);
            gap.syncBefore.startCell   = inOut.framed.nibbles[idx].startCell;
            gap.syncBefore.widthCells  = LatchFramer::kNibbleCells + inOut.framed.nibbles[idx].extraZeroCells;
            gap.syncBefore.count++;
            idx = (idx + n - 1) % n;
        }

        inOut.fieldGaps[f] = gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::FindFieldIssues
//
//  Each field's checksum, its epilogue, an address field's track number
//  against the physical track, a field with no partner, and data nibbles
//  outside the translate table.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::FindFieldIssues (TrackAnalysis & inOut, const DecodeChecks & checks)
{
    int   f         = 0;
    bool  isCounted = false;



    for (f = 0; f < static_cast<int> (inOut.fields.size()); f++)
    {
        const LocatedField &  field = inOut.fields[f];

        if (field.role == FieldRole::Address)
        {
            isCounted = field.isAddressChecksumGood || !checks.isAddressChecksumOn;

            if (checks.isAddressChecksumOn && !field.isAddressChecksumGood)
            {
                AddFinding (inOut, FindingCategory::Field, FindingKind::AddressChecksumFailed, f, field.checksumStored, field.checksumComputed);
            }

            if (isCounted && field.track != inOut.context.physicalTrack && field.track != inOut.context.alsoTrack)
            {
                AddFinding (inOut, FindingCategory::Field, FindingKind::AddressTrackDiffers, f, field.track, inOut.context.physicalTrack);
            }

            if (checks.isEpilogueOn && !field.isEpilogueStandard)
            {
                AddFinding (inOut, FindingCategory::Field, FindingKind::AddressEpilogueNonstandard, f, 0, 0);
                inOut.findings.back().bytes.assign (field.epilogueFound.begin(), field.epilogueFound.end());
            }

            if (field.pairedField < 0)
            {
                AddFinding (inOut, FindingCategory::Field, FindingKind::AddressWithoutData, f, 0, 0);
            }

            continue;
        }

        if (checks.isDataChecksumOn && !field.data.isChecksumGood)
        {
            AddFinding (inOut, FindingCategory::Field, FindingKind::DataChecksumFailed, f, field.data.storedChecksum, field.data.computedChecksum);
        }

        if (checks.isEpilogueOn && !field.isEpilogueStandard)
        {
            AddFinding (inOut, FindingCategory::Field, FindingKind::DataEpilogueNonstandard, f, 0, 0);
            inOut.findings.back().bytes.assign (field.epilogueFound.begin(), field.epilogueFound.end());
        }

        if (field.pairedField < 0)
        {
            AddFinding (inOut, FindingCategory::Field, FindingKind::DataWithoutAddress, f, 0, 0);
        }

        if (!field.data.invalidNibbles.empty())
        {
            AddFinding (inOut, FindingCategory::Field, FindingKind::NibblesOutsideTable, f, static_cast<int> (field.kind), 0);
            inOut.findings.back().nibbles = field.data.invalidNibbles;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::FindSectorIssues
//
//  Over the sectors whose address field passes or is not checked: a sector
//  number repeated, one outside the format's range, and on a standard track
//  the numbers missing from it. Random-bit regions on a formatted track are
//  listed here too.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::FindSectorIssues (TrackAnalysis & inOut)
{
    static constexpr int  kSectors16 = 16;
    static constexpr int  kSectors13 = 13;
    static constexpr int  kNumbers   = 256;



    std::array<int, kNumbers>  seen16  = {};
    std::array<int, kNumbers>  seen13  = {};
    int                        s       = 0;
    vector<int>                missing;
    uint64_t                   cells   = 0;



    for (const AnalyzedSector & sector : inOut.sectors)
    {
        if (sector.isAddressCheck && !sector.isAddressGood)
        {
            continue;
        }

        (sector.kind == DiskFieldKind::Sixteen ? seen16 : seen13)[sector.sector]++;
    }

    for (s = 0; s < kNumbers; s++)
    {
        for (int count : { seen16[s], seen13[s] })
        {
            if (count > 1)
            {
                AddFinding (inOut, FindingCategory::Track, FindingKind::SectorRepeated, -1, s, count);
            }
        }

        if ((seen16[s] > 0 && s >= kSectors16) || (seen13[s] > 0 && s >= kSectors13))
        {
            AddFinding (inOut, FindingCategory::Track, FindingKind::SectorNumberOutOfRange, -1, s, 0);
        }
    }

    for (s = 0; s < kSectors16 && inOut.has16; s++)
    {
        if (seen16[s] == 0)
        {
            missing.push_back (s);
        }
    }

    for (s = 0; s < kSectors13 && inOut.has13 && !inOut.has16; s++)
    {
        if (seen13[s] == 0)
        {
            missing.push_back (s);
        }
    }

    if (!missing.empty())
    {
        AddFinding (inOut, FindingCategory::Track, FindingKind::SectorsMissing, -1, static_cast<int> (missing.size()), 0);
        inOut.findings.back().sectors = missing;
    }

    for (const RandomRegion & r : inOut.framed.randomRegions)
    {
        cells += r.cellCount;
    }

    if ((inOut.has16 || inOut.has13) && !inOut.framed.randomRegions.empty())
    {
        AddFinding (inOut, FindingCategory::Track, FindingKind::RandomBitsOnFormattedTrack, -1,
                    static_cast<int> (inOut.framed.randomRegions.size()), static_cast<int> (cells));
        inOut.findings.back().cell    = inOut.framed.randomRegions.front().startCell;
        inOut.findings.back().hasCell = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::FindStrayMarks
//
//  A D5 AA pair outside every recognized field, with the nibble after it:
//  a mark the decode settings do not match, or the start of something else.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::FindStrayMarks (TrackAnalysis & inOut)
{
    static constexpr Byte  kMark0 = 0xD5;
    static constexpr Byte  kMark1 = 0xAA;



    const vector<FramedNibble> &  nibbles = inOut.framed.nibbles;
    size_t                        n       = nibbles.size();
    size_t                        i       = 0;



    for (i = 0; i < n; i++)
    {
        bool  isPair = nibbles[i].value == kMark0
                    && nibbles[(i + 1) % n].value == kMark1
                    && inOut.fieldOfNibble[i] < 0
                    && inOut.fieldOfNibble[(i + 1) % n] < 0;

        if (!isPair)
        {
            continue;
        }

        AddFinding (inOut, FindingCategory::Track, FindingKind::StrayFieldMark, -1, nibbles[(i + 2) % n].value, 0);
        inOut.findings.back().cell    = nibbles[i].startCell;
        inOut.findings.back().hasCell = true;
        inOut.findings.back().bytes   = { kMark0, kMark1, nibbles[(i + 2) % n].value };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzer::AddFinding
//
//  A finding on this record. A field's finding takes the field's start cell
//  and, when it has one, its sector.
//
////////////////////////////////////////////////////////////////////////////////

void TrackAnalyzer::AddFinding (TrackAnalysis & inOut, FindingCategory category, FindingKind kind, int field, int value, int value2)
{
    Finding  finding;



    finding.category = category;
    finding.kind     = kind;
    finding.slot     = inOut.slot;
    finding.field    = field;
    finding.value    = value;
    finding.value2   = value2;

    if (field >= 0)
    {
        const LocatedField &  located = inOut.fields[field];
        int                   address = (located.role == FieldRole::Address) ? field : located.pairedField;

        finding.cell    = located.startCell;
        finding.hasCell = true;
        finding.sector  = (address >= 0) ? inOut.fields[address].sector : -1;
    }

    inOut.findings.push_back (finding);
}
