#pragma once

#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DiskInspector/AnalysisScheduler.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"
#include "Ui/DiskInspector/InspectorViewModel.h"
#include "Ui/DiskInspector/PlatterRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow
//
//  The disk inspector: a toolbar, a tab per drive, the platter on the left
//  and the track's tabs on the right, with a splitter between them. It asks
//  its host for a copy of the disk, analyzes the copy in the background,
//  and draws each record as its analysis arrives. The host calls RenderFrame
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
    static constexpr double  kWheelZoomStep    = 1.25;

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
    void  RequestCopy        ();
    void  TakeReplies        ();
    void  TakeResults        ();
    void  UpdateRings        ();
    void  UpdateLabels       ();
    void  DrawPlatter        (const DxuiCustomDrawArgs & args);
    void  ZoomBy             (double factor, POINT anchorPx);
    bool  IsInPlatter        (POINT px) const;
    InspectorViewModel::Point  ToViewPoint (POINT px) const;

    const CassoTheme                                                                           * m_theme          = nullptr;
    IDiskInspectorHost                                                                         * m_host           = nullptr;
    NullDiskInspectorHost                                                                        m_nullHost;
    int                                                                                          m_drive          = 0;
    uint64_t                                                                                     m_pendingRequest = 0;
    DiskAnalysis                                                                                 m_analysis;
    InspectorViewModel                                                                           m_model;
    AnalysisScheduler                                                                            m_scheduler;
    PlatterRenderer                                                                              m_platter;
    bool                                                                                         m_isPlatterReady = false;
    bool                                                                                         m_hasDisk        = false;
    bool                                                                                         m_isDragging     = false;
    bool                                                                                         m_isSplitting    = false;
    POINT                                                                                        m_dragFrom       = {};
    int64_t                                                                                      m_lastClickMs    = 0;
    double                                                                                       m_splitFraction  = 0.55;
    std::array<std::shared_ptr<const PlatterRenderer::Levels>, DiskImage::kQuarterTrackCount>    m_levels;
    std::set<int>                                                                                m_changedSlots;
    DxuiDpiScaler                                                                                m_scaler;
    RECT                                                                                         m_toolbarPx      = {};
    RECT                                                                                         m_platterPx      = {};
    RECT                                                                                         m_splitterPx     = {};
    RECT                                                                                         m_rightPx        = {};

    DxuiTabStrip *                           m_driveTabs      = nullptr;
    DxuiTabStrip *                           m_trackTabs      = nullptr;
    DxuiButton *                             m_zoomOut        = nullptr;
    DxuiButton *                             m_zoomIn         = nullptr;
    DxuiButton *                             m_fit            = nullptr;
    DxuiLabel *                              m_zoomLabel      = nullptr;
    DxuiLabel *                              m_diskLabel      = nullptr;
    DxuiLabel *                              m_summaryLabel   = nullptr;
    DxuiLabel *                              m_trackLabel     = nullptr;
    DxuiCustomVisual *                       m_platterVisual  = nullptr;
};
