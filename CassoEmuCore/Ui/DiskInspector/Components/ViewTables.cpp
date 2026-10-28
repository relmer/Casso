#include "Pch.h"

#include "Ui/DiskInspector/Components/ViewTables.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/Views/DifferencesView.h"
#include "Ui/DiskInspector/Views/FilesView.h"
#include "Ui/DiskInspector/Views/IssuesView.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Ui/DiskInspector/Views/FluxView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ViewTables::ViewTables
//
////////////////////////////////////////////////////////////////////////////////

ViewTables::ViewTables (InspectorSession & session, IInspectorComponentHost & host) :
    m_session (session),
    m_host    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ViewTables::RefreshTables
//
//  The Tracks and Findings tabs after the analysis changes, and the Fields
//  selection when the quarter track changes.
//
////////////////////////////////////////////////////////////////////////////////

void ViewTables::RefreshTables()
{
    const TrackAnalysis *  track = m_session.model.GetTrack();
    int                    qt    = m_session.model.GetQuarterTrack();



    if (m_session.isTablesDirty)
    {
        vector<std::wstring>  columns = InspectorTables::GetTrackColumns();
        vector<TableRow>      rows    = m_session.context.hasDisk ? InspectorTables::BuildTracks (m_session.analysis, m_session.context.findMarks, &m_session.edits, &m_session.appliedSlots, m_session.context.drive) : vector<TableRow>();

        //  While comparing, each quarter track's verdict (FR-121).
        if (m_session.comparison.IsComparing())
        {
            ComparisonText::AddVerdictColumn (columns, rows, m_session.comparison.GetResult(), m_session.comparison.IsWhollyBusy() || !m_session.comparison.HasResult(), m_session.comparison.GetComparing());
        }

        if (m_session.comparison.IsComparing() != m_isTracksCompared)
        {
            m_session.views.tracks->SetColumns (columns);
            m_isTracksCompared = m_session.comparison.IsComparing();
        }

        m_session.views.tracks->SetRows    (std::move (rows));
        m_session.views.diffs->Refresh();
        m_session.views.tracks->SetCaption (m_session.context.hasDisk && InspectorTables::IsAlignmentNoteShown (m_session.analysis)
                                     ? L"This image's INFO says its tracks were not imaged in sync, so their alignment to one another was not kept"
                                     : L"");
        m_session.views.issues->Refresh();

        vector<TableRow>  imageRows = m_session.context.hasDisk ? InspectorTables::BuildImage (m_session.analysis.image, m_session.context.drive != nullptr ? m_session.analysis.copy.get() : nullptr,
                                                                                               m_session.context.drive)
                                                                : vector<TableRow>();

        //  With no Flux view on a disk without flux tracks, the Image view has
        //  the note on why the disk has no timing (FR-003).
        if (m_session.context.hasDisk && m_session.analysis.copy != nullptr && m_session.analysis.summary.fluxTracks == 0)
        {
            TableRow  timing;

            timing.cells = { L"Timing", FluxView::FormatNoTiming (m_session.analysis.copy->format, m_session.analysis.copy->woz.info.version) };
            imageRows.push_back (timing);
        }

        m_session.views.image->SetRows (std::move (imageRows));
        m_session.views.files->RefreshFileList();

        //  Casso does not analyze 3.5" disks; such a WOZ opens on the Image
        //  tab, and every other view is covered by a note (FR-054).
        m_session.is35 = m_session.context.hasDisk && m_session.analysis.image.isWoz && m_session.analysis.image.info.diskType == WozLoader::kDiskType35;

        if (m_session.is35)
        {
            m_host.ShowDiskTab (kTabImage);
        }

        m_host.Relayout();
    }

    if (m_session.isTablesDirty || qt != m_syncedQuarterTrack)
    {
        m_syncedQuarterTrack = qt;
        SyncTables();
    }

    m_session.isTablesDirty = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ViewTables::SyncTables
//
//  Each table selects the row for the window's selection.
//
////////////////////////////////////////////////////////////////////////////////

void ViewTables::SyncTables()
{
    int  qt = m_session.model.GetQuarterTrack();



    m_session.views.tracks->SelectRowWhere ([qt] (const TableRow & row) { return row.quarterTrack == qt; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ViewTables::SelectFromRow
//
//  A row goes to what it refers to: a field's nibbles, a sector, or a
//  quarter track.
//
////////////////////////////////////////////////////////////////////////////////

void ViewTables::SelectFromRow (const TableRow & row)
{
    const TrackAnalysis *  track = nullptr;



    if (row.quarterTrack >= 0)
    {
        m_session.model.SelectQuarterTrack (row.quarterTrack);
        track = m_session.model.GetTrack();

        if (row.sectorIndex >= 0)
        {
            m_session.model.SelectSector (row.quarterTrack, row.sectorIndex);
        }
        else if (track != nullptr && row.field >= 0 && row.field < static_cast<int> (track->fields.size()))
        {
            m_session.model.SelectNibbles (row.quarterTrack, track->fields[row.field].firstNibble, track->fields[row.field].nibbleCount);
        }

        m_host.OnSelection();
    }
}
