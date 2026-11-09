#include "Pch.h"

#include "Ui/DiskInspector/FindSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindSession::Run
//
////////////////////////////////////////////////////////////////////////////////

bool FindSession::Run (const DiskAnalysis & analysis, int quarterTrack, const SearchQuery & query, bool isAnalysisPartial)
{
    vector<SearchItem>  items;
    bool                isRead = query.text.empty() || InspectorSearch::ParsePattern (query.text, query.kind, items);



    m_query        = query;
    m_quarterTrack = quarterTrack;
    m_index        = -1;
    m_hits.clear();
    m_isPartial    = false;

    if (isRead && !query.text.empty())
    {
        m_hits      = InspectorSearch::Find (analysis, quarterTrack, query);
        m_isPartial = isAnalysisPartial;
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSession::Repeat
//
////////////////////////////////////////////////////////////////////////////////

void FindSession::Repeat (const DiskAnalysis & analysis, bool isAnalysisPartial)
{
    m_hits      = InspectorSearch::Find (analysis, m_quarterTrack, m_query);
    m_isPartial = isAnalysisPartial;
    m_index     = std::min (m_index, GetCount() - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSession::Clear
//
//  The hits and the selection go; the query stays for the next search.
//
////////////////////////////////////////////////////////////////////////////////

void FindSession::Clear()
{
    m_hits.clear();
    m_index     = -1;
    m_isPartial = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSession::Step
//
////////////////////////////////////////////////////////////////////////////////

int FindSession::Step (int step)
{
    int  count = GetCount();



    if (count == 0)
    {
        m_index = -1;
    }
    else if (m_index < 0)
    {
        m_index = (step >= 0) ? 0 : count - 1;
    }
    else
    {
        m_index = ((m_index + step) % count + count) % count;
    }

    return m_index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSession::Choose
//
////////////////////////////////////////////////////////////////////////////////

bool FindSession::Choose (int index)
{
    bool  isValid = index >= 0 && index < GetCount();



    m_index = isValid ? index : m_index;

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSession::GetCurrentHits
//
////////////////////////////////////////////////////////////////////////////////

vector<SearchHit> FindSession::GetCurrentHits (const DiskAnalysis & analysis) const
{
    vector<SearchHit>  current;



    std::copy_if (m_hits.begin(), m_hits.end(), std::back_inserter (current),
                  [&analysis] (const SearchHit & hit) { return !InspectorSearch::IsOutOfDate (hit, analysis); });

    return current;
}
