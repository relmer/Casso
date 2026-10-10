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
#include "Ui/DiskInspector/FindPanel.h"
#include "Ui/DiskInspector/FluxTimingTab.h"
#include "Ui/DiskInspector/GoToDialog.h"
#include "Ui/DiskInspector/NibblesTab.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterLegendView.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/SectorByteView.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"
#include "Ui/DiskInspector/TrackStripView.h"





static constexpr LPCWSTR  s_kpszWindowTitle = L"Disk inspector";
static constexpr LPCWSTR  s_kpszClassName   = L"CassoDiskInspector";
static constexpr LPCWSTR  s_kpszHint        = L"Scroll to zoom, drag to pan, double-click to fit";
static constexpr LPCWSTR  s_kpszStripHint   = L"Scroll to zoom, drag to pan, Shift+drag to select, double-click for the whole track";
static constexpr LPCWSTR  s_kpszNoDisk      = L"No disk";
static constexpr LPCWSTR  s_kpszAnalyzing   = L"Analyzing";

static constexpr int      s_kToolbarDip     = 40;
static constexpr int      s_kRowDip         = 28;
static constexpr int      s_kTabsDip        = 30;
static constexpr int      s_kStripDip       = 64;
static constexpr int      s_kHintRowDip     = 20;
static constexpr int      s_kWholeTrackDip  = 104;
static constexpr int      s_kReadoutDip     = 220;
static constexpr int      s_kSectorRowDip   = 76;
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
static constexpr double   s_kPlatterShare   = 0.55;
static constexpr double   s_kMapPlatterShare = 0.32;
static constexpr double   s_kMapGridShare   = 0.62;
static constexpr int      s_kMapCheckDip    = 190;
static constexpr int      s_kMapButtonDip   = 170;
static constexpr int      s_kMapVolumeDip   = 150;
static constexpr int      s_kOverlayDip     = 130;
static constexpr int      s_kMinTableDip    = 200;
static constexpr int      s_kDecodeButtonDip = 140;
static constexpr int      s_kExportButtonDip = 90;
static constexpr int      s_kFindButtonDip   = 64;
static constexpr int      s_kCopySectorDip   = 110;
static constexpr int      s_kCompareDip      = 130;
static constexpr int      s_kBarDip          = 36;
static constexpr int      s_kBarButtonDip    = 136;
static constexpr int      s_kStopButtonDip   = 116;
static constexpr int      s_kMinFileNameDip  = 200;
static constexpr int      s_kFocusGapDip    = 3;
static constexpr int      s_kModeTabDip     = 80;
static constexpr int      s_kRangeDip       = 120;
static constexpr double   s_kRanges[]       = { 0.01, 0.02, 0.03, 0.05, 0.10, 0.15, 0.20, 0.25 };

enum TrackTab
{
    kTabSectorData,
    kTabNibbles,
    kTabFields,
    kTabFluxTiming,
};


enum DiskTab
{
    kTabTracks,
    kTabFindings,
    kTabFileMap,
    kTabImage,
    kTabDifferences,
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
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;



    BAIL_OUT_IF (IsCreated(), S_OK);

    m_theme = theme;
    m_host  = (host != nullptr) ? host : &m_nullHost;

