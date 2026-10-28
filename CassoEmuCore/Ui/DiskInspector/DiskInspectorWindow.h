#pragma once

#include "Pch.h"

#include "Config/DiskInspectorPrefs.h"
#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Seams/Win32Clipboard.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/DiskInspector/AnalysisScheduler.h"
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
class DifferencesTab;
class FindBar;
class EditBar;





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
    FindBar,
    FindResults,
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
    HRESULT  ShowDrive   (int drive);
    HRESULT  RenderFrame ();
    int      GetDrive    () const { return m_drive; }

    //  Before something discards the pending edits (FR-108): true when there
    //  are none, or the user confirms that they are discarded.
    bool     ConfirmDiscardEdits (const std::wstring & action);

    //  The window's state to save, and the saved state applied to a new
    //  window before ShowDrive (FR-080). The host sets `isOpen`.
    void     GetPrefs    (DiskInspectorPrefs & inOut) const;
    void     ApplyPrefs  (const DiskInspectorPrefs & prefs);

    //  The walkthrough from its first step (FR-153).
    void     OpenWalkthrough () { m_walkthrough.Open(); }

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
    //  The toolbar's controls after the file name, in FR-007's order.
    enum class ToolId
    {
        Modes,
        Range,
        Follow,
        GoToHead,
        GoTo,
        Find,
        Compare,
        Export,
        Decode,
    };

    struct ToolItem
    {
        ToolId  id      = ToolId::Modes;
        int     widthPx = 0;
        RECT    rect    = {};
    };

    vector<ToolItem>  GetToolItems  (const DxuiDpiScaler & scaler) const;
    static int        FlowToolItems (vector<ToolItem> & inOutItems, int left, int firstLeft, int right, int top, int rowHeight, int gap);
    void              PlaceToolItem (const ToolItem & item, const DxuiDpiScaler & scaler);
    uint32_t          GetToolSig    () const;
    bool              IsHeadShown   () const;

    void  RequestCopy      ();
    void  StartDisk        (std::shared_ptr<const DiskCopy> copy, const std::wstring & reason);
    void  TakeComparison   ();
    void  SyncSideB        ();
    void  TickHead         ();
    void  RequestWrittenTracks ();
    void  TakeWrittenTracks    (const InspectorReply & reply);
    void  SetFollowHead    (bool isOn);
    void  GoToHead         ();
    void  SelectHeadTrack  (int quarterTrack);
    void  OpenCompare      ();
    void  StopComparing    ();
    void  EndComparison    (const std::wstring & why);
    void  CheckComparedSources ();
    void  SwapSides        ();
    void  OpenBSettings    ();
    void  StepDifference   (int step);
    void  SelectDifference (const Difference & difference);
    void  PaintComparisonBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    bool  IsWindowDiskA    () const;
    bool  IsWriteProtected () const { return m_context.drive != nullptr && m_context.drive->writeProtect.Any(); }
    void  PaintNoDisk      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void  PaintHelp        (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    bool  OnWalkthroughMouse (const DxuiMouseEvent & ev);
    bool  OnWalkthroughKey   (const DxuiKeyEvent & ev);
    RECT  GetWalkthroughArea (WalkthroughArea area) const;
    void  TakeReplies      ();
    void  TakeResults      ();
    void  UpdateRings      ();
    void  UpdateControls   ();
    void  OnSelection      ();
    void  ShowTrackTab     (int tab);
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
    void  OpenFindBar        ();
    void  CloseFindBar       ();
    void  RunFind            (const SearchQuery & query);
    void  RefreshFindResults ();
    void  SyncFindMarks      ();
    std::set<int>  GetQuarterTracksOf (const vector<RecordResult> & results) const;
    void  ToggleEditing      ();
    void  UpdateEditBar      ();
    void  PasteIntoEdit      ();
    void  FollowEdits        ();
    bool  IsSectorImage      () const;
    int   GetSelectedEdit    () const;
    bool  HandleEditKey      (const DxuiKeyEvent & ev);
    bool  ConfirmEdit        (const std::wstring & title, const vector<std::wstring> & lines, const std::wstring & action);
    std::wstring  GetApplyBlock () const;
    bool  IsHistoryCurrent   (const vector<RecordWriteCount> & counts) const;
    void  ApplyEdits         ();
    void  PostApply          (const InspectorRequest & request);
    void  PauseApply         ();
    void  CancelApply        ();
    void  UndoApplied        ();
    void  RedoApplied        ();
    void  TakeApplyReply     (const InspectorReply & reply);
    void  SaveEditedCopy     ();
    bool  GetBSectorBytes    (PendingEdit::Sector & outBytes) const;
    void  UseBBytes          ();
    void  TakeSaveCopyReply  (const InspectorReply & reply);
    static bool  IsSameFile  (const std::filesystem::path & a, const std::filesystem::path & b);
    void  RebuildFindMarks   ();
    void  RepeatFind         ();
    vector<int>  GetShownDiskTabs () const;
    int          GetTabIndex      () const;
    void  OpenExport ();
    void  PaintNot35 (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    bool  IsCovered  (POINT pointPx) const;
    const FileMap *  GetFileMap () const;
    void  LayoutFileMap        (const RECT & area, const DxuiDpiScaler & scaler);
    void  OpenFilePicker       (bool isOpen);
    void  SetZoom              (double zoom);
    void  LayoutContent        (const RECT & boundsPx, const DxuiDpiScaler & scaler);
    static bool  IsZoomKey     (WPARAM vk);
    void  PaintPicker          (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  SetPickerLabel       (const std::wstring & label);
    int   GetPickerWidthPx     (const DxuiDpiScaler & scaler) const;
    void  ShowFileMap          (bool isShown);
    void  RefreshFileList      ();
    void  ChooseFile           (int file);
    void  SelectMapCell        (int cell);
    void  StepFileSector       (int step);
    int   GetSelectedMapCell   () const;
    void  SyncFileMapSelection ();
    bool  IsOverMapGrid        (POINT pointPx) const;
    double   GetMostZoom       () const;
    HRESULT  GrowToMinSize     ();
    void  ZoomMapGrid          (float notches, POINT pointPx);
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
    uint64_t                                                                                     m_requestedMediaId  = UINT64_MAX;
    DiskAnalysis                                                                                 m_analysis;
    InspectorViewModel                                                                           m_model;
    InspectorViewContext                                                                         m_context;
    std::unique_ptr<AnalysisScheduler>                                                           m_scheduler;
    ComparisonSession                                                                            m_comparison;
    ComparisonSource                                                                             m_sourceA;
    uint64_t                                                                                     m_comparisonVersion = 0;
    bool                                                                                         m_isTracksCompared  = false;
    int                                                                                          m_diffIndex         = -1;
    uint32_t                                                                                     m_toolSig           = 0;
    RECT                                                                                         m_barPx             = {};
    InspectorViewModel                                                                           m_modelB;
    InspectorViewContext                                                                         m_contextB;
    uint64_t                                                                                     m_modelBDisk        = 0;
    vector<Difference>                                                                           m_nibbleDiffs;
    vector<int>                                                                                  m_diffCells;
    const TrackAnalysis                                                                        * m_diffsOfA          = nullptr;
    const TrackAnalysis                                                                        * m_diffsOfB          = nullptr;
    double                                                                                       m_bOffset           = 0.0;
    double                                                                                       m_linkStartB        = -1.0;
    double                                                                                       m_linkSpanB         = 0.0;
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
    DxuiTabStrip                          * m_tabs               = nullptr;
    DxuiButton                            * m_legendButton       = nullptr;
    DxuiPopupHost                         * m_legendPopup        = nullptr;
    bool                                    m_isLegendPinned     = false;
    int64_t                                 m_legendClosedMs     = 0;
    bool                                    m_isDiskGroup        = false;
    bool                                    m_isHintShown        = false;
    RECT                                    m_stripAreaPx        = {};
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
    DxuiCheckbox                          * m_followCheck        = nullptr;
    DxuiButton                            * m_goToHead           = nullptr;
    FollowHead                              m_followHead;
    bool                                    m_isHeadSelecting    = false;
    int                                     m_flownQuarterTrack  = -1;
    int                                     m_flownSector        = -1;
    bool                                    m_isFilesDefaulted   = false;
    bool                                    m_isFilesSaved       = false;
    InspectorWalkthrough                    m_walkthrough;
    RECT                                    m_helpPx             = {};
    std::shared_ptr<const DriveHeadState>   m_headState;
    uint64_t                                m_countsGeneration   = 0;
    vector<uint64_t>                        m_writeCounts;
    std::set<int>                           m_writtenSlots;
    uint64_t                                m_trackRequest       = 0;
    int64_t                                 m_trackRequestMs     = 0;
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
    FindSession                             m_find;
    FindBar                               * m_findBar            = nullptr;
    InspectorTableView                    * m_findResults        = nullptr;
    bool                                    m_isFindOpen         = false;
    FindMarks                               m_findMarks;
    EditController                          m_edits;
    EditBar                               * m_editBar            = nullptr;
    std::wstring                            m_editNote;
    std::wstring                            m_statusNote;
    int64_t                                 m_sourceCheckMs      = 0;
    int64_t                                 m_editSig            = 0;

    //  One apply's tracks before and after, the guest write counts it left,
    //  and those an undo of it left (FR-107).
    struct AppliedEditRecord
    {
        vector<std::shared_ptr<const TrackCopy>>  before;
        vector<std::shared_ptr<const TrackCopy>>  after;
        vector<RecordWriteCount>                  countsAfter;
        vector<RecordWriteCount>                  countsUndone;
        std::string                               savedTo;
    };

    vector<AppliedEditRecord>               m_history;
    int                                     m_historyAt          = 0;
    InspectorRequest                        m_lastApply;
    uint64_t                                m_applyRequest       = 0;
    uint64_t                                m_saveCopyRequest    = 0;
    bool                                    m_isApplyWaiting     = false;
    bool                                    m_isApplyPaused      = false;
    bool                                    m_isFirstApplyDone   = false;
    std::set<int>                           m_appliedSlots;
    GoToKind                                m_goToKind           = GoToKind::Track;
    vector<std::shared_ptr<DxuiCommand>>    m_menuCommands;
    PlatterView                           * m_platterView        = nullptr;
    TrackHeaderView                       * m_headerView         = nullptr;
    TrackStripView                        * m_stripView          = nullptr;
    TrackStripView                        * m_stripB             = nullptr;
    SectorRowView                         * m_sectorRow          = nullptr;
    SectorByteView                        * m_byteView           = nullptr;
    NibblesTab                            * m_nibblesTab         = nullptr;
    DxuiButton                            * m_decodeButton       = nullptr;
    DxuiCheckbox                          * m_alignmentCheck     = nullptr;
    DxuiCheckbox                          * m_filesCheck         = nullptr;
    DxuiCheckbox                          * m_readsCheck         = nullptr;
    DxuiButton                            * m_topDown            = nullptr;
    DxuiSegmentedControl                  * m_modeTabs           = nullptr;
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
    DxuiLabel                             * m_mapStepLabel       = nullptr;
    DxuiButton                            * m_filePicker         = nullptr;
    bool                                    m_isPickerOpen       = true;
    std::wstring                            m_pickerLabel        = L"Choose a file";
    float                                   m_pickerTextPx       = 0.0f;
    bool                                    m_isLayoutStale      = false;
    int                                     m_sectorRows         = 1;
    DxuiLabel                             * m_statusText         = nullptr;
    RECT                                    m_statusPx           = {};
    DxuiZoomControl                       * m_zoomControl        = nullptr;
    DxuiDpiScaler                           m_hostScaler;
    double                                  m_zoom               = 1.0;
    DxuiScrollPanel                       * m_mapScroll          = nullptr;
    int                                     m_revealedCell       = -1;
    RECT                                    m_tabGapPx           = {};
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
