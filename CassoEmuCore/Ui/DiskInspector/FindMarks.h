#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Devices/Disk/Inspector/InspectorSearch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindMarks
//
//  Where the find hits lie, gathered once per search for the views that
//  mark them (FR-056): the runs of nibbles the hits cover on each track,
//  the runs of bytes they cover in each sector's data, the number of hits
//  in each sector and on each track, and the file map cells with hits. A
//  hit in sector data covers the nibbles its bytes are encoded in.
//
////////////////////////////////////////////////////////////////////////////////

class FindMarks
{
public:
    struct Run
    {
        int  first = 0;
        int  count = 0;
    };

    struct TrackMarks
    {
        int                         quarterTrack = -1;
        int                         hitCount     = 0;
        vector<Run>                 nibbles;
        std::map<int, int>          sectorHits;
        std::map<int, vector<Run>>  bytes;
    };

    void  Build (const vector<SearchHit> & hits, const FileMap * map);
    void  Clear ();
    bool  IsEmpty () const { return m_tracks.empty(); }

    const std::map<const TrackAnalysis *, TrackMarks> &  GetTracks () const { return m_tracks; }

    const TrackMarks *  GetTrack       (const TrackAnalysis * track) const;
    bool                IsNibbleMarked (const TrackAnalysis * track, int nibble) const;
    bool                IsByteMarked   (const TrackAnalysis * track, int sectorIndex, int offset) const;
    int                 GetSectorHits  (const TrackAnalysis * track, int sectorIndex) const;
    int                 GetTrackHits   (const TrackAnalysis * track) const;
    bool                IsCellMarked   (int cell) const;

    //  The nibbles a hit covers: its own for a nibble hit, framed from its
    //  first cell for one at any bit offset, and for a hit in sector data
    //  the nibbles of its bytes, exact for 6-and-2 and in proportion for
    //  5-and-3, whose bytes are not in order.
    static bool  GetNibbles (const SearchHit & hit, int & outFirst, int & outCount);

    //  The index of the sector whose address or data field contains a
    //  nibble, or -1 for a nibble in a gap.
    static int   GetSectorOf (const TrackAnalysis & track, int nibble);

    //  Runs sorted by their first, those that overlap or touch joined.
    static void  Merge (vector<Run> & inOutRuns);

    //  Whether a run of a track with the given number of nibbles contains a
    //  nibble, around the index too.
    static bool  IsInRuns (const vector<Run> & runs, int size, int nibble);

private:
    std::map<const TrackAnalysis *, TrackMarks>  m_tracks;
    vector<Byte>                                 m_cells;
};