    params.title                    = s_kpszWindowTitle;
    params.hInstance                = hInstance;
    params.ownerHwnd                = hwndOwner;
    params.initialSizeDip           = { kOpeningWidthDip, kOpeningHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::Standard;
    params.classNameOverride        = s_kpszClassName;
    params.createNoActivate         = !activate;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;
    params.fitToWorkArea            = true;

    hr = DxuiWindow::Create (params);
    CHR (hr);

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

    //  Another drive's disk ends a comparison of this one.
    if (m_comparison.IsComparing())
    {
        m_comparison.End();
        ShowDiskTab (m_diskTab == kTabDifferences ? kTabTracks : m_diskTab);
    }

    m_drive   = drive;
    m_sourceA = { ComparisonSourceKind::DriveNow, drive };

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
    UpdateRings();
    UpdateControls();
    RefreshTables();

    m_tooltip.Tick     (static_cast<int64_t> (GetTickCount64()));
    m_tracksTab->Tick   (static_cast<int64_t> (GetTickCount64()));
    m_findingsTab->Tick (static_cast<int64_t> (GetTickCount64()));
    m_fieldsTab->Tick   (static_cast<int64_t> (GetTickCount64()));
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
    m_stripHint   = CreateChild<DxuiLabel> (s_kpszStripHint, DxuiTextRole::Muted, DxuiTextHAlign::Left);
    m_sectorRow   = CreateChild<SectorRowView>   (m_context);
    m_trackTabs   = CreateChild<DxuiTabStrip>();
    m_byteView    = CreateChild<SectorByteView>  (m_context);
    m_nibblesTab  = CreateChild<NibblesTab>      (m_context);
    m_fieldsTab   = CreateChild<InspectorTableView> (m_context);
    m_fluxTab     = CreateChild<FluxTimingTab>   (m_context);
    m_diskTabs    = CreateChild<DxuiTabStrip>();
    m_decodeButton = CreateChild<DxuiButton> (L"Decode settings...");
    m_exportButton = CreateChild<DxuiButton> (L"Export...");
    m_goToButton   = CreateChild<DxuiButton> (L"Go to");
    m_findButton   = CreateChild<DxuiButton> (L"Find");
    m_copySector   = CreateChild<DxuiButton> (L"Copy sector");
    m_compareButton = CreateChild<DxuiButton> (L"Compare with...");
    m_prevDiff     = CreateChild<DxuiButton> (L"Previous difference");
    m_nextDiff     = CreateChild<DxuiButton> (L"Next difference");
    m_swapButton   = CreateChild<DxuiButton> (L"Swap A and B");
    m_bSettingsButton = CreateChild<DxuiButton> (L"B's decode settings...");
    m_stopButton   = CreateChild<DxuiButton> (L"Stop comparing");
    m_diffsCheck   = CreateChild<DxuiCheckbox> (L"Differences");
    m_alignmentCheck = CreateChild<DxuiCheckbox> (L"Alignment");
    m_filesCheck     = CreateChild<DxuiCheckbox> (L"Files");
    m_modeTabs     = CreateChild<DxuiTabStrip>();
    m_rangeDown    = CreateChild<DxuiButton> (s_kpszMinus);
    m_rangeUp      = CreateChild<DxuiButton> (L"+");
    m_rangeLabel   = CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Center);
    m_legend       = CreateChild<PlatterLegendView> (m_context);
    m_tracksTab   = CreateChild<InspectorTableView> (m_context);
    m_findingsTab = CreateChild<FindingsTab>     (m_context);
    m_imageTab    = CreateChild<InspectorTableView> (m_context);
    m_mapGrid     = CreateChild<FileMapGridView>    (m_context);
    m_fileList    = CreateChild<InspectorTableView> (m_context);
    m_mapVolumes  = CreateChild<DxuiTabStrip>();
    m_mapDeleted  = CreateChild<DxuiCheckbox> (L"Show deleted files");
    m_mapBadOnly  = CreateChild<DxuiCheckbox> (L"Files touching bad sectors");
    m_mapPrev     = CreateChild<DxuiButton> (L"Previous sector in file");
    m_mapNext     = CreateChild<DxuiButton> (L"Next sector in file");
    m_mapCopy     = CreateChild<DxuiButton> (L"Copy map");
    m_diffsTab    = CreateChild<DifferencesTab> (m_context, m_comparison);

    m_driveTabs->SetOnChange ([this] (int index) { (void) ShowDrive (index); });
    m_trackTabs->SetOnChange ([this] (int index) { ShowTrackTab (index); });
    m_diskTabs->SetOnChange  ([this] (int index) { ShowDiskTab (index); });
    m_tracksTab->SetColumns  (InspectorTables::GetTrackColumns());
    m_fieldsTab->SetColumns  (InspectorTables::GetFieldColumns());
    m_imageTab->SetColumns   (InspectorTables::GetImageColumns());
    m_mapGrid->SetOnChoose   ([this] (int cell) { SelectMapCell (cell); });
    m_fileList->SetOnSelect  ([this] (const TableRow & row) { ChooseFile (row.finding); });
    m_fileList->SetOnSort    ([this] (int column) { m_fileSortDescending = (column == m_fileSortColumn) && !m_fileSortDescending; m_fileSortColumn = column; RefreshFileList(); });
    m_mapVolumes->SetOnChange ([this] (int index) { m_mapIndex = index; m_mapFile = -1; RefreshFileList(); });
    m_mapDeleted->SetOnChange ([this] (bool) { RefreshFileList(); });
    m_mapBadOnly->SetOnChange ([this] (bool) { RefreshFileList(); });
    m_mapPrev->SetOnClick    ([this] () { StepFileSector (-1); });
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
    m_findButton->SetOnClick   ([this] () { OpenFind(); });
    m_copySector->SetOnClick   ([this] () { CopySector(); });
    m_compareButton->SetOnClick   ([this] () { OpenCompare(); });
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
    m_modeTabs->SetOnChange  ([this] (int index) { m_context.isTimingMode = (index == 1); });
    m_rangeDown->SetOnClick  ([this] () { StepRange (-1); });
    m_rangeUp->SetOnClick    ([this] () { StepRange (1); });

    m_tooltip.SetPopupHost (GetPopupHost());

