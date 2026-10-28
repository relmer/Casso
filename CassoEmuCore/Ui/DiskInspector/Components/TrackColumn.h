#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class IInspectorComponentHost;
class InspectorSession;
class SectorRowView;
class TrackHeaderView;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn
//
//  The top of the column beside the platter: the track header, the sector
//  row, and the view selector under them, a segmented control with each view
//  present now in FR-160's order (FR-039, FR-160).
//
////////////////////////////////////////////////////////////////////////////////

class TrackColumn : public IInspectorComponent
{
public:
    //  One choice of the view selector: its label, its tab and group, and its
    //  tooltip.
    struct View
    {
        LPCWSTR  label;
        bool     isDisk;
        int      tab;
        LPCWSTR  tip;
    };

    TrackColumn (InspectorSession & session, IInspectorComponentHost & host);

    void          Create            () override;
    void          Update            () override;
    void          Tick              (int64_t nowMs) override;
    std::wstring  GetTip            (POINT pointPx, RECT & outAnchorPx) const override;
    void          OnAnalysisChanged () override;

    //  Lays out the header, the sector row and the selector from the top of
    //  the area; returns where the views' content starts.
    int   Layout         (const RECT & area, const DxuiDpiScaler & scaler);
    bool  IsLayoutStale  () const;
    RECT  GetBoundsPx    () const { return m_columnPx; }

    TrackHeaderView &  GetHeader    () { return *m_header; }
    SectorRowView   &  GetSectorRow () { return *m_sectorRow; }

    vector<View>         GetShownViews () const;
    static vector<View>  GetAllViews   ();

private:
    int   GetSelectedIndex () const;
    void  Choose           (int index);

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    TrackHeaderView          * m_header    = nullptr;
    SectorRowView            * m_sectorRow = nullptr;
    DxuiSegmentedControl     * m_views     = nullptr;
    vector<View>               m_shown;
    RECT                       m_columnPx  = {};
    int                        m_rows      = 1;
    bool                       m_hasIssues = false;
};
