#include "Pch.h"

#include "Ui/DiskInspector/FindMarks.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Ui/DiskInspector/PlatterGeometry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::Build
//
//  A hit's cell on the file map is found as the Find results tab finds its
//  owner: through its sector, on a whole track.
//
////////////////////////////////////////////////////////////////////////////////

void FindMarks::Build (const vector<SearchHit> & hits, const FileMap * map)
{
    int  first  = 0;
    int  count  = 0;
    int  sector = -1;
    int  cell   = -1;
    int  half   = 0;



    Clear();
    m_cells.assign ((map != nullptr) ? map->cells.size() : 0, 0);

    for (const SearchHit & hit : hits)
    {
        if (hit.track == nullptr)
        {
            continue;
        }

        TrackMarks &  marks = m_tracks[hit.track];

        marks.quarterTrack = (marks.quarterTrack < 0) ? hit.quarterTrack : marks.quarterTrack;
        marks.hitCount++;

        sector = hit.sectorIndex;

        if (GetNibbles (hit, first, count))
        {
            marks.nibbles.push_back ({ first, count });
            sector = (sector < 0) ? GetSectorOf (*hit.track, first) : sector;
        }

        if (sector >= 0)
        {
            marks.sectorHits[sector]++;
        }

        if (hit.offset >= 0 && sector >= 0)
        {
            marks.bytes[sector].push_back ({ hit.offset, std::max (hit.length, 1) });
        }

        if (map != nullptr && sector >= 0 && hit.quarterTrack % DiskImage::kQuarterTracksPerWholeTrack == 0)
        {
            cell = map->GetCellOf (hit.quarterTrack / DiskImage::kQuarterTracksPerWholeTrack, hit.track->sectors[sector].sector, half);

            if (cell >= 0 && cell < static_cast<int> (m_cells.size()))
            {
                m_cells[cell] = 1;
            }
        }
    }

    for (auto & [track, marks] : m_tracks)
    {
        Merge (marks.nibbles);

        for (auto & [s, runs] : marks.bytes)
        {
            Merge (runs);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::Clear
//
////////////////////////////////////////////////////////////////////////////////

void FindMarks::Clear()
{
    m_tracks.clear();
    m_cells.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::GetTrack
//
////////////////////////////////////////////////////////////////////////////////

const FindMarks::TrackMarks * FindMarks::GetTrack (const TrackAnalysis * track) const
{
    auto  at = m_tracks.find (track);



    return (at != m_tracks.end()) ? &at->second : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::IsNibbleMarked
//
////////////////////////////////////////////////////////////////////////////////

bool FindMarks::IsNibbleMarked (const TrackAnalysis * track, int nibble) const
{
    const TrackMarks *  marks = GetTrack (track);



    return marks != nullptr && IsInRuns (marks->nibbles, static_cast<int> (track->framed.nibbles.size()), nibble);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::IsByteMarked
//
////////////////////////////////////////////////////////////////////////////////

bool FindMarks::IsByteMarked (const TrackAnalysis * track, int sectorIndex, int offset) const
{
    const TrackMarks *  marks  = GetTrack (track);
    bool                isHit  = false;



    if (marks != nullptr)
    {
        auto  at = marks->bytes.find (sectorIndex);

        isHit = at != marks->bytes.end() && IsInRuns (at->second, 0, offset);
    }

    return isHit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::GetSectorHits
//
////////////////////////////////////////////////////////////////////////////////

int FindMarks::GetSectorHits (const TrackAnalysis * track, int sectorIndex) const
{
    const TrackMarks *  marks = GetTrack (track);
    int                 count = 0;



    if (marks != nullptr)
    {
        auto  at = marks->sectorHits.find (sectorIndex);

        count = (at != marks->sectorHits.end()) ? at->second : 0;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::GetTrackHits
//
////////////////////////////////////////////////////////////////////////////////

int FindMarks::GetTrackHits (const TrackAnalysis * track) const
{
    const TrackMarks *  marks = GetTrack (track);



    return (marks != nullptr) ? marks->hitCount : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::IsCellMarked
//
////////////////////////////////////////////////////////////////////////////////

bool FindMarks::IsCellMarked (int cell) const
{
    return cell >= 0 && cell < static_cast<int> (m_cells.size()) && m_cells[cell] != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::GetNibbles
//
//  A data field's body starts with the auxiliary nibbles, 86 for 6-and-2
//  and 154 for 5-and-3, then a nibble for each byte.
//
////////////////////////////////////////////////////////////////////////////////

bool FindMarks::GetNibbles (const SearchHit & hit, int & outFirst, int & outCount)
{
    const TrackAnalysis *  track = hit.track;
    int                    size  = (track != nullptr) ? static_cast<int> (track->framed.nibbles.size()) : 0;
    int                    body  = 0;
    int                    aux   = 0;



    outFirst = -1;
    outCount = 0;

    if (size == 0)
    {
        //  Nothing to mark.
    }
    else if (hit.offset >= 0)
    {
        if (hit.sectorIndex >= 0 && hit.sectorIndex < static_cast<int> (track->sectors.size()) && track->sectors[hit.sectorIndex].dataField >= 0)
        {
            const LocatedField &  field = track->fields[track->sectors[hit.sectorIndex].dataField];

            body     = (field.kind == DiskFieldKind::Thirteen) ? DiskFieldFormat::kBodyLength53 : DiskFieldFormat::kBodyLength62;
            aux      = body - DiskFieldFormat::kSectorBytes;
            outFirst = (field.bodyNibble + aux + hit.offset) % size;
            outCount = std::max (hit.length, 1);
        }
    }
    else if (!hit.isAligned)
    {
        outFirst = PlatterGeometry::GetNibbleAt (*track, hit.cell);
        outCount = std::max (hit.nibbleCount, 1);
    }
    else
    {
        outFirst = hit.firstNibble % size;
        outCount = std::max (hit.nibbleCount, 1);
    }

    return outFirst >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::GetSectorOf
//
////////////////////////////////////////////////////////////////////////////////

int FindMarks::GetSectorOf (const TrackAnalysis & track, int nibble)
{
    int     size   = static_cast<int> (track.fieldOfNibble.size());
    int     field  = (size > 0 && nibble >= 0) ? track.fieldOfNibble[nibble % size] : -1;
    int     sector = -1;
    size_t  s      = 0;



    for (s = 0; field >= 0 && sector < 0 && s < track.sectors.size(); s++)
    {
        if (track.sectors[s].addressField == field || track.sectors[s].dataField == field)
        {
            sector = static_cast<int> (s);
        }
    }

    return sector;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::Merge
//
////////////////////////////////////////////////////////////////////////////////

void FindMarks::Merge (vector<Run> & inOutRuns)
{
    vector<Run>  merged;



    std::sort (inOutRuns.begin(), inOutRuns.end(), [] (const Run & a, const Run & b) { return a.first < b.first; });

    for (const Run & run : inOutRuns)
    {
        if (!merged.empty() && run.first <= merged.back().first + merged.back().count)
        {
            merged.back().count = std::max (merged.back().count, run.first + run.count - merged.back().first);
        }
        else
        {
            merged.push_back (run);
        }
    }

    inOutRuns = std::move (merged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks::IsInRuns
//
//  The last run that starts at or before the nibble, and for a track the
//  last run of all, which can pass the index and end past the first
//  nibble. A size of 0 is for runs that cannot wrap.
//
////////////////////////////////////////////////////////////////////////////////

bool FindMarks::IsInRuns (const vector<Run> & runs, int size, int nibble)
{
    auto  after = std::upper_bound (runs.begin(), runs.end(), nibble, [] (int n, const Run & run) { return n < run.first; });
    bool  isIn  = false;



    if (after != runs.begin())
    {
        const Run &  run = *(after - 1);

        isIn = nibble < run.first + run.count;
    }

    if (!isIn && size > 0 && !runs.empty())
    {
        isIn = nibble + size < runs.back().first + runs.back().count;
    }

    return isIn;
}
