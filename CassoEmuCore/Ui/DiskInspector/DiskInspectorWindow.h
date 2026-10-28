#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Config/DiskInspectorPrefs.h"
#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Seams/Win32Clipboard.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DiskInspector/ComparisonSession.h"
#include "Ui/DiskInspector/FindSession.h"
#include "Ui/DiskInspector/FollowHead.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"
#include "Ui/DiskInspector/InspectorTables.h"
#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/InspectorWalkthrough.h"
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
class PlatterLegendView;
class FluxTimingTab;
class FileMapGridView;
class ComparisonPart;
class EditingPart;
class FindPart;
class HostLink;
class InspectorMenus;
class InspectorToolbar;
class StatusBand;
class FilesView;
class DifferencesTab;
class FindBar;
class EditBar;





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

class DiskInspectorWindow : public DxuiWindow, private IInspectorComponentHost
{
public:
    //  Wide enough for the track tabs and the disk tabs on one row, with
    //  Differences among them while comparing.
    static constexpr int     kOpeningWidthDip  = 1280;
    static constexpr int     kOpeningHeightDip = 800;
    static constexpr int     kMinWidthDip      = 960;
    static constexpr int     kMinHeightDip     = 720;
    static constexpr double  kZoomStep         = 1.25;

    DiskInspectorWindow  ();
    ~DiskInspectorWindow () override;

    HRESULT  Create      (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDiskInspectorHost * host, bool activate);
    HRESULT  ShowDrive   (int drive) override;
    HRESULT  RenderFrame ();
    int      GetDrive    () const { return m_session.drive; }

    //  Before something discards the pending edits (FR-108): true when there
    //  are none, or the user confirms that they are discarded.
    bool     ConfirmDiscardEdits (const std::wstring & action) override;

    //  The window's state to save, and the saved state applied to a new
    //  window before ShowDrive (FR-080). The host sets `isOpen`.
    void     GetPrefs    (DiskInspectorPrefs & inOut) const;
    void     ApplyPrefs  (const DiskInspectorPrefs & prefs);

    //  The walkthrough from its first step (FR-153).
    void     OpenWalkthrough () override { m_walkthrough.Open(); }

