#pragma once

#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DiskInspector/AnalysisScheduler.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"
#include "Ui/DiskInspector/InspectorTables.h"
#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/InspectorViewModel.h"
#include "Ui/DiskInspector/PlatterRenderer.h"

class PlatterView;
class TrackHeaderView;
class TrackStripView;
class SectorRowView;
class SectorByteView;
class NibblesTab;
class FindingsTab;
class InspectorTableView;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow
//
//  The disk inspector: a toolbar with the drive tabs, the file name and the
//  summary chips; the platter column on the left; and past the splitter the
//  track column with its header, strip, sector row and tabs. It asks its
//  host for a copy of the disk, analyzes the copy in the background, and
//  shows each record as its analysis arrives. The host calls RenderFrame
//  once per frame of its own.
//
////////////////////////////////////////////////////////////////////////////////

class DiskInspectorWindow : public DxuiWindow
{
public:
    static constexpr int     kOpeningWidthDip  = 980;
    static constexpr int     kOpeningHeightDip = 660;
    static constexpr int     kMinWidthDip      = 640;
    static constexpr int     kMinHeightDip     = 460;
    static constexpr double  kZoomStep         = 1.25;

    DiskInspectorWindow  ();
    ~DiskInspectorWindow () override;

    HRESULT  Create      (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDiskInspectorHost * host, bool activate);
    HRESULT  ShowDrive   (int drive);
    HRESULT  RenderFrame ();
    int      GetDrive    () const { return m_drive; }

    void     Layout      (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void     Paint       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse     (const DxuiMouseEvent & ev) override;
    bool     OnKey       (const DxuiKeyEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

protected:
    void  OnCreate      () override;
    void  OnWindowClose () override;

private:
    void  RequestCopy      ();
    void  TakeReplies      ();
    void  TakeResults      ();
    void  UpdateRings      ();
    void  UpdateControls   ();
    void  OnSelection      ();
    void  ShowTrackTab     (int tab);
    void  ShowDiskTab      (int tab);
    void  RefreshTables    ();
    void  SyncTables       ();
    void  SelectFromRow    (const TableRow & row);
    void  OpenDecodeSettings ();
    void  ApplySettings    (const DecodeSettings & settings);
    void  UpdateTooltip    (POINT pointPx);
    void  PaintToolbar     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  StepSector       (int delta, bool isToEnd);
    std::wstring  GetFileName () const;
    static bool   IsInside    (const RECT & rect, POINT pointPx);

    const CassoTheme                                                                           * m_theme          = nullptr;
    IDiskInspectorHost                                                                         * m_host           = nullptr;
    NullDiskInspectorHost                                                                        m_nullHost;
    int                                                                                          m_drive          = 0;
    uint64_t                                                                                     m_pendingRequest = 0;
    DiskAnalysis                                                                                 m_analysis;
    InspectorViewModel                                                                           m_model;
    InspectorViewContext                                                                         m_context;
    AnalysisScheduler                                                                            m_scheduler;
    DxuiTooltip                                                                                  m_tooltip;
    std::array<std::shared_ptr<const PlatterRenderer::Levels>, DiskImage::kQuarterTrackCount>    m_levels;
    std::set<int>                                                                                m_changedSlots;
    bool                                                                                         m_isSplitting    = false;
    double                                                                                       m_splitFraction  = 0.5;
    int                                                                                          m_trackTab       = 0;
    DxuiDpiScaler                                                                                m_scaler;
    RECT                                                                                         m_toolbarPx      = {};
    RECT                                                                                         m_fileNamePx     = {};
    RECT                                                                                         m_chipsPx        = {};
    RECT                                                                                         m_splitterPx     = {};
    std::wstring                                                                                 m_tooltipText;

    DxuiTabStrip *                           m_driveTabs      = nullptr;
    DxuiTabStrip *                           m_trackTabs      = nullptr;
    DxuiButton *                             m_zoomOut        = nullptr;
    DxuiButton *                             m_zoomIn         = nullptr;
    DxuiButton *                             m_fit            = nullptr;
    DxuiLabel *                              m_zoomLabel      = nullptr;
    DxuiLabel *                              m_hintLabel      = nullptr;
    PlatterView *                            m_platterView    = nullptr;
    TrackHeaderView *                        m_headerView     = nullptr;
    TrackStripView *                         m_stripView      = nullptr;
    SectorRowView *                          m_sectorRow      = nullptr;
    SectorByteView *                         m_byteView       = nullptr;
    NibblesTab *                             m_nibblesTab     = nullptr;
    DxuiTabStrip *                           m_diskTabs       = nullptr;
    DxuiButton *                             m_decodeButton   = nullptr;
    InspectorTableView *                     m_tracksTab      = nullptr;
    FindingsTab *                            m_findingsTab    = nullptr;
    InspectorTableView *                     m_fieldsTab      = nullptr;
    int                                      m_diskTab        = 0;
    bool                                     m_isTablesDirty  = true;
    int                                      m_fieldsOf       = -1;
    const TrackAnalysis *                    m_fieldsTrack    = nullptr;
};
