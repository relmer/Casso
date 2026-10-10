#include "Pch.h"

#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"





static constexpr int  s_kVoteLength   = 8;
static constexpr int  s_kPerTrack     = DiskImage::kQuarterTracksPerWholeTrack;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparison::GetListed
//
////////////////////////////////////////////////////////////////////////////////

vector<Difference> DiskComparison::GetListed (const ComparisonOptions & options) const
{
    vector<Difference>  listed;



    for (const Difference & d : differences)
    {
        bool  isHidden = (options.isIgnoringSync && d.isSyncOnly) || (options.isIgnoringVolumes && d.kind == DifferenceKind::Volume) ||
                         (options.isIgnoringDates && d.kind == DifferenceKind::FileDates);

        if (!isHidden)
        {
            listed.push_back (d);
        }
    }

    return listed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparison::Count
//
////////////////////////////////////////////////////////////////////////////////

void DiskComparison::Count (int & outIdentical, int & outDiffer, int & outOnlyInA, int & outOnlyInB, int & outNotCompared) const
{
    outIdentical   = 0;
    outDiffer      = 0;
    outOnlyInA     = 0;
    outOnlyInB     = 0;
    outNotCompared = 0;

    for (const TrackComparison & t : tracks)
    {
        switch (t.verdict)
        {
            case TrackVerdict::NothingRecorded:
            case TrackVerdict::StandardLayout:                       break;
            case TrackVerdict::Identical:       outIdentical++;      break;
            case TrackVerdict::OnlyInA:         outOnlyInA++;        break;
            case TrackVerdict::OnlyInB:         outOnlyInB++;        break;
            case TrackVerdict::NotCompared:     outNotCompared++;    break;
            default:                            outDiffer++;         break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::Compare
//
////////////////////////////////////////////////////////////////////////////////

DiskComparison DiskComparer::Compare (const DiskAnalysis & a, const DiskAnalysis & b)
{
    DiskComparison  out;
    int             qt  = 0;



    out.tracks.resize (DiskImage::kQuarterTrackCount);

    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        TrackComparison &          t       = out.tracks[qt];
        const QuarterTrackEntry &  entryA  = a.entries[qt];
        const QuarterTrackEntry &  entryB  = b.entries[qt];
        const TrackAnalysis *      trackA  = (entryA.slot >= 0 && entryA.slot < static_cast<int> (a.tracks.size())) ? a.tracks[entryA.slot].get() : nullptr;
        const TrackAnalysis *      trackB  = (entryB.slot >= 0 && entryB.slot < static_cast<int> (b.tracks.size())) ? b.tracks[entryB.slot].get() : nullptr;
        bool                       isA     = entryA.content != QuarterTrackContent::Nothing;
        bool                       isB     = entryB.content != QuarterTrackContent::Nothing;

        t.quarterTrack = qt;
        t.slotA        = entryA.slot;
        t.slotB        = entryB.slot;

        if (!isA && !isB)
        {
            t.verdict = TrackVerdict::NothingRecorded;
        }
        else if (qt % s_kPerTrack != 0 && IsStandardLayout (a, qt) && IsStandardLayout (b, qt))
        {
            t.verdict = TrackVerdict::StandardLayout;
        }
        else if (entryA.content == QuarterTrackContent::Damaged || entryB.content == QuarterTrackContent::Damaged)
        {
            t.verdict = TrackVerdict::NotCompared;
            t.reason  = std::format (L"{} record is damaged", entryA.content == QuarterTrackContent::Damaged ? L"A's" : L"B's");
        }
        else if (isA != isB)
        {
            t.verdict = isA ? TrackVerdict::OnlyInA : TrackVerdict::OnlyInB;
            out.differences.push_back ({ isA ? DifferenceKind::TrackOnlyInA : DifferenceKind::TrackOnlyInB, qt });
        }
        else if (trackA == nullptr || trackB == nullptr)
        {
            t.verdict = TrackVerdict::NotCompared;
            t.reason  = L"Still being analyzed";
        }
        else
        {
            CompareTrack (*trackA, *trackB, qt, t, out.differences);
        }
    }

    CompareFiles (a, b, out);

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::IsStandardLayout
//
//  A quarter track that holds nothing, or the record its disk holds at the
//  whole track below or above (FR-118).
//
////////////////////////////////////////////////////////////////////////////////

bool DiskComparer::IsStandardLayout (const DiskAnalysis & disk, int quarterTrack)
{
    int  slot  = disk.entries[quarterTrack].slot;
    int  below = quarterTrack / s_kPerTrack * s_kPerTrack;
    int  above = below + s_kPerTrack;



    return disk.entries[quarterTrack].content == QuarterTrackContent::Nothing || slot == disk.entries[below].slot ||
           (above < DiskImage::kQuarterTrackCount && slot == disk.entries[above].slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::CompareTrack
//
//  The strongest verdict that holds, and the differences at that level: none
//  for the same cells but the timing runs that differ, the sync and zero
//  cell hunks for the same nibbles, the sectors for sectors that differ, and
//  every hunk where neither side has standard sectors. A volume number that
//  differs is listed whatever the verdict.
//
////////////////////////////////////////////////////////////////////////////////

void DiskComparer::CompareTrack (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, TrackComparison & inOut, vector<Difference> & inOutDiffs)
{
    int                 rotation     = 0;
    bool                isTimingSame = true;
    bool                isFluxA      = a.framed.isFlux;
    bool                isFluxB      = b.framed.isFlux;
    vector<Difference>  sectorDiffs;
    int                 differing    = CompareSectors (a, b, quarterTrack, sectorDiffs);



    inOut.isFluxOnOneSide = isFluxA != isFluxB;
    inOut.cellCount       = a.framed.cellCount;

    if (IsSameCells (a, b, rotation, isTimingSame))
    {
        inOut.rotationCells  = rotation;
        inOut.isTimingDiffer = !isTimingSame;
        inOut.verdict        = (rotation == 0 && isTimingSame && !inOut.isFluxOnOneSide) ? TrackVerdict::Identical : TrackVerdict::SameCells;

        if (!isTimingSame)
        {
            ListTiming (a, b, quarterTrack, rotation, inOutDiffs);
        }
    }
    else if (IsSameNibbles (a, b))
    {
        inOut.verdict         = TrackVerdict::SameNibbles;
        inOut.rotationNibbles = Align (a, b);
        inOut.lengthChange    = static_cast<int> (b.framed.cellCount) - static_cast<int> (a.framed.cellCount);
        ListNibbles (a, b, quarterTrack, inOutDiffs);
    }
    else if (HasSectors (a) || HasSectors (b))
    {
        inOut.sectorsDiffer = differing;
        inOut.verdict       = (differing == 0) ? TrackVerdict::SameSectorData : TrackVerdict::SectorsDiffer;

        for (const Difference & d : sectorDiffs)
        {
            if (d.kind != DifferenceKind::Volume)
            {
                inOutDiffs.push_back (d);
            }
        }
    }
    else
    {
        inOut.verdict = TrackVerdict::NibblesDiffer;
        ListNibbles (a, b, quarterTrack, inOutDiffs);
    }

    for (const Difference & d : sectorDiffs)
    {
        if (d.kind == DifferenceKind::Volume)
        {
            inOutDiffs.push_back (d);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::IsSameCells
//
//  The same loop of cells at some rotation, found as A's cells inside B's
//  taken twice; on two flux tracks, each cell's time within the tolerance.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskComparer::IsSameCells (const TrackAnalysis & a, const TrackAnalysis & b, int & outRotation, bool & outIsTimingSame)
{
    const FramedTrack &  fa      = a.framed;
    const FramedTrack &  fb      = b.framed;
    uint32_t             n       = fa.cellCount;
    bool                 isSame  = n > 0 && n == fb.cellCount && fa.cells.size() >= n && fb.cells.size() >= n;
    vector<Byte>         doubled;



    outRotation     = 0;
    outIsTimingSame = true;

    if (isSame)
    {
        doubled.assign (fb.cells.begin(), fb.cells.begin() + n);
        doubled.insert (doubled.end(), fb.cells.begin(), fb.cells.begin() + n);

        auto  found = std::search (doubled.begin(), doubled.end(), std::boyer_moore_horspool_searcher (fa.cells.begin(), fa.cells.begin() + n));

        isSame      = found != doubled.end();
        outRotation = isSame ? static_cast<int> (found - doubled.begin()) : 0;
    }

    for (uint32_t c = 0; isSame && fa.isFlux && fb.isFlux && outIsTimingSame && c < n; c++)
    {
        double  ta = fa.cellTicks[c];
        double  tb = fb.cellTicks[(c + outRotation) % n];

        outIsTimingSame = std::abs (ta - tb) <= kTimingTolerance * std::max (ta, tb);
    }

    return isSame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::IsSameNibbles
//
//  The same nibbles in the same order once aligned, sync nibbles aside, so
//  only sync widths and counts, extra zero cells, rotation or length differ.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskComparer::IsSameNibbles (const TrackAnalysis & a, const TrackAnalysis & b)
{
    size_t        nB       = b.framed.nibbles.size();
    int           rotation = Align (a, b);
    vector<Byte>  na;
    vector<Byte>  nb;



    for (size_t i = 0; i < a.framed.nibbles.size(); i++)
    {
        if (a.nibbleKinds[i] != NibbleKind::Sync)
        {
            na.push_back (a.framed.nibbles[i].value);
        }
    }

    for (size_t i = 0; i < nB; i++)
    {
        size_t  at = (i + static_cast<size_t> (rotation)) % nB;

        if (b.nibbleKinds[at] != NibbleKind::Sync)
        {
            nb.push_back (b.framed.nibbles[at].value);
        }
    }

    return !na.empty() && na == nb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::CompareSectors
//
//  Sectors paired by number and encoding, repeats in passing order (FR-120):
//  each pair's bytes and checksum results, and each unpaired sector. A
//  volume number that differs is its own difference, one for the track.
//
////////////////////////////////////////////////////////////////////////////////

int DiskComparer::CompareSectors (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, vector<Difference> & inOutDiffs)
{
    vector<bool>  isPaired  (b.sectors.size(), false);
    int           differing = 0;
    int           volumeA   = -1;
    int           volumeB   = -1;



    for (const AnalyzedSector & sa : a.sectors)
    {
        int  match = -1;

        for (size_t j = 0; match < 0 && j < b.sectors.size(); j++)
        {
            match = (!isPaired[j] && b.sectors[j].sector == sa.sector && b.sectors[j].kind == sa.kind) ? static_cast<int> (j) : -1;
        }

        if (match < 0)
        {
            inOutDiffs.push_back ({ DifferenceKind::SectorOnlyInA, quarterTrack, sa.sector });
            inOutDiffs.back().cell = GetSectorCell (a, sa);
            differing++;
            continue;
        }

        isPaired[match] = true;

        const AnalyzedSector  & sb     = b.sectors[match];
        const Byte            * bytesA = (sa.dataField >= 0) ? a.fields[sa.dataField].data.bytes.data() : nullptr;
        const Byte            * bytesB = (sb.dataField >= 0) ? b.fields[sb.dataField].data.bytes.data() : nullptr;
        int                     count  = 0;
        int                     first  = -1;

        for (int k = 0; bytesA != nullptr && bytesB != nullptr && k < DiskFieldFormat::kSectorBytes; k++)
        {
            first  = (bytesA[k] != bytesB[k] && first < 0) ? k : first;
            count += (bytesA[k] != bytesB[k]) ? 1 : 0;
        }

        if (count > 0 || (bytesA == nullptr) != (bytesB == nullptr))
        {
            Difference  d { DifferenceKind::SectorBytes, quarterTrack, sa.sector };

            d.count       = (bytesA == nullptr || bytesB == nullptr) ? DiskFieldFormat::kSectorBytes : count;
            d.firstOffset = (first < 0) ? 0 : first;
            d.cell        = GetSectorCell (a, sa);
            inOutDiffs.push_back (d);
            differing++;
        }
        else if (sa.state != sb.state)
        {
            inOutDiffs.push_back ({ DifferenceKind::SectorChecksum, quarterTrack, sa.sector });
            inOutDiffs.back().cell = GetSectorCell (a, sa);
            differing++;
        }

        if (sa.addressField >= 0 && sb.addressField >= 0)
        {
            volumeA = (volumeA < 0) ? a.fields[sa.addressField].volume : volumeA;
            volumeB = (volumeB < 0) ? b.fields[sb.addressField].volume : volumeB;
        }
    }

    for (size_t j = 0; j < b.sectors.size(); j++)
    {
        if (!isPaired[j])
        {
            inOutDiffs.push_back ({ DifferenceKind::SectorOnlyInB, quarterTrack, b.sectors[j].sector });
            inOutDiffs.back().cell = GetSectorCell (b, b.sectors[j]);
            differing++;
        }
    }

    if (volumeA >= 0 && volumeB >= 0 && volumeA != volumeB)
    {
        Difference  d { DifferenceKind::Volume, quarterTrack };

        d.detail = std::format (L"Volume {} in A, {} in B", volumeA, volumeB);
        inOutDiffs.push_back (d);
    }

    return differing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::FindPairedSector
//
//  The nth sector of A with a number and encoding pairs with the nth of B.
//
////////////////////////////////////////////////////////////////////////////////

int DiskComparer::FindPairedSector (const TrackAnalysis & a, int sectorIndex, const TrackAnalysis & b)
{
    int     paired = -1;
    int     nth    = 0;
    size_t  i      = 0;



    if (sectorIndex >= 0 && sectorIndex < static_cast<int> (a.sectors.size()))
    {
        const AnalyzedSector &  sa = a.sectors[sectorIndex];

        for (i = 0; i < static_cast<size_t> (sectorIndex); i++)
        {
            nth += (a.sectors[i].sector == sa.sector && a.sectors[i].kind == sa.kind) ? 1 : 0;
        }

        for (i = 0; paired < 0 && i < b.sectors.size(); i++)
        {
            if (b.sectors[i].sector == sa.sector && b.sectors[i].kind == sa.kind && nth-- == 0)
            {
                paired = static_cast<int> (i);
            }
        }
    }

    return paired;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::GetSectorCell
//
//  Where a sector starts: its address field's first cell, or its data
//  field's for a sector found by its data alone.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskComparer::GetSectorCell (const TrackAnalysis & track, const AnalyzedSector & sector)
{
    int  field  = (sector.addressField >= 0) ? sector.addressField : sector.dataField;
    int  nibble = (field >= 0) ? track.fields[field].firstNibble : -1;



    return (nibble >= 0 && nibble < static_cast<int> (track.framed.nibbles.size())) ? track.framed.nibbles[nibble].startCell : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::ListNibbles
//
//  Each hunk of the aligned nibbles that differs, marked when every nibble
//  in it, on both sides, is sync. More edits than the limit leave the rest
//  of the track as one hunk.
//
////////////////////////////////////////////////////////////////////////////////

void DiskComparer::ListNibbles (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, vector<Difference> & inOutDiffs)
{
    int           rotation = Align (a, b);
    vector<Byte>  na       = GetNibbles (a, 0);
    vector<Byte>  nb       = GetNibbles (b, rotation);
    vector<Hunk>  hunks;
    size_t        nB       = b.framed.nibbles.size();



    if (!Diff (na, nb, hunks))
    {
        hunks = { { 0, static_cast<int> (na.size()), 0, static_cast<int> (nb.size()) } };
    }

    for (const Hunk & h : hunks)
    {
        Difference  d { DifferenceKind::Nibbles, quarterTrack };
        bool        isSync = true;

        for (int i = h.firstA; isSync && i < h.firstA + h.countA; i++)
        {
            isSync = a.nibbleKinds[i] == NibbleKind::Sync;
        }

        for (int i = h.firstB; isSync && i < h.firstB + h.countB; i++)
        {
            isSync = b.nibbleKinds[(static_cast<size_t> (i) + rotation) % std::max<size_t> (nB, 1)] == NibbleKind::Sync;
        }

        d.firstNibbleA = h.firstA;
        d.nibbleCountA = h.countA;
        d.firstNibbleB = static_cast<int> ((static_cast<size_t> (h.firstB) + rotation) % std::max<size_t> (nB, 1));
        d.nibbleCountB = h.countB;
        d.cell         = (h.firstA < static_cast<int> (a.framed.nibbles.size())) ? a.framed.nibbles[h.firstA].startCell : 0;
        d.isSyncOnly   = isSync;
        inOutDiffs.push_back (d);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::ListTiming
//
//  Each run of cells whose times differ by more than the tolerance.
//
////////////////////////////////////////////////////////////////////////////////

void DiskComparer::ListTiming (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, int rotation, vector<Difference> & inOutDiffs)
{
    uint32_t  n     = a.framed.cellCount;
    int       start = -1;



    for (uint32_t c = 0; c <= n; c++)
    {
        bool  isDiffer = false;

        if (c < n)
        {
            double  ta = a.framed.cellTicks[c];
            double  tb = b.framed.cellTicks[(c + rotation) % n];

            isDiffer = std::abs (ta - tb) > kTimingTolerance * std::max (ta, tb);
        }

        if (isDiffer && start < 0)
        {
            start = static_cast<int> (c);
        }
        else if (!isDiffer && start >= 0)
        {
            Difference  d { DifferenceKind::Timing, quarterTrack };

            d.cell  = static_cast<uint32_t> (start);
            d.count = static_cast<int> (c) - start;
            inOutDiffs.push_back (d);
            start = -1;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::Align
//
//  On the first address field in A's passing order whose sector B also
//  holds; with none, the offset most shared 8-nibble runs propose.
//
////////////////////////////////////////////////////////////////////////////////

int DiskComparer::Align (const TrackAnalysis & a, const TrackAnalysis & b)
{
    size_t                        nA       = a.framed.nibbles.size();
    size_t                        nB       = b.framed.nibbles.size();
    int                           rotation = 0;
    bool                          isFound  = false;
    std::unordered_map<uint64_t, vector<int>>  grams;
    std::unordered_map<int, int>  votes;
    int                           best     = 0;



    for (const AnalyzedSector & sa : a.sectors)
    {
        for (const AnalyzedSector & sb : b.sectors)
        {
            if (!isFound && sa.sector == sb.sector && sa.kind == sb.kind && sa.addressField >= 0 && sb.addressField >= 0)
            {
                int  ia = a.fields[sa.addressField].firstNibble;
                int  ib = b.fields[sb.addressField].firstNibble;

                rotation = static_cast<int> ((static_cast<size_t> (ib) + nB - static_cast<size_t> (ia) % std::max<size_t> (nB, 1)) % std::max<size_t> (nB, 1));
                isFound  = true;
            }
        }
    }

    auto  gram = [] (const TrackAnalysis & t, size_t at, size_t n)
    {
        uint64_t  key = 0;

        for (int k = 0; k < s_kVoteLength; k++)
        {
            key = (key << 8) | t.framed.nibbles[(at + k) % n].value;
        }

        return key;
    };

    for (size_t i = 0; !isFound && nA >= s_kVoteLength && i < nA; i++)
    {
        grams[gram (a, i, nA)].push_back (static_cast<int> (i));
    }

    for (size_t j = 0; !isFound && nB >= s_kVoteLength && j < nB; j++)
    {
        auto  found = grams.find (gram (b, j, nB));

        for (int i : (found != grams.end()) ? found->second : vector<int>())
        {
            int  offset = static_cast<int> ((j + nB - static_cast<size_t> (i) % nB) % nB);

            if (++votes[offset] > best)
            {
                best     = votes[offset];
                rotation = offset;
            }
        }
    }

    return rotation;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::Diff
//
//  Myers' greedy diff, giving up past kMaxEdits, then the edit script read
//  back as hunks.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskComparer::Diff (std::span<const Byte> a, std::span<const Byte> b, vector<Hunk> & outHunks)
{
    int                  n      = static_cast<int> (a.size());
    int                  m      = static_cast<int> (b.size());
    int                  offset = kMaxEdits + 1;
    vector<vector<int>>  trace;
    vector<int>          v      (2 * offset + 1, 0);
    bool                 isDone = false;
    int                  d      = 0;



    outHunks.clear();

    for (d = 0; !isDone && d <= kMaxEdits; d++)
    {
        trace.push_back (v);

        for (int k = -d; !isDone && k <= d; k += 2)
        {
            int  x = (k == -d || (k != d && v[offset + k - 1] < v[offset + k + 1])) ? v[offset + k + 1] : v[offset + k - 1] + 1;
            int  y = x - k;

            while (x < n && y < m && a[x] == b[y])
            {
                x++;
                y++;
            }

            v[offset + k] = x;
            isDone        = x >= n && y >= m;
        }
    }

    if (isDone)
    {
        //  Walk back through the trace, collecting the edits as hunks.
        int  x = n;
        int  y = m;

        for (int step = static_cast<int> (trace.size()) - 1; step > 0; step--)
        {
            const vector<int> &  pv      = trace[step];
            int                  k       = x - y;
            int                  prevK   = (k == -step || (k != step && pv[offset + k - 1] < pv[offset + k + 1])) ? k + 1 : k - 1;
            int                  prevX   = pv[offset + prevK];
            int                  prevY   = prevX - prevK;

            while (x > prevX && y > prevY)
            {
                x--;
                y--;
            }

            Hunk  h { prevX, x - prevX, prevY, y - prevY };

            if (!outHunks.empty() && outHunks.back().firstA == x && outHunks.back().firstB == y)
            {
                outHunks.back().firstA  = h.firstA;
                outHunks.back().firstB  = h.firstB;
                outHunks.back().countA += h.countA;
                outHunks.back().countB += h.countB;
            }
            else
            {
                outHunks.push_back (h);
            }

            x = prevX;
            y = prevY;
        }

        std::reverse (outHunks.begin(), outHunks.end());
    }

    return isDone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::GetNibbles
//
////////////////////////////////////////////////////////////////////////////////

vector<Byte> DiskComparer::GetNibbles (const TrackAnalysis & track, int rotation)
{
    size_t        n   = track.framed.nibbles.size();
    vector<Byte>  out (n, 0);



    for (size_t i = 0; i < n; i++)
    {
        out[i] = track.framed.nibbles[(i + static_cast<size_t> (rotation)) % n].value;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::CompareFiles
//
//  A's shown volume against B's volume of the same file system; files paired
//  by path, repeats in catalog order (FR-120).
//
////////////////////////////////////////////////////////////////////////////////

void DiskComparer::CompareFiles (const DiskAnalysis & a, const DiskAnalysis & b, DiskComparison & inOut)
{
    const FileMap *  mapA = nullptr;
    const FileMap *  mapB = nullptr;



    for (const FileMap & map : a.fileMaps)
    {
        mapA = (mapA == nullptr && map.notMapped == NotMappedReason::None) ? &map : mapA;
    }

    for (const FileMap & map : b.fileMaps)
    {
        mapB = (mapB == nullptr && mapA != nullptr && map.fileSystem == mapA->fileSystem) ? &map : mapB;
    }

    if (mapA == nullptr || mapB == nullptr)
    {
        inOut.fileNote = (mapA == nullptr) ? L"A holds no volume the file map reads, so its files are not compared"
                                           : L"B holds no volume of A's file system, so their files are not compared";
    }
    else
    {
        SectorSource  sourceA (a);
        SectorSource  sourceB (b);
        vector<bool>  isUsed  (mapB->files.size(), false);

        for (size_t i = 0; i < mapA->files.size(); i++)
        {
            const MappedFile &  fa    = mapA->files[i];
            int                 match = -1;
            FilePair            pair;

            if (fa.isDeleted)
            {
                continue;
            }

            for (size_t j = 0; match < 0 && j < mapB->files.size(); j++)
            {
                match = (!isUsed[j] && !mapB->files[j].isDeleted && mapB->files[j].path == fa.path) ? static_cast<int> (j) : -1;
            }

            pair.path  = fa.path;
            pair.fileA = static_cast<int> (i);
            pair.fileB = match;

            if (match < 0)
            {
                pair.outcome = DifferenceKind::FileOnlyInA;
                inOut.differences.push_back ({ DifferenceKind::FileOnlyInA, -1, -1, -1, 0, -1, 0, 0, 0, -1, false, fa.path });
            }
            else
            {
                const MappedFile &  fb      = mapB->files[match];
                vector<Byte>        bytesA;
                vector<Byte>        bytesB;
                bool                isReadA = ReadContents (*mapA, sourceA, fa, bytesA);
                bool                isReadB = ReadContents (*mapB, sourceB, fb, bytesB);

                isUsed[match] = true;
                pair.isSame   = true;

                if (!isReadA || !isReadB)
                {
                    pair.isSame  = false;
                    pair.outcome = DifferenceKind::FileNotCompared;
                    pair.reason  = std::format (L"{} holds a bad or missing sector, or its chain is broken", isReadA ? L"B's copy" : L"A's copy");
                    inOut.differences.push_back ({ DifferenceKind::FileNotCompared, -1, -1, -1, 0, -1, 0, 0, 0, -1, false, fa.path, pair.reason });
                }
                else
                {
                    size_t  common = std::min (bytesA.size(), bytesB.size());
                    int     count  = 0;
                    int     first  = -1;

                    for (size_t k = 0; k < common; k++)
                    {
                        first  = (bytesA[k] != bytesB[k] && first < 0) ? static_cast<int> (k) : first;
                        count += (bytesA[k] != bytesB[k]) ? 1 : 0;
                    }

                    if (count > 0)
                    {
                        pair.isSame  = false;
                        pair.outcome = DifferenceKind::FileContents;
                        inOut.differences.push_back ({ DifferenceKind::FileContents, -1, -1, -1, 0, -1, 0, 0, count, first, false, fa.path });
                    }

                    if (bytesA.size() != bytesB.size())
                    {
                        pair.isSame = false;
                        inOut.differences.push_back ({ DifferenceKind::FileLength, -1, -1, -1, 0, -1, 0, 0, static_cast<int> (bytesB.size()) - static_cast<int> (bytesA.size()),
                                                       -1, false, fa.path });
                    }

                    if (fa.type != fb.type || fa.isLocked != fb.isLocked)
                    {
                        pair.isSame = false;
                        inOut.differences.push_back ({ DifferenceKind::FileAttributes, -1, -1, -1, 0, -1, 0, 0, 0, -1, false, fa.path });
                    }

                    if (fa.created != fb.created || fa.modified != fb.modified)
                    {
                        inOut.differences.push_back ({ DifferenceKind::FileDates, -1, -1, -1, 0, -1, 0, 0, 0, -1, false, fa.path });
                    }
                }
            }

            inOut.files.push_back (pair);
        }

        for (size_t j = 0; j < mapB->files.size(); j++)
        {
            if (!isUsed[j] && !mapB->files[j].isDeleted)
            {
                FilePair  pair;

                pair.path    = mapB->files[j].path;
                pair.fileB   = static_cast<int> (j);
                pair.outcome = DifferenceKind::FileOnlyInB;
                inOut.files.push_back (pair);
                inOut.differences.push_back ({ DifferenceKind::FileOnlyInB, -1, -1, -1, 0, -1, 0, 0, 0, -1, false, pair.path });
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::ReadContents
//
//  A file's data in file order, each hole as zeros of its unit (a sector,
//  a block, or a CP/M allocation block), cut to the size its entry records
//  where that is in bytes. False when its chain is broken or a sector of
//  it cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskComparer::ReadContents (const FileMap & map, const SectorSource & source, const MappedFile & file, vector<Byte> & outBytes)
{
    size_t  hole   = (map.fileSystem == MapFileSystem::Dos33) ? 256u : (map.fileSystem == MapFileSystem::Cpm) ? 1024u : 512u;
    bool    isRead = file.chainReason == ChainReason::None;



    outBytes.clear();

    for (const FilePlace & place : file.sectors)
    {
        if (!isRead || (place.cell >= 0 && place.role != SectorRole::FileData))
        {
            continue;
        }

        if (place.cell < 0)
        {
            outBytes.insert (outBytes.end(), hole, Byte (0));
        }
        else
        {
            isRead = ReadCell (map, source, place.cell, outBytes);
        }
    }

    if (isRead && map.fileSystem != MapFileSystem::Dos33 && file.recordedSize > 0 && file.recordedSize < outBytes.size())
    {
        outBytes.resize (static_cast<size_t> (file.recordedSize));
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer::ReadCell
//
//  A cell's bytes: a DOS 3.3 or CP/M sector, or both halves of a block.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskComparer::ReadCell (const FileMap & map, const SectorSource & source, int cell, vector<Byte> & inOut)
{
    bool  isBlock = map.fileSystem == MapFileSystem::ProDos || map.fileSystem == MapFileSystem::Pascal;
    bool  isRead  = true;



    for (int half = 0; isRead && half < (isBlock ? 2 : 1); half++)
    {
        const SectorSource::Sector &  sector = source.GetPhysical (map.GetTrack (cell), map.GetPhysical (cell, half));

        isRead = SectorSource::IsReadable (sector.result) && sector.bytes != nullptr;

        if (isRead)
        {
            inOut.insert (inOut.end(), sector.bytes, sector.bytes + SectorSource::kBytes);
        }
    }

    return isRead;
}
