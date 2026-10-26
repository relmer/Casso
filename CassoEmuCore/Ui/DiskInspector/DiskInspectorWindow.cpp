#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorWindow.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Ui/DiskInspector/CompareDialog.h"
#include "Ui/DiskInspector/DecodeSettingsDialog.h"
#include "Ui/DiskInspector/DifferencesTab.h"
#include "Ui/DiskInspector/FindingsTab.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DurableCommit.h"
#include "Seams/Win32DiskFileIo.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Seams/Win32HostDialogs.h"
#include "Ui/DiskInspector/ExportDialog.h"
#include "Ui/DiskInspector/FileMapGridView.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/FindBar.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/DiskInspector/FluxTimingTab.h"
#include "Ui/DiskInspector/GoToDialog.h"
#include "Ui/DiskInspector/NibblesTab.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterLegendView.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/SectorByteView.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"
#include "Ui/DiskInspector/TrackStripView.h"





static constexpr LPCWSTR  s_kpszWindowTitle = L"Disk inspector";
static constexpr LPCWSTR  s_kpszClassName   = L"CassoDiskInspector";
static constexpr LPCWSTR  s_kpszFindBadHex  = L"Enter pairs of hex digits, such as D5 AA 96.";
static constexpr LPCWSTR  s_kpszFindBadText = L"Enter the text to find, in plain ASCII.";

//  A command added to the system menu: below SC_SIZE, low four bits clear.
static constexpr UINT     s_kDefaultSizeCommand = 0x0010;

static constexpr LPCWSTR  s_kpszHint        = L"Scroll to zoom, drag to pan, double-click to fit";
static constexpr LPCWSTR  s_kpszStripHint   = L"Scroll to zoom, drag to select, Shift+drag to pan, double-click for the whole track";
static constexpr LPCWSTR  s_kpszNoDisk      = L"No disk";
static constexpr LPCWSTR  s_kpszAnalyzing   = L"Analyzing";

static constexpr int      s_kToolbarDip     = 40;
static constexpr int      s_kRowDip         = 28;
static constexpr int      s_kTabsDip        = 30;
static constexpr int      s_kStripDip       = 64;
static constexpr int      s_kHintRowDip     = 20;
static constexpr int      s_kWholeTrackDip  = 104;
static constexpr int      s_kReadoutDip     = 220;
static constexpr int      s_kSectorRowDip   = 32;
static constexpr int      s_kMarginDip      = 8;
static constexpr int      s_kSplitterDip    = 6;
static constexpr int      s_kButtonDip      = 32;
static constexpr int      s_kTabWidthDip    = 96;
static constexpr int      s_kFileNameDip    = 260;
static constexpr float    s_kChipPadDip     = 8.0f;
static constexpr float    s_kChipGapDip     = 6.0f;
static constexpr float    s_kChipHeightDip  = 22.0f;
static constexpr float    s_kChipTextDip    = 12.0f;
static constexpr float    s_kAnalyzingDip   = 90.0f;
static constexpr double   s_kMinSplit       = 0.3;
static constexpr double   s_kMaxSplit       = 0.7;
static constexpr double   s_kPanStep        = 0.1;
static constexpr int      s_kMapCheckDip    = 190;
static constexpr int      s_kMapButtonDip   = 30;
static constexpr int      s_kMapStepLabelDip = 150;
static constexpr int      s_kPickerPadDip   = 8;
static constexpr int      s_kChevronDip     = 5;
static constexpr int      s_kStatusDip      = 28;
static constexpr int      s_kMapVolumeDip   = 150;
static constexpr int      s_kFitDip         = 48;
static constexpr int      s_kMinPlatterColumnDip = 420;
static constexpr int      s_kZoomLabelDip   = 40;
static constexpr int      s_kCheckLabelPadDip = 30;
static constexpr int      s_kTabLabelPadDip = 24;
static constexpr int      s_kLegendButtonDip = 80;
static constexpr int64_t  s_kLegendReopenMs = 300;

//  The tab strip's tips, in its order: the track tabs, then the disk tabs.
static constexpr LPCWSTR  s_kpszTabTips[] =
{
    L"The selected sector's 256 bytes, under the details of its address and data fields",
    L"Every nibble on the track, as the disk controller reads them",
    L"Every address field and data field on the track: its marks, sector and volume numbers, and checksum results",
    L"How fast or slow each cell runs, on a track recorded as flux",
    L"Every quarter track: what is recorded there and how well it reads",
    L"Anything unusual or damaged on the disk",
    L"Which file owns each sector of the disk's volume",
    L"What the image file itself contains",
    L"Everything that differs between disk A and disk B",
};
static constexpr double   s_kCharWidthDip   = 6.6;
static constexpr int      s_kMinTableDip    = 200;
static constexpr int      s_kDecodeButtonDip = 132;
static constexpr int      s_kExportButtonDip = 80;
static constexpr int      s_kFindButtonDip   = 60;
static constexpr int      s_kCopySectorDip   = 110;
static constexpr int      s_kCompareDip      = 122;
static constexpr int      s_kFollowDip       = 110;
static constexpr int      s_kGoToHeadDip     = 94;
static constexpr int64_t  s_kRecopyMs        = 500;
static constexpr int      s_kBarDip          = 36;
static constexpr int      s_kBarButtonDip    = 136;
static constexpr int      s_kStopButtonDip   = 116;
static constexpr int      s_kMinFileNameDip  = 200;
static constexpr int      s_kFocusGapDip    = 3;
static constexpr int      s_kModeTabDip     = 76;
static constexpr int      s_kRangeDip       = 110;
static constexpr double   s_kRanges[]       = { 0.01, 0.02, 0.03, 0.05, 0.10, 0.15, 0.20, 0.25 };

enum TrackTab
{
    kTabSectorData,
    kTabNibbles,
    kTabFields,
    kTabFluxTiming,
    kTrackTabCount,
};


