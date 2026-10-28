#include "Pch.h"

#include "Ui/DiskInspector/Components/FindPart.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/FindBar.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/InspectorTables.h"
#include "Ui/DiskInspector/PlatterGeometry.h"





static constexpr LPCWSTR  s_kpszFindBadHex  = L"Enter pairs of hex digits, such as D5 AA 96.";
static constexpr LPCWSTR  s_kpszFindBadText = L"Enter the text to find, in plain ASCII.";





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::FindPart
//
////////////////////////////////////////////////////////////////////////////////

FindPart::FindPart (InspectorSession & session, IInspectorComponentHost & host) :
    m_session (session),
    m_host    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::Create
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::Create()
{
    DxuiPanel &  parent = m_host.GetControlParent();



    m_findResults = parent.CreateChild<InspectorTableView> (m_session.context);
    m_findBar     = parent.CreateChild<FindBar>();

    m_findResults->SetColumns  (InspectorTables::GetSearchHitColumns());
    m_findResults->SetOnSelect ([this] (const TableRow & row) { if (m_session.find.Choose (row.finding)) { SelectHit (m_session.find.GetHits()[row.finding]); m_findBar->SetResult (row.finding, m_session.find.GetCount()); } });
    m_findBar->Init            (m_host.GetOwnerHwnd(), m_host.GetTextRenderer(), m_host.GetPopupHost(), m_session.theme);
    m_findBar->SetOnSearch     ([this] (const SearchQuery & query) { RunFind (query); });
    m_findBar->SetOnStep       ([this] (int step) { StepFind (step); });
    m_findBar->SetOnList       ([this] () { m_host.ShowDiskTab (kTabFindResults); });
    m_findBar->SetOnClose      ([this] () { CloseFindBar(); });
    m_findBar->SetVisible      (false);
    m_session.find.GetQuery().isWholeDisk = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::Tick
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::Tick (int64_t nowMs)
{
    m_findBar->Tick (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::Layout
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::Layout (const RECT & bar, const DxuiDpiScaler & scaler)
{
    m_findBar->Layout (bar, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::OnKey
//
//  Keys go to the find bar while it has the focus, all but the window's
//  zoom and its Ctrl+F and Ctrl+G.
//
////////////////////////////////////////////////////////////////////////////////

bool FindPart::OnKey (const DxuiKeyEvent & ev)
{
    bool  isWindowKey = ev.ctrl && (ev.vk == '0' || ev.vk == VK_NUMPAD0 || ev.vk == VK_OEM_PLUS || ev.vk == VK_ADD || ev.vk == VK_OEM_MINUS ||
                                    ev.vk == VK_SUBTRACT || ev.vk == 'F' || ev.vk == 'G');



    return m_session.isFindOpen && m_session.keyTarget == KeyTarget::FindBar && !isWindowKey && m_findBar->OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::OnPress
//
//  A press anywhere but the bar takes the focus out of its pattern.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::OnPress()
{
    m_findBar->SetInputFocused (m_session.keyTarget == KeyTarget::FindBar && m_findBar->IsInputFocused());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::IsBarAt
//
////////////////////////////////////////////////////////////////////////////////

bool FindPart::IsBarAt (POINT pointPx) const
{
    RECT  bounds = m_findBar->GetBounds();



    return m_session.isFindOpen && PtInRect (&bounds, pointPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::OnDiskStarted
//
//  Hits point into the analysis they were found in.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::OnDiskStarted()
{
    m_session.find.Clear();
    m_findBar->SetResult (-1, 0);
    RefreshFindResults();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::OpenFindBar
//
//  The find bar opens on the last query, with the focus in its pattern.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::OpenFindBar()
{
    if (!m_session.isFindOpen)
    {
        m_session.isFindOpen = true;
        m_findBar->SetQuery  (m_session.find.GetQuery());
        m_findBar->SetResult (m_session.find.GetIndex(), m_session.find.GetCount());
        m_findBar->SetVisible (true);
        SyncFindMarks();
        m_host.Relayout();
    }

    m_session.keyTarget = KeyTarget::FindBar;
    m_findBar->SetInputFocused (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::CloseFindBar
//
//  The hits stay for F3 and the Find results tab.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::CloseFindBar()
{
    m_session.isFindOpen = false;
    m_findBar->SetInputFocused (false);
    m_findBar->SetVisible (false);
    SyncFindMarks();
    m_session.keyTarget = KeyTarget::Platter;
    m_host.Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::RunFind
//
//  The query's hits replace the last ones, and the first is selected. For a
//  pattern that cannot be read, the hits are cleared and the bar displays
//  an example of a valid pattern. The Find results tab is visible only
//  while there are hits.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::RunFind (const SearchQuery & query)
{
    bool  isRead = m_session.find.Run (m_session.analysis, m_session.model.GetQuarterTrack(), query, m_session.scheduler->HasPending());



    if (!m_session.context.hasDisk)
    {
        m_session.find.Clear();
    }

    m_findBar->SetSearched (query);
    m_findBar->SetError (isRead ? std::wstring() : (query.kind == SearchKind::SectorText ? std::wstring (s_kpszFindBadText) : std::wstring (s_kpszFindBadHex)));

    RefreshFindResults();
    StepFind (1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::RefreshFindResults
//
//  The Find results tab's rows for the hits; with none, a window showing
//  the tab goes back to Tracks.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::RefreshFindResults()
{
    m_findResults->SetRows (InspectorTables::BuildSearchHits (m_session.find.GetHits(), m_session.context.hasDisk ? &m_session.analysis : nullptr, m_session.GetFileMap()));
    RebuildFindMarks();

    if (m_session.find.IsEmpty() && m_session.isDiskGroup && m_session.diskTab == kTabFindResults)
    {
        m_host.ShowDiskTab (kTabTracks);
    }
    else if (m_session.scaler.GetDpi() != 0)
    {
        m_host.Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::RebuildFindMarks
//
//  The marks for the hits whose record has not been analyzed again since the
//  search. A hit on a record analyzed again refers to an analysis that is
//  gone, so it is left out before anything reads through it.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::RebuildFindMarks()
{
    m_session.findMarks.Build (m_session.find.GetCurrentHits (m_session.analysis), m_session.GetFileMap(), m_session.find.GetQuery().text);
    SyncFindMarks();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::RepeatFind
//
//  The last search again over the whole analysis, without moving the
//  selection. The selected hit's index is clamped to the new count.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::RepeatFind()
{
    m_session.find.Repeat (m_session.analysis, m_session.scheduler->HasPending());
    RefreshFindResults();
    m_findBar->SetResult (m_session.find.GetIndex(), m_session.find.GetCount());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::SyncFindMarks
//
//  The views mark the hits while the find bar is open (FR-056), and the
//  Tracks tab counts them.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::SyncFindMarks()
{
    m_session.context.findMarks = (m_session.isFindOpen && !m_session.findMarks.IsEmpty()) ? &m_session.findMarks : nullptr;
    m_session.isTablesDirty     = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::StepFind
//
//  Find next and Find previous, wrapping around the hits.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::StepFind (int step)
{
    int  index = m_session.find.Step (step);



    if (index >= 0)
    {
        SelectHit (m_session.find.GetHits()[index]);
        m_findResults->SelectRowWhere ([index] (const TableRow & row) { return row.finding == index; });
    }

    m_findBar->SetResult (index, m_session.find.GetCount());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart::SelectHit
//
//  A sector hit selects its sector; a nibble hit its nibbles, and one off
//  the framed nibbles the nibble its first cell falls in.
//
////////////////////////////////////////////////////////////////////////////////

void FindPart::SelectHit (const SearchHit & hit)
{
    const TrackAnalysis *  track  = nullptr;
    int                    nibble = hit.firstNibble;



    if (hit.sectorIndex >= 0)
    {
        m_session.model.SelectSector (hit.quarterTrack, hit.sectorIndex);
    }
    else
    {
        m_session.model.SelectQuarterTrack (hit.quarterTrack);
        track  = m_session.model.GetTrack();
        nibble = (!hit.isAligned && track != nullptr) ? PlatterGeometry::GetNibbleAt (*track, hit.cell) : nibble;
        m_session.model.SelectNibbles (hit.quarterTrack, nibble, hit.nibbleCount);
    }

    m_host.OnSelection();
}