    ShowTrackTab (kTabSectorData);
    ShowDiskTab  (kTabTracks);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnWindowClose
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnWindowClose()
{
    m_tooltip.HideImmediate();
    Hide();
    m_host->OnInspectorClosed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Layout
//
//  A toolbar across the top; under it the platter column on the left, the
//  platter kept square, with its zoom row and hint below; past the splitter
//  the track column: header, strip, sector row, tabs and the tab's content.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
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
    int                        ctlW      = scaler.ToPx (2 * s_kModeTabDip + s_kRangeDip + 2 * s_kFindButtonDip + s_kExportButtonDip + s_kDecodeButtonDip + s_kCompareDip) + 7 * margin;
    bool                       isTwoRows = 2 * margin + drives * tab + scaler.ToPx (s_kMinFileNameDip) + ctlW > width;
    int                        bar       = m_comparison.IsComparing() ? scaler.ToPx (s_kBarDip) : 0;
    int                        top       = rowBottom + (isTwoRows ? toolbar : 0) + bar;
    int                        ctlTop    = top - bar - toolbar;
    int                        column    = splitX - boundsDip.left - 2 * margin;
    int                        below     = 4 * margin + 2 * scaler.ToPx (s_kRowDip) + scaler.ToPx (PlatterLegendView::kRowDip * PlatterLegendView::kRows + s_kTabsDip + s_kMinTableDip);
    int                        side      = std::max (0, std::min ({ column, static_cast<int> ((boundsDip.bottom - top) * s_kPlatterShare), static_cast<int> (boundsDip.bottom - top) - below }));
    int                        x         = boundsDip.left + margin;
    int                        y         = 0;
    int                        i         = 0;
    int                        header    = scaler.ToPx (TrackHeaderView::kLineDip * TrackHeaderView::kLines);
    int                        diskTab   = std::min (tab, (column + margin) / (m_comparison.IsComparing() ? 5 : 4));
    RECT                       right     = {};
    RECT                       platter   = {};
    vector<DxuiTabStrip::Tab>  tabs;



    SetBounds (boundsDip);
    m_scaler = scaler;
    m_tooltip.SetDpi (scaler.GetDpi());
    m_tooltip.SetViewportSize (width, boundsDip.bottom - boundsDip.top);

    m_toolbarPx  = { boundsDip.left, boundsDip.top, boundsDip.right, top - bar };
    m_barPx      = { boundsDip.left, top - bar, boundsDip.right, top };
    m_splitterPx = { splitX, top, splitX + scaler.ToPx (s_kSplitterDip), boundsDip.bottom };
    right        = { m_splitterPx.right + margin, top + margin, boundsDip.right - margin, boundsDip.bottom - margin };

    for (i = 0; i < drives; i++)
    {
        tabs.push_back ({ { x + i * tab, boundsDip.top + margin / 2, x + (i + 1) * tab, rowBottom - margin / 2 }, std::format (L"Drive {}", i + 1) });
    }

    m_driveTabs->SetTabs (std::move (tabs));
    m_driveTabs->SetSelected (m_drive);
    m_driveTabs->Layout  ({ x, boundsDip.top, x + drives * tab, rowBottom }, scaler);

    //  The buttons and the mode controls from the right; on one row with the
    //  file name when there is room for it, or on a row of their own below.
    i = boundsDip.right - margin;
    m_decodeButton->Layout ({ i - scaler.ToPx (s_kDecodeButtonDip), ctlTop + margin / 2, i, ctlTop + toolbar - margin / 2 }, scaler);
    i -= scaler.ToPx (s_kDecodeButtonDip) + margin;
    m_compareButton->Layout ({ i - scaler.ToPx (s_kCompareDip), ctlTop + margin / 2, i, ctlTop + toolbar - margin / 2 }, scaler);
    i -= scaler.ToPx (s_kCompareDip) + margin;
    m_exportButton->Layout ({ i - scaler.ToPx (s_kExportButtonDip), ctlTop + margin / 2, i, ctlTop + toolbar - margin / 2 }, scaler);
    i -= scaler.ToPx (s_kExportButtonDip) + margin;
    m_findButton->Layout   ({ i - scaler.ToPx (s_kFindButtonDip), ctlTop + margin / 2, i, ctlTop + toolbar - margin / 2 }, scaler);
    i -= scaler.ToPx (s_kFindButtonDip) + margin;
    m_goToButton->Layout   ({ i - scaler.ToPx (s_kFindButtonDip), ctlTop + margin / 2, i, ctlTop + toolbar - margin / 2 }, scaler);
    i -= scaler.ToPx (s_kFindButtonDip) + margin;

    m_rangeDown->Layout  ({ i - scaler.ToPx (s_kRangeDip),          ctlTop + margin, i - scaler.ToPx (s_kRangeDip) + button, ctlTop + toolbar - margin }, scaler);
    m_rangeLabel->Layout ({ i - scaler.ToPx (s_kRangeDip) + button, ctlTop + margin, i - button,                            ctlTop + toolbar - margin }, scaler);
    m_rangeUp->Layout    ({ i - button,                             ctlTop + margin, i,                                     ctlTop + toolbar - margin }, scaler);
    i -= scaler.ToPx (s_kRangeDip);

    tabs.clear();
    tabs.push_back ({ { i - scaler.ToPx (2 * s_kModeTabDip), ctlTop + margin / 2, i - scaler.ToPx (s_kModeTabDip), ctlTop + toolbar - margin / 2 }, L"Structure" });
    tabs.push_back ({ { i - scaler.ToPx (s_kModeTabDip),     ctlTop + margin / 2, i,                                ctlTop + toolbar - margin / 2 }, L"Timing" });
    m_modeTabs->SetTabs     (std::move (tabs));
    m_modeTabs->SetSelected (m_context.isTimingMode ? 1 : 0);
    m_modeTabs->Layout      ({ i - scaler.ToPx (2 * s_kModeTabDip), ctlTop, i, ctlTop + toolbar }, scaler);
    i -= scaler.ToPx (2 * s_kModeTabDip) + margin;

    m_fileNamePx = { x + drives * tab + margin, boundsDip.top, std::min (x + drives * tab + margin + scaler.ToPx (s_kFileNameDip), isTwoRows ? static_cast<int> (boundsDip.right) - margin : i),
                     rowBottom };
    m_chipsPx    = { m_fileNamePx.right + margin, boundsDip.top, isTwoRows ? static_cast<int> (boundsDip.right) - margin : i, rowBottom };

    //  The comparison bar's buttons from the right, its sources on the left.
    i = boundsDip.right - margin;

    for (DxuiButton * barButton : { m_stopButton, m_bSettingsButton, m_swapButton, m_nextDiff, m_prevDiff })
    {
        int  w = scaler.ToPx (barButton == m_stopButton ? s_kStopButtonDip : s_kBarButtonDip);

        barButton->Layout ({ i - w, m_barPx.top + margin / 2, i, m_barPx.bottom - margin / 2 }, scaler);
        i -= w + margin / 2;
    }

    //  The File map takes most of the column; the platter stays in view for
    //  the Files overlay, and the legend and hint give way.
    side = (m_diskTab == kTabFileMap) ? std::min (side, static_cast<int> ((boundsDip.bottom - top) * s_kMapPlatterShare)) : side;

    platter = { boundsDip.left + margin + (column - side) / 2, top + margin, boundsDip.left + margin + (column - side) / 2 + side, top + margin + side };
    m_platterView->Layout (platter, scaler);

    y = platter.bottom + margin;
    m_zoomOut->Layout   ({ x,              y, x + button,     y + row }, scaler);
    m_zoomIn->Layout    ({ x + button,     y, x + 2 * button, y + row }, scaler);
    m_fit->Layout       ({ x + 2 * button, y, x + 4 * button, y + row }, scaler);
    m_zoomLabel->Layout ({ x + 4 * button + margin, y, x + 6 * button, y + row }, scaler);
    m_alignmentCheck->Layout ({ x + 6 * button + margin, y, x + 6 * button + margin + scaler.ToPx (s_kOverlayDip), y + row }, scaler);
    m_filesCheck->Layout     ({ x + 6 * button + margin + scaler.ToPx (s_kOverlayDip), y, x + 6 * button + margin + scaler.ToPx (2 * s_kOverlayDip), y + row }, scaler);
    m_diffsCheck->Layout     ({ x + 6 * button + margin + scaler.ToPx (2 * s_kOverlayDip), y, splitX - margin, y + row }, scaler);
    m_hintLabel->Layout ({ x, y + row, splitX - margin, y + 2 * row }, scaler);

    if (m_diskTab == kTabFileMap)
    {
        y += row + margin / 2;
    }
    else
    {
        y += 2 * row;
        m_legend->Layout ({ x, y, splitX - margin, y + scaler.ToPx (PlatterLegendView::kRowDip * PlatterLegendView::kRows) }, scaler);
        y += scaler.ToPx (PlatterLegendView::kRowDip * PlatterLegendView::kRows);
    }

    tabs.clear();

    for (LPCWSTR label : { L"Tracks", L"Findings", L"File map", L"Image", L"Differences" })
    {
        i = static_cast<int> (tabs.size());

        if (i == kTabDifferences && !m_comparison.IsComparing())
        {
            break;
        }

        tabs.push_back ({ { x + i * diskTab, y, x + (i + 1) * diskTab, y + scaler.ToPx (s_kTabsDip) }, label });
    }

    m_diskTabs->SetTabs     (std::move (tabs));
    m_diskTabs->SetSelected (m_diskTab);
    m_diskTabs->Layout      ({ x, y, splitX - margin, y + scaler.ToPx (s_kTabsDip) }, scaler);
    y += scaler.ToPx (s_kTabsDip) + margin / 2;

    m_tracksTab->Layout   ({ x, y, splitX - margin, boundsDip.bottom - margin }, scaler);
    m_findingsTab->Layout ({ x, y, splitX - margin, boundsDip.bottom - margin }, scaler);
    m_imageTab->Layout    ({ x, y, splitX - margin, boundsDip.bottom - margin }, scaler);
    m_diffsTab->Layout    ({ x, y, splitX - margin, boundsDip.bottom - margin }, scaler);
    LayoutFileMap ({ x, y, splitX - margin, static_cast<int> (boundsDip.bottom) - margin }, scaler);
    m_diskContentPx = { x, y, splitX - margin, boundsDip.bottom - margin };
    m_platterAreaPx = { boundsDip.left, top, splitX, y - scaler.ToPx (s_kTabsDip) - margin / 2 };
    m_trackAreaPx   = { m_splitterPx.right, top, boundsDip.right, boundsDip.bottom };

    y = right.top;
    m_headerView->Layout ({ right.left, y, right.right, y + header }, scaler);
    y += header;
    m_stripView->Layout ({ right.left, y, right.right, y + scaler.ToPx (s_kStripDip) }, scaler);
    y += scaler.ToPx (s_kStripDip) + margin / 2;

    //  While comparing, B's strip under A's (FR-121).
    m_stripB->Layout ({ right.left, y, right.right, y + (m_comparison.IsComparing() ? scaler.ToPx (s_kStripDip) : 0) }, scaler);
    y += m_comparison.IsComparing() ? scaler.ToPx (s_kStripDip) + margin / 2 : 0;
    m_stripOut->Layout     ({ right.left,          y, right.left + button,     y + row }, scaler);
    m_stripIn->Layout      ({ right.left + button, y, right.left + 2 * button, y + row }, scaler);
    m_stripWhole->Layout   ({ right.left + 2 * button, y, right.left + 2 * button + scaler.ToPx (s_kWholeTrackDip), y + row }, scaler);
    m_stripReadout->Layout ({ right.left + 2 * button + scaler.ToPx (s_kWholeTrackDip) + margin, y, right.right, y + row }, scaler);
    y += row;
    m_stripHint->Layout    ({ right.left, y, right.right, y + scaler.ToPx (s_kHintRowDip) }, scaler);
    y += scaler.ToPx (s_kHintRowDip) + margin / 2;
    m_sectorRow->Layout ({ right.left, y, right.right, y + scaler.ToPx (s_kSectorRowDip) }, scaler);
    y += scaler.ToPx (s_kSectorRowDip);

    tabs.clear();

    //  The track tabs share their row with "Copy sector", narrowing to fit.
    i   = std::min (tab, static_cast<int> (right.right - right.left - scaler.ToPx (s_kCopySectorDip) - margin) / 4);
    tab = i;

    for (LPCWSTR label : { L"Sector data", L"Nibbles", L"Fields", L"Flux timing" })
    {
        i = static_cast<int> (tabs.size());
        tabs.push_back ({ { right.left + i * tab, y, right.left + (i + 1) * tab, y + scaler.ToPx (s_kTabsDip) }, label });
    }

    m_trackTabs->SetTabs (std::move (tabs));
    m_trackTabs->SetSelected (m_trackTab);
    m_trackTabs->Layout  ({ right.left, y, right.right - scaler.ToPx (s_kCopySectorDip) - margin, y + scaler.ToPx (s_kTabsDip) }, scaler);
    m_copySector->Layout ({ right.right - scaler.ToPx (s_kCopySectorDip), y + margin / 4, right.right, y + scaler.ToPx (s_kTabsDip) - margin / 4 }, scaler);
    y += scaler.ToPx (s_kTabsDip) + margin;

    m_byteView->Layout   ({ right.left, y, right.right, right.bottom }, scaler);
    m_nibblesTab->Layout ({ right.left, y, right.right, right.bottom }, scaler);
    m_fieldsTab->Layout  ({ right.left, y, right.right, right.bottom }, scaler);
    m_fluxTab->Layout    ({ right.left, y, right.right, right.bottom }, scaler);
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

    painter.FillRect (static_cast<float> (m_splitterPx.left), static_cast<float> (m_splitterPx.top),
                      static_cast<float> (m_splitterPx.right - m_splitterPx.left), static_cast<float> (m_splitterPx.bottom - m_splitterPx.top),
                      theme.Divider());

    PaintToolbar (painter, text, theme);

    if (m_comparison.IsComparing())
    {
        PaintComparisonBar (painter, text, theme);
    }

    DxuiWindow::Paint (painter, text, theme);

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

    //  The view the keys act on, outlined once a key has been used (FR-059).
    if (m_isFocusShown && m_context.hasDisk && !m_is35)
    {
        focus = GetKeyTargetBounds();
        InflateRect (&focus, m_scaler.ToPx (s_kFocusGapDip), m_scaler.ToPx (s_kFocusGapDip));
        painter.OutlineRect (static_cast<float> (focus.left), static_cast<float> (focus.top), static_cast<float> (focus.right - focus.left),
                             static_cast<float> (focus.bottom - focus.top), m_scaler.ToPxf (2.0f), theme.FocusRing());
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
    POINT  p         = ev.positionDip;
    int    width     = m_boundsDip.right - m_boundsDip.left;
    bool   isCovered = IsCovered (p) && ev.kind != DxuiMouseEventKind::Leave;
    bool   isHandled = isCovered;



    //  Over a 3.5" disk's notes, the views under them take nothing.
    if (!isCovered && ev.kind == DxuiMouseEventKind::Down)
    {
        m_keyTarget    = GetKeyTarget (p);
        m_isFocusShown = false;
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
        Layout (m_boundsDip, m_scaler);
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

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        UpdateTooltip (p);
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



    if (ev.kind == DxuiKeyEventKind::Down && !ev.alt && m_context.hasDisk)
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
                case 'F':      OpenFind();                           break;
                case 'G':      OpenGoTo();                           break;
                default:       isHandled = false;                    break;
            }
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
        if (!m_comparison.OfferReply (reply) && reply.requestId == m_pendingRequest)
        {
            m_pendingRequest = 0;
            StartDisk (reply.disk, (reply.status == InspectorReplyStatus::Unopenable) ? reply.reason : std::wstring());
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
    DecodeSettings  settings = m_comparison.IsComparing() ? m_analysis.settings : DecodeSettings::MakeStandard();
    int             slot     = 0;



    m_context.hasDisk = (copy != nullptr);
    m_openError       = reason;
    m_changedSlots.clear();
    m_levels.fill (nullptr);
    m_timingLevels.fill (nullptr);

    if (m_context.hasDisk)
    {
        m_model.SetDisk (copy->mediaId);

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
    }
    else
    {
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
        m_comparison.MarkAChanged();
        m_isTablesDirty = true;
    }
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
    if (m_zoomLabel != nullptr)
    {
        m_zoomLabel->SetText    (std::format (L"{:.0f}{}", m_model.GetZoom(), s_kpszMultiplyX));
        m_fit->SetEnabled       (!m_model.IsAtFit());
        m_zoomOut->SetVisible   (m_context.hasDisk);
        m_decodeButton->SetVisible (m_context.hasDisk);
        m_exportButton->SetVisible (m_context.hasDisk);
        m_goToButton->SetVisible   (m_context.hasDisk);
        m_findButton->SetVisible   (m_context.hasDisk);
        m_copySector->SetVisible   (m_context.hasDisk && m_trackTab == kTabSectorData);
        m_copySector->SetEnabled   (m_model.GetSector() != nullptr && m_model.GetSector()->dataField >= 0);
        m_compareButton->SetVisible   (m_context.hasDisk || m_comparison.IsComparing());
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
        m_hintLabel->SetVisible (m_context.hasDisk && m_diskTab != kTabFileMap);
        m_legend->SetVisible    (m_diskTab != kTabFileMap);
        SyncFileMapSelection();
        m_stripOut->SetVisible     (m_context.hasDisk);
        m_stripIn->SetVisible      (m_context.hasDisk);
        m_stripWhole->SetVisible   (m_context.hasDisk);
        m_stripWhole->SetEnabled   (m_model.GetStripSpan() < 1.0);
        m_stripReadout->SetVisible (m_context.hasDisk);
        m_stripReadout->SetText    (m_stripView->GetReadout() + ((m_model.GetTrack() != nullptr && m_model.GetNibbleCount() > 0)
                                    ? L"    " + InspectorText::FormatSelection (*m_model.GetTrack(), m_model.GetFirstNibble(), m_model.GetNibbleCount()) : L""));
        m_stripHint->SetVisible    (m_context.hasDisk);
        m_alignmentCheck->SetVisible (m_context.hasDisk);
        m_filesCheck->SetVisible     (m_context.hasDisk);
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowTrackTab
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowTrackTab (int tab)
{
    m_trackTab = tab;
    m_byteView->SetVisible   (tab == kTabSectorData);
    m_nibblesTab->SetVisible (tab == kTabNibbles);
    m_fieldsTab->SetVisible  (tab == kTabFields);
    m_fluxTab->SetVisible    (tab == kTabFluxTiming);
    m_trackTabs->SetSelected (tab);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateTooltip
//
//  The first view with a tooltip for the point gives it; the file name gives
//  its full text.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateTooltip (POINT pointPx)
{
    std::wstring                          tip;
    RECT                                  anchor = {};
    int64_t                               now    = static_cast<int64_t> (GetTickCount64());
    const std::array<InspectorView *, 7>  views  = { m_platterView, m_stripView, m_stripB, m_sectorRow, m_nibblesTab, m_fluxTab, m_mapGrid };



    for (InspectorView * view : views)
    {
        if (tip.empty() && view != nullptr && view->IsVisible() && IsInside (view->GetBounds(), pointPx))
        {
            view->GetTooltip (pointPx, tip, anchor);
        }
    }

    if (tip.empty() && m_context.hasDisk)
    {
        tip = GetButtonTip (pointPx, anchor);
    }

    if (tip.empty() && m_context.hasDisk && PtInRect (&m_fileNamePx, pointPx))
    {
        tip    = GetFileName();
        anchor = m_fileNamePx;
    }

    if (tip.empty())
    {
        m_tooltip.RequestHide (now);
    }
    else if (tip != m_tooltipText)
    {
        m_tooltip.RequestShow (anchor, tip, now);
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
    m_diskTab = tab;
    m_tracksTab->SetVisible   (tab == kTabTracks);
    m_findingsTab->SetVisible (tab == kTabFindings);
    m_imageTab->SetVisible    (tab == kTabImage);
    m_diffsTab->SetVisible    (tab == kTabDifferences);
    ShowFileMap (tab == kTabFileMap);

    if (m_scaler.GetDpi() != 0)
    {
        Layout (m_boundsDip, m_scaler);
    }

    m_diskTabs->SetSelected   (tab);
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
        vector<TableRow>      rows    = m_context.hasDisk ? InspectorTables::BuildTracks (m_analysis) : vector<TableRow>();

        //  While comparing, each quarter track's verdict (FR-121).
        if (m_comparison.IsComparing())
        {
            ComparisonText::AddVerdictColumn (columns, rows, m_comparison.GetResult(), m_comparison.IsBusy() || !m_comparison.HasResult());
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
        m_imageTab->SetRows (m_context.hasDisk ? InspectorTables::BuildImage (m_analysis.image) : vector<TableRow>());
        RefreshFileList();

        //  Casso does not analyze 3.5" disks; such a WOZ opens on the Image
        //  tab, and every other view is covered by a note (FR-054).
        m_is35 = m_context.hasDisk && m_analysis.image.isWoz && m_analysis.image.info.diskType == WozLoader::kDiskType35;

        if (m_is35)
        {
            ShowDiskTab (kTabImage);
        }

        Layout (m_boundsDip, m_scaler);
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
    if (m_context.hasDisk && m_analysis.copy != nullptr)
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
//  DiskInspectorWindow::OpenFind
//
//  The panel opens on the last query and its hits; a hit chosen there is
//  selected, and the hits stay for F3 either way.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenFind()
{
    HRESULT                   hr     = S_OK;
    FindPanel                 panel;
    DxuiWindow::CreateParams  params;



    panel.Configure (m_theme, m_context, m_model.GetQuarterTrack(), m_findQuery, m_findHits);

    params.title                    = L"Find";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = FindPanel::kSizeDip;
    params.minSizeDip               = FindPanel::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = panel.Create (params);
    CHRA (hr);

    panel.SetTheme (m_theme);
    panel.ShowModalDialog (IDOK);

    m_findQuery = panel.GetQuery();
    m_findHits  = panel.GetHits();
    m_findIndex = panel.GetChosen();

    if (m_findIndex >= 0)
    {
        SelectHit (m_findHits[m_findIndex]);
    }

Error:
    return;
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
    int  count = static_cast<int> (m_findHits.size());



    if (count > 0)
    {
        m_findIndex = (m_findIndex < 0) ? (step > 0 ? 0 : count - 1) : (m_findIndex + step + count) % count;
        SelectHit (m_findHits[m_findIndex]);
    }
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



    if (IsInside (m_stripView->GetBounds(), pointPx) || (m_stripB->IsVisible() && IsInside (m_stripB->GetBounds(), pointPx)) || IsInside (m_stripOut->GetBounds(), pointPx) || IsInside (m_stripIn->GetBounds(), pointPx) ||
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

    isGoingOn = list.empty() || DxuiMessageBox (GetHwnd(), m_theme, (L"The export will hold these as noted:\n\n" + list + L"\nExport anyway?").c_str(),
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
//  Every button's tooltip (FR-059), with the key that does the same.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskInspectorWindow::GetButtonTip (POINT pointPx, RECT & outAnchorPx) const
{
    const std::pair<const IDxuiControl *, LPCWSTR>  tips[] =
    {
        { m_zoomOut,        L"Zoom the platter out (minus)" },
        { m_zoomIn,         L"Zoom the platter in (plus)" },
        { m_fit,            L"Show the whole disk (0)" },
        { m_alignmentCheck, L"Mark where sector 0 and the longest sync start on each track" },
        { m_filesCheck,     L"Color each sector's data field by what the file map says it holds" },
        { m_stripOut,       L"Zoom the strip out" },
        { m_stripIn,        L"Zoom the strip in" },
        { m_stripWhole,     L"Show the whole track in the strip" },
        { m_rangeDown,      L"Narrow the timing range" },
        { m_rangeUp,        L"Widen the timing range" },
        { m_goToButton,     L"Go to a track, sector, block, nibble or cell (Ctrl+G)" },
        { m_findButton,     L"Find nibbles, bytes or text (Ctrl+F)" },
        { m_copySector,     L"Copy the whole sector as a hex dump (Ctrl+Shift+C)" },
        { m_exportButton,   L"Save sectors, nibbles or this quarter track's bits to a file" },
        { m_decodeButton,   L"Change the marks and checks used to decode tracks" },
        { m_compareButton,  L"Compare two disks track by track and file by file" },
        { m_prevDiff,       L"Go to the previous difference" },
        { m_nextDiff,       L"Go to the next difference" },
        { m_swapButton,     L"Make A the disk B is, and B the disk A is" },
        { m_bSettingsButton, L"Give B decode settings of its own" },
        { m_stopButton,     L"End the comparison" },
        { m_diffsCheck,     L"Mark the quarter tracks that differ" },
    };



    RECT          modes = m_modeTabs->GetBounds();
    std::wstring  tip;



    for (const auto & [control, text] : tips)
    {
        if (tip.empty() && control != nullptr && control->IsVisible() && IsInside (control->GetBounds(), pointPx))
        {
            tip         = text;
            outAnchorPx = control->GetBounds();
        }
    }

    if (tip.empty() && m_modeTabs->IsVisible() && IsInside (modes, pointPx))
    {
        tip         = (pointPx.x < (modes.left + modes.right) / 2) ? L"Color the platter by what each cell holds" : L"Color flux tracks by how fast or slow each cell runs";
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
    const std::array<std::pair<KeyTarget, const IDxuiControl *>, 10>  order =
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
    static constexpr LPCWSTR  kpszNote = L"Casso does not analyze 3.5\" disks. The Image tab shows what the file holds.";



    RECT  empty = {};



    for (const RECT & area : { m_platterAreaPx, m_trackAreaPx, m_diskTab == kTabImage ? empty : m_diskContentPx })
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
    return m_is35 && (IsInside (m_platterAreaPx, pointPx) || IsInside (m_trackAreaPx, pointPx) || (m_diskTab != kTabImage && IsInside (m_diskContentPx, pointPx)));
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
    int                        y       = area.top;
    int                        gridH   = 0;
    bool                       isList  = map != nullptr && map->notMapped == NotMappedReason::None;
    vector<DxuiTabStrip::Tab>  tabs;



    m_mapDeleted->Layout ({ area.left,         y, area.left + check,     y + row }, scaler);
    m_mapBadOnly->Layout ({ area.left + check, y, area.left + 2 * check, y + row }, scaler);
    m_mapCopy->Layout    ({ area.right - scaler.ToPx (s_kExportButtonDip), y, area.right, y + row }, scaler);
    y += row + margin / 2;

    m_mapPrev->Layout ({ area.left,          y, area.left + button,     y + row }, scaler);
    m_mapNext->Layout ({ area.left + button, y, area.left + 2 * button, y + row }, scaler);

    for (size_t v = 0; v < m_analysis.fileMaps.size() && m_analysis.fileMaps.size() > 1; v++)
    {
        int  left = area.left + 2 * button + margin + static_cast<int> (v) * volume;

        tabs.push_back ({ { left, y, left + volume, y + row }, FileMapText::FormatVolume (m_analysis.fileMaps[v]) });
    }

    m_mapVolumes->SetTabs     (std::move (tabs));
    m_mapVolumes->SetSelected (m_mapIndex);
    m_mapVolumes->Layout      ({ area.left + 2 * button + margin, y, area.right, y + row }, scaler);
    y += row + margin / 2;

    gridH = isList ? std::min (m_mapGrid->GetHeightFor (area.right - area.left), static_cast<int> ((area.bottom - y) * s_kMapGridShare)) : area.bottom - y;
    m_mapGrid->Layout  ({ area.left, y, area.right, y + gridH }, scaler);
    m_fileList->Layout ({ area.left, y + gridH + margin / 2, area.right, area.bottom }, scaler);
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



    m_mapGrid->SetVisible    (isShown);
    m_fileList->SetVisible   (isList);
    m_mapDeleted->SetVisible (isList);
    m_mapBadOnly->SetVisible (isList);
    m_mapPrev->SetVisible    (isList);
    m_mapNext->SetVisible    (isList);
    m_mapCopy->SetVisible    (isList);
    m_mapVolumes->SetVisible (isShown && m_analysis.fileMaps.size() > 1);
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
    ShowFileMap (m_diskTab == kTabFileMap);
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
//  The physical sector that holds a cell, or the first half of a block, on
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
    m_mapGrid->SetSelected (GetSelectedMapCell(), m_mapFile);
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
