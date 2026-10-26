#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindSession
//
//  The inspector's last search (FR-056): its query, the track it started
//  from, its hits and the selected hit's index, with stepping that wraps
//  around the hits. A search made while the disk is being analyzed is
//  partial and is run again once the analysis is done. Each hit refers to
//  the track analysis it was found in, so a hit on a record analyzed again
//  since is out of date, and only its place in the list is used. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class FindSession
{
public:
    //  Searches the analysis; false, with no hits, for a pattern that cannot
    //  be read. An empty pattern clears the hits and is not an error.
    bool  Run    (const DiskAnalysis & analysis, int quarterTrack, const SearchQuery & query, bool isAnalysisPartial);

    //  The same search again, keeping the selected hit's place among the
    //  hits where the new count allows.
    void  Repeat (const DiskAnalysis & analysis, bool isAnalysisPartial);

    void  Clear  ();

    //  The next or previous hit, wrapping; from no selection, the first for a
    //  step forward and the last for a step back. -1 with no hits.
    int   Step   (int step);

    //  Selects a hit by its index; false for an index out of range.
    bool  Choose (int index);

    bool  IsRepeatDue (bool isAnalysisPartial) const { return m_isPartial && !isAnalysisPartial; }

    //  The hits whose record has not been analyzed again since the search.
    vector<SearchHit>  GetCurrentHits (const DiskAnalysis & analysis) const;

    const vector<SearchHit> &  GetHits  () const { return m_hits; }
    const SearchQuery       &  GetQuery () const { return m_query; }
    SearchQuery             &  GetQuery ()       { return m_query; }
    int                        GetIndex () const { return m_index; }
    int                        GetCount () const { return static_cast<int> (m_hits.size()); }
    bool                       IsEmpty  () const { return m_hits.empty(); }

private:
    SearchQuery        m_query;
    vector<SearchHit>  m_hits;
    int                m_index        = -1;
    int                m_quarterTrack = 0;
    bool               m_isPartial    = false;
};
