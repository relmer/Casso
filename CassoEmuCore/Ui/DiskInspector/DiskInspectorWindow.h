#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Seams/Win32Clipboard.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DiskInspector/AnalysisScheduler.h"
#include "Ui/DiskInspector/ComparisonSession.h"
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
class PlatterLegendView;
class FluxTimingTab;
class FileMapGridView;
class DifferencesTab;





//  Which view the keys act on: the one pressed last (FR-029, FR-034, FR-043).
enum class KeyTarget
{
    Platter,
    Strip,
    SectorRow,
    SectorData,
    Nibbles,
    Fields,
    FluxTiming,
    Tracks,
    Findings,
    Image,
    FileList,
    Differences,
};





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
    void  StartDisk        (std::shared_ptr<const DiskCopy> copy, const std::wstring & reason);
    void  TakeComparison   ();
    void  OpenCompare      ();
    void  StopComparing    ();
    void  SwapSides        ();
    void  OpenBSettings    ();
    void  StepDifference   (int step);
    void  SelectDifference (const Difference & difference);
    void  PaintComparisonBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    bool  IsWindowDiskA    () const;
    void  TakeReplies      ();
    void  TakeResults      ();
    void  UpdateRings      ();
    void  UpdateControls   ();
    void  OnSelection      ();
    void  ShowTrackTab     (int tab);
    static bool  IsStripKey (WPARAM vk);
    KeyTarget    GetKeyTarget (POINT pointPx) const;
    void  SelectAll       ();
    void  ExtendSelection (WPARAM vk);
    void  Copy            ();
    void  CopySector      ();
    void  OpenFind  ();
    void  OpenExport ();
    void  PaintNot35 (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    bool  IsCovered  (POINT pointPx) const;
    const FileMap *  GetFileMap () const;
    void  LayoutFileMap        (const RECT & area, const DxuiDpiScaler & scaler);
    void  ShowFileMap          (bool isShown);
    void  RefreshFileList      ();
    void  ChooseFile           (int file);
    void  SelectMapCell        (int cell);
    void  StepFileSector       (int step);
    int   GetSelectedMapCell   () const;
    void  SyncFileMapSelection ();
    void  CopyFileMap          ();
    void  ShowContextMenu (POINT pointPx);
    std::wstring  GetButtonTip       (POINT pointPx, RECT & outAnchorPx) const;
    void          StepKeyTarget      (int step);
    RECT          GetKeyTargetBounds () const;
    void  OpenGoTo  ();
    void  StepFind  (int step);
    void  SelectHit (const SearchHit & hit);
    void  ShowDiskTab      (int tab);
    void  RefreshTables    ();
    void  SyncTables       ();
    void  SelectFromRow    (const TableRow & row);
    void  OpenDecodeSettings ();
    void  ApplySettings    (const DecodeSettings & settings);
    void  StepRange        (int delta);
    void  UpdateTooltip    (POINT pointPx);
    void  PaintToolbar     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  StepSector       (int delta, bool isToEnd);
    std::wstring  GetFileName () const;
    static bool   IsInside    (const RECT & rect, POINT pointPx);

    const CassoTheme                                                                           * m_theme             = nullptr;
    IDiskInspectorHost                                                                         * m_host              = nullptr;
    NullDiskInspectorHost                                                                        m_nullHost;
    int                                                                                          m_drive             = 0;
    uint64_t                                                                                     m_pendingRequest    = 0;
    DiskAnalysis                                                                                 m_analysis;
    InspectorViewModel                                                                           m_model;
    InspectorViewContext                                                                         m_context;
    std::unique_ptr<AnalysisScheduler>                                                           m_scheduler;
    ComparisonSession                                                                            m_comparison;
    ComparisonSource                                                                             m_sourceA;
    uint64_t                                                                                     m_comparisonVersion = 0;
    bool                                                                                         m_isTracksCompared  = false;
    int                                                                                          m_diffIndex         = -1;
    RECT                                                                                         m_barPx             = {};
    DxuiTooltip                                                                                  m_tooltip;
    std::array<std::shared_ptr<const PlatterRenderer::Levels>, DiskImage::kQuarterTrackCount>    m_levels;
    std::array<std::shared_ptr<const PlatterRenderer::Levels>, DiskImage::kQuarterTrackCount>    m_timingLevels;
    std::set<int>                                                                                m_changedSlots;
    bool                                                                                         m_isSplitting       = false;
    double                                                                                       m_splitFraction     = 0.5;
    int                                                                                          m_trackTab          = 0;
    DxuiDpiScaler                                                                                m_scaler;
    RECT                                                                                         m_toolbarPx         = {};
    RECT                                                                                         m_fileNamePx        = {};
    RECT                                                                                         m_chipsPx           = {};
    RECT                                                                                         m_splitterPx        = {};
    std::wstring                                                                                 m_tooltipText;

    DxuiTabStrip                          * m_driveTabs          = nullptr;
    DxuiTabStrip                          * m_trackTabs          = nullptr;
    DxuiButton                            * m_zoomOut            = nullptr;
    DxuiButton                            * m_zoomIn             = nullptr;
    DxuiButton                            * m_fit                = nullptr;
    DxuiLabel                             * m_zoomLabel          = nullptr;
    DxuiLabel                             * m_hintLabel          = nullptr;
    DxuiButton                            * m_exportButton       = nullptr;
    DxuiButton                            * m_goToButton         = nullptr;
    DxuiButton                            * m_findButton         = nullptr;
    DxuiButton                            * m_copySector         = nullptr;
    DxuiButton                            * m_compareButton      = nullptr;
    DxuiButton                            * m_prevDiff           = nullptr;
    DxuiButton                            * m_nextDiff           = nullptr;
    DxuiButton                            * m_swapButton         = nullptr;
    DxuiButton                            * m_bSettingsButton    = nullptr;
    DxuiButton                            * m_stopButton         = nullptr;
    DxuiCheckbox                          * m_diffsCheck         = nullptr;
    DifferencesTab                        * m_diffsTab           = nullptr;
    DxuiButton                            * m_stripOut           = nullptr;
    DxuiButton                            * m_stripIn            = nullptr;
    DxuiButton                            * m_stripWhole         = nullptr;
    DxuiLabel                             * m_stripReadout       = nullptr;
    DxuiLabel                             * m_stripHint          = nullptr;
    KeyTarget                               m_keyTarget          = KeyTarget::Platter;
    bool                                    m_isFocusShown       = false;
    Win32Clipboard                          m_clipboard;
    SearchQuery                             m_findQuery;
    vector<SearchHit>                       m_findHits;
    int                                     m_findIndex          = -1;
    GoToKind                                m_goToKind           = GoToKind::Track;
    vector<std::shared_ptr<DxuiCommand>>    m_menuCommands;
    PlatterView                           * m_platterView        = nullptr;
    TrackHeaderView                       * m_headerView         = nullptr;
    TrackStripView                        * m_stripView          = nullptr;
    SectorRowView                         * m_sectorRow          = nullptr;
    SectorByteView                        * m_byteView           = nullptr;
    NibblesTab                            * m_nibblesTab         = nullptr;
    DxuiTabStrip                          * m_diskTabs           = nullptr;
    DxuiButton                            * m_decodeButton       = nullptr;
    DxuiCheckbox                          * m_alignmentCheck     = nullptr;
    DxuiCheckbox                          * m_filesCheck         = nullptr;
    DxuiTabStrip                          * m_modeTabs           = nullptr;
    DxuiButton                            * m_rangeDown          = nullptr;
    DxuiButton                            * m_rangeUp            = nullptr;
    DxuiLabel                             * m_rangeLabel         = nullptr;
    PlatterLegendView                     * m_legend             = nullptr;
    int                                     m_rangeStep          = 3;
    InspectorTableView                    * m_tracksTab          = nullptr;
    FindingsTab                           * m_findingsTab        = nullptr;
    InspectorTableView                    * m_fieldsTab          = nullptr;
    InspectorTableView                    * m_imageTab           = nullptr;
    FileMapGridView                       * m_mapGrid            = nullptr;
    InspectorTableView                    * m_fileList           = nullptr;
    DxuiTabStrip                          * m_mapVolumes         = nullptr;
    DxuiCheckbox                          * m_mapDeleted         = nullptr;
    DxuiCheckbox                          * m_mapBadOnly         = nullptr;
    DxuiButton                            * m_mapPrev            = nullptr;
    DxuiButton                            * m_mapNext            = nullptr;
    DxuiButton                            * m_mapCopy            = nullptr;
    int                                     m_mapIndex           = 0;
    int                                     m_mapFile            = -1;
    int                                     m_fileSortColumn     = -1;
    bool                                    m_fileSortDescending = false;
    bool                                    m_is35               = false;
    std::wstring                            m_openError;
    RECT                                    m_platterAreaPx      = {};
    RECT                                    m_trackAreaPx        = {};
    RECT                                    m_diskContentPx      = {};
    FluxTimingTab                         * m_fluxTab            = nullptr;
    int                                     m_diskTab            = 0;
    bool                                    m_isTablesDirty      = true;
    int                                     m_fieldsOf           = -1;
    const TrackAnalysis                   * m_fieldsTrack        = nullptr;
};
