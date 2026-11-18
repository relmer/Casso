#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class IInspectorComponentHost;
class InspectorSession;
struct TableRow;





////////////////////////////////////////////////////////////////////////////////
//
//  ViewTables
//
//  The rows of the Tracks, Issues and Image views after the analysis
//  changes, each table's row for the selection, and the selection a row
//  makes (FR-047, FR-048, FR-050).
//
////////////////////////////////////////////////////////////////////////////////

class ViewTables : public IInspectorComponent
{
public:
    ViewTables (InspectorSession & session, IInspectorComponentHost & host);

    void  Create () override {}

    void  RefreshTables ();
    void  SyncTables    ();
    void  SelectFromRow (const TableRow & row);

private:
    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    bool                       m_isTracksCompared   = false;
    int                        m_syncedQuarterTrack = -1;
};