enum DiskTab
{
    kTabTracks,
    kTabFindings,
    kTabFileMap,
    kTabImage,
    kTabDifferences,
    kTabFindResults,
    kDiskTabCount,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::DiskInspectorWindow
//
//  Analysis results are taken on the window's own frame, so the scheduler
//  needs no wake-up of its own.
//
////////////////////////////////////////////////////////////////////////////////

DiskInspectorWindow::DiskInspectorWindow() :
    m_scheduler (std::make_unique<AnalysisScheduler> (nullptr))
{
    m_context.analysis           = &m_analysis;
    m_context.model              = &m_model;
    m_context.onSelectionChanged = [this] () { OnSelection(); };
    m_contextB.model             = &m_modelB;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::~DiskInspectorWindow
//
////////////////////////////////////////////////////////////////////////////////

DiskInspectorWindow::~DiskInspectorWindow()
{
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::Create (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDiskInspectorHost * host, bool activate)
{
    HRESULT                   hr      = S_OK;
    DxuiWindow::CreateParams  params;
    HMENU                     menu    = nullptr;
    BOOL                      isAdded = FALSE;



    BAIL_OUT_IF (IsCreated(), S_OK);

    m_theme = theme;
    m_host  = (host != nullptr) ? host : &m_nullHost;

    params.title                    = s_kpszWindowTitle;
    params.hInstance                = hInstance;
    //  Not owned, so the main window can cover it; it opens centered over
    //  the main window.
    params.ownerHwnd                = nullptr;
    params.placementAnchorHwnd      = hwndOwner;
    params.initialSizeDip           = { kOpeningWidthDip, kOpeningHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::Standard;
    params.classNameOverride        = s_kpszClassName;
    params.createNoActivate         = !activate;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;
    params.fitToWorkArea            = true;

    //  Presented without waiting for vblank: RenderFrame runs in the main
    //  window's frame, on the thread that presents the vsynced main window, so
    //  an interval of 1 adds a second vblank wait to every frame and halves
    //  the frame rate of both windows. DWM composes at vsync, so 0 does not
    //  tear.
    params.presentSyncInterval      = 0;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    //  "Default size" on the system menu, after a separator.
    menu = GetSystemMenu (GetHwnd(), FALSE);
    CWRA (menu);

    isAdded = AppendMenuW (menu, MF_SEPARATOR, 0, nullptr) && AppendMenuW (menu, MF_STRING, s_kDefaultSizeCommand, L"Default size");
    CWRA (isAdded);

    SetTheme (m_theme);
    m_tooltip.SetTheme (*m_theme);
    Show (activate);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowDrive
//
//  Starts over on the disk in the drive: the window asks for a copy and
//  shows "Analyzing" until the copy arrives.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::ShowDrive (int drive)
{
    HRESULT  hr = S_OK;



    CBRA (drive >= 0);

    if (drive != m_drive && !ConfirmDiscardEdits (L"Switching to the other drive"))
    {
        m_driveTabs->SetSelected (m_drive);
        BAIL_OUT_IF (true, S_OK);
    }

    //  Another drive's disk ends a comparison of this one.
    if (m_comparison.IsComparing())
    {
        m_comparison.End();
        ShowDiskTab (m_diskTab == kTabDifferences ? kTabTracks : m_diskTab);
    }

    m_drive            = drive;
    m_sourceA          = { ComparisonSourceKind::DriveNow, drive };
    //  No disk has this id, and an empty drive's is 0, so an eject is seen.
    m_requestedMediaId = UINT64_MAX;
    m_host->SetHeadStateWanted (true);

    if (m_driveTabs != nullptr)
    {
        m_driveTabs->SetSelected (drive);
    }

    RequestCopy();
    UpdateControls();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RenderFrame
//
//  Once per host frame: replies from the host, then analysis results, then
//  a repaint. The palette is resolved each frame, so a theme change shows
//  without reopening (FR-077).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::RenderFrame()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!IsCreated() || !IsWindowVisible (GetHwnd()), S_OK);

    m_context.palette = DiskInspectorPalette::Resolve (*m_theme);
    m_tooltip.SetTheme (*m_theme);

    TakeReplies();
    TakeResults();
    TakeComparison();
    SyncSideB();
    TickHead();
    UpdateRings();
    UpdateControls();
    RefreshTables();

    //  While the File map tab is up, the selected file's sectors are marked
    //  in the platter and the strip as in the grid (FR-090).
    m_context.isFileHighlighted = m_isDiskGroup && m_diskTab == kTabFileMap && m_context.selectedFile >= 0;

    //  The picker's width matches its label, measured as it is drawn, and the
    //  sector row's height its row count for the track selected.
    m_isLayoutStale = m_isLayoutStale || m_sectorRow->GetRowCount (m_sectorRow->GetBounds().right - m_sectorRow->GetBounds().left, m_scaler) != m_sectorRows;

    if (m_isLayoutStale)
    {
        m_isLayoutStale = false;
        Layout (m_boundsDip, m_hostScaler);
    }

    if (m_model.IsFlying())
    {
        m_model.StepFlight (InspectorMarquee::GetClockMs());
    }

    m_tooltip.Tick     (static_cast<int64_t> (GetTickCount64()));

    //  The window's own context menu opens with an animation each frame
    //  advances, as the main window's does.
    if (GetPopupHost() != nullptr && GetPopupHost()->GetContextMenu().WantsTick())
    {
        GetPopupHost()->GetContextMenu().Tick (static_cast<int64_t> (GetTickCount64()));
    }

    m_tracksTab->Tick   (static_cast<int64_t> (GetTickCount64()));
    m_findingsTab->Tick (static_cast<int64_t> (GetTickCount64()));
    m_fieldsTab->Tick   (static_cast<int64_t> (GetTickCount64()));
    m_findBar->Tick     (static_cast<int64_t> (GetTickCount64()));
    Invalidate();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnCreate()
{
    m_driveTabs   = CreateChild<DxuiTabStrip>();
    m_platterView = CreateChild<PlatterView> (m_context);
    m_zoomOut     = CreateChild<DxuiButton> (s_kpszMinus);
    m_zoomIn      = CreateChild<DxuiButton> (L"+");
    m_fit         = CreateChild<DxuiButton> (L"Fit");
    m_zoomLabel   = CreateChild<DxuiLabel> (L"",         DxuiTextRole::Body,    DxuiTextHAlign::Left);
    m_hintLabel   = CreateChild<DxuiLabel> (s_kpszHint,  DxuiTextRole::Muted  , DxuiTextHAlign::Left);
    m_headerView  = CreateChild<TrackHeaderView> (m_context);
    m_stripView   = CreateChild<TrackStripView>  (m_context);
    m_stripB      = CreateChild<TrackStripView>  (m_contextB);
    m_stripOut    = CreateChild<DxuiButton> (s_kpszMinus);
    m_stripIn     = CreateChild<DxuiButton> (L"+");
    m_stripWhole  = CreateChild<DxuiButton> (L"Whole track");
    m_stripReadout = CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_stripHint   = CreateChild<DxuiLabel> (s_kpszStripHint, DxuiTextRole::Muted, DxuiTextHAlign::Right);
    m_sectorRow   = CreateChild<SectorRowView>   (m_context);
    m_tabs        = CreateChild<DxuiTabStrip>();
    m_byteView    = CreateChild<SectorByteView>  (m_context);
    m_nibblesTab  = CreateChild<NibblesTab>      (m_context);
    m_fieldsTab   = CreateChild<InspectorTableView> (m_context);
    m_fluxTab     = CreateChild<FluxTimingTab>   (m_context);
    m_legendButton = CreateChild<DxuiButton> (L"Legend");
    m_decodeButton = CreateChild<DxuiButton> (L"Decode settings...");
    m_exportButton = CreateChild<DxuiButton> (L"Export...");
    m_goToButton   = CreateChild<DxuiButton> (L"Go to");
    m_findButton   = CreateChild<DxuiButton> (L"Find");
    m_copySector   = CreateChild<DxuiButton> (L"Copy sector");
    m_compareButton = CreateChild<DxuiButton> (L"Compare with...");
    m_followCheck  = CreateChild<DxuiCheckbox> (L"Follow head");
    m_goToHead     = CreateChild<DxuiButton> (L"Go to head");
    m_prevDiff     = CreateChild<DxuiButton> (L"Previous difference");
    m_nextDiff     = CreateChild<DxuiButton> (L"Next difference");
    m_swapButton   = CreateChild<DxuiButton> (L"Swap A and B");
    m_bSettingsButton = CreateChild<DxuiButton> (L"B's decode settings...");
    m_stopButton   = CreateChild<DxuiButton> (L"Stop comparing");
    m_diffsCheck   = CreateChild<DxuiCheckbox> (L"Differences");
    m_alignmentCheck = CreateChild<DxuiCheckbox> (L"Alignment");
    m_filesCheck     = CreateChild<DxuiCheckbox> (L"Files");
    m_readsCheck     = CreateChild<DxuiCheckbox> (L"Reads");
    m_modeTabs     = CreateChild<DxuiTabStrip>();
    m_rangeDown    = CreateChild<DxuiButton> (s_kpszMinus);
    m_rangeUp      = CreateChild<DxuiButton> (L"+");
    m_rangeLabel   = CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Center);
    m_legend       = CreateChild<PlatterLegendView> (m_context);
    m_tracksTab   = CreateChild<InspectorTableView> (m_context);
    m_findingsTab = CreateChild<FindingsTab>     (m_context);
    m_imageTab    = CreateChild<InspectorTableView> (m_context);
    m_mapScroll   = CreateChild<DxuiScrollPanel>();
    m_mapGrid     = m_mapScroll->CreateChild<FileMapGridView>    (m_context);
    m_fileList    = m_mapScroll->CreateChild<InspectorTableView> (m_context);
    m_mapVolumes  = CreateChild<DxuiTabStrip>();
    m_mapDeleted  = CreateChild<DxuiCheckbox> (L"Show deleted files");
    m_mapBadOnly  = CreateChild<DxuiCheckbox> (L"Files touching bad sectors");
    m_mapPrev     = CreateChild<DxuiButton> (L"<");
    m_mapNext     = CreateChild<DxuiButton> (L">");
    m_mapStepLabel = CreateChild<DxuiLabel> (L"Previous/next sector in", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_filePicker   = CreateChild<DxuiButton> (L"");
    m_statusText   = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Left);
    m_zoomControl  = CreateChild<DxuiZoomControl>();
    m_mapCopy     = CreateChild<DxuiButton> (L"Copy map");
    m_diffsTab    = CreateChild<DifferencesTab> (m_context, m_comparison);
    m_findResults = CreateChild<InspectorTableView> (m_context);
    m_findBar     = CreateChild<FindBar>();
    m_editBar     = CreateChild<EditBar>();

    m_driveTabs->SetOnChange ([this] (int index) { (void) ShowDrive (index); });
    m_legendButton->SetOnClick ([this] () { if (m_isLegendPinned) { HideLegend(); } else if (static_cast<int64_t> (GetTickCount64()) - m_legendClosedMs > s_kLegendReopenMs) { ShowLegend (true); } });
    m_tabs->SetOnChange      ([this] (int index) { if (index < kTrackTabCount) { ShowTrackTab (index); } else { ShowDiskTab (GetShownDiskTabs()[static_cast<size_t> (index - kTrackTabCount)]); } });
    m_tracksTab->SetColumns  (InspectorTables::GetTrackColumns());
    m_fieldsTab->SetColumns  (InspectorTables::GetFieldColumns());
    m_imageTab->SetColumns   (InspectorTables::GetImageColumns());
    m_mapGrid->SetOnChoose   ([this] (int cell) { SelectMapCell (cell); });
    m_mapGrid->SetOnZoom     ([this] () { Layout (m_boundsDip, m_hostScaler); });
    m_fileList->SetOnSelect  ([this] (const TableRow & row) { ChooseFile (row.finding); });
    m_fileList->SetOnSort    ([this] (int column) { m_fileSortDescending = (column == m_fileSortColumn) && !m_fileSortDescending; m_fileSortColumn = column; RefreshFileList(); });
    m_mapVolumes->SetOnChange ([this] (int index) { m_mapIndex = index; m_mapFile = -1; RefreshFileList(); });
    m_mapDeleted->SetOnChange ([this] (bool) { RefreshFileList(); });
    m_mapBadOnly->SetOnChange ([this] (bool) { RefreshFileList(); });
    m_mapPrev->SetOnClick    ([this] () { StepFileSector (-1); });
    m_filePicker->SetOnClick ([this] () { OpenFilePicker (!m_isPickerOpen); });
    m_zoomControl->SetOnZoom ([this] (double zoom) { SetZoom (zoom); });
    m_mapNext->SetOnClick    ([this] () { StepFileSector (1); });
    m_mapCopy->SetOnClick    ([this] () { CopyFileMap(); });
    m_tracksTab->SetOnSelect   ([this] (const TableRow & row) { SelectFromRow (row); });
    m_findingsTab->SetOnSelect ([this] (const TableRow & row) { SelectFromRow (row); });
    m_fieldsTab->SetOnSelect   ([this] (const TableRow & row) { SelectFromRow (row); });
    m_zoomOut->SetOnClick    ([this] () { m_platterView->ZoomAboutCenter (1.0 / kZoomStep); });
    m_zoomIn->SetOnClick     ([this] () { m_platterView->ZoomAboutCenter (kZoomStep); });
    m_fit->SetOnClick        ([this] () { m_model.Fit(); });
    m_stripOut->SetOnClick   ([this] () { m_stripView->ZoomAboutCenter (1.0 / kZoomStep); });
    m_stripIn->SetOnClick    ([this] () { m_stripView->ZoomAboutCenter (kZoomStep); });
    m_stripWhole->SetOnClick ([this] () { m_stripView->ShowWholeTrack(); });
    m_decodeButton->SetOnClick ([this] () { OpenDecodeSettings(); });
    m_exportButton->SetOnClick ([this] () { OpenExport(); });
    m_goToButton->SetOnClick   ([this] () { OpenGoTo(); });
    m_findButton->SetOnClick   ([this] () { OpenFindBar(); });
    m_findResults->SetColumns  (InspectorTables::GetSearchHitColumns());
    m_context.edits = &m_edits;
    m_editBar->SetOnEdit          ([this] () { ToggleEditing(); });
    m_editBar->SetOnChecksum      ([this] (bool isOn) { m_edits.SetChecksumMode (GetSelectedEdit(), isOn ? ChecksumMode::Recompute : ChecksumMode::KeepStored); });
    m_editBar->SetOnDiscardSector ([this] () { m_edits.DiscardSector (GetSelectedEdit()); });
    m_editBar->SetOnDiscard       ([this] () { m_edits.DiscardAll(); });
    m_editBar->SetOnApply         ([this] () { ApplyEdits(); });
    m_editBar->SetOnSaveCopy      ([this] () { SaveEditedCopy(); });
    m_editBar->SetOnUseB          ([this] () { UseBBytes(); });
    m_editBar->SetOnPauseApply    ([this] () { PauseApply(); });
    m_editBar->SetOnCancelApply   ([this] () { CancelApply(); });
    m_editBar->SetOnUndoApplied   ([this] () { UndoApplied(); });
    m_editBar->SetOnRedoApplied   ([this] () { RedoApplied(); });
    m_findResults->SetOnSelect ([this] (const TableRow & row) { if (m_find.Choose (row.finding)) { SelectHit (m_find.GetHits()[row.finding]); m_findBar->SetResult (row.finding, m_find.GetCount()); } });
    m_findBar->Init            (GetHwnd(), GetTextRenderer(), GetPopupHost(), m_theme);
    m_findBar->SetOnSearch     ([this] (const SearchQuery & query) { RunFind (query); });
    m_findBar->SetOnStep       ([this] (int step) { StepFind (step); });
    m_findBar->SetOnList       ([this] () { ShowDiskTab (kTabFindResults); });
    m_findBar->SetOnClose      ([this] () { CloseFindBar(); });
    m_findBar->SetVisible      (false);
    m_find.GetQuery().isWholeDisk = true;
    m_copySector->SetOnClick   ([this] () { CopySector(); });
    m_compareButton->SetOnClick   ([this] () { OpenCompare(); });
    m_followCheck->SetOnChange    ([this] (bool isChecked) { SetFollowHead (isChecked); });
    m_goToHead->SetOnClick        ([this] () { GoToHead(); });
    m_prevDiff->SetOnClick        ([this] () { StepDifference (-1); });
    m_nextDiff->SetOnClick        ([this] () { StepDifference (1); });
    m_swapButton->SetOnClick      ([this] () { SwapSides(); });
    m_bSettingsButton->SetOnClick ([this] () { OpenBSettings(); });
    m_stopButton->SetOnClick      ([this] () { StopComparing(); });
    m_diffsCheck->SetOnChange     ([this] (bool isChecked) { m_context.isDiffsOverlay = isChecked; });
    m_stripB->SetSide (ComparisonSession::kSideB);
    m_diffsTab->SetOnSelect       ([this] (const TableRow & row) { if (const Difference * d = m_diffsTab->GetDifference (row)) { m_diffIndex = row.finding; SelectDifference (*d); } });
    m_alignmentCheck->SetOnChange ([this] (bool isChecked) { m_platterView->SetAlignmentShown (isChecked); });
    m_filesCheck->SetOnChange     ([this] (bool isChecked) { m_context.isFilesOverlay = isChecked; });
    m_readsCheck->SetOnChange     ([this] (bool isChecked) { m_context.isReadsOverlay = isChecked; });
    m_modeTabs->SetOnChange  ([this] (int index) { m_context.isTimingMode = (index == 1); });
    m_rangeDown->SetOnClick  ([this] () { StepRange (-1); });
    m_rangeUp->SetOnClick    ([this] () { StepRange (1); });

    m_tooltip.SetPopupHost (GetPopupHost());

    //  The tips read out what the pointer rests on, so they stay while it
    //  does rather than dismissing themselves.
    m_tooltip.SetHasLifetime (false);

    m_diskTab = kTabTracks;
    ShowTrackTab (kTabSectorData);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnSysCommand
//
//  On "Default size", the window is returned to the size it opens at,
//  centered over the main window.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DiskInspectorWindow::OnSysCommand (UINT command)
{
    HRESULT            hr     = S_OK;
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    BAIL_OUT_IF (command != s_kDefaultSizeCommand || GetPopupHost() == nullptr, S_OK);

    result = DxuiMessageResult::Handled;
    hr     = GetPopupHost()->RestoreInitialPlacement();
    CHR (hr);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnWindowClose
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnWindowClose()
{
    //  The window stays open when the user cancels (FR-108).
    if (ConfirmDiscardEdits (L"Closing the disk inspector"))
    {
        m_tooltip.HideImmediate();
        HideLegend();
        m_host->SetHeadStateWanted (false);
        Hide();
        m_host->OnInspectorClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Layout
//
//  A status band along the bottom at the window's own scale, with the hint
//  for the view under the pointer and the zoom control, and the rest at the
//  zoom: every size and font multiplied by it (FR-154).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    DxuiDpiScaler  zoomed;
    int            status  = scaler.ToPx (s_kStatusDip);
    int            pad     = scaler.ToPx (s_kMarginDip);
    int            zoomW   = scaler.ToPx (DxuiZoomControl::kWidthDip);
    RECT           content = { boundsDip.left, boundsDip.top, boundsDip.right, boundsDip.bottom - status };



    m_hostScaler = scaler;
    zoomed.SetDpi (static_cast<UINT> (std::lround (scaler.GetDpi() * m_zoom)));

    SetBounds (boundsDip);
    m_statusPx = { boundsDip.left, content.bottom, boundsDip.right, boundsDip.bottom };
    m_helpPx   = { boundsDip.right - zoomW - 2 * pad - (status - 6), content.bottom + 3, boundsDip.right - zoomW - 2 * pad, boundsDip.bottom - 3 };
    m_statusText->Layout  ({ boundsDip.left + pad, content.bottom, m_helpPx.left - pad, boundsDip.bottom }, scaler);
    m_zoomControl->Layout ({ boundsDip.right - zoomW - pad, content.bottom + 1, boundsDip.right - pad, boundsDip.bottom - 1 }, scaler);
    LayoutContent (content, zoomed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SetZoom
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SetZoom (double zoom)
{
    double  most = std::clamp (GetMostZoom(), DxuiZoomControl::kMinZoom, DxuiZoomControl::kMaxZoom);



    m_zoom = std::clamp (zoom, DxuiZoomControl::kMinZoom, most);
    m_zoomControl->SetZoom (m_zoom);

    //  The layout fits in the window's minimum size times the zoom; a smaller
    //  window is enlarged to it, and the user cannot resize it smaller.
    SetMinSizeDip ({ static_cast<LONG> (std::lround (kMinWidthDip * std::max (1.0, m_zoom))), static_cast<LONG> (std::lround (kMinHeightDip * std::max (1.0, m_zoom))) });
    GrowToMinSize();

    Layout (m_boundsDip, m_hostScaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetMostZoom
//
//  The largest zoom whose layout fits the work area of the window's
//  monitor: the window's minimum size times the zoom, in DIPs.
//
////////////////////////////////////////////////////////////////////////////////

double DiskInspectorWindow::GetMostZoom() const
{
    HRESULT      hr      = S_OK;
    double       most    = DxuiZoomControl::kMaxZoom;
    MONITORINFO  monitor = { sizeof (monitor) };
    BOOL         isRead  = FALSE;
    double       scale   = static_cast<double> (GetDpi()) / USER_DEFAULT_SCREEN_DPI;



    BAIL_OUT_IF (GetHwnd() == nullptr, S_OK);

    isRead = GetMonitorInfoW (MonitorFromWindow (GetHwnd(), MONITOR_DEFAULTTONEAREST), &monitor);
    CWRA (isRead);

    most = std::min ((monitor.rcWork.right - monitor.rcWork.left) / scale / kMinWidthDip, (monitor.rcWork.bottom - monitor.rcWork.top) / scale / kMinHeightDip);

Error:
    return most;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GrowToMinSize
//
//  A window smaller than its minimum size grows to it, moved back onto the
//  work area of its monitor where it would run off it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::GrowToMinSize()
{
    HRESULT      hr       = S_OK;
    MONITORINFO  monitor  = { sizeof (monitor) };
    RECT         windowPx = {};
    BOOL         isRead   = FALSE;
    BOOL         isPlaced = FALSE;
    int          widthPx  = 0;
    int          heightPx = 0;
    int          x        = 0;
    int          y        = 0;



    BAIL_OUT_IF (GetHwnd() == nullptr || ::IsZoomed (GetHwnd()), S_OK);

    isRead = GetWindowRect (GetHwnd(), &windowPx) && GetMonitorInfoW (MonitorFromWindow (GetHwnd(), MONITOR_DEFAULTTONEAREST), &monitor);
    CWRA (isRead);

    widthPx  = std::max (static_cast<int> (windowPx.right - windowPx.left), MulDiv (static_cast<int> (std::lround (kMinWidthDip  * std::max (1.0, m_zoom))), static_cast<int> (GetDpi()), USER_DEFAULT_SCREEN_DPI));
    heightPx = std::max (static_cast<int> (windowPx.bottom - windowPx.top), MulDiv (static_cast<int> (std::lround (kMinHeightDip * std::max (1.0, m_zoom))), static_cast<int> (GetDpi()), USER_DEFAULT_SCREEN_DPI));

    BAIL_OUT_IF (widthPx == windowPx.right - windowPx.left && heightPx == windowPx.bottom - windowPx.top, S_OK);

    x = std::max (static_cast<int> (monitor.rcWork.left), std::min (static_cast<int> (windowPx.left), static_cast<int> (monitor.rcWork.right)  - widthPx));
    y = std::max (static_cast<int> (monitor.rcWork.top),  std::min (static_cast<int> (windowPx.top),  static_cast<int> (monitor.rcWork.bottom) - heightPx));

    isPlaced = SetWindowPos (GetHwnd(), nullptr, x, y, widthPx, heightPx, SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (isPlaced);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsZoomKey
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsZoomKey (WPARAM vk)
{
    return vk == '0' || vk == VK_NUMPAD0 || vk == VK_OEM_PLUS || vk == VK_ADD || vk == VK_OEM_MINUS || vk == VK_SUBTRACT;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::LayoutContent
//
//  A toolbar across the top. Under it the platter column, the platter as
//  large as the column allows with its controls on one row beneath it; past
//  the splitter the track column: header, sector row, one tab strip with the
//  track tabs then the disk tabs, and the chosen tab's content. Along the
//  bottom, across both columns, the strip with its controls and hint.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::LayoutContent (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int                        margin    = scaler.ToPx (s_kMarginDip);
    int                        row       = scaler.ToPx (s_kRowDip);
    int                        tab       = scaler.ToPx (s_kTabWidthDip);
    int                        button    = scaler.ToPx (s_kButtonDip);
    int                        width     = boundsDip.right - boundsDip.left;
    int                        splitX    = boundsDip.left + static_cast<int> (width * m_splitFraction);
    int                        drives    = std::max (1, m_host != nullptr ? m_host->GetDriveCount() : 1);
    int                        toolbar   = scaler.ToPx (s_kToolbarDip);
    int                        rowBottom = boundsDip.top + toolbar;
    int                        nameLeft  = boundsDip.left + 2 * margin + drives * tab;
    vector<ToolItem>           items     = GetToolItems (scaler);
    int                        rows      = FlowToolItems (items, boundsDip.left + margin, nameLeft + scaler.ToPx (s_kMinFileNameDip) + margin, boundsDip.right - margin, boundsDip.top, toolbar, margin / 2);
    int                        bar       = m_comparison.IsComparing() ? scaler.ToPx (s_kBarDip) : 0;
    int                        find      = m_isFindOpen ? FindBar::GetHeightPx (scaler) : 0;
    int                        top       = boundsDip.top + rows * toolbar + bar + find;
    int                        ctlLeft   = static_cast<int> (boundsDip.right) - margin;
    int                        strip     = scaler.ToPx (s_kStripDip);
    int                        band      = 2 * margin + strip + (m_comparison.IsComparing() ? strip + margin / 2 : 0) + margin / 2 + row;
    int                        bandTop   = std::max (top, static_cast<int> (boundsDip.bottom) - band);
    int                        x         = boundsDip.left + margin;
    int                        column    = splitX - boundsDip.left - 2 * margin;
    int                        areaH     = std::max (0, bandTop - top - 2 * margin - row);
    int                        side      = std::max (0, std::min (column, areaH));
    int                        y         = 0;
    int                        i         = 0;
    int                        header    = scaler.ToPx (TrackHeaderView::kLineDip * TrackHeaderView::kLines);
    int                        rowsH     = 0;
    RECT                       right     = {};
    RECT                       platter   = {};
    vector<DxuiTabStrip::Tab>  tabs;



    //  A platter its height limits leaves its column no wider than it and its
    //  control row need, and the track column gains the rest.
    if (column > std::max (areaH, scaler.ToPx (s_kMinPlatterColumnDip)))
    {
        column = std::max (areaH, scaler.ToPx (s_kMinPlatterColumnDip));
        splitX = boundsDip.left + 2 * margin + column;
        side   = std::min (column, areaH);
    }

    m_scaler  = scaler;
    m_toolSig = GetToolSig();
    m_tooltip.SetDpi (scaler.GetDpi());
    m_tooltip.SetViewportSize (width, boundsDip.bottom - boundsDip.top);
    m_context.scenePx = boundsDip;

    m_toolbarPx  = { boundsDip.left, boundsDip.top, boundsDip.right, top - bar - find };
    m_barPx      = { boundsDip.left, top - bar - find, boundsDip.right, top - find };
    m_findBar->Layout ({ boundsDip.left, top - find, boundsDip.right, top }, scaler);
    m_splitterPx = { splitX, top, splitX + scaler.ToPx (s_kSplitterDip), bandTop };
    right        = { m_splitterPx.right + margin, top + margin, boundsDip.right - margin, bandTop - margin / 2 };

    for (i = 0; i < drives; i++)
    {
        tabs.push_back ({ { x + i * tab, boundsDip.top + margin / 2, x + (i + 1) * tab, rowBottom - margin / 2 }, std::format (L"Drive {}", i + 1) });
    }

    m_driveTabs->SetTabs (std::move (tabs));
    m_driveTabs->SetSelected (m_drive);
    m_driveTabs->Layout  ({ x, boundsDip.top, x + drives * tab, rowBottom }, scaler);

    //  The controls after the file name when they fit beside it, or on rows
    //  of their own below (FR-007).
    for (const ToolItem & item : items)
    {
        PlaceToolItem (item, scaler);
        ctlLeft = (item.rect.top == boundsDip.top) ? std::min (ctlLeft, static_cast<int> (item.rect.left) - margin) : ctlLeft;
    }

    m_fileNamePx = { nameLeft, boundsDip.top, std::min (nameLeft + scaler.ToPx (s_kFileNameDip), ctlLeft), rowBottom };
    m_chipsPx    = { m_fileNamePx.right + margin, boundsDip.top, ctlLeft, rowBottom };

    //  The comparison bar's buttons from the right, its sources on the left.
    i = boundsDip.right - margin;

    for (DxuiButton * barButton : { m_stopButton, m_bSettingsButton, m_swapButton, m_nextDiff, m_prevDiff })
    {
        int  w = scaler.ToPx (barButton == m_stopButton ? s_kStopButtonDip : s_kBarButtonDip);

        barButton->Layout ({ i - w, m_barPx.top + margin / 2, i, m_barPx.bottom - margin / 2 }, scaler);
        i -= w + margin / 2;
    }

    //  The platter as large as its column allows; its hint shows only where
    //  the column has height to spare once the platter fills its width.
    platter = { boundsDip.left + margin + (column - side) / 2, top + margin, boundsDip.left + margin + (column - side) / 2 + side, top + margin + side };
    m_platterView->Layout (platter, scaler);

    m_isHintShown = areaH - side >= row;
    m_hintLabel->Layout ({ x, platter.bottom + margin / 2, splitX - margin, platter.bottom + margin / 2 + row }, scaler);

    LayoutPlatterControls ({ x, bandTop - margin - row, splitX - margin, bandTop - margin }, scaler);

    m_platterAreaPx = { boundsDip.left, top, splitX, bandTop };

    //  The track column.
    y = right.top;
    m_headerView->Layout ({ right.left, y, right.right, y + header }, scaler);
    y += header;
    //  Rows of sector buttons past the first push the tabs down.
    m_sectorRows = m_sectorRow->GetRowCount (right.right - right.left, scaler);
    rowsH        = scaler.ToPx (s_kSectorRowDip) + (m_sectorRows - 1) * scaler.ToPx (SectorRowView::kRowDip + SectorRowView::kGapDip);
    m_sectorRow->Layout ({ right.left, y, right.right, y + rowsH }, scaler);
    y += rowsH;
    m_trackAreaPx = { m_splitterPx.right, top, boundsDip.right, y };

    y = LayoutTabs ({ right.left, y, right.right, right.bottom }, scaler);

    m_diskContentPx = { right.left, y, right.right, right.bottom };

    for (IDxuiControl * content : std::initializer_list<IDxuiControl *> { m_nibblesTab, m_fieldsTab, m_fluxTab, m_tracksTab, m_findingsTab, m_imageTab, m_diffsTab, m_findResults })
    {
        content->Layout (m_diskContentPx, scaler);
    }

    //  The edit bar above the Sector data tab's header and bytes.
    m_editBar->Layout  ({ right.left, y, right.right, y + m_editBar->GetHeightPx (right.right - right.left, scaler) }, scaler);
    y += m_editBar->GetHeightPx (right.right - right.left, scaler);
    m_byteView->Layout ({ right.left, y, right.right, right.bottom }, scaler);

    LayoutFileMap (m_diskContentPx, scaler);

    //  "Copy sector" at the end of the Sector data header's first line, with
    //  room reserved for it there.
    m_copySector->Layout       ({ right.right - scaler.ToPx (s_kCopySectorDip), y, right.right, y + scaler.ToPx (SectorByteView::kHeaderDip) }, scaler);
    m_byteView->SetHeaderReserve (scaler.ToPx (s_kCopySectorDip) + margin);

    //  The strip across the bottom, B's under it while comparing (FR-121).
    y = bandTop + margin;
    m_stripView->Layout ({ x, y, boundsDip.right - margin, y + strip }, scaler);
    y += strip + margin / 2;
    m_stripB->Layout ({ x, y, boundsDip.right - margin, y + (m_comparison.IsComparing() ? strip : 0) }, scaler);
    y += m_comparison.IsComparing() ? strip + margin / 2 : 0;
    m_stripOut->Layout     ({ x,              y, x + button,     y + row }, scaler);
    m_stripIn->Layout      ({ x + button,     y, x + 2 * button, y + row }, scaler);
    m_stripWhole->Layout   ({ x + 2 * button, y, x + 2 * button + scaler.ToPx (s_kWholeTrackDip), y + row }, scaler);
    m_stripReadout->Layout ({ x + 2 * button + scaler.ToPx (s_kWholeTrackDip) + margin, y, boundsDip.right - margin, y + row }, scaler);
    m_stripHint->Layout    ({ x + 2 * button + scaler.ToPx (s_kWholeTrackDip + s_kReadoutDip) + margin, y, boundsDip.right - margin, y + row }, scaler);

    m_stripAreaPx = { boundsDip.left, bandTop, boundsDip.right, boundsDip.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::LayoutPlatterControls
//
//  One row under the platter: the zoom controls, then each overlay that is
//  offered, as wide as its label, then "Legend" at the row's right end.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::LayoutPlatterControls (const RECT & area, const DxuiDpiScaler & scaler)
{
    int  button = scaler.ToPx (s_kButtonDip);
    int  gap    = scaler.ToPx (s_kMarginDip) / 2;
    int  x      = area.left;



    m_zoomOut->Layout   ({ x, area.top, x + button, area.bottom }, scaler);
    x += button;
    m_zoomIn->Layout    ({ x, area.top, x + button, area.bottom }, scaler);
    x += button;
    m_fit->Layout       ({ x, area.top, x + scaler.ToPx (s_kFitDip), area.bottom }, scaler);
    x += scaler.ToPx (s_kFitDip) + gap;
    m_zoomLabel->Layout ({ x, area.top, x + scaler.ToPx (s_kZoomLabelDip), area.bottom }, scaler);
    x += scaler.ToPx (s_kZoomLabelDip) + gap;

    for (const auto & [check, label] : { std::pair { m_alignmentCheck, L"Alignment" }, std::pair { m_filesCheck, L"Files" }, std::pair { m_readsCheck, L"Reads" },
                                     std::pair { m_diffsCheck, L"Differences" } })
    {
        int  w = GetLabelWidthPx (label, s_kCheckLabelPadDip, scaler);

        check->Layout ({ x, area.top, x + w, area.bottom }, scaler);
        x += (check == m_diffsCheck && !m_comparison.IsComparing()) ? 0 : w + gap;
    }

    m_legendButton->Layout ({ area.right - scaler.ToPx (s_kLegendButtonDip), area.top, area.right, area.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::LayoutTabs
//
//  The one tab strip: the track tabs, a gap, then the disk tabs, each as
//  wide as its label; the disk tabs take a row of their own when the column
//  is too narrow for both groups. Returns where the tabs' content starts.
//
////////////////////////////////////////////////////////////////////////////////

int DiskInspectorWindow::LayoutTabs (const RECT & area, const DxuiDpiScaler & scaler)
{
    static constexpr LPCWSTR  kpszTrackTabs[] = { L"Sector data", L"Nibbles", L"Fields", L"Flux timing" };
    static constexpr LPCWSTR  kpszDiskTabs[]  = { L"Tracks", L"Findings", L"File map", L"Image", L"Differences", L"Find results" };



    int                        tabsH  = scaler.ToPx (s_kTabsDip);
    int                        margin = scaler.ToPx (s_kMarginDip);
    vector<int>                shown  = GetShownDiskTabs();
    int                        trackW = 0;
    int                        diskW  = 0;
    int                        x      = area.left;
    int                        y      = area.top;
    bool                       isOne  = false;
    vector<DxuiTabStrip::Tab>  tabs;



    for (LPCWSTR label : kpszTrackTabs)
    {
        trackW += GetLabelWidthPx (label, s_kTabLabelPadDip, scaler);
    }

    for (int i : shown)
    {
        diskW += GetLabelWidthPx (kpszDiskTabs[i], s_kTabLabelPadDip, scaler);
    }

    isOne = trackW + 2 * margin + diskW <= area.right - area.left;

    for (LPCWSTR label : kpszTrackTabs)
    {
        int  w = GetLabelWidthPx (label, s_kTabLabelPadDip, scaler);

        tabs.push_back ({ { x, y, x + w, y + tabsH }, label });
        x += w;
    }

    //  A divider between the track tabs and the disk tabs when they share a
    //  row.
    m_tabGapPx = isOne ? RECT { x + margin, y + tabsH / 4, x + margin + scaler.ToPx (1), y + tabsH * 3 / 4 } : RECT {};

    x  = isOne ? x + 2 * margin : area.left;
    y += isOne ? 0 : tabsH;

    for (int i : shown)
    {
        int  w = GetLabelWidthPx (kpszDiskTabs[i], s_kTabLabelPadDip, scaler);

        tabs.push_back ({ { x, y, x + w, y + tabsH }, kpszDiskTabs[i] });
        x += w;
    }

    m_tabs->SetTabs     (std::move (tabs));
    m_tabs->SetSelected (GetTabIndex());
    m_tabs->Layout      ({ area.left, area.top, area.right, y + tabsH }, scaler);

    return y + tabsH + margin / 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetLabelWidthPx
//
//  A control's width for its label at the window's text size, estimated
//  from the label's length, plus the room the control needs around it.
//
////////////////////////////////////////////////////////////////////////////////

int DiskInspectorWindow::GetLabelWidthPx (LPCWSTR label, int padDip, const DxuiDpiScaler & scaler)
{
    return scaler.ToPx (static_cast<int> (std::ceil (wcslen (label) * s_kCharWidthDip)) + padDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  focus = {};



    painter.FillRect (static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top),
                      static_cast<float> (m_boundsDip.right - m_boundsDip.left), static_cast<float> (m_boundsDip.bottom - m_boundsDip.top),
                      theme.Background());

    //  The status band's surface, with a divider along its top.
    painter.FillRect (static_cast<float> (m_statusPx.left), static_cast<float> (m_statusPx.top), static_cast<float> (m_statusPx.right - m_statusPx.left),
                      static_cast<float> (m_statusPx.bottom - m_statusPx.top), theme.BackgroundElevated());
    painter.FillRect (static_cast<float> (m_statusPx.left), static_cast<float> (m_statusPx.top), static_cast<float> (m_statusPx.right - m_statusPx.left), 1.0f,
                      theme.Divider());

    painter.FillRect (static_cast<float> (m_splitterPx.left), static_cast<float> (m_splitterPx.top),
                      static_cast<float> (m_splitterPx.right - m_splitterPx.left), static_cast<float> (m_splitterPx.bottom - m_splitterPx.top),
                      theme.Divider());

    PaintToolbar (painter, text, theme);

    if (m_comparison.IsComparing())
    {
        PaintComparisonBar (painter, text, theme);
    }

    DxuiWindow::Paint (painter, text, theme);

    if (m_filePicker->IsVisible())
    {
        PaintPicker (painter, text, theme);
    }

    painter.FillRect (static_cast<float> (m_tabGapPx.left), static_cast<float> (m_tabGapPx.top), static_cast<float> (m_tabGapPx.right - m_tabGapPx.left),
                      static_cast<float> (m_tabGapPx.bottom - m_tabGapPx.top), theme.Divider());

    if (m_is35)
    {
        PaintNot35 (painter, text, theme);
    }

    //  A file that could not be opened says why where the platter would be.
    if (!m_context.hasDisk && !m_openError.empty())
    {
        text.DrawString (m_openError.c_str(), static_cast<float> (m_platterAreaPx.left), static_cast<float> (m_platterAreaPx.top),
                         static_cast<float> (m_platterAreaPx.right - m_platterAreaPx.left), static_cast<float> (m_platterAreaPx.bottom - m_platterAreaPx.top),
                         theme.ErrorForeground(), m_scaler.ToPxf (InspectorView::kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                         DxuiTextVAlign::Center, DxuiFontWeight::Normal, true);
    }

    if (!m_context.hasDisk && m_openError.empty() && m_pendingRequest == 0)
    {
        PaintNoDisk (painter, text, theme);
    }

    PaintHelp (text, theme);

    //  The view the keys act on, outlined once a key has been used (FR-059).
    if (m_isFocusShown && m_context.hasDisk && !m_is35)
    {
        focus = GetKeyTargetBounds();
        InflateRect (&focus, m_scaler.ToPx (s_kFocusGapDip), m_scaler.ToPx (s_kFocusGapDip));
        painter.OutlineRect (static_cast<float> (focus.left), static_cast<float> (focus.top), static_cast<float> (focus.right - focus.left),
                             static_cast<float> (focus.bottom - focus.top), m_scaler.ToPxf (2.0f), theme.FocusRing());
    }

    if (m_walkthrough.IsOpen())
    {
        m_walkthrough.Paint (text, theme, m_scaler, m_boundsDip, GetWalkthroughArea (m_walkthrough.GetArea()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PaintToolbar
//
//  The file name, clipped to its room, then the summary chips that fit
//  (FR-018); the name's tooltip gives it whole.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PaintToolbar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float         pad     = m_scaler.ToPxf (s_kChipPadDip);
    float         gap     = m_scaler.ToPxf (s_kChipGapDip);
    float         chipH   = m_scaler.ToPxf (s_kChipHeightDip);
    float         textPx  = m_scaler.ToPxf (s_kChipTextDip);
    float         x       = static_cast<float> (m_chipsPx.left);
    float         y       = (m_chipsPx.top + m_chipsPx.bottom - chipH) / 2.0f;
    float         w       = 0;
    float         h       = 0;
    bool          isFull  = false;
    std::wstring  name    = m_context.hasDisk ? GetFileName() : std::wstring (m_pendingRequest != 0 ? s_kpszAnalyzing : s_kpszNoDisk);



    //  A lock before the name of a write-protected disk in the drive (FR-065).
    name = (m_context.hasDisk && IsWriteProtected()) ? std::wstring (s_kpszLock) + L" " + name : name;

    painter.FillRect (static_cast<float> (m_toolbarPx.left), static_cast<float> (m_toolbarPx.top),
                      static_cast<float> (m_toolbarPx.right - m_toolbarPx.left), static_cast<float> (m_toolbarPx.bottom - m_toolbarPx.top),
                      theme.BackgroundElevated());

    text.DrawString (name.c_str(), static_cast<float> (m_fileNamePx.left), static_cast<float> (m_fileNamePx.top),
                     static_cast<float> (m_fileNamePx.right - m_fileNamePx.left), static_cast<float> (m_fileNamePx.bottom - m_fileNamePx.top),
                     theme.HeadingForeground(), m_scaler.ToPxf (InspectorView::kTextDip + 1.0f), DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

    if (m_context.hasDisk)
    {
        //  While comparing, the result comes first (FR-121).
        if (m_comparison.IsComparing())
        {
            std::wstring  result = (m_comparison.IsBusy() || !m_comparison.HasResult()) ? std::wstring (L"Comparing") : ComparisonText::FormatResult (m_comparison.GetResult());

            text.MeasureString (result.c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            painter.FillRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, theme.SelectionBackground());
            text.DrawString (result.c_str(), x, y, w + 2 * pad, chipH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
            x += w + 2 * pad + gap;
        }

        for (const SummaryChip & chip : InspectorText::BuildChips (m_analysis.summary))
        {
            text.MeasureString (chip.text.c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            isFull = isFull || x + w + 2 * pad > m_chipsPx.right;

            if (!isFull)
            {
                painter.FillRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, theme.ButtonIdle());
                painter.OutlineRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, m_scaler.ToPxf (1.0f),
                                            chip.isBad ? m_context.palette.colors.sectorBad : theme.ButtonBorder());
                text.DrawString (chip.text.c_str(), x, y, w + 2 * pad, chipH, chip.isBad ? m_context.palette.colors.sectorBad : theme.Foreground(), textPx,
                                 DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

                x += w + 2 * pad + gap;
            }
        }

        if (!m_analysis.settings.IsStandard() && !isFull)
        {
            text.MeasureString (L"Custom decode settings", textPx, DxuiTheme::kBodyFace, w, h);
            painter.FillRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, theme.SelectionBackground());
            text.DrawString (L"Custom decode settings", x, y, w + 2 * pad, chipH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            x += w + 2 * pad + gap;
        }

        if (m_scheduler->HasPending())
        {
            text.DrawString (s_kpszAnalyzing, x, y, m_scaler.ToPxf (s_kAnalyzingDip), chipH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnMouse
//
//  The splitter drags here; everything else goes to the views and controls,
//  and the tooltip follows the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OnMouse (const DxuiMouseEvent & ev)
{
    //  While the walkthrough is open it takes all input (FR-153).
    if (OnWalkthroughMouse (ev))
    {
        return true;
    }

    POINT  p         = ev.positionDip;
    int    width     = m_boundsDip.right - m_boundsDip.left;
    bool   isCovered = IsCovered (p) && ev.kind != DxuiMouseEventKind::Leave;
    bool   isHandled = isCovered;



    //  Ctrl and the wheel zoom the File map grid over it (FR-157), and the
    //  whole window everywhere else (FR-154).
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal && IsOverMapGrid (p))
    {
        ZoomMapGrid (ev.wheelDelta, p);
        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal)
    {
        SetZoom (DxuiZoomControl::StepZoom (m_zoom, (ev.wheelDelta > 0) ? 1 : -1));
        isHandled = true;
    }

    //  Over a 3.5" disk's notes, the views under them take nothing.
    if (!isCovered && ev.kind == DxuiMouseEventKind::Down)
    {
        m_keyTarget    = GetKeyTarget (p);
        m_isFocusShown = false;
        m_findBar->SetInputFocused (m_keyTarget == KeyTarget::FindBar && m_findBar->IsInputFocused());
    }

    if (!isCovered && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right && m_context.hasDisk &&
        (m_keyTarget == KeyTarget::SectorData || m_keyTarget == KeyTarget::Nibbles || m_keyTarget == KeyTarget::Tracks))
    {
        ShowContextMenu (p);
        isHandled = true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && PtInRect (&m_splitterPx, p))
    {
        m_isSplitting = true;
        isHandled     = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Move && m_isSplitting && width > 0)
    {
        m_splitFraction = std::clamp (static_cast<double> (p.x - m_boundsDip.left) / width, s_kMinSplit, s_kMaxSplit);
        Layout (m_boundsDip, m_hostScaler);
        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && m_isSplitting)
    {
        m_isSplitting = false;
        isHandled     = true;
    }

    if (!isHandled)
    {
        isHandled = DxuiWindow::OnMouse (ev);
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        TrackLegendHover (p);
    }

    //  The views draw their hover from where the pointer is (FR-148).
    m_context.hoverPx = (ev.kind == DxuiMouseEventKind::Leave) ? POINT { SHRT_MIN, SHRT_MIN } : (ev.kind == DxuiMouseEventKind::Move ? p : m_context.hoverPx);

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        UpdateTooltip (p);
        m_statusText->SetText (!m_context.hasDisk ? m_statusNote : IsInside (m_platterAreaPx, p) ? s_kpszHint : IsInside (m_stripAreaPx, p) ? s_kpszStripHint : m_statusNote);
    }
    else if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_tooltip.RequestHide (static_cast<int64_t> (GetTickCount64()));
        m_tooltipText.clear();
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnKey
//
//  Up and Down step a quarter track, Page Up and Page Down go to the next
//  whole track, Left and Right step through the sectors in passing order
//  wrapping at the index, Home and End go to the first and last (FR-029);
//  plus and minus zoom, 0 returns to fit (FR-025), and Ctrl with an arrow
//  pans the zoomed platter. Ctrl+F finds, F3 and Shift+F3 step through the
//  hits, and Ctrl+G goes to a target. F6 and Shift+F6 move the keys from
//  view to view, and once a key is used the view they act on is outlined. After a press on the strip, plus and minus zoom
//  it, 0 shows the whole track, and Left and Right pan it (FR-034).
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OnKey (const DxuiKeyEvent & ev)
{
    static constexpr int  kPerTrack = DiskImage::kQuarterTracksPerWholeTrack;



    bool  isHandled = false;
    int   qt        = m_model.GetQuarterTrack();



    if (OnWalkthroughKey (ev))
    {
        return true;
    }

    //  Keys go to the find bar while it has the focus, all but the window's
    //  zoom and its Ctrl+F and Ctrl+G.
    if (m_isFindOpen && m_keyTarget == KeyTarget::FindBar && !(ev.ctrl && (IsZoomKey (ev.vk) || ev.vk == 'F' || ev.vk == 'G')) && m_findBar->OnKey (ev))
    {
        isHandled = true;
    }
    else if (m_keyTarget == KeyTarget::SectorData && m_byteView->IsEditing() && HandleEditKey (ev))
    {
        //  While editing, the Sector data tab takes typing, Ctrl+Z, Ctrl+Y
        //  and Ctrl+V, and Escape stops editing (FR-097).
        isHandled = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && IsZoomKey (ev.vk))
    {
        //  Ctrl with 0, + or - zooms the whole window (FR-154).
        SetZoom ((ev.vk == '0' || ev.vk == VK_NUMPAD0) ? 1.0 : DxuiZoomControl::StepZoom (m_zoom, (ev.vk == VK_OEM_PLUS || ev.vk == VK_ADD) ? 1 : -1));
        isHandled = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && !ev.alt && m_context.hasDisk)
    {
        isHandled = true;

        if (ev.ctrl)
        {
            switch (ev.vk)
            {
                case VK_LEFT:  m_model.PanBy ({  s_kPanStep, 0.0 }); break;
                case VK_RIGHT: m_model.PanBy ({ -s_kPanStep, 0.0 }); break;
                case VK_UP:    m_model.PanBy ({ 0.0,  s_kPanStep }); break;
                case VK_DOWN:  m_model.PanBy ({ 0.0, -s_kPanStep }); break;
                case 'A':      SelectAll();                          break;
                case 'C':      if (ev.shift) { CopySector(); } else { Copy(); } break;
                case 'F':      OpenFindBar();                        break;
                case 'G':      OpenGoTo();                           break;
                default:       isHandled = false;                    break;
            }
        }
        else if (ev.vk == VK_ESCAPE && !ev.shift)
        {
            m_model.ClearSelection();
            OnSelection();
        }
        else if (ev.shift && (m_keyTarget == KeyTarget::Nibbles || m_keyTarget == KeyTarget::SectorData) &&
                 (ev.vk == VK_LEFT || ev.vk == VK_RIGHT || ev.vk == VK_UP || ev.vk == VK_DOWN))
        {
            ExtendSelection (ev.vk);
        }
        else if (m_keyTarget == KeyTarget::Strip && IsStripKey (ev.vk))
        {
            switch (ev.vk)
            {
                case VK_OEM_PLUS:  case VK_ADD:      m_stripView->ZoomAboutCenter (kZoomStep);       break;
                case VK_OEM_MINUS: case VK_SUBTRACT: m_stripView->ZoomAboutCenter (1.0 / kZoomStep); break;
                case VK_LEFT:                        m_stripView->PanBy (-s_kPanStep);               break;
                case VK_RIGHT:                       m_stripView->PanBy (s_kPanStep);                break;
                default:                             m_stripView->ShowWholeTrack();                  break;
            }
        }
        else
        {
            switch (ev.vk)
            {
                case VK_OEM_PLUS:  case VK_ADD:      m_platterView->ZoomAboutCenter (kZoomStep);       break;
                case VK_OEM_MINUS: case VK_SUBTRACT: m_platterView->ZoomAboutCenter (1.0 / kZoomStep); break;
                case '0':          case VK_NUMPAD0:  m_model.Fit();                                    break;
                case VK_UP:    m_model.SelectQuarterTrack (qt - 1);                           OnSelection(); break;
                case VK_DOWN:  m_model.SelectQuarterTrack (qt + 1);                           OnSelection(); break;
                case VK_PRIOR: m_model.SelectQuarterTrack ((qt - 1) / kPerTrack * kPerTrack); OnSelection(); break;
                case VK_NEXT:  m_model.SelectQuarterTrack ((qt / kPerTrack + 1) * kPerTrack); OnSelection(); break;
                case VK_LEFT:  StepSector (-1, false); break;
                case VK_RIGHT: StepSector ( 1, false); break;
                case VK_HOME:  StepSector (-1, true);  break;
                case VK_F3:    StepFind (ev.shift ? -1 : 1); break;
                case VK_F6:    StepKeyTarget (ev.shift ? -1 : 1); break;
                case VK_END:   StepSector ( 1, true);  break;
                default:       isHandled = false;      break;
            }
        }
    }

    m_isFocusShown = m_isFocusShown || isHandled;

    if (!isHandled)
    {
        isHandled = DxuiWindow::OnKey (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DiskInspectorWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = IDC_ARROW;



    if (m_isSplitting || PtInRect (&m_splitterPx, clientPx))
    {
        cursor = IDC_SIZEWE;
    }
    else if (m_platterView != nullptr && (m_platterView->IsDragging() || (IsInside (m_platterView->GetBounds(), clientPx) && !m_model.IsAtFit())))
    {
        cursor = IDC_SIZEALL;
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RequestCopy
//
//  A copy of whatever disk is in the drive; the reply says which disk it is.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RequestCopy()
{
    InspectorRequest  request;



    request.kind     = InspectorRequestKind::CopyDisk;
    request.drive    = m_drive;
    m_pendingRequest = m_host->PostInspectorRequest (request);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeReplies
//
//  Only the reply to the latest request counts. Its disk starts the view
//  over unless it is the same disk; every record waits for its analysis.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeReplies()
{
    vector<InspectorReply>  replies;



    m_host->TakeInspectorReplies (replies);

    for (InspectorReply & reply : replies)
    {
        if (m_comparison.OfferReply (reply))
        {
            continue;
        }

        if (reply.requestId == m_pendingRequest)
        {
            m_pendingRequest = 0;
            StartDisk (reply.disk, (reply.status == InspectorReplyStatus::Unopenable) ? reply.reason : std::wstring());
        }
        else if (reply.requestId == m_trackRequest)
        {
            m_trackRequest = 0;
            TakeWrittenTracks (reply);
        }
        else if (reply.kind == InspectorRequestKind::Apply && reply.requestId == m_applyRequest)
        {
            TakeApplyReply (reply);
        }
        else if (reply.requestId == m_saveCopyRequest)
        {
            TakeSaveCopyReply (reply);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StartDisk
//
//  A new disk for side A starts the view over; every record waits for its
//  analysis. With no disk, the reason it could not be opened is kept.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StartDisk (std::shared_ptr<const DiskCopy> copy, const std::wstring & reason)
{
    DecodeSettings  settings  = m_comparison.IsComparing() ? m_analysis.settings : DecodeSettings::MakeStandard();
    int             slot      = 0;
    uint64_t        lastMedia = m_analysis.mediaId;
    std::string     lastFile  = (m_analysis.copy != nullptr) ? m_analysis.copy->fileName : std::string();
    bool            isReload  = copy != nullptr && !lastFile.empty() && copy->fileName == lastFile;



    if (copy != nullptr && copy->mediaId != lastMedia)
    {
        m_statusNote.clear();
    }

    //  End a comparison of this drive once its disk is ejected or replaced.
    if (m_comparison.IsComparing() && m_sourceA == ComparisonSource { ComparisonSourceKind::DriveNow, m_drive } && lastMedia != 0 &&
        (copy == nullptr || copy->mediaId != lastMedia))
    {
        EndComparison (std::format (L"The comparison ended. The disk in drive {} was ejected or replaced.", m_drive + 1));
    }

    m_context.hasDisk = (copy != nullptr);
    m_openError       = reason;
    m_changedSlots.clear();
    m_levels.fill (nullptr);
    m_timingLevels.fill (nullptr);

    if (m_context.hasDisk)
    {
        m_model.SetDisk (copy->mediaId);
        m_mapGrid->ResetZoom();

        m_analysis           = DiskAnalysis();
        m_analysis.mediaId   = copy->mediaId;
        m_analysis.copy      = copy;
        m_analysis.settings  = settings;
        m_analysis.headLimit = Disk2Controller::kMaxQuarterTrack;
        m_analysis.tracks.resize (copy->tracks.size());
        DiskAnalyzer::Assemble (m_analysis);
        m_scheduler->Restart (copy, m_analysis.settings);

        for (slot = 0; slot < static_cast<int> (copy->tracks.size()); slot++)
        {
            m_changedSlots.insert (slot);
        }

        //  A saved Files choice applies to every disk (FR-080).
        m_isFilesDefaulted = m_isFilesSaved;

        //  Pending edits are discarded with the disk they were made on. A
        //  remount of the same file, as Reset, Power Cycle and a machine
        //  change do, is a reload: the edits follow their fields once each
        //  track is analyzed again (FR-108).
        if (copy->mediaId != lastMedia && !isReload && !m_edits.IsEmpty())
        {
            m_editNote = L"Pending edits were discarded: the disk in the drive changed";
        }

        if (copy->mediaId != lastMedia && !isReload)
        {
            m_edits = EditController();
            m_byteView->SetEditing (false);
        }

        if (copy->mediaId != lastMedia)
        {
            m_history.clear();
            m_historyAt = 0;
            m_appliedSlots.clear();
        }

        //  Hits point into the analysis they were found in.
        m_find.Clear();
        m_findBar->SetResult (-1, 0);
        RefreshFindResults();

        //  The copy's own write counts are where the guest's writes start.
        m_writeCounts.clear();
        m_writtenSlots.clear();
        m_trackRequest = 0;

        for (const std::shared_ptr<const TrackCopy> & track : copy->tracks)
        {
            m_writeCounts.push_back (track != nullptr ? track->guestWriteCount : 0);
        }
    }
    else
    {
        if (!m_edits.IsEmpty())
        {
            m_editNote   = L"Pending edits were discarded: the disk was ejected";
            m_statusNote = m_editNote;
            m_statusText->SetText (m_statusNote);
        }

        m_edits = EditController();
        m_byteView->SetEditing (false);
        m_history.clear();
        m_historyAt = 0;
        m_appliedSlots.clear();
        m_model.SetDisk (0);
        m_analysis = DiskAnalysis();
    }

    m_model.SetAnalysis (m_context.hasDisk ? &m_analysis : nullptr);
    m_comparison.MarkAChanged();
    m_isTablesDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeResults
//
//  Every result that arrived since the last frame is stored, then the disk
//  is rebuilt once.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeResults()
{
    vector<RecordResult>  results;



    m_scheduler->TakeResults (results);

    for (RecordResult & result : results)
    {
        if (result.mediaId == m_analysis.mediaId)
        {
            DiskAnalyzer::Accept (result.copy, result.slot, std::move (result.analysis), m_analysis);
            m_changedSlots.insert (result.slot);
            m_levels[result.slot]       = nullptr;
            m_timingLevels[result.slot] = nullptr;
        }
    }

    if (!results.empty())
    {
        DiskAnalyzer::Assemble (m_analysis);
        m_model.SetAnalysis (&m_analysis);
        m_comparison.MarkAChanged (GetQuarterTracksOf (results));
        m_isTablesDirty = true;
        FollowEdits();

        //  A search made while the disk was being analyzed is run again once
        //  the analysis is done. Otherwise the marks are rebuilt without the
        //  hits on tracks analyzed again, and with any new file map.
        if (m_find.IsRepeatDue (m_scheduler->HasPending()))
        {
            RepeatFind();
        }
        else if (!m_find.IsEmpty())
        {
            RebuildFindMarks();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetQuarterTracksOf
//
//  Every quarter track mapped to one of the analyzed tracks' slots.
//
////////////////////////////////////////////////////////////////////////////////

std::set<int> DiskInspectorWindow::GetQuarterTracksOf (const vector<RecordResult> & results) const
{
    std::set<int>  slots;
    std::set<int>  quarterTracks;



    for (const RecordResult & result : results)
    {
        slots.insert (result.slot);
    }

    for (int qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        if (slots.contains (m_analysis.entries[qt].slot))
        {
            quarterTracks.insert (qt);
        }
    }

    return quarterTracks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateRings
//
//  The platter's rings for every record whose analysis or state changed. A
//  record's cells are built once and shared by the quarter tracks that play
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateRings()
{
    const DiskCopy *   copy     = m_analysis.copy.get();
    PlatterRenderer &  renderer = m_platterView->GetRenderer();
    int                qt       = 0;
    int                slot     = 0;
    bool               damaged  = false;
    vector<Byte>       cells;



    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        slot    = (copy != nullptr) ? copy->playedSlot[qt] : -1;
        damaged = copy != nullptr && slot < 0 && copy->mappedSlot[qt] >= 0 && copy->IsSlotDamaged (copy->mappedSlot[qt]);

        if (slot < 0)
        {
            renderer.SetRing (qt, damaged ? PlatterRingState::Damaged : PlatterRingState::Nothing, nullptr);
        }
        else if (m_scheduler->IsPending (slot) || slot >= static_cast<int> (m_analysis.tracks.size()) || m_analysis.tracks[slot] == nullptr)
        {
            renderer.SetRing (qt, PlatterRingState::Pending, nullptr);
        }
        else if (m_changedSlots.contains (slot) || m_levels[slot] == nullptr)
        {
            if (m_levels[slot] == nullptr)
            {
                auto  levels = std::make_shared<PlatterRenderer::Levels>();

                PlatterCells::BuildCells  (*m_analysis.tracks[slot], cells);
                PlatterCells::BuildLevels (cells, *levels);
                m_levels[slot] = levels;

                if (m_analysis.tracks[slot]->framed.isFlux)
                {
                    auto  timing = std::make_shared<PlatterRenderer::Levels>();

                    FluxTiming::BuildDeviations (*m_analysis.tracks[slot], cells);
                    FluxTiming::BuildLevels     (cells, *timing);
                    m_timingLevels[slot] = timing;
                }
            }

            renderer.SetRing (qt, PlatterRingState::Data, m_levels[slot], m_timingLevels[slot]);
        }
    }

    m_changedSlots.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateControls
//
//  The zoom controls are hidden with no disk, and "Fit" is unavailable at
//  fit (FR-025).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateControls()
{
    //  A control shown or hidden moves the others along the toolbar.
    if (m_zoomLabel != nullptr && m_scaler.GetDpi() != 0 && GetToolSig() != m_toolSig)
    {
        Layout (m_boundsDip, m_hostScaler);
    }

    if (m_zoomLabel != nullptr)
    {
        m_zoomLabel->SetText    (std::format (L"{:.0f}{}", m_model.GetZoom(), s_kpszMultiplyX));
        m_fit->SetEnabled       (!m_model.IsAtFit());
        m_zoomOut->SetEnabled   (!m_model.IsAtFit());
        m_zoomOut->SetVisible   (m_context.hasDisk);
        m_decodeButton->SetVisible (m_context.hasDisk);
        m_exportButton->SetVisible (m_context.hasDisk);
        m_goToButton->SetVisible   (m_context.hasDisk);
        m_findButton->SetVisible   (m_context.hasDisk);
        m_copySector->SetVisible   (m_context.hasDisk && !m_isDiskGroup && m_trackTab == kTabSectorData);
        m_editBar->SetVisible      (m_context.hasDisk && !m_isDiskGroup && m_trackTab == kTabSectorData);
        UpdateEditBar();
        m_copySector->SetEnabled   (m_model.GetSector() != nullptr && m_model.GetSector()->dataField >= 0);
        m_compareButton->SetVisible   (m_context.hasDisk || m_comparison.IsComparing());
        m_followCheck->SetVisible     (IsHeadShown());
        m_goToHead->SetVisible        (IsHeadShown());
        m_goToHead->SetEnabled        (m_headState != nullptr && m_headState->hasDisk);
        m_prevDiff->SetVisible        (m_comparison.IsComparing());
        m_nextDiff->SetVisible        (m_comparison.IsComparing());
        m_swapButton->SetVisible      (m_comparison.IsComparing());
        m_bSettingsButton->SetVisible (m_comparison.IsComparing());
        m_stopButton->SetVisible      (m_comparison.IsComparing());
        m_diffsCheck->SetVisible      (m_comparison.IsComparing() && m_context.hasDisk);
        m_prevDiff->SetEnabled        (!m_comparison.GetListed().empty());
        m_nextDiff->SetEnabled        (!m_comparison.GetListed().empty());
        m_swapButton->SetEnabled      (m_context.hasDisk && m_comparison.GetB().copy != nullptr);
        m_bSettingsButton->SetEnabled (m_comparison.GetB().copy != nullptr);
        m_zoomIn->SetVisible    (m_context.hasDisk);
        m_fit->SetVisible       (m_context.hasDisk);
        m_zoomLabel->SetVisible (m_context.hasDisk);
        m_hintLabel->SetVisible    (false);
        m_legend->SetVisible       (false);
        m_legendButton->SetVisible (m_context.hasDisk);
        SyncFileMapSelection();
        m_stripOut->SetVisible     (m_context.hasDisk);
        m_stripIn->SetVisible      (m_context.hasDisk);
        m_stripWhole->SetVisible   (m_context.hasDisk);
        m_stripWhole->SetEnabled   (m_model.GetStripSpan() < 1.0);
        m_stripOut->SetEnabled     (m_model.GetStripSpan() < 1.0);
        m_stripReadout->SetVisible (m_context.hasDisk);
        m_stripReadout->SetText    (m_stripView->GetReadout() + ((m_model.GetTrack() != nullptr && m_model.GetNibbleCount() > 0)
                                    ? L"    " + InspectorText::FormatSelection (*m_model.GetTrack(), m_model.GetFirstNibble(), m_model.GetNibbleCount()) : L""));
        m_stripHint->SetVisible    (false);
        m_alignmentCheck->SetVisible (m_context.hasDisk);
        m_filesCheck->SetVisible     (m_context.hasDisk);
        m_readsCheck->SetVisible     (m_context.hasDisk && IsWindowDiskA());
        m_modeTabs->SetVisible   (m_context.hasDisk);
        m_rangeDown->SetVisible  (m_context.hasDisk && m_context.isTimingMode);
        m_rangeUp->SetVisible    (m_context.hasDisk && m_context.isTimingMode);
        m_rangeLabel->SetVisible (m_context.hasDisk && m_context.isTimingMode);
        m_rangeLabel->SetText    (std::wstring (s_kpszPlusMinus) + InspectorFormat::FormatPercent (m_context.timingRange).substr (1));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnSelection
//
//  After any view changes the selection, the Nibbles tab scrolls to it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnSelection()
{
    const TrackAnalysis *   track  = m_model.GetTrack();
    const AnalyzedSector *  sector = m_model.GetSector();
    int                     nibble = m_model.GetFirstNibble();



    if (nibble < 0 && track != nullptr && sector != nullptr)
    {
        nibble = track->fields[sector->addressField].firstNibble;
    }

    m_nibblesTab->ScrollTo (nibble);
    SyncTables();

    //  A new sector the user chooses anywhere but on the platter starts a
    //  flight to it. One chosen on the platter is in view there, and one
    //  chosen by Follow head starts none.
    if (m_context.selectionSource == m_platterView)
    {
        m_model.StopFlight();
    }
    else if (!m_isHeadSelecting && sector != nullptr && (m_model.GetQuarterTrack() != m_flownQuarterTrack || m_model.GetSectorIndex() != m_flownSector))
    {
        m_model.StartFlight (InspectorMarquee::GetClockMs());
    }

    m_flownQuarterTrack = m_model.GetQuarterTrack();
    m_flownSector       = m_model.GetSectorIndex();

    //  A selection made by hand ends Follow head (FR-067).
    if (!m_isHeadSelecting && m_followHead.IsOn())
    {
        m_followHead.OnHandSelection();
        m_followCheck->SetChecked (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowTrackTab
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowTrackTab (int tab)
{
    m_trackTab    = tab;
    m_isDiskGroup = false;
    ShowTabContent();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateTooltip
//
//  The first view with a tooltip for the point supplies it; over the file
//  name, the tip has the name in full. Over the platter, the strips and the
//  File map grid, the tip appears at once and follows the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateTooltip (POINT pointPx)
{
    std::wstring                          tip;
    RECT                                  anchor    = {};
    int64_t                               now       = static_cast<int64_t> (GetTickCount64());
    vector<DxuiTooltipSwatch>             swatches;
    bool                                  isInstant = false;
    const std::array<InspectorView *, 9>  views     = { m_platterView, m_stripView, m_stripB, m_sectorRow, m_byteView, m_nibblesTab, m_fluxTab, m_mapGrid, m_headerView };



    for (InspectorView * view : views)
    {
        //  The File map grid scrolls, so only its part in the viewport counts.
        bool  isShown = view != m_mapGrid || IsInside (m_mapScroll->GetViewportPx(), pointPx);

        if (tip.empty() && isShown && view != nullptr && view->IsVisible() && IsInside (view->GetBounds(), pointPx) && view->GetTooltip (pointPx, tip, anchor))
        {
            view->GetTooltipSwatches (pointPx, swatches);
            isInstant = view == m_platterView || view == m_stripView || view == m_stripB || view == m_mapGrid;
        }
    }

    if (tip.empty() && m_context.hasDisk)
    {
        tip = GetButtonTip (pointPx, anchor);
    }

    if (tip.empty() && PtInRect (&m_helpPx, pointPx))
    {
        tip    = L"Open the walkthrough";
        anchor = m_helpPx;
    }

    //  A write-protected disk's causes follow its name (FR-065).
    if (tip.empty() && m_context.hasDisk && PtInRect (&m_fileNamePx, pointPx))
    {
        tip    = GetFileName();
        tip   += IsWriteProtected() ? L"\nWrite-protected: " + InspectorText::FormatWriteProtect (m_context.drive->writeProtect) : L"";
        anchor = m_fileNamePx;
    }

    if (tip.empty())
    {
        m_tooltip.RequestHide (now);
    }
    else if (isInstant || tip != m_tooltipText || (!m_tooltip.IsShown() && !m_tooltip.IsPending()))
    {
        m_tooltip.SetSwatches (swatches);

        if (isInstant)
        {
            m_tooltip.RequestShowNow (anchor, tip, now);
        }
        else
        {
            m_tooltip.RequestShow (anchor, tip, now);
        }
    }

    m_tooltipText = tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepSector
//
//  The previous or next sector in passing order, wrapping at the index, or
//  the first or last.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepSector (int delta, bool isToEnd)
{
    const TrackAnalysis *  track = m_model.GetTrack();
    vector<int>            order = (track != nullptr) ? SectorRowView::GetPassingOrder (*track) : vector<int>();
    int                    count = static_cast<int> (order.size());
    int                    at    = 0;



    if (count > 0)
    {
        at = static_cast<int> (std::find (order.begin(), order.end(), m_model.GetSectorIndex()) - order.begin());
        at = isToEnd ? (delta < 0 ? 0 : count - 1) : (at + delta + count) % count;

        m_model.SelectSector (m_model.GetQuarterTrack(), order[at]);
        OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskInspectorWindow::GetFileName() const
{
    std::wstring  name;
    size_t        slash = 0;



    if (m_analysis.copy != nullptr)
    {
        name  = TextEncoding::Utf8ToWide (m_analysis.copy->fileName);
        slash = name.find_last_of (L"\\/");
        name  = (slash == std::wstring::npos) ? name : name.substr (slash + 1);
    }

    return name.empty() ? std::wstring (L"Untitled disk") : name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsInside (const RECT & rect, POINT pointPx)
{
    return PtInRect (&rect, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowDiskTab
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowDiskTab (int tab)
{
    m_diskTab     = tab;
    m_isDiskGroup = true;
    ShowTabContent();

    if (m_scaler.GetDpi() != 0)
    {
        Layout (m_boundsDip, m_hostScaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowTabContent
//
//  The track tabs and the disk tabs share one strip and one content area:
//  only the chosen tab of the group chosen last has its content shown.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowTabContent()
{
    bool  isTrack = !m_isDiskGroup;



    m_byteView->SetVisible    (isTrack && m_trackTab == kTabSectorData);
    m_nibblesTab->SetVisible  (isTrack && m_trackTab == kTabNibbles);
    m_fieldsTab->SetVisible   (isTrack && m_trackTab == kTabFields);
    m_fluxTab->SetVisible     (isTrack && m_trackTab == kTabFluxTiming);
    m_tracksTab->SetVisible   (!isTrack && m_diskTab == kTabTracks);
    m_findingsTab->SetVisible (!isTrack && m_diskTab == kTabFindings);
    m_imageTab->SetVisible    (!isTrack && m_diskTab == kTabImage);
    m_diffsTab->SetVisible    (!isTrack && m_diskTab == kTabDifferences);
    m_findResults->SetVisible (!isTrack && m_diskTab == kTabFindResults);
    ShowFileMap (!isTrack && m_diskTab == kTabFileMap);
    m_tabs->SetSelected (GetTabIndex());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RefreshTables
//
//  The Tracks and Findings tabs after the analysis changes, and the Fields
//  tab when its track or that track's analysis changes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RefreshTables()
{
    const TrackAnalysis *  track = m_model.GetTrack();
    int                    qt    = m_model.GetQuarterTrack();



    if (m_isTablesDirty)
    {
        vector<std::wstring>  columns = InspectorTables::GetTrackColumns();
        vector<TableRow>      rows    = m_context.hasDisk ? InspectorTables::BuildTracks (m_analysis, m_context.findMarks, &m_edits, &m_appliedSlots, m_context.drive) : vector<TableRow>();

        //  While comparing, each quarter track's verdict (FR-121).
        if (m_comparison.IsComparing())
        {
            ComparisonText::AddVerdictColumn (columns, rows, m_comparison.GetResult(), m_comparison.IsWhollyBusy() || !m_comparison.HasResult(), m_comparison.GetComparing());
        }

        if (m_comparison.IsComparing() != m_isTracksCompared)
        {
            m_tracksTab->SetColumns (columns);
            m_isTracksCompared = m_comparison.IsComparing();
        }

        m_tracksTab->SetRows    (std::move (rows));
        m_diffsTab->Refresh();
        m_tracksTab->SetCaption (m_context.hasDisk && InspectorTables::IsAlignmentNoteShown (m_analysis)
                                     ? L"This image's INFO says its tracks were not imaged in sync, so their alignment to one another was not kept"
                                     : L"");
        m_findingsTab->Refresh();

        //  A disk with a volume the map reads opens on the Files overlay, the
        //  most telling view of it; the user's choice stands after that.
        if (!m_isFilesDefaulted && GetFileMap() != nullptr && GetFileMap()->notMapped == NotMappedReason::None)
        {
            m_isFilesDefaulted       = true;
            m_context.isFilesOverlay = true;
            m_filesCheck->SetChecked (true);
        }

        m_imageTab->SetRows (m_context.hasDisk ? InspectorTables::BuildImage (m_analysis.image, m_context.drive != nullptr ? m_analysis.copy.get() : nullptr, m_context.drive)
                                               : vector<TableRow>());
        RefreshFileList();

        //  Casso does not analyze 3.5" disks; such a WOZ opens on the Image
        //  tab, and every other view is covered by a note (FR-054).
        m_is35 = m_context.hasDisk && m_analysis.image.isWoz && m_analysis.image.info.diskType == WozLoader::kDiskType35;

        if (m_is35)
        {
            ShowDiskTab (kTabImage);
        }

        Layout (m_boundsDip, m_hostScaler);
    }

    if (m_isTablesDirty || qt != m_fieldsOf || track != m_fieldsTrack)
    {
        m_fieldsTab->SetRows (m_context.hasDisk ? InspectorTables::BuildFields (m_analysis, qt) : vector<TableRow>());
        m_fieldsOf    = qt;
        m_fieldsTrack = track;
        SyncTables();
    }

    m_isTablesDirty = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SyncTables
//
//  Each table selects the row for the window's selection.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SyncTables()
{
    int  qt     = m_model.GetQuarterTrack();
    int  sector = m_model.GetSectorIndex();



    m_tracksTab->SelectRowWhere ([qt] (const TableRow & row) { return row.quarterTrack == qt; });
    m_fieldsTab->SelectRowWhere ([sector] (const TableRow & row) { return sector >= 0 && row.sectorIndex == sector; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SelectFromRow
//
//  A row goes to what it refers to: a field's nibbles, a sector, or a
//  quarter track.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SelectFromRow (const TableRow & row)
{
    const TrackAnalysis *  track = nullptr;



    if (row.quarterTrack >= 0)
    {
        m_model.SelectQuarterTrack (row.quarterTrack);
        track = m_model.GetTrack();

        if (row.sectorIndex >= 0)
        {
            m_model.SelectSector (row.quarterTrack, row.sectorIndex);
        }
        else if (track != nullptr && row.field >= 0 && row.field < static_cast<int> (track->fields.size()))
        {
            m_model.SelectNibbles (row.quarterTrack, track->fields[row.field].firstNibble, track->fields[row.field].nibbleCount);
        }

        OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenDecodeSettings
//
//  The dialog opens on the settings in force for the selected track.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenDecodeSettings()
{
    HRESULT                   hr     = S_OK;
    DecodeSettingsDialog      dialog;
    DxuiWindow::CreateParams  params;



    dialog.Configure (m_theme, m_analysis.settings, m_model.GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack);

    params.title                    = L"Decode settings";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = DecodeSettingsDialog::kSizeDip;
    params.minSizeDip               = DecodeSettingsDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_theme);
    dialog.ShowModalDialog (IDOK);

    if (dialog.GetOutcome() != DecodeSettingsDialog::Outcome::Cancelled)
    {
        ApplySettings (dialog.GetSettings());
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ApplySettings
//
//  Every record is analyzed again under the new settings; each record's old
//  result shows until its new one arrives.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ApplySettings (const DecodeSettings & settings)
{
    //  Edits on sectors that no longer decode under the new settings are
    //  discarded after the analysis (FR-108).
    if (m_context.hasDisk && m_analysis.copy != nullptr && (m_edits.IsEmpty() ||
        ConfirmEdit (L"Decode settings", { L"Pending edits on sectors that no longer decode as editable under the new settings are discarded." }, L"Change settings")))
    {
        m_analysis.settings = settings;
        m_scheduler->Restart (m_analysis.copy, settings);
        m_comparison.ApplySettings (settings);
        m_comparison.MarkAChanged();
        m_isTablesDirty = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepRange
//
//  The timing range steps through ±1% to ±25% (FR-024).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepRange (int delta)
{
    m_rangeStep           = std::clamp (m_rangeStep + delta, 0, static_cast<int> (std::size (s_kRanges)) - 1);
    m_context.timingRange = s_kRanges[m_rangeStep];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsStripKey
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsStripKey (WPARAM vk)
{
    return vk == VK_OEM_PLUS || vk == VK_ADD || vk == VK_OEM_MINUS || vk == VK_SUBTRACT || vk == VK_LEFT || vk == VK_RIGHT || vk == '0' || vk == VK_NUMPAD0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenFindBar
//
//  The find bar opens on the last query, with the focus in its pattern.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenFindBar()
{
    if (!m_isFindOpen)
    {
        m_isFindOpen = true;
        m_findBar->SetQuery  (m_find.GetQuery());
        m_findBar->SetResult (m_find.GetIndex(), m_find.GetCount());
        m_findBar->SetVisible (true);
        SyncFindMarks();
        Layout (m_boundsDip, m_hostScaler);
    }

    m_keyTarget = KeyTarget::FindBar;
    m_findBar->SetInputFocused (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::CloseFindBar
//
//  The hits stay for F3 and the Find results tab.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::CloseFindBar()
{
    m_isFindOpen = false;
    m_findBar->SetInputFocused (false);
    m_findBar->SetVisible (false);
    SyncFindMarks();
    m_keyTarget = KeyTarget::Platter;
    Layout (m_boundsDip, m_hostScaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RunFind
//
//  The query's hits replace the last ones, and the first is selected. For a
//  pattern that cannot be read, the hits are cleared and the bar displays
//  an example of a valid pattern. The Find results tab is visible only
//  while there are hits.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RunFind (const SearchQuery & query)
{
    bool  isRead = m_find.Run (m_analysis, m_model.GetQuarterTrack(), query, m_scheduler->HasPending());



    if (!m_context.hasDisk)
    {
        m_find.Clear();
    }

    m_findBar->SetSearched (query);
    m_findBar->SetError (isRead ? std::wstring() : (query.kind == SearchKind::SectorText ? std::wstring (s_kpszFindBadText) : std::wstring (s_kpszFindBadHex)));

    RefreshFindResults();
    StepFind (1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RefreshFindResults
//
//  The Find results tab's rows for the hits; with none, a window showing
//  the tab goes back to Tracks.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RefreshFindResults()
{
    m_findResults->SetRows (InspectorTables::BuildSearchHits (m_find.GetHits(), m_context.hasDisk ? &m_analysis : nullptr, GetFileMap()));
    RebuildFindMarks();

    if (m_find.IsEmpty() && m_isDiskGroup && m_diskTab == kTabFindResults)
    {
        ShowDiskTab (kTabTracks);
    }
    else if (m_scaler.GetDpi() != 0)
    {
        Layout (m_boundsDip, m_hostScaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetSelectedEdit
//
//  The selected sector's pending edit, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int DiskInspectorWindow::GetSelectedEdit() const
{
    const TrackAnalysis *  track = m_model.GetTrack();



    return (track != nullptr) ? m_edits.Find (*track, m_model.GetSectorIndex()) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsSectorImage
//
//  A .dsk, .do or .po stores only sector data, so its checksum is always
//  recomputed (FR-098).
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsSectorImage() const
{
    DiskFormat  format = (m_analysis.copy != nullptr) ? m_analysis.copy->format : DiskFormat::Woz;



    return format == DiskFormat::Dsk || format == DiskFormat::Do || format == DiskFormat::Po;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ToggleEditing
//
//  Starting to edit begins an edit on the selected sector, selects its
//  first byte when no byte is selected, and moves the keys to the Sector
//  data tab.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ToggleEditing()
{
    const TrackAnalysis *  track = m_model.GetTrack();
    std::wstring           reason;
    int                    edit  = -1;



    if (m_byteView->IsEditing())
    {
        m_byteView->SetEditing (false);
    }
    else if (track != nullptr)
    {
        edit       = m_edits.Begin (*track, m_model.GetQuarterTrack(), m_model.GetSectorIndex(), reason);
        m_editNote = reason;

        if (edit >= 0)
        {
            m_byteView->SetEditing (true);
            m_keyTarget = KeyTarget::SectorData;

            if (m_model.GetFirstByte() < 0)
            {
                m_model.SelectBytes (0, 1, false);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::HandleEditKey
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::HandleEditKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = true;



    m_editNote.clear();

    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && ev.vk == 'Z')
    {
        (void) m_edits.Undo();
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && ev.vk == 'Y')
    {
        (void) m_edits.Redo();
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && ev.vk == 'V')
    {
        PasteIntoEdit();
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE)
    {
        ToggleEditing();
    }
    else
    {
        isHandled = m_byteView->OnEditKey (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PasteIntoEdit
//
//  From the cursor, in the column it is in; the bytes past $FF are counted
//  in the note.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PasteIntoEdit()
{
    std::wstring  text;
    int           overflow = 0;
    int           written  = 0;



    if (GetSelectedEdit() >= 0 && m_model.GetFirstByte() >= 0 && m_clipboard.GetText (GetHwnd(), text))
    {
        written    = m_edits.Paste (GetSelectedEdit(), m_model.GetFirstByte(), text, m_model.IsTextColumn(), overflow);
        m_editNote = (written == 0)  ? std::wstring (m_model.IsTextColumn() ? L"Nothing to paste: the clipboard has no printable text"
                                                                            : L"Nothing to paste: the clipboard has no pairs of hex digits")
                   : (overflow > 0)  ? InspectorText::FormatCount (overflow, L"byte", L"bytes") + L" did not fit before the end of the sector"
                                     : std::wstring();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::FollowEdits
//
//  After re-analysis, each pending edit moves with its field; the window
//  lists the edits whose sectors can no longer be edited (FR-108).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::FollowEdits()
{
    vector<PendingEdit>  discarded;
    std::wstring         list;



    m_edits.Follow (m_analysis, discarded);

    for (const PendingEdit & edit : discarded)
    {
        list += (list.empty() ? L"" : L", ") + InspectorFormat::FormatSector (edit.sectorNumber) + L" on track " + InspectorFormat::FormatQuarterTrack (edit.quarterTrack);
    }

    if (!list.empty())
    {
        m_editNote = L"Edits discarded, the sectors can no longer be edited: " + list;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateEditBar
//
//  While editing, moving to another editable sector begins an edit there,
//  so typing always goes to the selected sector.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateEditBar()
{
    const TrackAnalysis *  track  = m_model.GetTrack();
    std::wstring           reason = (track != nullptr) ? EditController::GetUneditableReason (*track, m_model.GetSectorIndex()) : std::wstring (L"No track");
    int                    edit   = GetSelectedEdit();
    EditBarState           state;
    EditController::Counts counts = m_edits.GetCounts();
    int                    width  = 0;
    int                    rows   = 0;
    PendingEdit::Sector    paired = {};



    if (m_byteView->IsEditing() && edit < 0 && reason.empty())
    {
        edit = m_edits.Begin (*track, m_model.GetQuarterTrack(), m_model.GetSectorIndex(), reason);
    }

    //  The Tracks tab counts the edited sectors of each track.
    m_isTablesDirty = m_isTablesDirty || (static_cast<int64_t> (counts.bytes) << 16 | counts.sectors) != m_editSig;
    m_editSig       = static_cast<int64_t> (counts.bytes) << 16 | counts.sectors;

    state.canEdit          = m_context.hasDisk && reason.empty();
    state.isEditing        = m_byteView->IsEditing();
    state.hasSectorEdit    = edit >= 0;
    state.isChecksumOn     = IsSectorImage() || edit < 0 || m_edits.GetEdit (edit).checksumMode == ChecksumMode::Recompute;
    state.isChecksumLocked = IsSectorImage();
    state.bytes            = counts.bytes;
    state.sectors          = counts.sectors;
    state.applyBlocked     = GetApplyBlock();
    state.canApply         = state.applyBlocked.empty() && m_applyRequest == 0;
    state.isWaiting        = m_isApplyWaiting;
    state.isMachinePaused  = m_isApplyPaused;
    state.canUseB          = reason.empty() && GetBSectorBytes (paired);
    state.canUndoApplied   = m_historyAt > 0 && m_applyRequest == 0 && state.applyBlocked.empty() && IsHistoryCurrent (m_history[m_historyAt - 1].countsAfter);
    state.canRedoApplied   = m_historyAt < static_cast<int> (m_history.size()) && m_applyRequest == 0 && state.applyBlocked.empty() &&
                             IsHistoryCurrent (m_history[m_historyAt].countsUndone);
    state.note             = !m_editNote.empty() ? m_editNote : (m_model.GetSector() != nullptr && !reason.empty()) ? L"Cannot edit: " + reason : std::wstring();

    //  A change of the bar's rows moves the bytes under it.
    width = m_editBar->GetBounds().right - m_editBar->GetBounds().left;
    rows  = m_editBar->GetRowCount (width, m_scaler);
    m_editBar->SetState (state);
    m_isLayoutStale = m_isLayoutStale || (width > 0 && m_editBar->GetRowCount (width, m_scaler) != rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ConfirmEdit
//
//  A modal question with the lines given and a button for the action and
//  one for Cancel; true for the action.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::ConfirmEdit (const std::wstring & title, const vector<std::wstring> & lines, const std::wstring & action)
{
    static constexpr int  kWidthDip  = 560;
    static constexpr int  kChromeDip = 108;
    static constexpr int  kMaxDip    = 640;
    static constexpr int  kYes       = 1;



    auto                                content = std::make_unique<DialogBodyContent>();
    MessageDialog                       dialog;
    DxuiWindow::CreateParams            params;
    vector<DialogTextRun>               runs;
    HRESULT                             hr      = S_OK;
    int                                 result  = 0;



    for (const std::wstring & line : lines)
    {
        runs.push_back ({ line + L"\n" });
    }

    content->SetRuns (runs);
    params.initialSizeDip = { kWidthDip, std::min (kMaxDip, kChromeDip + content->GetPreferredHeightDip()) };

    dialog.Configure (std::move (content), { { action, kYes, true, false }, { L"Cancel", 0, false, true } }, 0);

    params.title                    = title;
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_theme);
    result = dialog.TranslateResult (dialog.ShowModalDialog (dialog.GetDefaultCommandId()));

Error:
    return result == kYes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ConfirmDiscardEdits
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::ConfirmDiscardEdits (const std::wstring & action)
{
    EditController::Counts  counts    = m_edits.GetCounts();
    bool                    isGoingOn = counts.sectors == 0;



    if (!isGoingOn)
    {
        isGoingOn = ConfirmEdit (L"Pending edits", { EditBar::FormatCount (counts.bytes, counts.sectors) + L" and not applied.",
                                                     action + L" discards these edits." }, L"Discard edits");
    }

    if (isGoingOn && counts.sectors > 0)
    {
        m_edits      = EditController();
        m_editNote   = action + L" discarded the pending edits.";
        m_statusNote = m_editNote;
        m_statusText->SetText (m_statusNote);
        m_byteView->SetEditing (false);
    }

    return isGoingOn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetApplyBlock
//
//  Why "Apply", "Undo applied edit" and "Redo applied edit" cannot change
//  the disk now, with every write-protect cause listed, or empty
//  (FR-102).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskInspectorWindow::GetApplyBlock() const
{
    const DriveHeadState *  state  = m_headState.get();
    std::wstring            causes;



    if (!IsWindowDiskA() || state == nullptr || !state->hasDisk)
    {
        causes = L"Only the disk in the drive can be changed";
    }
    else
    {
        causes = InspectorText::FormatWriteProtect (state->writeProtect);
        causes = causes.empty() ? causes : L"Write-protected: " + causes;
    }

    return causes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsHistoryCurrent
//
//  An applied edit can be undone, or an undone one redone, only while the
//  guest has not written to any record it changed since (FR-107).
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsHistoryCurrent (const vector<RecordWriteCount> & counts) const
{
    return std::all_of (counts.begin(), counts.end(), [this] (const RecordWriteCount & count)
                        { return count.slot >= 0 && count.slot < static_cast<int> (m_writeCounts.size()) && m_writeCounts[count.slot] == count.guestWriteCount; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ApplyEdits
//
//  Nothing changes until the user confirms the list of changes (FR-100).
//  The edits then go to the emulation thread, which applies them at once
//  when the drive is not turning, or waits for it to stop (FR-103).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ApplyEdits()
{
    InspectorRequest       request;
    vector<std::wstring>   lines;
    std::wstring           records;
    EditController::Counts counts  = m_edits.GetCounts();
    bool                   isWoz1  = m_analysis.copy != nullptr && m_analysis.copy->format == DiskFormat::Woz && m_analysis.copy->woz.info.version < 2;
    bool                   isNib   = m_analysis.copy != nullptr && m_analysis.copy->format == DiskFormat::Nib;
    HRESULT                hr      = S_OK;



    request.kind      = InspectorRequestKind::Apply;
    request.drive     = m_drive;
    request.mediaId   = m_analysis.mediaId;
    request.applyKind = ApplyKind::Apply;
    request.applyMode = ApplyMode::WaitForStop;
    request.writes    = m_edits.BuildWrites (m_analysis);

    BAIL_OUT_IF (request.writes.empty(), S_OK);

    lines.push_back (L"Apply " + EditBar::FormatCount (counts.bytes, counts.sectors).substr (0, EditBar::FormatCount (counts.bytes, counts.sectors).size() - std::wstring (L" changed").size())
                     + L" and save " + GetFileName() + L"?");

    for (const PendingEdit & edit : m_edits.GetEdits())
    {
        const TrackAnalysis *  track = (edit.slot >= 0 && edit.slot < static_cast<int> (m_analysis.tracks.size())) ? m_analysis.tracks[edit.slot].get() : nullptr;
        vector<int>            readers;

        if (edit.CountChanged() == 0 || track == nullptr)
        {
            continue;
        }

        for (int qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            if (m_analysis.entries[qt].slot == edit.slot)
            {
                readers.push_back (qt);
            }
        }

        records = L"Sector " + InspectorFormat::FormatSector (edit.sectorNumber) + L", read at track " + InspectorFormat::FormatQuarterTrackList (readers);
        records += edit.isOutOfDate ? L": changed since editing began, so only the bytes you changed are written" : L"";
        records += m_analysis.entries[readers.empty() ? 0 : readers.front()].isFromFluxMap ? L"; only the flux record changes, and the bit record is left as it was" : L"";
        records += (isNib && m_writtenSlots.contains (edit.slot)) ? L"; the guest also wrote this track, so Casso rebuilds it when saving" : L"";
        lines.push_back (records);

        request.expectedCounts.push_back ({ edit.slot, track->guestWriteCount });

        for (const LocatedField & field : track->fields)
        {
            if (field.startCell == edit.fieldStartCell && track->framed.cellCount > 0)
            {
                request.spans.push_back ({ edit.slot, static_cast<double> (field.startCell) / track->framed.cellCount, static_cast<double> (field.endCell) / track->framed.cellCount });
            }
        }
    }

    if (isWoz1)
    {
        lines.push_back (L"This disk is a WOZ 1 image. Casso saves it as WOZ 2.");
    }

    if (!m_isFirstApplyDone)
    {
        lines.push_back (L"DOS or ProDOS may have one of these sectors in memory and write its own copy back over the edit.");
    }

    if (ConfirmEdit (L"Apply edits", lines, L"Apply"))
    {
        PostApply (request);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PostApply
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PostApply (const InspectorRequest & request)
{
    m_lastApply      = request;
    m_applyRequest   = m_host->PostInspectorRequest (request);
    m_isApplyWaiting = false;
    m_editNote       = L"Applying...";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PauseApply
//
//  The waiting apply again, made at the next safe point with the machine
//  running on, or for a paused machine with it paused again afterward.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PauseApply()
{
    InspectorRequest  cancel;
    InspectorRequest  request = m_lastApply;



    cancel.kind     = InspectorRequestKind::CancelApply;
    cancel.drive    = m_drive;
    request.applyMode = m_isApplyPaused ? ApplyMode::RunToSafePointStayPaused : ApplyMode::PauseAtSafePoint;

    m_applyRequest = 0;
    (void) m_host->PostInspectorRequest (cancel);
    PostApply (request);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::CancelApply
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::CancelApply()
{
    InspectorRequest  cancel;



    cancel.kind  = InspectorRequestKind::CancelApply;
    cancel.drive = m_drive;
    (void) m_host->PostInspectorRequest (cancel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UndoApplied
//
//  Undo and redo each restore tracks exactly as the history has them, once the
//  user confirms (FR-107).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UndoApplied()
{
    InspectorRequest  request;
    HRESULT           hr = S_OK;



    BAIL_OUT_IF (m_historyAt <= 0, S_OK);

    request.kind           = InspectorRequestKind::Apply;
    request.drive          = m_drive;
    request.mediaId        = m_analysis.mediaId;
    request.applyKind      = ApplyKind::Undo;
    request.restore        = m_history[m_historyAt - 1].before;
    request.expectedCounts = m_history[m_historyAt - 1].countsAfter;

    if (ConfirmEdit (L"Undo applied edit", { L"Put back the sectors the last apply changed, and save " + GetFileName() + L"?" }, L"Undo"))
    {
        PostApply (request);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RedoApplied
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RedoApplied()
{
    InspectorRequest  request;
    HRESULT           hr = S_OK;



    BAIL_OUT_IF (m_historyAt >= static_cast<int> (m_history.size()), S_OK);

    request.kind           = InspectorRequestKind::Apply;
    request.drive          = m_drive;
    request.mediaId        = m_analysis.mediaId;
    request.applyKind      = ApplyKind::Redo;
    request.restore        = m_history[m_historyAt].after;
    request.expectedCounts = m_history[m_historyAt].countsUndone;

    if (ConfirmEdit (L"Redo applied edit", { L"Apply the undone edit again, and save " + GetFileName() + L"?" }, L"Redo"))
    {
        PostApply (request);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeApplyReply
//
//  The window marks an edit as applied only once the file has it (FR-100);
//  the changed tracks then go into the window's copy, so only they are
//  analyzed again. A new apply clears the redo history (FR-107).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeApplyReply (const InspectorReply & reply)
{
    InspectorReply  tracks;
    bool            isApplied = reply.applyOutcome == ApplyOutcome::Applied || reply.applyOutcome == ApplyOutcome::AppliedToPreservedCopy;



    m_isApplyWaiting = reply.applyOutcome == ApplyOutcome::Waiting;
    m_isApplyPaused  = reply.isMachinePaused;

    switch (reply.applyOutcome)
    {
        case ApplyOutcome::Waiting:   m_editNote = m_isApplyPaused ? L"The machine is paused with the head over an edited field" : L"Waiting for the drive to stop"; break;
        case ApplyOutcome::Cancelled: m_editNote = L"Apply canceled; the edits are still pending";                                                               break;
        case ApplyOutcome::WaitEnded: m_editNote = L"The guest wrote to a track these edits change. Check the edits and apply again.";                           break;
        case ApplyOutcome::NotApplied: m_editNote = reply.reason;                                                                                               break;
        default:                       break;
    }

    if (!m_isApplyWaiting)
    {
        m_applyRequest = 0;
    }

    if (isApplied)
    {
        if (reply.applyKind == ApplyKind::Apply)
        {
            m_history.resize (m_historyAt);
            m_history.push_back ({ reply.before, reply.after, reply.countsAfter, {}, reply.savedPath });
            m_historyAt        = static_cast<int> (m_history.size());
            m_edits            = EditController();
            m_isFirstApplyDone = true;
            m_byteView->SetEditing (false);
        }
        else if (reply.applyKind == ApplyKind::Undo)
        {
            m_historyAt--;
            m_history[m_historyAt].countsUndone = reply.countsAfter;
        }
        else
        {
            m_history[m_historyAt].countsAfter = reply.countsAfter;
            m_historyAt++;
        }

        for (const std::shared_ptr<const TrackCopy> & copy : reply.after)
        {
            m_appliedSlots.insert (copy->slot);
        }

        m_isTablesDirty = true;

        m_editNote = (reply.applyOutcome == ApplyOutcome::AppliedToPreservedCopy)
                         ? L"Saved to a preserved copy, since the file changed outside Casso: " + std::filesystem::path (TextEncoding::Utf8ToWide (reply.savedPath)).filename().wstring()
                         : std::wstring ((reply.applyKind == ApplyKind::Undo) ? L"Undone" : (reply.applyKind == ApplyKind::Redo) ? L"Redone" : L"Applied") + L" and saved to "
                           + std::filesystem::path (TextEncoding::Utf8ToWide (reply.savedPath)).filename().wstring();

        tracks.mediaId = reply.mediaId;
        tracks.tracks  = reply.after;
        TakeWrittenTracks (tracks);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetBSectorBytes
//
//  B's bytes for the sector paired with A's selected one, while A is the
//  window's disk as it is now (FR-123).
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::GetBSectorBytes (PendingEdit::Sector & outBytes) const
{
    const TrackAnalysis *  trackA  = m_model.GetTrack();
    const TrackAnalysis *  trackB  = m_context.trackB;
    int                    paired  = -1;
    bool                   isFound = false;



    if (m_comparison.IsComparing() && m_sourceA == ComparisonSource { ComparisonSourceKind::DriveNow, m_drive } && trackA != nullptr && trackB != nullptr &&
        m_model.GetSectorIndex() >= 0)
    {
        paired  = DiskComparer::FindPairedSector (*trackA, m_model.GetSectorIndex(), *trackB);
        isFound = paired >= 0 && trackB->sectors[paired].dataField >= 0;

        if (isFound)
        {
            outBytes = trackB->fields[trackB->sectors[paired].dataField].data.bytes;
        }
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UseBBytes
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UseBBytes()
{
    const TrackAnalysis *  track = m_model.GetTrack();
    PendingEdit::Sector    bytes = {};
    std::wstring           reason;
    int                    edit  = -1;



    if (track != nullptr && GetBSectorBytes (bytes))
    {
        edit       = m_edits.Begin (*track, m_model.GetQuarterTrack(), m_model.GetSectorIndex(), reason);
        m_editNote = reason;

        if (edit >= 0)
        {
            (void) m_edits.SetSector (edit, bytes);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SaveEditedCopy
//
//  "Save edited copy..." (FR-101): the emulation thread builds the copy's
//  bytes from the drive's disk and the pending edits, and the reply opens
//  the Save dialog.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SaveEditedCopy()
{
    InspectorRequest  request;



    request.kind      = InspectorRequestKind::SaveCopy;
    request.drive     = m_drive;
    request.mediaId   = m_analysis.mediaId;
    request.writes    = m_edits.BuildWrites (m_analysis);
    m_saveCopyRequest = m_host->PostInspectorRequest (request);
    m_editNote        = L"Preparing the copy...";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeSaveCopyReply
//
//  The copy goes to a new file the user chooses, never the disk's file
//  or one in a drive, whole or not at all through DurableCommit. The
//  pending edits stay pending, and the drive's disk and its file are left
//  as they were.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeSaveCopyReply (const InspectorReply & reply)
{
    HRESULT                hr        = S_OK;
    Win32HostDialogs       dialogs;
    Win32DiskFileIo        fileIo;
    FileDialogSpec         spec;
    CommitPlan::Progress   progress;
    std::filesystem::path  image     = std::filesystem::path (TextEncoding::Utf8ToWide (m_analysis.copy != nullptr ? m_analysis.copy->fileName : std::string()));
    std::filesystem::path  target;
    std::wstring           extension = image.has_extension() ? image.extension().wstring().substr (1) : std::wstring (L"dsk");
    bool                   isPicked  = false;
    bool                   isTaken   = true;
    bool                   isDamaged = m_analysis.copy != nullptr && (!m_analysis.copy->damagedTracks.empty() || !m_analysis.copy->damagedQuarterTracks.empty());



    m_saveCopyRequest = 0;
    m_editNote.clear();

    if (reply.status != InspectorReplyStatus::Done || reply.sourceBytes == nullptr)
    {
        m_editNote = L"Not saved: " + (reply.reason.empty() ? std::wstring (L"the disk in the drive changed") : reply.reason);
        BAIL_OUT_IF (true, S_OK);
    }

    if (isDamaged)
    {
        BAIL_OUT_IF (!ConfirmEdit (L"Save edited copy", { L"This image is damaged. The copy gets a new checksum, and each damaged track record is saved as Casso read it.",
                                                         L"Save the copy?" }, L"Save"), S_OK);
    }

    extension             = (m_analysis.copy != nullptr && m_analysis.copy->format == DiskFormat::Woz) ? std::wstring (L"woz") : extension;
    spec.filters          = { { L"Disk images", L"*." + extension } };
    spec.defaultExtension = extension;
    spec.defaultFileName  = image.stem().wstring() + L" edited." + extension;
    spec.initialFolder    = image.parent_path();

    //  Open the Save dialog again until the file is neither the disk's file
    //  nor one in a drive.
    while (isTaken)
    {
        hr = dialogs.PickFileToSave (GetHwnd(), spec, target, isPicked);
        CHR (hr);
        BAIL_OUT_IF (!isPicked, S_OK);

        isTaken = IsSameFile (target, image);

        for (const std::string & mounted : reply.mountedPaths)
        {
            isTaken = isTaken || IsSameFile (target, std::filesystem::path (TextEncoding::Utf8ToWide (mounted)));
        }

        if (isTaken)
        {
            (void) DxuiMessageBox (GetHwnd(), m_theme, L"That file is a disk in a drive. Choose another file for the copy.", L"Save edited copy", MB_OK | MB_ICONWARNING);
        }
    }

    hr = DurableCommit::Commit (fileIo, TextEncoding::WideToUtf8 (target.wstring()), *reply.sourceBytes, GetTickCount64(),
                                std::filesystem::exists (target) ? CommitMode::Replace : CommitMode::CreateNew, progress);

    m_editNote = SUCCEEDED (hr) ? L"Saved a copy with the pending edits to " + target.filename().wstring()
                                : L"Not saved: the copy could not be written to " + target.filename().wstring();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsSameFile
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsSameFile (const std::filesystem::path & a, const std::filesystem::path & b)
{
    std::error_code  error;
    bool             isSame = std::filesystem::equivalent (a, b, error);



    return (!error && isSame) || _wcsicmp (std::filesystem::absolute (a, error).wstring().c_str(), std::filesystem::absolute (b, error).wstring().c_str()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RebuildFindMarks
//
//  The marks for the hits whose record has not been analyzed again since the
//  search. A hit on a record analyzed again refers to an analysis that is
//  gone, so it is left out before anything reads through it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RebuildFindMarks()
{
    m_findMarks.Build (m_find.GetCurrentHits (m_analysis), GetFileMap(), m_find.GetQuery().text);
    SyncFindMarks();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RepeatFind
//
//  The last search again over the whole analysis, without moving the
//  selection. The selected hit's index is clamped to the new count.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RepeatFind()
{
    m_find.Repeat (m_analysis, m_scheduler->HasPending());
    RefreshFindResults();
    m_findBar->SetResult (m_find.GetIndex(), m_find.GetCount());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SyncFindMarks
//
//  The views mark the hits while the find bar is open (FR-056), and the
//  Tracks tab counts them.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SyncFindMarks()
{
    m_context.findMarks = (m_isFindOpen && !m_findMarks.IsEmpty()) ? &m_findMarks : nullptr;
    m_isTablesDirty     = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetShownDiskTabs
//
//  The disk tabs in the strip, in order: Differences only while comparing,
//  and Find results only with hits.
//
////////////////////////////////////////////////////////////////////////////////

vector<int> DiskInspectorWindow::GetShownDiskTabs() const
{
    vector<int>  shown = { kTabTracks, kTabFindings, kTabFileMap, kTabImage };



    if (m_comparison.IsComparing())
    {
        shown.push_back (kTabDifferences);
    }

    if (!m_find.IsEmpty())
    {
        shown.push_back (kTabFindResults);
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetTabIndex
//
//  The chosen tab's place in the strip.
//
////////////////////////////////////////////////////////////////////////////////

int DiskInspectorWindow::GetTabIndex() const
{
    vector<int>  shown = GetShownDiskTabs();
    auto         at    = std::find (shown.begin(), shown.end(), m_diskTab);



    return !m_isDiskGroup ? m_trackTab : kTrackTabCount + static_cast<int> ((at != shown.end()) ? at - shown.begin() : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenGoTo
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenGoTo()
{
    HRESULT                   hr     = S_OK;
    GoToDialog                dialog;
    DxuiWindow::CreateParams  params;
    GoToTarget                target;



    dialog.Configure (m_theme, m_analysis, m_model.GetQuarterTrack(), m_goToKind);

    params.title                    = L"Go to";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = GoToDialog::kSizeDip;
    params.minSizeDip               = GoToDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_theme);
    dialog.ShowModalDialog (IDOK);

    m_goToKind = dialog.GetKind();
    target     = dialog.GetTarget();

    if (dialog.IsChosen() && target.firstNibble >= 0)
    {
        m_model.SelectNibbles (target.quarterTrack, target.firstNibble, 1);
        OnSelection();
    }
    else if (dialog.IsChosen() && target.sectorIndex >= 0)
    {
        m_model.SelectSector (target.quarterTrack, target.sectorIndex);
        OnSelection();
    }
    else if (dialog.IsChosen())
    {
        m_model.SelectQuarterTrack (target.quarterTrack);
        OnSelection();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepFind
//
//  Find next and Find previous, wrapping around the hits.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepFind (int step)
{
    int  index = m_find.Step (step);



    if (index >= 0)
    {
        SelectHit (m_find.GetHits()[index]);
        m_findResults->SelectRowWhere ([index] (const TableRow & row) { return row.finding == index; });
    }

    m_findBar->SetResult (index, m_find.GetCount());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SelectHit
//
//  A sector hit selects its sector; a nibble hit its nibbles, and one off
//  the framed nibbles the nibble its first cell falls in.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SelectHit (const SearchHit & hit)
{
    const TrackAnalysis *  track  = nullptr;
    int                    nibble = hit.firstNibble;



    if (hit.sectorIndex >= 0)
    {
        m_model.SelectSector (hit.quarterTrack, hit.sectorIndex);
    }
    else
    {
        m_model.SelectQuarterTrack (hit.quarterTrack);
        track  = m_model.GetTrack();
        nibble = (!hit.isAligned && track != nullptr) ? PlatterGeometry::GetNibbleAt (*track, hit.cell) : nibble;
        m_model.SelectNibbles (hit.quarterTrack, nibble, hit.nibbleCount);
    }

    OnSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetKeyTarget
//
//  The view under a press, for the keys that act on the view pressed last.
//
////////////////////////////////////////////////////////////////////////////////

KeyTarget DiskInspectorWindow::GetKeyTarget (POINT pointPx) const
{
    KeyTarget  target = KeyTarget::Platter;



    if (m_isFindOpen && IsInside (m_findBar->GetBounds(), pointPx))
    {
        target = KeyTarget::FindBar;
    }
    else if (IsInside (m_stripView->GetBounds(), pointPx) || (m_stripB->IsVisible() && IsInside (m_stripB->GetBounds(), pointPx)) || IsInside (m_stripOut->GetBounds(), pointPx) || IsInside (m_stripIn->GetBounds(), pointPx) ||
        IsInside (m_stripWhole->GetBounds(), pointPx))
    {
        target = KeyTarget::Strip;
    }
    else if (IsInside (m_sectorRow->GetBounds(), pointPx))
    {
        target = KeyTarget::SectorRow;
    }
    else if (m_byteView->IsVisible() && IsInside (m_byteView->GetBounds(), pointPx))
    {
        target = KeyTarget::SectorData;
    }
    else if (m_nibblesTab->IsVisible() && IsInside (m_nibblesTab->GetBounds(), pointPx))
    {
        target = KeyTarget::Nibbles;
    }
    else if (m_fieldsTab->IsVisible() && IsInside (m_fieldsTab->GetBounds(), pointPx))
    {
        target = KeyTarget::Fields;
    }
    else if (m_fluxTab->IsVisible() && IsInside (m_fluxTab->GetBounds(), pointPx))
    {
        target = KeyTarget::FluxTiming;
    }
    else if (m_tracksTab->IsVisible() && IsInside (m_tracksTab->GetBounds(), pointPx))
    {
        target = KeyTarget::Tracks;
    }
    else if (m_findingsTab->IsVisible() && IsInside (m_findingsTab->GetBounds(), pointPx))
    {
        target = KeyTarget::Findings;
    }
    else if (m_imageTab->IsVisible() && IsInside (m_imageTab->GetBounds(), pointPx))
    {
        target = KeyTarget::Image;
    }
    else if (m_fileList->IsVisible() && IsInside (m_fileList->GetBounds(), pointPx))
    {
        target = KeyTarget::FileList;
    }
    else if (m_diffsTab->IsVisible() && IsInside (m_diffsTab->GetBounds(), pointPx))
    {
        target = KeyTarget::Differences;
    }
    else if (m_findResults->IsVisible() && IsInside (m_findResults->GetBounds(), pointPx))
    {
        target = KeyTarget::FindResults;
    }

    return target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SelectAll
//
//  The sector's bytes in the Sector data tab, the track's nibbles in the
//  Nibbles tab and the strip (FR-043).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SelectAll()
{
    if (m_keyTarget == KeyTarget::SectorData)
    {
        m_model.SelectAllBytes();
    }
    else if (m_keyTarget == KeyTarget::Nibbles || m_keyTarget == KeyTarget::Strip)
    {
        m_model.SelectAllNibbles();
        OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ExtendSelection
//
//  Shift with an arrow moves the end of the selection away from where it
//  started: a byte or a nibble across, a row up or down.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ExtendSelection (WPARAM vk)
{
    const TrackAnalysis *  track  = m_model.GetTrack();
    bool                   isByte = m_keyTarget == KeyTarget::SectorData;
    int                    row    = isByte ? SectorByteView::kBytesPerRow : std::max (m_nibblesTab->GetPerRow(), 1);
    int                    first  = isByte ? m_model.GetFirstByte()  : m_model.GetFirstNibble();
    int                    count  = isByte ? m_model.GetByteCount()  : m_model.GetNibbleCount();
    int                    anchor = isByte ? m_model.GetByteAnchor() : m_model.GetNibbleAnchor();
    int                    last   = isByte ? DiskFieldFormat::kSectorBytes - 1 : (track != nullptr ? static_cast<int> (track->framed.nibbles.size()) - 1 : -1);
    int                    end    = (first == anchor) ? first + count - 1 : first;
    int                    step   = (vk == VK_LEFT) ? -1 : (vk == VK_RIGHT) ? 1 : (vk == VK_UP) ? -row : row;



    if (first >= 0 && count > 0 && last >= 0)
    {
        if (isByte)
        {
            m_model.ExtendBytes (std::clamp (end + step, 0, last));
        }
        else
        {
            m_model.ExtendNibbles (std::clamp (end + step, 0, last));
            OnSelection();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Copy
//
//  Whatever the view pressed last has selected, as the window contract
//  gives it (FR-057): bytes as hex or text, nibbles as hex, a table's row,
//  the histogram; the sector as a hex dump when nothing smaller is chosen.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Copy()
{
    const TrackAnalysis *   track  = m_model.GetTrack();
    const AnalyzedSector *  sector = m_model.GetSector();
    std::wstring            text;



    switch (m_keyTarget)
    {
        case KeyTarget::Fields:     text = m_fieldsTab->GetSelectedText();   break;
        case KeyTarget::Tracks:     text = m_tracksTab->GetSelectedText();   break;
        case KeyTarget::Findings:   text = m_findingsTab->GetSelectedText(); break;
        case KeyTarget::Image:      text = m_imageTab->GetSelectedText();    break;
        case KeyTarget::FileList:   text = m_fileList->GetSelectedText();    break;
        case KeyTarget::Differences: text = m_diffsTab->GetSelectedText();   break;
        case KeyTarget::FindResults: text = m_findResults->GetSelectedText(); break;
        case KeyTarget::FluxTiming: text = m_fluxTab->GetHistogramText();    break;

        case KeyTarget::Strip:
        case KeyTarget::Nibbles:
            text = (track != nullptr && m_model.GetNibbleCount() > 0) ? InspectorClipboard::FormatNibbles (*track, m_model.GetFirstNibble(), m_model.GetNibbleCount())
                                                                      : std::wstring();
            break;

        case KeyTarget::SectorData:
            if (track != nullptr && sector != nullptr && sector->dataField >= 0 && m_model.GetByteCount() > 0)
            {
                std::span<const Byte>  bytes = std::span<const Byte> (track->fields[sector->dataField].data.bytes).subspan (m_model.GetFirstByte(), m_model.GetByteCount());

                text = m_model.IsTextColumn() ? InspectorClipboard::FormatText (bytes) : InspectorClipboard::FormatHex (bytes);
            }

            break;

        default:
            break;
    }

    if (text.empty())
    {
        CopySector();
    }
    else
    {
        (void) m_clipboard.SetText (GetHwnd(), text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::CopySector
//
//  "Copy sector": the whole sector as a hex dump (FR-057).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::CopySector()
{
    const TrackAnalysis *   track  = m_model.GetTrack();
    const AnalyzedSector *  sector = m_model.GetSector();



    if (track != nullptr && sector != nullptr && sector->dataField >= 0)
    {
        (void) m_clipboard.SetText (GetHwnd(), InspectorClipboard::FormatHexDump (track->fields[sector->dataField].data.bytes));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenExport
//
//  "Export..." (FR-058): the dialog builds the bytes; a list of what was
//  written as decoded or as zeros is shown first and can stop it; the save
//  dialog picks the file, its own prompt confirming a replace; and the
//  bytes go through DurableCommit. The image is never touched.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenExport()
{
    HRESULT                   hr        = S_OK;
    ExportDialog              dialog;
    DxuiWindow::CreateParams  params;
    Win32HostDialogs          dialogs;
    Win32DiskFileIo           fileIo;
    FileDialogSpec            spec;
    CommitPlan::Progress      progress;
    std::filesystem::path     image;
    std::filesystem::path     target;
    std::wstring              list;
    bool                      isPicked  = false;
    bool                      isGoingOn = false;



    image = (m_analysis.copy != nullptr) ? std::filesystem::path (TextEncoding::Utf8ToWide (m_analysis.copy->fileName)) : std::filesystem::path (L"Disk");
    dialog.Configure (m_theme, m_analysis, m_model.GetQuarterTrack(), m_model.GetSectorIndex(), image.stem().wstring());

    params.title                    = L"Export";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = ExportDialog::kSizeDip;
    params.minSizeDip               = ExportDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_theme);
    dialog.ShowModalDialog (IDOK);
    BAIL_OUT_IF (!dialog.IsChosen(), S_OK);

    for (const std::wstring & note : dialog.GetRequest().notes)
    {
        list += note + L"\n";
    }

    isGoingOn = list.empty() || DxuiMessageBox (GetHwnd(), m_theme, (L"The export will include these as noted:\n\n" + list + L"\nExport anyway?").c_str(),
                                                L"Export", MB_OKCANCEL | MB_ICONWARNING) == IDOK;
    BAIL_OUT_IF (!isGoingOn, S_OK);

    spec.filters          = { { dialog.GetRequest().extension == L"woz" ? L"WOZ images" : L"Binary files", L"*." + dialog.GetRequest().extension } };
    spec.defaultExtension = dialog.GetRequest().extension;
    spec.defaultFileName  = dialog.GetRequest().fileName;
    spec.initialFolder    = image.parent_path();

    hr = dialogs.PickFileToSave (GetHwnd(), spec, target, isPicked);
    CHR (hr);
    BAIL_OUT_IF (!isPicked, S_OK);

    hr = DurableCommit::Commit (fileIo, TextEncoding::WideToUtf8 (target.wstring()), dialog.GetRequest().bytes, GetTickCount64(),
                                std::filesystem::exists (target) ? CommitMode::Replace : CommitMode::CreateNew, progress);

    if (FAILED (hr))
    {
        (void) DxuiMessageBox (GetHwnd(), m_theme, (L"The export could not be written to " + target.wstring() + L".").c_str(), L"Export", MB_OK | MB_ICONWARNING);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowContextMenu
//
//  The Sector data, Nibbles and Tracks tabs' right-click menu: Copy, "Copy
//  sector" in the Sector data tab, and "Export..." (FR-057, FR-058).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowContextMenu (POINT pointPx)
{
    DxuiHwndSource                  * host   = GetPopupHost();
    std::vector<DxuiPopupMenuItem>    items;
    std::shared_ptr<DxuiCommand>      copy   = std::make_shared<DxuiCommand>();
    std::shared_ptr<DxuiCommand>      sector = std::make_shared<DxuiCommand>();
    std::shared_ptr<DxuiCommand>      save   = std::make_shared<DxuiCommand>();



    copy->label          = L"Copy";
    copy->accelerator    = L"Ctrl+C";
    copy->dispatch       = [this] () { Copy(); };
    sector->label        = L"Copy sector";
    sector->accelerator  = L"Ctrl+Shift+C";
    sector->dispatch     = [this] () { CopySector(); };
    sector->isEnabled    = [this] () { return m_model.GetSector() != nullptr && m_model.GetSector()->dataField >= 0; };
    save->label          = L"Export...";
    save->dispatch       = [this] () { OpenExport(); };

    items.push_back (DxuiPopupMenuItem::ForCommand (copy));

    if (m_keyTarget == KeyTarget::SectorData)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (sector));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (save));

    m_menuCommands = { copy, sector, save };

    if (host != nullptr)
    {
        DxuiContextMenu::Show (*host, pointPx.x, pointPx.y, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetButtonTip
//
//  Every button's tooltip (FR-059), with the key that does the same, or
//  for one that is unavailable, why.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskInspectorWindow::GetButtonTip (POINT pointPx, RECT & outAnchorPx) const
{
    //  Each control's tip, and for one that can be unavailable, why it is.
    struct ButtonTip
    {
        const IDxuiControl  * control;
        LPCWSTR               text;
        LPCWSTR               whyUnavailable;
    };

    const ButtonTip  tips[] =
    {
        { m_zoomOut,         L"Zoom the platter out (minus)",                                    L"The whole disk is shown, so the platter cannot zoom out further" },
        { m_zoomIn,          L"Zoom the platter in (plus)",                                      nullptr },
        { m_fit,             L"Show the whole disk (0)",                                         L"The whole disk is already shown" },
        { m_alignmentCheck,  L"Mark where sector 0 and the longest sync start on each track",   nullptr },
        { m_filesCheck,      L"Color each sector's data field by its role in the file map",     nullptr },
        { m_readsCheck,      L"Mark each quarter track the drive has read since the disk was inserted", nullptr },
        { m_mapPrev,         L"Select the previous sector in the selected file",                 nullptr },
        { m_filePicker,      L"Choose the file whose sectors the map marks",                      nullptr },
        { m_mapNext,         L"Select the next sector in the selected file",                     nullptr },
        { m_stripOut,        L"Zoom the strip out",                                              L"The strip already shows the whole track" },
        { m_stripIn,         L"Zoom the strip in",                                               nullptr },
        { m_stripWhole,      L"Show the whole track in the strip",                               L"The strip already shows the whole track" },
        { m_rangeDown,       L"Narrow the timing range",                                         nullptr },
        { m_rangeUp,         L"Widen the timing range",                                          nullptr },
        { m_goToButton,      L"Go to a track, sector, block, nibble or cell (Ctrl+G)",           nullptr },
        { m_findButton,      L"Find nibbles, bytes or text (Ctrl+F)",                            nullptr },
        { m_copySector,      L"Copy the whole sector as a hex dump (Ctrl+Shift+C)",              L"The selected sector has no data field to copy" },
        { m_exportButton,    L"Save sectors, nibbles or this quarter track's bits to a file",    nullptr },
        { m_decodeButton,    L"Change the marks and checks used to decode tracks",               nullptr },
        { m_compareButton,   L"Compare two disks track by track and file by file",               nullptr },
        { m_followCheck,     L"Show the track the head is on",                                   nullptr },
        { m_goToHead,        L"Select the track the head is on now",                             L"There is no disk in the drive" },
        { m_prevDiff,        L"Go to the previous difference",                                   L"There are no differences to step through" },
        { m_nextDiff,        L"Go to the next difference",                                       L"There are no differences to step through" },
        { m_swapButton,      L"Make A the disk B is, and B the disk A is",                       L"B is still being read" },
        { m_bSettingsButton, L"Give B decode settings of its own",                               L"B is still being read" },
        { m_stopButton,      L"End the comparison",                                              nullptr },
        { m_diffsCheck,      L"Mark the quarter tracks that differ",                             nullptr },
    };


    RECT          modes = m_modeTabs->GetBounds();
    std::wstring  tip   = m_editBar->IsVisible() ? m_editBar->GetTip (pointPx, outAnchorPx) : std::wstring();



    for (const ButtonTip & entry : tips)
    {
        if (tip.empty() && entry.control != nullptr && entry.control->IsVisible() && IsInside (entry.control->GetBounds(), pointPx))
        {
            tip         = (entry.control->IsEnabled() || entry.whyUnavailable == nullptr) ? entry.text : entry.whyUnavailable;
            outAnchorPx = entry.control->GetBounds();
        }
    }

    //  Each tab says what its content is (FR-059).
    if (tip.empty() && m_tabs->IsVisible() && IsInside (m_tabs->GetBounds(), pointPx))
    {
        int  index = m_tabs->HitTest (pointPx.x, pointPx.y);

        if (index >= 0 && index < static_cast<int> (std::size (s_kpszTabTips)))
        {
            tip         = s_kpszTabTips[index];
            outAnchorPx = m_tabs->GetTabs()[index].rect;
        }
    }

    if (tip.empty() && m_modeTabs->IsVisible() && IsInside (modes, pointPx))
    {
        tip         = (pointPx.x < (modes.left + modes.right) / 2) ? L"Color the platter by what each cell contains" : L"Color flux tracks by how fast or slow each cell runs";
        outAnchorPx = modes;
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepKeyTarget
//
//  F6 and Shift+F6: the keys move to the next or previous view shown.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepKeyTarget (int step)
{
    const std::array<std::pair<KeyTarget, const IDxuiControl *>, 11>  order =
    {{
        { KeyTarget::Platter,    m_platterView },
        { KeyTarget::Strip,      m_stripView },
        { KeyTarget::SectorRow,  m_sectorRow },
        { KeyTarget::SectorData, m_byteView },
        { KeyTarget::Nibbles,    m_nibblesTab },
        { KeyTarget::Fields,     m_fieldsTab },
        { KeyTarget::FluxTiming, m_fluxTab },
        { KeyTarget::Tracks,     m_tracksTab },
        { KeyTarget::Findings,   m_findingsTab },
        { KeyTarget::Differences, m_diffsTab },
        { KeyTarget::FindResults, m_findResults },
    }};



    int  count = static_cast<int> (order.size());
    int  at    = 0;
    int  k     = 0;



    for (k = 0; k < count; k++)
    {
        at = (order[k].first == m_keyTarget) ? k : at;
    }

    for (k = 1; k < count; k++)
    {
        const auto &  next = order[(at + step * k + count * k) % count];

        if (next.second != nullptr && next.second->IsVisible())
        {
            m_keyTarget = next.first;
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetKeyTargetBounds
//
////////////////////////////////////////////////////////////////////////////////

RECT DiskInspectorWindow::GetKeyTargetBounds() const
{
    RECT  bounds = m_platterView->GetBounds();



    switch (m_keyTarget)
    {
        case KeyTarget::Strip:      bounds = m_stripView->GetBounds();   break;
        case KeyTarget::SectorRow:  bounds = m_sectorRow->GetBounds();   break;
        case KeyTarget::SectorData: bounds = m_byteView->GetBounds();    break;
        case KeyTarget::Nibbles:    bounds = m_nibblesTab->GetBounds();  break;
        case KeyTarget::Fields:     bounds = m_fieldsTab->GetBounds();   break;
        case KeyTarget::FluxTiming: bounds = m_fluxTab->GetBounds();     break;
        case KeyTarget::Tracks:     bounds = m_tracksTab->GetBounds();   break;
        case KeyTarget::Findings:   bounds = m_findingsTab->GetBounds(); break;
        case KeyTarget::Differences: bounds = m_diffsTab->GetBounds();   break;
        case KeyTarget::FindResults: bounds = m_findResults->GetBounds(); break;
        default:                                                         break;
    }

    return bounds;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PaintNot35
//
//  On a 3.5" disk, a note over every view but the Image tab (FR-054).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PaintNot35 (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    static constexpr LPCWSTR  kpszNote = L"Casso does not analyze 3.5\" disks. The Image tab lists what the file contains.";



    RECT  empty = {};



    for (const RECT & area : { m_platterAreaPx, m_trackAreaPx, m_stripAreaPx, IsImageShown() ? empty : m_diskContentPx })
    {
        if (!IsRectEmpty (&area))
        {
            painter.FillRect (static_cast<float> (area.left), static_cast<float> (area.top), static_cast<float> (area.right - area.left),
                              static_cast<float> (area.bottom - area.top), theme.Background());
            text.DrawString (kpszNote, static_cast<float> (area.left), static_cast<float> (area.top), static_cast<float> (area.right - area.left),
                             static_cast<float> (area.bottom - area.top), theme.ForegroundMuted(), m_scaler.ToPxf (InspectorView::kTextDip),
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, true);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsCovered
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsCovered (POINT pointPx) const
{
    return m_is35 && (IsInside (m_platterAreaPx, pointPx) || IsInside (m_trackAreaPx, pointPx) || IsInside (m_stripAreaPx, pointPx) ||
                      (!IsImageShown() && IsInside (m_diskContentPx, pointPx)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsImageShown
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsImageShown() const
{
    return m_isDiskGroup && m_diskTab == kTabImage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetFileMap
//
//  The map of the volume chosen in the File map tab, or none while the
//  disk is still being analyzed.
//
////////////////////////////////////////////////////////////////////////////////

const FileMap * DiskInspectorWindow::GetFileMap() const
{
    const vector<FileMap> &  maps = m_analysis.fileMaps;



    return (m_context.hasDisk && !maps.empty()) ? &maps[std::clamp (m_mapIndex, 0, static_cast<int> (maps.size()) - 1)] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::LayoutFileMap
//
//  Two rows of controls, the grid, then the file list; a disk that is not
//  mapped gives the grid the whole room.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::LayoutFileMap (const RECT & area, const DxuiDpiScaler & scaler)
{
    const FileMap *            map     = GetFileMap();
    int                        row     = scaler.ToPx (s_kRowDip);
    int                        margin  = scaler.ToPx (s_kMarginDip);
    int                        check   = scaler.ToPx (s_kMapCheckDip);
    int                        button  = scaler.ToPx (s_kMapButtonDip);
    int                        volume  = scaler.ToPx (s_kMapVolumeDip);
    int                        label   = scaler.ToPx (s_kMapStepLabelDip);
    int                        picker  = GetPickerWidthPx (scaler);
    int                        y       = area.top;
    int                        width   = 0;
    int                        gridH   = 0;
    bool                       isList  = map != nullptr && map->notMapped == NotMappedReason::None;
    vector<DxuiTabStrip::Tab>  tabs;



    //  The step buttons, their label and the file picker, the volume tabs,
    //  and "Copy map" at the right, on one row.
    m_mapPrev->Layout      ({ area.left,          y, area.left + button,     y + row }, scaler);
    m_mapNext->Layout      ({ area.left + button, y, area.left + 2 * button, y + row }, scaler);
    m_mapStepLabel->Layout ({ area.left + 2 * button + margin, y, area.left + 2 * button + margin + label, y + row }, scaler);
    m_filePicker->Layout   ({ area.left + 2 * button + margin + label, y, area.left + 2 * button + margin + label + picker, y + row }, scaler);
    m_mapCopy->Layout      ({ area.right - scaler.ToPx (s_kExportButtonDip), y, area.right, y + row }, scaler);

    for (size_t v = 0; v < m_analysis.fileMaps.size() && m_analysis.fileMaps.size() > 1; v++)
    {
        int  left = area.left + 2 * button + 2 * margin + label + picker + static_cast<int> (v) * volume;

        tabs.push_back ({ { left, y, left + volume, y + row }, FileMapText::FormatVolume (m_analysis.fileMaps[v]) });
    }

    m_mapVolumes->SetTabs     (std::move (tabs));
    m_mapVolumes->SetSelected (m_mapIndex);
    m_mapVolumes->Layout      ({ area.left + 2 * button + 2 * margin + label + picker, y, area.right - scaler.ToPx (s_kExportButtonDip) - margin, y + row }, scaler);
    y += row + margin / 2;

    //  Open, the picker has the list's filters and the file list in the rest
    //  of the tab; closed, the tab has the grid, its rows no shorter than its
    //  labels, with a scrollbar at the right when it does not fit.
    width = area.right - area.left;

    if (isList && m_isPickerOpen)
    {
        m_mapDeleted->Layout ({ area.left,         y, area.left + check,     y + row }, scaler);
        m_mapBadOnly->Layout ({ area.left + check, y, area.left + 2 * check, y + row }, scaler);
        y += row + margin / 2;

        m_mapScroll->PlaceChild (*m_fileList, { area.left, y, area.right, area.bottom });
        m_mapScroll->PlaceChild (*m_mapGrid,  { area.left, y, area.right, y });
    }
    else if (m_mapGrid->IsZoomed())
    {
        //  Zoomed, the grid fills the view and pans in it.
        m_mapScroll->PlaceChild (*m_mapGrid,  { area.left, y, area.right, area.bottom });
        m_mapScroll->PlaceChild (*m_fileList, { area.left, area.bottom, area.right, area.bottom });
    }
    else
    {
        gridH = isList ? m_mapGrid->GetHeightFor (width) : area.bottom - y;

        if (isList && gridH > area.bottom - y)
        {
            width -= scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip) + margin / 2;
            gridH  = m_mapGrid->GetHeightFor (width);
        }

        m_mapScroll->PlaceChild (*m_mapGrid,  { area.left, y, area.left + width, y + gridH });
        m_mapScroll->PlaceChild (*m_fileList, { area.left, y + gridH, area.left + width, y + gridH });
    }

    m_mapScroll->SetLineStepPx (scaler.ToPx (s_kRowDip));
    m_mapScroll->Layout        ({ area.left, y, area.right, area.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowFileMap
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowFileMap (bool isShown)
{
    const FileMap *  map    = GetFileMap();
    bool             isList = isShown && map != nullptr && map->notMapped == NotMappedReason::None;



    bool             isOpen = isList && m_isPickerOpen;



    m_mapScroll->SetVisible  (isShown);
    m_mapGrid->SetVisible    (isShown && !isOpen);
    m_fileList->SetVisible   (isOpen);
    m_mapDeleted->SetVisible (isOpen);
    m_mapBadOnly->SetVisible (isOpen);
    m_filePicker->SetVisible (isList);

    //  The step buttons are enabled only with a file chosen and the picker
    //  closed.
    m_mapPrev->SetEnabled (!m_isPickerOpen && m_mapFile >= 0);
    m_mapNext->SetEnabled (!m_isPickerOpen && m_mapFile >= 0);
    m_mapPrev->SetVisible    (isList);
    m_mapNext->SetVisible    (isList);
    m_mapStepLabel->SetVisible (isList);
    m_mapCopy->SetVisible    (isList);
    m_mapVolumes->SetVisible (isShown && m_analysis.fileMaps.size() > 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenFilePicker
//
//  The file picker opens to the file list and closes to the chosen file's
//  map, as a drop-down list does.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenFilePicker (bool isOpen)
{
    m_isPickerOpen = isOpen;
    ShowFileMap (m_isDiskGroup && m_diskTab == kTabFileMap);
    Layout      (m_boundsDip, m_hostScaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PaintPicker
//
//  The picker's label at its left and a chevron at its right, as in a
//  drop-down list: pointing down while the picker is closed and up while it
//  is open. A label of a new width causes a relayout of the row, so the
//  picker is as wide as its label and chevron.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PaintPicker (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT   b      = m_filePicker->GetBounds();
    float  half   = m_scaler.ToPxf (static_cast<float> (s_kChevronDip));
    float  cx     = static_cast<float> (b.right) - 3 * half;
    float  cy     = (b.top + b.bottom) / 2.0f;
    float  tip    = m_isPickerOpen ? -half / 2 : half / 2;
    float  textPx = m_scaler.ToPxf (DxuiTheme::kBodySizeDip);
    float  w      = 0;
    float  h      = 0;



    text.MeasureString (m_pickerLabel.c_str(), textPx, DxuiTheme::kBodyFace, w, h);

    if (std::abs (w - m_pickerTextPx) > 0.5f)
    {
        m_pickerTextPx  = w;
        m_isLayoutStale = true;
    }

    text.DrawString (m_pickerLabel.c_str(), static_cast<float> (b.left) + m_scaler.ToPxf (static_cast<float> (s_kPickerPadDip)), static_cast<float> (b.top),
                     static_cast<float> (b.right - b.left), static_cast<float> (b.bottom - b.top), theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

    painter.DrawLine (cx - half, cy - tip, cx,        cy + tip, 1.5f, theme.Foreground());
    painter.DrawLine (cx,        cy + tip, cx + half, cy - tip, 1.5f, theme.Foreground());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SetPickerLabel
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SetPickerLabel (const std::wstring & label)
{
    m_pickerLabel   = label;
    m_isLayoutStale = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetPickerWidthPx
//
//  The label's measured width, or before it has been drawn a guess from its
//  length, with padding and room for the chevron.
//
////////////////////////////////////////////////////////////////////////////////

int DiskInspectorWindow::GetPickerWidthPx (const DxuiDpiScaler & scaler) const
{
    float  label = (m_pickerTextPx > 0) ? m_pickerTextPx : static_cast<float> (m_pickerLabel.size()) * scaler.ToPxf (static_cast<float> (s_kCharWidthDip));



    return static_cast<int> (std::ceil (label)) + scaler.ToPx (2 * s_kPickerPadDip + 4 * s_kChevronDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RefreshFileList
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RefreshFileList()
{
    const FileMap *  map = GetFileMap();



    m_mapFile = (map != nullptr && m_mapFile < static_cast<int> (map->files.size())) ? m_mapFile : -1;
    m_mapGrid->SetMap (map);

    if (map != nullptr)
    {
        m_fileList->SetColumns (FileMapText::GetFileColumns (*map));
        m_fileList->SetRows    (FileMapText::BuildFileRows (*map, m_mapDeleted->IsChecked(), m_mapBadOnly->IsChecked(), m_fileSortColumn, m_fileSortDescending));
        m_fileList->SetCaption (map->isCatalogComplete ? L"" : L"The catalog is incomplete: " + FileMapText::FormatCell (*map, map->unreadableCell) + L" could not be read");
    }
    else
    {
        m_fileList->SetRows (vector<TableRow>());
    }

    m_context.fileMap      = map;
    m_context.selectedFile = m_mapFile;
    SetPickerLabel ((map != nullptr && m_mapFile >= 0) ? map->files[m_mapFile].path : std::wstring (L"Choose a file"));
    ShowFileMap (m_isDiskGroup && m_diskTab == kTabFileMap);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ChooseFile
//
//  A file chosen in the list: its sectors marked, and its first selected
//  in every view (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ChooseFile (int file)
{
    const FileMap *  map = GetFileMap();



    m_mapFile              = file;
    m_context.selectedFile = file;

    //  A chosen file closes the picker, which then has the file's name.
    SetPickerLabel ((map != nullptr && file >= 0) ? map->files[file].path : std::wstring (L"Choose a file"));
    OpenFilePicker (false);

    for (const FilePlace & place : (map != nullptr && file >= 0) ? map->files[file].sectors : vector<FilePlace>())
    {
        if (place.cell >= 0)
        {
            SelectMapCell (place.cell);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SelectMapCell
//
//  The physical sector that contains a cell, or the first half of a block, on
//  its whole track, from the first field with that number and an address
//  field that reads (FR-090).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SelectMapCell (int cell)
{
    const FileMap *        map      = GetFileMap();
    int                    qt       = 0;
    int                    physical = 0;
    int                    found    = -1;
    const TrackAnalysis *  track    = nullptr;



    if (map != nullptr && cell >= 0)
    {
        qt       = map->GetTrack (cell) * DiskImage::kQuarterTracksPerWholeTrack;
        physical = map->GetPhysical (cell, 0);

        m_model.SelectQuarterTrack (qt);
        track = m_model.GetTrack();

        for (size_t s = 0; track != nullptr && found < 0 && s < track->sectors.size(); s++)
        {
            const AnalyzedSector &  sector = track->sectors[s];

            found = (sector.sector == physical && (sector.isAddressGood || !sector.isAddressCheck)) ? static_cast<int> (s) : -1;
        }

        if (found >= 0)
        {
            m_model.SelectSector (qt, found);
        }

        OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepFileSector
//
//  "Next sector in file" and "Previous sector in file", across tracks.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepFileSector (int step)
{
    const FileMap *  map     = GetFileMap();
    vector<int>      cells;
    int              current = GetSelectedMapCell();
    int              at      = -1;



    for (const FilePlace & place : (map != nullptr && m_mapFile >= 0) ? map->files[m_mapFile].sectors : vector<FilePlace>())
    {
        if (place.cell >= 0)
        {
            at = (place.cell == current && at < 0) ? static_cast<int> (cells.size()) : at;
            cells.push_back (place.cell);
        }
    }

    if (!cells.empty())
    {
        SelectMapCell (cells[std::clamp (at < 0 ? 0 : at + step, 0, static_cast<int> (cells.size()) - 1)]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetSelectedMapCell
//
////////////////////////////////////////////////////////////////////////////////

int DiskInspectorWindow::GetSelectedMapCell() const
{
    const FileMap *         map    = GetFileMap();
    const AnalyzedSector *  sector = m_model.GetSector();
    int                     qt     = m_model.GetQuarterTrack();
    int                     half   = 0;



    return (map != nullptr && sector != nullptr && qt % DiskImage::kQuarterTracksPerWholeTrack == 0)
               ? map->GetCellOf (qt / DiskImage::kQuarterTracksPerWholeTrack, sector->sector, half) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SyncFileMapSelection
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SyncFileMapSelection()
{
    int   cell   = GetSelectedMapCell();
    RECT  target = {};



    m_mapGrid->SetSelected (cell, m_mapFile);

    //  A sector selected anywhere is scrolled to the middle of the grid's
    //  view, once, or panned there in a zoomed grid.
    if (cell >= 0 && cell != m_revealedCell && m_mapGrid->GetCellRect (cell, target) && m_mapGrid->IsZoomed())
    {
        RECT  view = m_mapGrid->GetBounds();

        m_mapGrid->ShiftPan ((target.left + target.right) / 2 - (view.left + view.right) / 2, (target.top + target.bottom) / 2 - (view.top + view.bottom) / 2);
    }
    else if (cell >= 0 && cell != m_revealedCell && m_mapGrid->GetCellRect (cell, target))
    {
        RECT  view = m_mapScroll->GetViewportPx();

        m_mapScroll->SetScrollPosPx (m_mapScroll->GetScrollPosPx() + (target.top + target.bottom) / 2 - (view.top + view.bottom) / 2);
    }

    m_revealedCell = cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsOverMapGrid
//
//  Whether the point is on the File map grid in view: the File map tab
//  chosen, the picker closed, and the point in the grid's viewport.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsOverMapGrid (POINT pointPx) const
{
    RECT  grid = m_mapGrid->GetBounds();



    return m_mapGrid->IsVisible() && !m_isPickerOpen && IsInside (m_mapScroll->GetViewportPx(), pointPx) && PtInRect (&grid, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ZoomMapGrid
//
//  The grid zoomed about the point by the wheel's notches. Entering or
//  leaving the zoom changes the grid's bounds, from its whole height in the
//  scrolling view to the view itself, so the pan shifts by as much as the
//  bounds moved and the point stays under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ZoomMapGrid (float notches, POINT pointPx)
{
    RECT  before   = m_mapGrid->GetBounds();
    RECT  after    = {};
    bool  isZoomed = m_mapGrid->IsZoomed();



    m_mapGrid->ZoomAbout (std::pow (kZoomStep, notches), pointPx);

    if (m_mapGrid->IsZoomed() != isZoomed)
    {
        m_mapScroll->SetScrollPosPx (0);
        Layout (m_boundsDip, m_hostScaler);

        after = m_mapGrid->GetBounds();
        m_mapGrid->ShiftPan (after.left - before.left, after.top - before.top);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::CopyFileMap
//
//  "Copy map" (FR-095).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::CopyFileMap()
{
    const FileMap *  map = GetFileMap();



    if (map != nullptr)
    {
        (void) m_clipboard.SetText (GetHwnd(), FileMapText::FormatMap (*map));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeComparison
//
//  Once a frame, after A's results: the comparison takes its reads and B's
//  results, hands over A's disk when A was read from another source, and
//  compares once both sides are analyzed. A new result rebuilds the tables.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeComparison()
{
    vector<LoadedDisk>  loads;
    bool                isAReady = m_context.hasDisk && m_pendingRequest == 0 && !m_scheduler->HasPending();



    m_comparison.Tick (m_analysis, isAReady, loads);

    for (LoadedDisk & loaded : loads)
    {
        StartDisk (loaded.copy, loaded.reason);
    }

    m_context.comparison = (m_comparison.IsComparing() && m_comparison.HasResult()) ? &m_comparison.GetResult() : nullptr;
    m_context.diffCells  = (m_comparison.IsComparing() && !m_diffCells.empty()) ? &m_diffCells : nullptr;
    m_context.analysisB  = m_comparison.IsComparing() ? &m_comparison.GetB() : nullptr;

    if (m_comparison.GetVersion() != m_comparisonVersion)
    {
        m_comparisonVersion = m_comparison.GetVersion();
        m_diffIndex         = std::min (m_diffIndex, static_cast<int> (m_comparison.GetListed().size()) - 1);
        m_isTablesDirty     = true;
    }

    CheckComparedSources();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::CheckComparedSources
//
//  Ends the comparison when B is the other drive and its disk is ejected or
//  replaced. When a compared image file is deleted or renamed, a note
//  appears in the status band and the comparison is not cleared. File
//  existence is checked once a second.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::CheckComparedSources()
{
    static constexpr int64_t  kCheckMs = 1000;



    int64_t                   now   = static_cast<int64_t> (GetTickCount64());
    const ComparisonSource  & b     = m_comparison.GetSourceB();
    uint64_t                  media = m_comparison.GetB().mediaId;
    std::error_code           error;



    if (!m_comparison.IsComparing())
    {
        return;
    }

    if (b.kind == ComparisonSourceKind::DriveNow && b.drive != m_drive && media != 0)
    {
        std::shared_ptr<const DriveHeadState>  state = m_host->GetHeadState (b.drive);

        if (state != nullptr && (!state->hasDisk || state->mediaId != media))
        {
            EndComparison (std::format (L"The comparison ended. The disk in drive {} was ejected or replaced.", b.drive + 1));
        }
    }

    if (m_comparison.IsComparing() && now - m_sourceCheckMs >= kCheckMs)
    {
        m_sourceCheckMs = now;

        for (const ComparisonSource & source : { m_sourceA, b })
        {
            if (source.kind == ComparisonSourceKind::ImageFile && !source.path.empty() && !std::filesystem::exists (source.path, error))
            {
                m_statusNote = L"The compared file " + std::filesystem::path (source.path).filename().wstring() + L" is gone. This comparison is from before it was removed.";
                m_statusText->SetText (m_statusNote);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::EndComparison
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::EndComparison (const std::wstring & why)
{
    StopComparing();
    m_statusNote = why;
    m_statusText->SetText (why);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SyncSideB
//
//  Once a frame while comparing: B's view of the selected quarter track,
//  the nibble hunks and alignment of that track (again whenever either
//  side's record changes), and the two strips' zoom and pan linked, the
//  strip moved since the last frame leading (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SyncSideB()
{
    const DiskAnalysis &   b        = m_comparison.GetB();
    int                    qt       = m_model.GetQuarterTrack();
    const TrackAnalysis *  trackA   = m_context.hasDisk ? m_model.GetTrack() : nullptr;
    const TrackAnalysis *  trackB   = nullptr;
    bool                   isShown  = m_comparison.IsComparing() && b.copy != nullptr;
    bool                   isBMoved = false;
    int                    rotation = 0;



    m_contextB.palette      = m_context.palette;
    m_contextB.isTimingMode = m_context.isTimingMode;
    m_contextB.timingRange  = m_context.timingRange;
    m_contextB.hasDisk      = isShown;
    m_contextB.analysis     = &b;
    m_contextB.comparison   = m_context.comparison;

    if (isShown)
    {
        if (m_modelBDisk != b.mediaId)
        {
            m_modelB.SetDisk (b.mediaId);
            m_modelBDisk = b.mediaId;
            m_linkStartB = -1.0;
        }

        m_modelB.SetAnalysis (&b);

        if (m_modelB.GetQuarterTrack() != qt)
        {
            m_modelB.SelectQuarterTrack (qt);
        }

        trackB = m_modelB.GetTrack();
    }

    if (trackA != m_diffsOfA || trackB != m_diffsOfB)
    {
        m_nibbleDiffs.clear();
        m_bOffset = 0.0;

        if (trackA != nullptr && trackB != nullptr && !trackA->framed.nibbles.empty() && !trackB->framed.nibbles.empty())
        {
            DiskComparer::ListNibbles (*trackA, *trackB, qt, m_nibbleDiffs);

            rotation  = DiskComparer::Align (*trackA, *trackB);
            m_bOffset = TrackAnalyzer::GetAngle (*trackB, trackB->framed.nibbles[static_cast<size_t> (rotation) % trackB->framed.nibbles.size()].startCell) -
                        TrackAnalyzer::GetAngle (*trackA, trackA->framed.nibbles[0].startCell);
        }

        m_diffsOfA   = trackA;
        m_diffsOfB   = trackB;
        m_linkStartB = -1.0;
    }

    m_context.trackB       = trackB;
    m_context.nibbleDiffs  = (trackB != nullptr) ? &m_nibbleDiffs : nullptr;
    m_contextB.nibbleDiffs = m_context.nibbleDiffs;

    if (isShown)
    {
        isBMoved = m_linkStartB >= 0.0 && (m_modelB.GetStripStart() != m_linkStartB || m_modelB.GetStripSpan() != m_linkSpanB);

        if (isBMoved)
        {
            m_model.SetStrip (m_modelB.GetStripStart() - m_bOffset, m_modelB.GetStripSpan());
        }
        else
        {
            m_modelB.SetStrip (m_model.GetStripStart() + m_bOffset, m_model.GetStripSpan());
        }

        m_linkStartB = m_modelB.GetStripStart();
        m_linkSpanB  = m_modelB.GetStripSpan();
    }

    m_stripB->SetVisible (isShown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenCompare
//
//  "Compare with..." (FR-117): the dialog gives A's and B's sources. A that
//  is this window's drive keeps its disk, or reads it again after a swap;
//  any other A is read by the comparison and shown in its place.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenCompare()
{
    HRESULT                   hr      = S_OK;
    CompareDialog             dialog;
    DxuiWindow::CreateParams  params;
    ComparisonSource          b       = m_comparison.IsComparing() ? m_comparison.GetSourceB() : ComparisonSource { ComparisonSourceKind::ImageFile, 0 };
    ComparisonSource          own     = { ComparisonSourceKind::DriveNow, m_drive };
    bool                      isSame  = false;



    dialog.Configure (m_theme, std::max (1, m_host->GetDriveCount()), m_sourceA, b);

    params.title                    = L"Compare with";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = CompareDialog::kSizeDip;
    params.minSizeDip               = CompareDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_theme);
    dialog.ShowModalDialog (IDOK);
    BAIL_OUT_IF (!dialog.IsChosen(), S_OK);

    isSame    = dialog.GetSource (ComparisonSession::kSideA) == m_sourceA && m_context.hasDisk;
    m_sourceA = dialog.GetSource (ComparisonSession::kSideA);
    m_diffIndex = -1;

    m_comparison.Begin (m_sourceA, dialog.GetSource (ComparisonSession::kSideB), !isSame && !(m_sourceA == own), m_analysis.settings, *m_host);

    if (!isSame && m_sourceA == own)
    {
        RequestCopy();
    }

    m_isTablesDirty = true;
    ShowDiskTab (kTabDifferences);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StopComparing
//
//  The window goes back to its own disk, read again when A was another.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StopComparing()
{
    bool  isOwn = IsWindowDiskA();



    m_comparison.End();
    m_diffCells.clear();
    m_context.comparison = nullptr;
    m_context.analysisB  = nullptr;
    m_diffIndex          = -1;
    m_isTablesDirty      = true;

    if (!isOwn)
    {
        (void) ShowDrive (m_drive);
    }

    ShowDiskTab (m_diskTab == kTabDifferences ? kTabTracks : m_diskTab);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SwapSides
//
//  "Swap A and B": the views show what was B, whose rings are built anew.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SwapSides()
{
    int  slot = 0;



    m_comparison.Swap (m_analysis, m_scheduler, m_sourceA);

    m_context.hasDisk = m_analysis.copy != nullptr;
    m_model.SetDisk     (m_analysis.mediaId);
    m_model.SetAnalysis (m_context.hasDisk ? &m_analysis : nullptr);
    m_levels.fill (nullptr);
    m_timingLevels.fill (nullptr);
    m_changedSlots.clear();

    for (slot = 0; slot < static_cast<int> (m_analysis.tracks.size()); slot++)
    {
        m_changedSlots.insert (slot);
    }

    m_diffIndex     = -1;
    m_isTablesDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenBSettings
//
//  B's decode settings of its own (FR-117), which the window's settings no
//  longer change.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenBSettings()
{
    HRESULT                   hr     = S_OK;
    DecodeSettingsDialog      dialog;
    DxuiWindow::CreateParams  params;



    dialog.Configure (m_theme, m_comparison.GetB().settings, m_model.GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack);

    params.title                    = L"B's decode settings";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = DecodeSettingsDialog::kSizeDip;
    params.minSizeDip               = DecodeSettingsDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_theme);
    dialog.ShowModalDialog (IDOK);

    if (dialog.GetOutcome() != DecodeSettingsDialog::Outcome::Cancelled)
    {
        m_comparison.ApplyOwnSettings (dialog.GetSettings());
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepDifference
//
//  "Next difference" and "Previous difference" step through the listed
//  differences across the disk, wrapping at either end (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepDifference (int step)
{
    const vector<Difference> &  listed = m_comparison.GetListed();
    int                         count  = static_cast<int> (listed.size());



    if (count > 0)
    {
        m_diffIndex = (m_diffIndex < 0) ? (step > 0 ? 0 : count - 1) : (m_diffIndex + step + count) % count;

        SelectDifference (listed[m_diffIndex]);
        m_diffsTab->SelectRowWhere ([this] (const TableRow & row) { return row.finding == m_diffIndex; });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SelectDifference
//
//  A difference goes to what it is about on A: its nibbles, its sector, its
//  quarter track, or its file in the file map.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SelectDifference (const Difference & difference)
{
    const TrackAnalysis *  track  = nullptr;
    const FileMap *        map    = GetFileMap();
    int                    sector = -1;
    int                    file   = -1;
    size_t                 i      = 0;



    m_diffCells.clear();

    if (difference.quarterTrack >= 0)
    {
        m_model.SelectQuarterTrack (difference.quarterTrack);
        track = m_model.GetTrack();

        for (i = 0; track != nullptr && sector < 0 && difference.sector >= 0 && i < track->sectors.size(); i++)
        {
            sector = (track->sectors[i].sector == difference.sector) ? static_cast<int> (i) : -1;
        }

        if (difference.firstNibbleA >= 0 && difference.nibbleCountA > 0)
        {
            m_model.SelectNibbles (difference.quarterTrack, difference.firstNibbleA, difference.nibbleCountA);
        }
        else if (sector >= 0)
        {
            m_model.SelectSector (difference.quarterTrack, sector);
        }

        OnSelection();
    }
    else if (!difference.path.empty() && map != nullptr)
    {
        for (i = 0; file < 0 && i < map->files.size(); i++)
        {
            file = (map->files[i].path == difference.path && !map->files[i].isDeleted) ? static_cast<int> (i) : -1;
        }

        //  A file pair marks A's sectors that differ from B's (FR-120).
        for (const FilePair & pair : m_comparison.GetResult().files)
        {
            m_diffCells = (file >= 0 && pair.fileA == file) ? pair.differingCells : m_diffCells;
        }

        if (file >= 0)
        {
            ChooseFile (file);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PaintComparisonBar
//
//  A's and B's sources and file names (FR-121), with the reason a side
//  could not be read in place of its name.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PaintComparisonBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float             margin = m_scaler.ToPxf (static_cast<float> (s_kMarginDip));
    float             left   = static_cast<float> (m_barPx.left) + margin;
    float             right  = static_cast<float> (m_prevDiff->GetBounds().left) - margin;
    float             half   = std::max (0.0f, (right - left) / 2.0f);
    float             height = static_cast<float> (m_barPx.bottom - m_barPx.top);
    const DiskCopy  * copyB  = m_comparison.GetB().copy.get();
    std::wstring      nameB  = (copyB != nullptr) ? std::filesystem::path (TextEncoding::Utf8ToWide (copyB->fileName)).filename().wstring() : std::wstring();
    std::wstring   sideA  = std::format (L"A  {}  {}  {}", ComparisonText::FormatSource (m_comparison.GetSourceA()), s_kpszMiddleDot,
                                         m_context.hasDisk ? GetFileName() : m_comparison.GetAError());
    std::wstring   sideB  = std::format (L"B  {}  {}  {}", ComparisonText::FormatSource (m_comparison.GetSourceB()), s_kpszMiddleDot,
                                         m_comparison.GetBError().empty() ? (nameB.empty() ? std::wstring (s_kpszAnalyzing) : nameB) : m_comparison.GetBError());



    painter.FillRect (static_cast<float> (m_barPx.left), static_cast<float> (m_barPx.top), static_cast<float> (m_barPx.right - m_barPx.left), height,
                      theme.SelectionBackground());

    text.DrawString (sideA.c_str(), left, static_cast<float> (m_barPx.top), half, height, theme.Foreground(), m_scaler.ToPxf (InspectorView::kTextDip),
                     DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    text.DrawString (sideB.c_str(), left + half, static_cast<float> (m_barPx.top), half, height,
                     m_comparison.GetBError().empty() ? theme.Foreground() : theme.ErrorForeground(), m_scaler.ToPxf (InspectorView::kTextDip),
                     DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsWindowDiskA
//
//  True while A is the window's own drive as it is now, the one disk the
//  window can edit (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsWindowDiskA() const
{
    return m_sourceA == ComparisonSource { ComparisonSourceKind::DriveNow, m_drive };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TickHead
//
//  Once a frame: the drive's head as last published, and with Follow head
//  on, the head's track once the head has settled on it (FR-067).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TickHead()
{
    int            qt       = -1;
    bool           isOnDisk = false;
    InspectorHead  head;



    m_headState = m_host->GetHeadState (m_drive);

    //  A disk inserted, swapped or ejected has a new media id; a copy of it is
    //  requested once, and the reply replaces the analysis on screen.
    if (m_headState != nullptr && IsWindowDiskA() && m_pendingRequest == 0 && m_headState->mediaId != m_analysis.mediaId &&
        m_headState->mediaId != m_requestedMediaId)
    {
        m_requestedMediaId = m_headState->mediaId;
        RequestCopy();
    }

    RequestWrittenTracks();

    isOnDisk = m_headState != nullptr && m_headState->hasDisk && m_headState->mediaId == m_analysis.mediaId && IsWindowDiskA();

    if (isOnDisk)
    {
        qt = m_followHead.Tick (m_headState->headQuarterTrack, static_cast<int64_t> (GetTickCount64()));

        head.isShown      = true;
        head.quarterTrack = m_headState->headQuarterTrack;
        head.turn         = m_headState->angle;
        head.activity     = m_headState->activity;
    }

    m_context.head  = head;
    m_context.drive = isOnDisk ? m_headState.get() : nullptr;

    //  While the drive turns, the platter turns under the head at 12
    //  o'clock. Zoomed in, the platter does not turn and the head moves
    //  around it (FR-064).
    if (isOnDisk && m_headState->isTurning && m_model.IsAtFit())
    {
        m_model.SetRotation (-m_headState->angle);
    }

    if (qt >= 0)
    {
        SelectHeadTrack (qt);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SetFollowHead
//
//  Turning Follow head on selects the head's track at once.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SetFollowHead (bool isOn)
{
    int  head = (m_headState != nullptr) ? m_headState->headQuarterTrack : m_model.GetQuarterTrack();
    int  qt   = m_followHead.SetOn (isOn, head, static_cast<int64_t> (GetTickCount64()));



    if (qt >= 0)
    {
        SelectHeadTrack (qt);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GoToHead
//
//  "Go to head": the head's track, once, leaving Follow head as it is.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::GoToHead()
{
    if (m_headState != nullptr && m_headState->hasDisk)
    {
        SelectHeadTrack (m_headState->headQuarterTrack);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SelectHeadTrack
//
//  A selection made for the head rather than by hand, so Follow head stays.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SelectHeadTrack (int quarterTrack)
{
    m_isHeadSelecting = true;
    m_model.SelectQuarterTrack (quarterTrack);
    OnSelection();
    m_isHeadSelecting = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RequestWrittenTracks
//
//  The records the guest has written since the window's copy, from the
//  published write counts, copied again at most twice a second so a track
//  being written continuously still shows within half a second (FR-069).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RequestWrittenTracks()
{
    const DriveHeadState *  state = m_headState.get();
    int64_t                 now   = static_cast<int64_t> (GetTickCount64());
    InspectorRequest        request;
    size_t                  slot  = 0;



    if (state != nullptr && m_analysis.copy != nullptr && state->mediaId == m_analysis.mediaId && IsWindowDiskA() && state->countsGeneration != m_countsGeneration)
    {
        for (slot = 0; slot < state->recordWriteCounts.size() && slot < m_analysis.copy->tracks.size(); slot++)
        {
            if (slot >= m_writeCounts.size() || state->recordWriteCounts[slot] != m_writeCounts[slot])
            {
                m_writtenSlots.insert (static_cast<int> (slot));
            }
        }

        m_writeCounts      = state->recordWriteCounts;
        m_countsGeneration = state->countsGeneration;
        m_isTablesDirty    = true;
    }

    if (!m_writtenSlots.empty() && m_trackRequest == 0 && now - m_trackRequestMs >= s_kRecopyMs)
    {
        request.kind     = InspectorRequestKind::CopyTracks;
        request.drive    = m_drive;
        request.mediaId  = m_analysis.mediaId;
        request.slots.assign (m_writtenSlots.begin(), m_writtenSlots.end());
        m_trackRequest   = m_host->PostInspectorRequest (request);
        m_trackRequestMs = now;
        m_writtenSlots.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeWrittenTracks
//
//  The written records go into a new copy of the disk, sharing every other
//  record with the old one, and only they are analyzed again. A reply for a
//  disk no longer in the drive starts the window over on the new one.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeWrittenTracks (const InspectorReply & reply)
{
    vector<int>  slots;



    if (reply.status == InspectorReplyStatus::DiskChanged || reply.status == InspectorReplyStatus::NoDisk)
    {
        RequestCopy();
    }
    else if (m_analysis.copy != nullptr && reply.mediaId == m_analysis.mediaId && !reply.tracks.empty())
    {
        auto  copy = std::make_shared<DiskCopy> (*m_analysis.copy);

        for (const std::shared_ptr<const TrackCopy> & track : reply.tracks)
        {
            if (track != nullptr && track->slot >= 0 && track->slot < static_cast<int> (copy->tracks.size()))
            {
                copy->tracks[track->slot] = track;
                slots.push_back (track->slot);
            }
        }

        m_analysis.copy = copy;
        m_scheduler->Update (copy, slots);
        m_isTablesDirty = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetToolItems
//
//  The toolbar's controls after the file name in FR-007's order, each with
//  its width; a control not shown takes no room.
//
////////////////////////////////////////////////////////////////////////////////

vector<DiskInspectorWindow::ToolItem> DiskInspectorWindow::GetToolItems (const DxuiDpiScaler & scaler) const
{
    vector<ToolItem>  items;
    bool              hasDisk = m_context.hasDisk;



    if (hasDisk)
    {
        items.push_back ({ ToolId::Modes, scaler.ToPx (2 * s_kModeTabDip) });
    }

    if (hasDisk && m_context.isTimingMode)
    {
        items.push_back ({ ToolId::Range, scaler.ToPx (s_kRangeDip) });
    }

    if (IsHeadShown())
    {
        items.push_back ({ ToolId::Follow,   scaler.ToPx (s_kFollowDip) });
        items.push_back ({ ToolId::GoToHead, scaler.ToPx (s_kGoToHeadDip) });
    }

    if (hasDisk)
    {
        items.push_back ({ ToolId::GoTo, scaler.ToPx (s_kFindButtonDip) });
        items.push_back ({ ToolId::Find, scaler.ToPx (s_kFindButtonDip) });
    }

    if (hasDisk || m_comparison.IsComparing())
    {
        items.push_back ({ ToolId::Compare, scaler.ToPx (s_kCompareDip) });
    }

    if (hasDisk)
    {
        items.push_back ({ ToolId::Export, scaler.ToPx (s_kExportButtonDip) });
        items.push_back ({ ToolId::Decode, scaler.ToPx (s_kDecodeButtonDip) });
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::FlowToolItems
//
//  On the first row, right-aligned after the file name, when every control
//  fits there; otherwise left to right on rows of their own below, a new
//  row taken whenever the next control would not fit. Returns the rows the
//  toolbar takes.
//
////////////////////////////////////////////////////////////////////////////////

int DiskInspectorWindow::FlowToolItems (vector<ToolItem> & inOutItems, int left, int firstLeft, int right, int top, int rowHeight, int gap)
{
    int  total = 0;
    int  rows  = 1;
    int  x     = 0;
    int  y     = top;



    for (const ToolItem & item : inOutItems)
    {
        total += item.widthPx + ((total > 0) ? gap : 0);
    }

    if (firstLeft + total <= right)
    {
        x = right - total;
    }
    else if (!inOutItems.empty())
    {
        x     = left;
        y    += rowHeight;
        rows += 1;
    }

    for (ToolItem & item : inOutItems)
    {
        if (y != top && x > left && x + item.widthPx > right)
        {
            x     = left;
            y    += rowHeight;
            rows += 1;
        }

        item.rect = { x, y, x + item.widthPx, y + rowHeight };
        x        += item.widthPx + gap;
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PlaceToolItem
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PlaceToolItem (const ToolItem & item, const DxuiDpiScaler & scaler)
{
    int                        inset  = scaler.ToPx (s_kMarginDip) / 2;
    int                        button = scaler.ToPx (s_kButtonDip);
    RECT                       r      = item.rect;
    RECT                       inner  = { r.left, r.top + inset, r.right, r.bottom - inset };
    vector<DxuiTabStrip::Tab>  tabs;



    switch (item.id)
    {
        case ToolId::Modes:
            tabs.push_back ({ { r.left,                               inner.top, r.left + scaler.ToPx (s_kModeTabDip), inner.bottom }, L"Structure" });
            tabs.push_back ({ { r.left + scaler.ToPx (s_kModeTabDip), inner.top, r.right,                              inner.bottom }, L"Timing" });
            m_modeTabs->SetTabs     (std::move (tabs));
            m_modeTabs->SetSelected (m_context.isTimingMode ? 1 : 0);
            m_modeTabs->Layout      (r, scaler);
            break;

        case ToolId::Range:
            m_rangeDown->Layout  ({ r.left,          r.top + 2 * inset, r.left + button,  r.bottom - 2 * inset }, scaler);
            m_rangeLabel->Layout ({ r.left + button, r.top + 2 * inset, r.right - button, r.bottom - 2 * inset }, scaler);
            m_rangeUp->Layout    ({ r.right - button, r.top + 2 * inset, r.right,         r.bottom - 2 * inset }, scaler);
            break;

        case ToolId::Follow:   m_followCheck->Layout   (inner, scaler); break;
        case ToolId::GoToHead: m_goToHead->Layout      (inner, scaler); break;
        case ToolId::GoTo:     m_goToButton->Layout    (inner, scaler); break;
        case ToolId::Find:     m_findButton->Layout    (inner, scaler); break;
        case ToolId::Compare:  m_compareButton->Layout (inner, scaler); break;
        case ToolId::Export:   m_exportButton->Layout  (inner, scaler); break;
        case ToolId::Decode:   m_decodeButton->Layout  (inner, scaler); break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetToolSig
//
//  Which toolbar controls are shown, so a change lays the toolbar out again.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskInspectorWindow::GetToolSig() const
{
    uint32_t  sig = 0;



    for (const ToolItem & item : GetToolItems (m_scaler))
    {
        sig |= 1u << static_cast<int> (item.id);
    }

    return sig;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsHeadShown
//
//  "Follow head" and "Go to head" are offered while Casso publishes the
//  head of the window's own drive.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsHeadShown() const
{
    return m_context.hasDisk && m_headState != nullptr && IsWindowDiskA();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowLegend
//
//  The legend in a popup above the "Legend" button (FR-030). A preview on
//  hover lets the pointer through and closes when the pointer leaves the
//  button; a pinned one, after a click, stays until the next click outside
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowLegend (bool isPinned)
{
    HRESULT                    hr       = S_OK;
    DxuiHwndSource           * host     = GetPopupHost();
    DxuiPopupHost::ShowParams  params;
    RECT                       button   = m_legendButton->GetBounds();
    POINT                      topLeft  = { button.left,  button.top };
    POINT                      botRight = { button.right, button.bottom };



    HideLegend();
    BAIL_OUT_IF (host == nullptr, S_OK);

    m_legendPopup = host->AcquirePopup();
    BAIL_OUT_IF (m_legendPopup == nullptr, S_OK);

    ClientToScreen (GetHwnd(), &topLeft);
    ClientToScreen (GetHwnd(), &botRight);

    params.ownerHwnd        = GetHwnd();
    params.anchorRectScreen = { topLeft.x, topLeft.y, botRight.x, botRight.y };
    params.placement        = DxuiPopupPlacement::Above;
    params.flipIfOffscreen  = true;
    params.dismiss          = isPinned ? DxuiPopupDismiss::OnClickOutside : DxuiPopupDismiss::Manual;
    params.input            = isPinned ? DxuiPopupInput::Interactive : DxuiPopupInput::PassThrough;
    params.sizeDip          = { PlatterLegendView::kWidthDip, m_legend->GetHeightDip() };
    params.backgroundArgb   = m_theme->BackgroundElevated();
    params.renderContent    = [this] (IDxuiPainter & painter, IDxuiTextRenderer & text)
                              {
                                  m_legend->Layout ({ 0, 0, m_scaler.ToPx (PlatterLegendView::kWidthDip), m_scaler.ToPx (m_legend->GetHeightDip()) }, m_scaler);
                                  m_legend->Paint  (painter, text, *m_theme);
                              };
    params.onClosed         = [this] ()
                              {
                                  m_legendPopup    = nullptr;
                                  m_isLegendPinned = false;
                                  m_legendClosedMs = static_cast<int64_t> (GetTickCount64());
                              };

    hr = m_legendPopup->Show (std::move (params));
    CHRF (hr, host->ReleasePopup (m_legendPopup); m_legendPopup = nullptr);

    m_isLegendPinned = isPinned;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::HideLegend
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::HideLegend()
{
    DxuiPopupHost  * popup = m_legendPopup;



    m_legendPopup    = nullptr;
    m_isLegendPinned = false;

    if (popup != nullptr && GetPopupHost() != nullptr)
    {
        GetPopupHost()->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TrackLegendHover
//
//  The pointer over the "Legend" button previews the legend; leaving the
//  button closes the preview, and leaves a pinned legend open.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TrackLegendHover (POINT pointPx)
{
    bool  isOver = m_legendButton->IsVisible() && IsInside (m_legendButton->GetBounds(), pointPx);



    if (isOver && m_legendPopup == nullptr)
    {
        ShowLegend (false);
    }
    else if (!isOver && m_legendPopup != nullptr && !m_isLegendPinned)
    {
        HideLegend();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetPrefs
//
//  The placement is the restored one, in the workspace coordinates
//  SetWindowPlacement takes back, so a maximized window comes back to the
//  size it had before.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::GetPrefs (DiskInspectorPrefs & inOut) const
{
    WINDOWPLACEMENT  placement = { sizeof (WINDOWPLACEMENT) };



    if (GetHwnd() != nullptr && GetWindowPlacement (GetHwnd(), &placement))
    {
        inOut.x           = placement.rcNormalPosition.left;
        inOut.y           = placement.rcNormalPosition.top;
        inOut.w           = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
        inOut.h           = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
        inOut.isMaximized = placement.showCmd == SW_SHOWMAXIMIZED || IsZoomed (GetHwnd());
    }

    inOut.isTimingMode  = m_context.isTimingMode;
    inOut.isDiskGroup   = m_isDiskGroup;
    inOut.trackTab      = m_trackTab;
    inOut.diskTab       = (m_diskTab == kTabDifferences) ? kTabTracks : m_diskTab;
    inOut.timingRange   = m_context.timingRange;
    inOut.isAlignment   = m_alignmentCheck->IsChecked();
    inOut.isFiles       = m_context.isFilesOverlay;
    inOut.isReads       = m_context.isReadsOverlay;
    inOut.isShowDeleted = m_mapDeleted->IsChecked();
    inOut.isFollowHead  = m_followHead.IsOn();
    inOut.zoom          = m_zoom;
    inOut.lastDrive     = m_drive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ApplyPrefs
//
//  A saved placement is used only while it is on a monitor, so a window
//  saved on a monitor since removed opens centered on Casso instead. It is
//  set twice: a move onto a monitor of another DPI rescales the window, so
//  the second call sets the saved size at that monitor's DPI.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ApplyPrefs (const DiskInspectorPrefs & prefs)
{
    WINDOWPLACEMENT  placement = { sizeof (WINDOWPLACEMENT) };
    RECT             saved     = { prefs.x, prefs.y, prefs.x + prefs.w, prefs.y + prefs.h };
    int              step      = 0;



    if (prefs.w > 0 && prefs.h > 0 && MonitorFromRect (&saved, MONITOR_DEFAULTTONULL) != nullptr && GetWindowPlacement (GetHwnd(), &placement))
    {
        placement.rcNormalPosition = saved;
        placement.showCmd          = prefs.isMaximized ? SW_SHOWMAXIMIZED : (IsWindowVisible (GetHwnd()) ? SW_SHOWNOACTIVATE : SW_HIDE);
        SetWindowPlacement (GetHwnd(), &placement);
        SetWindowPlacement (GetHwnd(), &placement);
    }

    m_context.isTimingMode   = prefs.isTimingMode;
    m_context.isFilesOverlay = prefs.isFiles;
    m_context.isReadsOverlay = prefs.isReads;
    m_isFilesDefaulted       = prefs.w > 0;
    m_isFilesSaved           = prefs.w > 0;
    m_trackTab               = std::clamp (prefs.trackTab, 0, kTrackTabCount - 1);
    m_diskTab                = std::clamp (prefs.diskTab,  0, kDiskTabCount  - 1);
    m_diskTab                = (m_diskTab == kTabDifferences) ? kTabTracks : m_diskTab;

    //  The timing range step nearest the saved range.
    for (step = 0; step < static_cast<int> (std::size (s_kRanges)); step++)
    {
        m_rangeStep = (std::abs (s_kRanges[step] - prefs.timingRange) < std::abs (s_kRanges[m_rangeStep] - prefs.timingRange)) ? step : m_rangeStep;
    }

    m_context.timingRange = s_kRanges[m_rangeStep];

    m_modeTabs->SetSelected  (prefs.isTimingMode ? 1 : 0);
    m_alignmentCheck->SetChecked (prefs.isAlignment);
    m_platterView->SetAlignmentShown (prefs.isAlignment);
    m_filesCheck->SetChecked (prefs.isFiles);
    m_readsCheck->SetChecked (prefs.isReads);
    m_mapDeleted->SetChecked (prefs.isShowDeleted);
    m_followCheck->SetChecked (prefs.isFollowHead);
    SetFollowHead (prefs.isFollowHead);
    SetZoom (prefs.zoom);

    if (prefs.isDiskGroup)
    {
        ShowDiskTab (m_diskTab);
    }
    else
    {
        ShowTrackTab (m_trackTab);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PaintNoDisk
//
//  An empty drive: a dashed outline of the disk and its hub where the
//  platter would be, and over it the reason no disk is drawn (FR-061,
//  FR-070).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PaintNoDisk (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    static constexpr double  kTwoPi    = 6.283185307179586;
    static constexpr float   kInsetDip = 16.0f;
    static constexpr float   kDashDip  = 12.0f;



    float          cx      = (m_platterAreaPx.left + m_platterAreaPx.right) / 2.0f;
    float          cy      = (m_platterAreaPx.top + m_platterAreaPx.bottom) / 2.0f;
    float          rim     = std::min (m_platterAreaPx.right - m_platterAreaPx.left, m_platterAreaPx.bottom - m_platterAreaPx.top) / 2.0f - m_scaler.ToPxf (kInsetDip);
    float          line    = m_scaler.ToPxf (1.5f);
    bool           isNoCtl = m_headState != nullptr && !m_headState->hasController;
    std::wstring   why     = isNoCtl ? std::wstring (L"This machine has no Disk II controller in slot 6") : std::format (L"No disk in drive {}", m_drive + 1);
    double         a       = 0.0;
    double         b       = 0.0;
    int            k       = 0;
    int            dashes  = 0;



    //  The same dash length on both circles.
    for (float radius : { rim, rim * PlatterRenderer::kInnerFraction })
    {
        dashes = std::max (8, static_cast<int> (kTwoPi * radius / (2.0f * m_scaler.ToPxf (kDashDip))));

        for (k = 0; radius > 0.0f && k < dashes; k++)
        {
            a = kTwoPi * k / dashes;
            b = kTwoPi * (k + 0.5) / dashes;

            painter.DrawLine (cx + radius * static_cast<float> (std::sin (a)), cy - radius * static_cast<float> (std::cos (a)),
                              cx + radius * static_cast<float> (std::sin (b)), cy - radius * static_cast<float> (std::cos (b)), line, theme.ForegroundMuted());
        }
    }

    text.DrawString (why.c_str(), static_cast<float> (m_platterAreaPx.left), static_cast<float> (m_platterAreaPx.top),
                     static_cast<float> (m_platterAreaPx.right - m_platterAreaPx.left), static_cast<float> (m_platterAreaPx.bottom - m_platterAreaPx.top),
                     theme.ForegroundMuted(), m_scaler.ToPxf (InspectorView::kTextDip + 1.0f), DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                     DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PaintHelp
//
//  The "?" button in a circle in the status band, which opens the
//  walkthrough (FR-153).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PaintHelp (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    float  cx     = (m_helpPx.left + m_helpPx.right) / 2.0f;
    float  cy     = (m_helpPx.top + m_helpPx.bottom) / 2.0f;
    float  radius = std::min (m_helpPx.right - m_helpPx.left, m_helpPx.bottom - m_helpPx.top) / 2.0f;



    text.DrawEllipse (cx, cy, radius - 0.5f, radius - 0.5f, 1.0f, theme.ForegroundMuted());
    text.DrawString  (L"?", static_cast<float> (m_helpPx.left), static_cast<float> (m_helpPx.top), static_cast<float> (m_helpPx.right - m_helpPx.left),
                      static_cast<float> (m_helpPx.bottom - m_helpPx.top), theme.Foreground(), radius * 1.3f, DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                      DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnWalkthroughMouse
//
//  A press on "?" opens the walkthrough; while it is open, a press on its
//  buttons steps or closes it, and every other event goes nowhere.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OnWalkthroughMouse (const DxuiMouseEvent & ev)
{
    bool  isPress   = ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left;
    bool  isHandled = m_walkthrough.IsOpen();



    if (isHandled && isPress)
    {
        switch (m_walkthrough.HitTest (ev.positionDip))
        {
            case WalkthroughButton::Back:  m_walkthrough.Back();  break;
            case WalkthroughButton::Next:  m_walkthrough.Next();  break;
            case WalkthroughButton::Close: m_walkthrough.Close(); break;
            case WalkthroughButton::None:                         break;
        }
    }
    else if (isPress && PtInRect (&m_helpPx, ev.positionDip))
    {
        m_tooltip.HideImmediate();
        m_walkthrough.Open();
        isHandled = true;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnWalkthroughKey
//
//  While the walkthrough is open: Escape closes it, Left steps back, and
//  Right or Enter steps on. It takes every key.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OnWalkthroughKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = m_walkthrough.IsOpen();



    if (isHandled && ev.kind == DxuiKeyEventKind::Down)
    {
        switch (ev.vk)
        {
            case VK_ESCAPE: m_walkthrough.Close(); break;
            case VK_LEFT:   m_walkthrough.Back();  break;
            case VK_RIGHT:
            case VK_RETURN: m_walkthrough.Next();  break;
            default:                               break;
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetWalkthroughArea
//
////////////////////////////////////////////////////////////////////////////////

RECT DiskInspectorWindow::GetWalkthroughArea (WalkthroughArea area) const
{
    RECT  rect   = {};
    RECT  header = m_headerView->GetBounds();
    RECT  row    = m_sectorRow->GetBounds();
    RECT  tabs   = m_tabs->GetBounds();



    switch (area)
    {
        case WalkthroughArea::Platter: rect = m_platterAreaPx;                                       break;
        case WalkthroughArea::Toolbar: rect = m_toolbarPx;                                           break;
        case WalkthroughArea::Track:   UnionRect (&rect, &header, &row);                             break;
        case WalkthroughArea::Tabs:    rect = { m_trackAreaPx.left, tabs.top, m_trackAreaPx.right, m_trackAreaPx.bottom }; break;
        case WalkthroughArea::Strip:   rect = m_stripAreaPx;                                         break;
        case WalkthroughArea::None:                                                                  break;
    }

    return rect;
}
