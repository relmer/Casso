#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class FindBar;
class IInspectorComponentHost;
class InspectorSession;
class InspectorTableView;





////////////////////////////////////////////////////////////////////////////////
//
//  FindPart
//
//  Find: the bar docked under the toolbar, the Find results tab, stepping
//  through the hits, and the marks the views draw for them (FR-056).
//
////////////////////////////////////////////////////////////////////////////////

class FindPart : public IInspectorComponent
{
public:
    FindPart (InspectorSession & session, IInspectorComponentHost & host);

    void  Create () override;
    void  Tick   (int64_t nowMs) override;
    bool  OnKey  (const DxuiKeyEvent & ev) override;

    void  Layout             (const RECT & bar, const DxuiDpiScaler & scaler);
    void  OpenFindBar        ();
    void  CloseFindBar       ();
    void  StepFind           (int step);
    void  RefreshFindResults ();
    void  RepeatFind         ();
    void  RebuildFindMarks   ();
    void  OnDiskStarted      ();
    void  OnPress            ();
    bool  IsBarAt            (POINT pointPx) const;

    InspectorTableView & GetResults () const { return *m_findResults; }

private:
    void  RunFind       (const SearchQuery & query);
    void  SyncFindMarks ();
    void  SelectHit     (const SearchHit & hit);

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    FindBar                  * m_findBar     = nullptr;
    InspectorTableView       * m_findResults = nullptr;
};