    void     Layout      (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void     Paint       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse     (const DxuiMouseEvent & ev) override;
    bool     OnKey       (const DxuiKeyEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

protected:
    void  OnCreate      () override;
    void  OnWindowClose () override;

    DxuiMessageResult  OnSysCommand (UINT command) override;

private:
    //  IInspectorComponentHost, through which the components call the window.
    IInspectorComponentHost & GetHost () { return *this; }
    DxuiPanel          & GetControlParent () override { return *this; }
    HWND                 GetOwnerHwnd     () const override { return GetHwnd(); }
    DxuiHwndSource     * GetPopupHost     () const override { return DxuiWindow::GetPopupHost(); }
    IDxuiTextRenderer  * GetTextRenderer  () const override { return DxuiWindow::GetTextRenderer(); }
    void                 Relayout         () override;
    void                 SetStatusNote    (const std::wstring & note) override;
    bool                 Confirm          (const std::wstring & title, const vector<std::wstring> & lines, const std::wstring & action) override;
    void                 TakeTracks       (const InspectorReply & reply) override;
    void                 RequestCopy      () override;
    void                 StartDisk        (std::shared_ptr<const DiskCopy> copy, const std::wstring & reason) override;
    void                 NotifyDiskStarting    (const DiskChange & change) override;
    void                 NotifyDiskStarted     (const DiskChange & change) override;
    void                 NotifyAnalysisChanged () override;
    bool                 OfferReply            (const InspectorReply & reply) override;
    void                 SetWindowZoom    (double zoom) override { SetZoom (zoom); }

    void          BuildCommands ();
    static bool   RunCommand    (const InspectorCommands::Command & command);
    void          UpdateTitle   ();



    void  PaintNoDisk      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    bool  OnWalkthroughMouse (const DxuiMouseEvent & ev);
    bool  OnWalkthroughKey   (const DxuiKeyEvent & ev);
    RECT  GetWalkthroughArea (WalkthroughArea area) const;
    void  UpdateRings      ();
    void  UpdateControls   ();
    void  OnSelection      () override;
    void  ShowTrackTab     (int tab) override;
    void  ShowTabContent   ();
    void  ShowLegend       (bool isPinned);
    void  HideLegend       ();
    void  TrackLegendHover (POINT pointPx);
    bool  IsImageShown     () const;
    void  LayoutPlatterControls (const RECT & area, const DxuiDpiScaler & scaler);
    int   GetPlatterControlsWidthPx (const DxuiDpiScaler & scaler) const;
    int   LayoutTabs       (const RECT & area, const DxuiDpiScaler & scaler);
    static int  GetLabelWidthPx (LPCWSTR label, int padDip, const DxuiDpiScaler & scaler);
    static bool  IsStripKey (WPARAM vk);
    KeyTarget    GetKeyTarget (POINT pointPx) const;
    void  SelectAll       ();
    void  ExtendSelection (WPARAM vk);
    void  Copy            ();
    void  CopySector      ();
    vector<int>  GetShownDiskTabs () const;
    int          GetTabIndex      () const;
    void  OpenExport ();
    void  PaintNot35 (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    bool  IsCovered  (POINT pointPx) const;
    void  SetZoom              (double zoom);
    void  LayoutContent        (const RECT & boundsPx, const DxuiDpiScaler & scaler);
    static bool  IsZoomKey     (WPARAM vk);
    double   GetMostZoom       () const;
    HRESULT  GrowToMinSize     ();
    void  ShowContextMenu (POINT pointPx);
    std::wstring  GetButtonTip       (POINT pointPx, RECT & outAnchorPx) const;
    void          StepKeyTarget      (int step);
    RECT          GetKeyTargetBounds () const;
    void  OpenGoTo  ();
    void  ShowDiskTab      (int tab) override;
    void  RefreshTables    ();
    void  SyncTables       ();
    void  SelectFromRow    (const TableRow & row);
    void  OpenDecodeSettings ();
    void  ApplySettings    (const DecodeSettings & settings);
    void  UpdateTooltip    (POINT pointPx);
    void  StepSector       (int delta, bool isToEnd);
    static bool   IsInside    (const RECT & rect, POINT pointPx);

    InspectorSession                                                                             m_session;
    std::unique_ptr<FilesView>                                                                   m_files;
    std::unique_ptr<EditingPart>                                                                 m_editing;
    std::unique_ptr<ComparisonPart>                                                              m_comparisonPart;
    std::unique_ptr<FindPart>                                                                    m_findPart;
    std::unique_ptr<HostLink>                                                                    m_hostLink;
    std::unique_ptr<InspectorMenus>                                                              m_menus;
    std::unique_ptr<InspectorToolbar>                                                            m_toolbar;
    std::unique_ptr<StatusBand>                                                                  m_statusBand;
    vector<IInspectorComponent *>                                                                m_components;
    InspectorCommands                                                                            m_commands;
    std::wstring                                                                                 m_title;
    double                                                                                       m_lastTilt          = 0.5235987755982988;
    bool                                                                                         m_isTracksCompared  = false;
    DxuiTooltip                                                                                  m_tooltip;
    bool                                                                                         m_isSplitting       = false;
    double                                                                                       m_splitFraction     = 0.5;
    RECT                                                                                         m_toolbarPx         = {};
    RECT                                                                                         m_splitterPx        = {};
    std::wstring                                                                                 m_tooltipText;

    DxuiTabStrip                          * m_tabs               = nullptr;
    DxuiButton                            * m_legendButton       = nullptr;
    DxuiPopupHost                         * m_legendPopup        = nullptr;
    bool                                    m_isLegendPinned     = false;
    int64_t                                 m_legendClosedMs     = 0;
    bool                                    m_isHintShown        = false;
    RECT                                    m_stripAreaPx        = {};
    DxuiButton                            * m_zoomOut            = nullptr;
    DxuiButton                            * m_zoomIn             = nullptr;
    DxuiButton                            * m_fit                = nullptr;
    DxuiLabel                             * m_zoomLabel          = nullptr;
    DxuiLabel                             * m_hintLabel          = nullptr;
    DxuiButton                            * m_copySector         = nullptr;
    int                                     m_flownQuarterTrack  = -1;
    int                                     m_flownSector        = -1;
    bool                                    m_isFilesDefaulted   = false;
    bool                                    m_isFilesSaved       = false;
    InspectorWalkthrough                    m_walkthrough;
    DxuiCheckbox                          * m_diffsCheck         = nullptr;
    DifferencesTab                        * m_diffsTab           = nullptr;
    DxuiButton                            * m_stripOut           = nullptr;
    DxuiButton                            * m_stripIn            = nullptr;
    DxuiButton                            * m_stripWhole         = nullptr;
    DxuiLabel                             * m_stripReadout       = nullptr;
    DxuiLabel                             * m_stripHint          = nullptr;
    bool                                    m_isFocusShown       = false;
    InspectorTableView                    * m_findResults        = nullptr;


    GoToKind                                m_goToKind           = GoToKind::Track;
    vector<std::shared_ptr<DxuiCommand>>    m_menuCommands;
    PlatterView                           * m_platterView        = nullptr;
    TrackHeaderView                       * m_headerView         = nullptr;
    TrackStripView                        * m_stripView          = nullptr;
    TrackStripView                        * m_stripB             = nullptr;
    SectorRowView                         * m_sectorRow          = nullptr;
    SectorByteView                        * m_byteView           = nullptr;
    NibblesTab                            * m_nibblesTab         = nullptr;
    DxuiCheckbox                          * m_alignmentCheck     = nullptr;
    DxuiCheckbox                          * m_filesCheck         = nullptr;
    DxuiCheckbox                          * m_readsCheck         = nullptr;
    DxuiButton                            * m_topDown            = nullptr;
    PlatterLegendView                     * m_legend             = nullptr;
    InspectorTableView                    * m_tracksTab          = nullptr;
    FindingsTab                           * m_findingsTab        = nullptr;
    InspectorTableView                    * m_fieldsTab          = nullptr;
    InspectorTableView                    * m_imageTab           = nullptr;
    int                                     m_sectorRows         = 1;
    double                                  m_zoom               = 1.0;
    RECT                                    m_tabGapPx           = {};
    RECT                                    m_platterAreaPx      = {};
    RECT                                    m_trackAreaPx        = {};
    RECT                                    m_diskContentPx      = {};
    FluxTimingTab                         * m_fluxTab            = nullptr;
    int                                     m_fieldsOf           = -1;
    const TrackAnalysis                   * m_fieldsTrack        = nullptr;
};
