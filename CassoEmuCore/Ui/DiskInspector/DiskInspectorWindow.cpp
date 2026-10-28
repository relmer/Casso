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
#include "Ui/DiskInspector/Views/FilesView.h"
#include "Ui/DiskInspector/Components/ComparisonPart.h"
#include "Ui/DiskInspector/Components/EditingPart.h"
#include "Ui/DiskInspector/Components/FindPart.h"
#include "Ui/DiskInspector/Components/HostLink.h"
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
static constexpr int      s_kStatusDip      = 28;
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
static constexpr int      s_kBarDip          = 36;
static constexpr int      s_kMinFileNameDip  = 200;
static constexpr int      s_kFocusGapDip    = 3;
static constexpr int      s_kModeTabDip     = 76;
static constexpr int      s_kRangeDip       = 110;
static constexpr double   s_kRanges[]       = { 0.01, 0.02, 0.03, 0.05, 0.10, 0.15, 0.20, 0.25 };





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::DiskInspectorWindow
//
////////////////////////////////////////////////////////////////////////////////

DiskInspectorWindow::DiskInspectorWindow()
{
    m_session.context.onSelectionChanged = [this] () { OnSelection(); };
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

    m_session.theme = theme;
    m_session.host  = (host != nullptr) ? host : &m_session.nullHost;

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

    SetTheme (m_session.theme);
    m_tooltip.SetTheme (*m_session.theme);
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

    if (drive != m_session.drive && !ConfirmDiscardEdits (L"Switching to the other drive"))
    {
        m_driveTabs->SetSelected (m_session.drive);
        BAIL_OUT_IF (true, S_OK);
    }

    //  Another drive's disk ends a comparison of this one.
    if (m_session.comparison.IsComparing())
    {
        m_session.comparison.End();
        ShowDiskTab (m_session.diskTab == kTabDifferences ? kTabTracks : m_session.diskTab);
    }

    m_session.drive   = drive;
    m_session.sourceA = { ComparisonSourceKind::DriveNow, drive };
    //  No disk has this id, and an empty drive's is 0, so an eject is seen.
    m_session.requestedMediaId = UINT64_MAX;
    m_session.host->SetHeadStateWanted (true);

    if (m_driveTabs != nullptr)
    {
        m_driveTabs->SetSelected (drive);
    }

    m_hostLink->RequestCopy();
    UpdateControls();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Relayout
//
//  The window laid out again at its bounds, after a component's content
//  changes size.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Relayout()
{
    if (m_session.scaler.GetDpi() != 0)
    {
        Layout (m_boundsDip, m_session.hostScaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SetStatusNote
//
//  A note in the status band, replaced by a view's hint when the pointer moves
//  over a view that has one.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SetStatusNote (const std::wstring & note)
{
    m_session.statusNote = note;
    m_statusText->SetText (note);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RequestCopy
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RequestCopy()
{
    m_hostLink->RequestCopy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StartDisk
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StartDisk (std::shared_ptr<const DiskCopy> copy, const std::wstring & reason)
{
    m_hostLink->StartDisk (copy, reason);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeTracks
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeTracks (const InspectorReply & reply)
{
    m_hostLink->TakeWrittenTracks (reply);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::NotifyDiskStarting
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::NotifyDiskStarting (const DiskChange & change)
{
    for (IInspectorComponent * component : m_components)
    {
        component->OnDiskStarting (change);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::NotifyDiskStarted
//
//  A saved Files choice applies to every disk (FR-080).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::NotifyDiskStarted (const DiskChange & change)
{
    if (change.hasDisk)
    {
        m_isFilesDefaulted = m_isFilesSaved;
    }

    for (IInspectorComponent * component : m_components)
    {
        component->OnDiskStarted (change);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::NotifyAnalysisChanged
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::NotifyAnalysisChanged()
{
    for (IInspectorComponent * component : m_components)
    {
        component->OnAnalysisChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OfferReply
//
//  A reply goes to the first component that takes it.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OfferReply (const InspectorReply & reply)
{
    bool  isTaken = false;



    for (IInspectorComponent * component : m_components)
    {
        isTaken = isTaken || component->TakeReply (reply);
    }

    return isTaken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ConfirmDiscardEdits
//
//  Before something discards the pending edits (FR-108): true when there are
//  none, or the user confirms that they are discarded.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::ConfirmDiscardEdits (const std::wstring & action)
{
    return m_editing->ConfirmDiscardEdits (action);
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

    m_session.context.palette = DiskInspectorPalette::Resolve (*m_session.theme);
    m_tooltip.SetTheme (*m_session.theme);

    m_hostLink->TakeReplies();
    m_hostLink->TakeResults();
    m_comparisonPart->TakeComparison();
    m_comparisonPart->SyncSideB();
    m_hostLink->TickHead();
    UpdateRings();
    UpdateControls();
    RefreshTables();

    //  While the File map tab is up, the selected file's sectors are marked
    //  in the platter and the strip as in the grid (FR-090).
    m_session.context.isFileHighlighted = m_session.isDiskGroup && m_session.diskTab == kTabFileMap && m_session.context.selectedFile >= 0;

    //  The picker's width matches its label, measured as it is drawn, and the
    //  sector row's height its row count for the track selected.
    m_session.isLayoutStale = m_session.isLayoutStale || m_sectorRow->GetRowCount (m_sectorRow->GetBounds().right - m_sectorRow->GetBounds().left, m_session.scaler) != m_sectorRows;

    if (m_session.isLayoutStale)
    {
        m_session.isLayoutStale = false;
        Layout (m_boundsDip, m_session.hostScaler);
    }

    if (m_session.model.IsFlying())
    {
        m_session.model.StepFlight (InspectorMarquee::GetClockMs());
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
    m_findPart->Tick    (static_cast<int64_t> (GetTickCount64()));
    m_modeTabs->Tick    (static_cast<int64_t> (GetTickCount64()));
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
    m_hostLink    = std::make_unique<HostLink> (m_session, GetHost());
    m_driveTabs   = CreateChild<DxuiTabStrip>();
    m_platterView = CreateChild<PlatterView> (m_session.context);
    m_zoomOut     = CreateChild<DxuiButton> (s_kpszMinus);
    m_zoomIn      = CreateChild<DxuiButton> (L"+");
    m_fit         = CreateChild<DxuiButton> (L"Fit");
    m_topDown     = CreateChild<DxuiButton> (L"Top down");
    m_zoomLabel   = CreateChild<DxuiLabel> (L"",         DxuiTextRole::Body,    DxuiTextHAlign::Left);
    m_hintLabel   = CreateChild<DxuiLabel> (s_kpszHint,  DxuiTextRole::Muted  , DxuiTextHAlign::Left);
    m_headerView  = CreateChild<TrackHeaderView> (m_session.context);
    m_stripView   = CreateChild<TrackStripView>  (m_session.context);
    m_stripB      = CreateChild<TrackStripView>  (m_session.contextB);
    m_stripOut    = CreateChild<DxuiButton> (s_kpszMinus);
    m_stripIn     = CreateChild<DxuiButton> (L"+");
    m_stripWhole  = CreateChild<DxuiButton> (L"Whole track");
    m_stripReadout = CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_stripHint   = CreateChild<DxuiLabel> (s_kpszStripHint, DxuiTextRole::Muted, DxuiTextHAlign::Right);
    m_sectorRow   = CreateChild<SectorRowView>   (m_session.context);
    m_tabs        = CreateChild<DxuiTabStrip>();
    m_byteView    = CreateChild<SectorByteView>  (m_session.context);
    m_nibblesTab  = CreateChild<NibblesTab>      (m_session.context);
    m_fieldsTab   = CreateChild<InspectorTableView> (m_session.context);
    m_fluxTab     = CreateChild<FluxTimingTab>   (m_session.context);
    m_legendButton = CreateChild<DxuiButton> (L"Legend");
    m_decodeButton = CreateChild<DxuiButton> (L"Decode settings...");
    m_exportButton = CreateChild<DxuiButton> (L"Export...");
    m_goToButton   = CreateChild<DxuiButton> (L"Go to");
    m_findButton   = CreateChild<DxuiButton> (L"Find");
    m_copySector   = CreateChild<DxuiButton> (L"Copy sector");
    m_compareButton = CreateChild<DxuiButton> (L"Compare with...");
    m_followCheck  = CreateChild<DxuiCheckbox> (L"Follow head");
    m_goToHead     = CreateChild<DxuiButton> (L"Go to head");
    m_diffsCheck   = CreateChild<DxuiCheckbox> (L"Differences");
    m_alignmentCheck = CreateChild<DxuiCheckbox> (L"Alignment");
    m_filesCheck     = CreateChild<DxuiCheckbox> (L"Files");
    m_readsCheck     = CreateChild<DxuiCheckbox> (L"Reads");
    m_modeTabs     = CreateChild<DxuiSegmentedControl>();
    m_rangeDown    = CreateChild<DxuiButton> (s_kpszMinus);
    m_rangeUp      = CreateChild<DxuiButton> (L"+");
    m_rangeLabel   = CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Center);
    m_legend       = CreateChild<PlatterLegendView> (m_session.context);
    m_tracksTab   = CreateChild<InspectorTableView> (m_session.context);
    m_findingsTab = CreateChild<FindingsTab>     (m_session.context);
    m_imageTab    = CreateChild<InspectorTableView> (m_session.context);
    m_files       = std::make_unique<FilesView> (m_session, GetHost());
    m_files->Create();
    m_statusText   = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Left);
    m_zoomControl  = CreateChild<DxuiZoomControl>();
    m_diffsTab    = CreateChild<DifferencesTab> (m_session.context, m_session.comparison);
    m_comparisonPart = std::make_unique<ComparisonPart> (m_session, GetHost(), *m_diffsTab, *m_stripB, *m_files);
    m_comparisonPart->Create();
    m_findPart    = std::make_unique<FindPart> (m_session, GetHost());
    m_findPart->Create();
    m_findResults = &m_findPart->GetResults();
    m_editing     = std::make_unique<EditingPart> (m_session, GetHost(), *m_byteView);
    m_editing->Create();

    m_driveTabs->SetOnChange ([this] (int index) { (void) ShowDrive (index); });
    m_legendButton->SetOnClick ([this] () { if (m_isLegendPinned) { HideLegend(); } else if (static_cast<int64_t> (GetTickCount64()) - m_legendClosedMs > s_kLegendReopenMs) { ShowLegend (true); } });
    m_tabs->SetOnChange      ([this] (int index) { if (index < kTrackTabCount) { ShowTrackTab (index); } else { ShowDiskTab (GetShownDiskTabs()[static_cast<size_t> (index - kTrackTabCount)]); } });
    m_tracksTab->SetColumns  (InspectorTables::GetTrackColumns());
    m_fieldsTab->SetColumns  (InspectorTables::GetFieldColumns());
    m_imageTab->SetColumns   (InspectorTables::GetImageColumns());
    m_zoomControl->SetOnZoom ([this] (double zoom) { SetZoom (zoom); });
    m_tracksTab->SetOnSelect   ([this] (const TableRow & row) { SelectFromRow (row); });
    m_findingsTab->SetOnSelect ([this] (const TableRow & row) { SelectFromRow (row); });
    m_fieldsTab->SetOnSelect   ([this] (const TableRow & row) { SelectFromRow (row); });
    m_zoomOut->SetOnClick    ([this] () { m_platterView->ZoomAboutCenter (1.0 / kZoomStep); });
    m_zoomIn->SetOnClick     ([this] () { m_platterView->ZoomAboutCenter (kZoomStep); });
    m_fit->SetOnClick        ([this] () { m_session.model.Fit(); });
    m_topDown->SetOnClick    ([this] () { m_session.model.SetTilt (0.0); });
    m_stripOut->SetOnClick   ([this] () { m_stripView->ZoomAboutCenter (1.0 / kZoomStep); });
    m_stripIn->SetOnClick    ([this] () { m_stripView->ZoomAboutCenter (kZoomStep); });
    m_stripWhole->SetOnClick ([this] () { m_stripView->ShowWholeTrack(); });
    m_decodeButton->SetOnClick ([this] () { OpenDecodeSettings(); });
    m_exportButton->SetOnClick ([this] () { OpenExport(); });
    m_goToButton->SetOnClick   ([this] () { OpenGoTo(); });
    m_findButton->SetOnClick   ([this] () { m_findPart->OpenFindBar(); });
    m_session.context.edits = &m_session.edits;
    m_copySector->SetOnClick   ([this] () { CopySector(); });
    m_compareButton->SetOnClick   ([this] () { m_comparisonPart->OpenCompare(); });
    m_followCheck->SetOnChange    ([this] (bool isChecked) { m_hostLink->SetFollowHead (isChecked); });
    m_goToHead->SetOnClick        ([this] () { m_hostLink->GoToHead(); });
    m_diffsCheck->SetOnChange     ([this] (bool isChecked) { m_session.context.isDiffsOverlay = isChecked; });
    m_stripB->SetSide (ComparisonSession::kSideB);
    m_alignmentCheck->SetOnChange ([this] (bool isChecked) { m_platterView->SetAlignmentShown (isChecked); });
    m_filesCheck->SetOnChange     ([this] (bool isChecked) { m_session.context.isFilesOverlay = isChecked; });
    m_readsCheck->SetOnChange     ([this] (bool isChecked) { m_session.context.isReadsOverlay = isChecked; });
    m_modeTabs->SetSegments  ({ L"Structure", L"Timing" });
    m_modeTabs->SetOnChange  ([this] (int index) { m_session.context.isTimingMode = (index == 1); });
    m_rangeDown->SetOnClick  ([this] () { StepRange (-1); });
    m_rangeUp->SetOnClick    ([this] () { StepRange (1); });

    m_components = { m_hostLink.get(), m_files.get(), m_editing.get(), m_comparisonPart.get(), m_findPart.get() };

    m_tooltip.SetPopupHost (GetPopupHost());

    //  The tips read out what the pointer rests on, so they stay while it
    //  does rather than dismissing themselves.
    m_tooltip.SetHasLifetime (false);

    m_session.diskTab = kTabTracks;
    ShowTrackTab (kTabSectorData);

    //  Tab and Shift+Tab walk the window's controls (FR-059).
    GetFocusManager().SetTheme (GetTheme());
    GetFocusManager().Attach   (this);
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
        m_session.host->SetHeadStateWanted (false);
        Hide();
        m_session.host->OnInspectorClosed();
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



    m_session.hostScaler = scaler;
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

    Layout (m_boundsDip, m_session.hostScaler);
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
    int                        drives    = std::max (1, m_session.host != nullptr ? m_session.host->GetDriveCount() : 1);
    int                        toolbar   = scaler.ToPx (s_kToolbarDip);
    int                        rowBottom = boundsDip.top + toolbar;
    int                        nameLeft  = boundsDip.left + 2 * margin + drives * tab;
    vector<ToolItem>           items     = GetToolItems (scaler);
    int                        rows      = FlowToolItems (items, boundsDip.left + margin, nameLeft + scaler.ToPx (s_kMinFileNameDip) + margin, boundsDip.right - margin, boundsDip.top, toolbar, margin / 2);
    int                        bar       = m_session.comparison.IsComparing() ? scaler.ToPx (s_kBarDip) : 0;
    int                        find      = m_session.isFindOpen ? FindBar::GetHeightPx (scaler) : 0;
    int                        top       = boundsDip.top + rows * toolbar + bar + find;
    int                        ctlLeft   = static_cast<int> (boundsDip.right) - margin;
    int                        strip     = scaler.ToPx (s_kStripDip);
    int                        band      = 2 * margin + strip + (m_session.comparison.IsComparing() ? strip + margin / 2 : 0) + margin / 2 + row;
    int                        bandTop   = std::max (top, static_cast<int> (boundsDip.bottom) - band);
    int                        x         = boundsDip.left + margin;
    int                        column    = splitX - boundsDip.left - 2 * margin;
    int                        areaH     = std::max (0, bandTop - top - 2 * margin - row);
    int                        side      = std::max (0, std::min (column, areaH));
    int                        y         = 0;
    int                        i         = 0;
    int                        header    = scaler.ToPx (TrackHeaderView::kLineDip * TrackHeaderView::kLines);
    int                        rowsH     = 0;
    int                        minColumn = std::max (scaler.ToPx (s_kMinPlatterColumnDip), GetPlatterControlsWidthPx (scaler));
    RECT                       right     = {};
    RECT                       platter   = {};
    vector<DxuiTabStrip::Tab>  tabs;



    //  Wherever the splitter is, the platter column is at least as wide as
    //  its control row, up to the splitter's largest share of the window.
    if (column < minColumn)
    {
        column = std::max (0, std::min (minColumn, static_cast<int> (width * s_kMaxSplit) - 2 * margin));
        splitX = boundsDip.left + 2 * margin + column;
        side   = std::max (0, std::min (column, areaH));
    }

    //  A platter its height limits leaves its column no wider than it and its
    //  control row need, and the track column gains the rest.
    if (column > std::max (areaH, minColumn))
    {
        column = std::max (areaH, minColumn);
        splitX = boundsDip.left + 2 * margin + column;
        side   = std::min (column, areaH);
    }

    m_session.scaler = scaler;
    m_toolSig        = GetToolSig();
    m_tooltip.SetDpi (scaler.GetDpi());
    m_tooltip.SetViewportSize (width, boundsDip.bottom - boundsDip.top);
    m_session.context.scenePx = boundsDip;

    m_toolbarPx  = { boundsDip.left, boundsDip.top, boundsDip.right, top - bar - find };
    m_findPart->Layout ({ boundsDip.left, top - find, boundsDip.right, top }, scaler);
    m_splitterPx = { splitX, top, splitX + scaler.ToPx (s_kSplitterDip), bandTop };
    right        = { m_splitterPx.right + margin, top + margin, boundsDip.right - margin, bandTop - margin / 2 };

    for (i = 0; i < drives; i++)
    {
        tabs.push_back ({ { x + i * tab, boundsDip.top + margin / 2, x + (i + 1) * tab, rowBottom - margin / 2 }, std::format (L"Drive {}", i + 1) });
    }

    m_driveTabs->SetTabs (std::move (tabs));
    m_driveTabs->SetSelected (m_session.drive);
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

    m_comparisonPart->Layout ({ boundsDip.left, top - bar - find, boundsDip.right, top - find }, scaler);

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
    m_editing->Layout ({ right.left, y, right.right, y + m_editing->GetHeightPx (right.right - right.left, scaler) }, scaler);
    y += m_editing->GetHeightPx (right.right - right.left, scaler);
    m_byteView->Layout ({ right.left, y, right.right, right.bottom }, scaler);

    m_files->Layout (m_diskContentPx, scaler);

    //  "Copy sector" at the end of the Sector data header's first line, with
    //  room reserved for it there.
    m_copySector->Layout       ({ right.right - scaler.ToPx (s_kCopySectorDip), y, right.right, y + scaler.ToPx (SectorByteView::kHeaderDip) }, scaler);
    m_byteView->SetHeaderReserve (scaler.ToPx (s_kCopySectorDip) + margin);

    //  The strip across the bottom, B's under it while comparing (FR-121).
    y = bandTop + margin;
    m_stripView->Layout ({ x, y, boundsDip.right - margin, y + strip }, scaler);
    y += strip + margin / 2;
    m_stripB->Layout ({ x, y, boundsDip.right - margin, y + (m_session.comparison.IsComparing() ? strip : 0) }, scaler);
    y += m_session.comparison.IsComparing() ? strip + margin / 2 : 0;
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
    m_topDown->Layout   ({ x, area.top, x + GetLabelWidthPx (L"Top down", s_kCheckLabelPadDip, scaler), area.bottom }, scaler);
    x += GetLabelWidthPx (L"Top down", s_kCheckLabelPadDip, scaler) + gap;

    for (const auto & [check, label] : { std::pair { m_alignmentCheck, L"Alignment" }, std::pair { m_filesCheck, L"Files" }, std::pair { m_readsCheck, L"Reads" },
                                     std::pair { m_diffsCheck, L"Differences" } })
    {
        int  w = GetLabelWidthPx (label, s_kCheckLabelPadDip, scaler);

        check->Layout ({ x, area.top, x + w, area.bottom }, scaler);
        x += (check == m_diffsCheck && !m_session.comparison.IsComparing()) ? 0 : w + gap;
    }

    m_legendButton->Layout ({ area.right - scaler.ToPx (s_kLegendButtonDip), area.top, area.right, area.bottom }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetPlatterControlsWidthPx
//
//  The width LayoutPlatterControls lays its row out in, so the platter
//  column is never narrower than the row.
//
////////////////////////////////////////////////////////////////////////////////

int DiskInspectorWindow::GetPlatterControlsWidthPx (const DxuiDpiScaler & scaler) const
{
    int  gap   = scaler.ToPx (s_kMarginDip) / 2;
    int  width = 2 * scaler.ToPx (s_kButtonDip) + scaler.ToPx (s_kFitDip) + gap + scaler.ToPx (s_kZoomLabelDip) + gap +
                 GetLabelWidthPx (L"Top down", s_kCheckLabelPadDip, scaler) + gap + scaler.ToPx (s_kLegendButtonDip);



    for (LPCWSTR label : { L"Alignment", L"Files", L"Reads" })
    {
        width += GetLabelWidthPx (label, s_kCheckLabelPadDip, scaler) + gap;
    }

    if (m_session.comparison.IsComparing())
    {
        width += GetLabelWidthPx (L"Differences", s_kCheckLabelPadDip, scaler) + gap;
    }

    return width;
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

    if (m_session.comparison.IsComparing())
    {
        m_comparisonPart->PaintUnder (painter, text, theme);
    }

    DxuiWindow::Paint (painter, text, theme);

    m_files->PaintOver (painter, text, theme);

    painter.FillRect (static_cast<float> (m_tabGapPx.left), static_cast<float> (m_tabGapPx.top), static_cast<float> (m_tabGapPx.right - m_tabGapPx.left),
                      static_cast<float> (m_tabGapPx.bottom - m_tabGapPx.top), theme.Divider());

    if (m_session.is35)
    {
        PaintNot35 (painter, text, theme);
    }

    //  A file that could not be opened says why where the platter would be.
    if (!m_session.context.hasDisk && !m_session.openError.empty())
    {
        text.DrawString (m_session.openError.c_str(), static_cast<float> (m_platterAreaPx.left), static_cast<float> (m_platterAreaPx.top),
                         static_cast<float> (m_platterAreaPx.right - m_platterAreaPx.left), static_cast<float> (m_platterAreaPx.bottom - m_platterAreaPx.top),
                         theme.ErrorForeground(), m_session.scaler.ToPxf (InspectorView::kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                         DxuiTextVAlign::Center, DxuiFontWeight::Normal, true);
    }

    if (!m_session.context.hasDisk && m_session.openError.empty() && m_session.pendingRequest == 0)
    {
        PaintNoDisk (painter, text, theme);
    }

    PaintHelp (text, theme);

    //  The view the keys act on, outlined once a key has been used, unless a
    //  control has the focus (FR-059).
    if (m_isFocusShown && m_session.context.hasDisk && !m_session.is35 && GetFocusManager().GetFocusedControl() == nullptr)
    {
        focus = GetKeyTargetBounds();
        InflateRect (&focus, m_session.scaler.ToPx (s_kFocusGapDip), m_session.scaler.ToPx (s_kFocusGapDip));
        painter.OutlineRect (static_cast<float> (focus.left), static_cast<float> (focus.top), static_cast<float> (focus.right - focus.left),
                             static_cast<float> (focus.bottom - focus.top), m_session.scaler.ToPxf (2.0f), theme.FocusRing());
    }

    if (m_walkthrough.IsOpen())
    {
        m_walkthrough.Paint (text, theme, m_session.scaler, m_boundsDip, GetWalkthroughArea (m_walkthrough.GetArea()));
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
    float         pad     = m_session.scaler.ToPxf (s_kChipPadDip);
    float         gap     = m_session.scaler.ToPxf (s_kChipGapDip);
    float         chipH   = m_session.scaler.ToPxf (s_kChipHeightDip);
    float         textPx  = m_session.scaler.ToPxf (s_kChipTextDip);
    float         x       = static_cast<float> (m_chipsPx.left);
    float         y       = (m_chipsPx.top + m_chipsPx.bottom - chipH) / 2.0f;
    float         w       = 0;
    float         h       = 0;
    bool          isFull  = false;
    std::wstring  name    = m_session.context.hasDisk ? m_session.GetFileName() : std::wstring (m_session.pendingRequest != 0 ? s_kpszAnalyzing : s_kpszNoDisk);



    //  A lock before the name of a write-protected disk in the drive (FR-065).
    name = (m_session.context.hasDisk && m_session.IsWriteProtected()) ? std::wstring (s_kpszLock) + L" " + name : name;

    painter.FillRect (static_cast<float> (m_toolbarPx.left), static_cast<float> (m_toolbarPx.top),
                      static_cast<float> (m_toolbarPx.right - m_toolbarPx.left), static_cast<float> (m_toolbarPx.bottom - m_toolbarPx.top),
                      theme.BackgroundElevated());

    text.DrawString (name.c_str(), static_cast<float> (m_fileNamePx.left), static_cast<float> (m_fileNamePx.top),
                     static_cast<float> (m_fileNamePx.right - m_fileNamePx.left), static_cast<float> (m_fileNamePx.bottom - m_fileNamePx.top),
                     theme.HeadingForeground(), m_session.scaler.ToPxf (InspectorView::kTextDip + 1.0f), DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

    if (m_session.context.hasDisk)
    {
        //  While comparing, the result comes first (FR-121).
        if (m_session.comparison.IsComparing())
        {
            std::wstring  result = (m_session.comparison.IsBusy() || !m_session.comparison.HasResult()) ? std::wstring (L"Comparing") : ComparisonText::FormatResult (m_session.comparison.GetResult());

            text.MeasureString (result.c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            painter.FillRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, theme.SelectionBackground());
            text.DrawString (result.c_str(), x, y, w + 2 * pad, chipH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
            x += w + 2 * pad + gap;
        }

        for (const SummaryChip & chip : InspectorText::BuildChips (m_session.analysis.summary))
        {
            text.MeasureString (chip.text.c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            isFull = isFull || x + w + 2 * pad > m_chipsPx.right;

            if (!isFull)
            {
                painter.FillRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, theme.ButtonIdle());
                painter.OutlineRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, m_session.scaler.ToPxf (1.0f),
                                            chip.isBad ? m_session.context.palette.colors.sectorBad : theme.ButtonBorder());
                text.DrawString (chip.text.c_str(), x, y, w + 2 * pad, chipH, chip.isBad ? m_session.context.palette.colors.sectorBad : theme.Foreground(), textPx,
                                 DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

                x += w + 2 * pad + gap;
            }
        }

        if (!m_session.analysis.settings.IsStandard() && !isFull)
        {
            text.MeasureString (L"Custom decode settings", textPx, DxuiTheme::kBodyFace, w, h);
            painter.FillRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, theme.SelectionBackground());
            text.DrawString (L"Custom decode settings", x, y, w + 2 * pad, chipH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            x += w + 2 * pad + gap;
        }

        if (m_session.scheduler->HasPending())
        {
            text.DrawString (s_kpszAnalyzing, x, y, m_session.scaler.ToPxf (s_kAnalyzingDip), chipH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
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
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal && m_files->IsOverGrid (p))
    {
        m_files->ZoomGrid (ev.wheelDelta, p);
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
        m_session.keyTarget = GetKeyTarget (p);
        m_isFocusShown      = false;
        GetFocusManager().SetFocused (nullptr);
        m_findPart->OnPress();
    }

    if (!isCovered && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right && m_session.context.hasDisk &&
        (m_session.keyTarget == KeyTarget::SectorData || m_session.keyTarget == KeyTarget::Nibbles || m_session.keyTarget == KeyTarget::Tracks))
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
        Layout (m_boundsDip, m_session.hostScaler);
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
    m_session.context.hoverPx = (ev.kind == DxuiMouseEventKind::Leave) ? POINT { SHRT_MIN, SHRT_MIN } : (ev.kind == DxuiMouseEventKind::Move ? p : m_session.context.hoverPx);

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        UpdateTooltip (p);
        m_statusText->SetText (!m_session.context.hasDisk ? m_session.statusNote : IsInside (m_platterAreaPx, p) ? s_kpszHint : IsInside (m_stripAreaPx, p) ? s_kpszStripHint : m_session.statusNote);
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
    int   qt        = m_session.model.GetQuarterTrack();



    if (OnWalkthroughKey (ev))
    {
        return true;
    }

    //  Keys go to the find bar while it has the focus, all but the window's
    //  zoom and its Ctrl+F and Ctrl+G.
    if (m_findPart->OnKey (ev))
    {
        isHandled = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_TAB && !ev.ctrl && !ev.alt)
    {
        //  Tab and Shift+Tab move the focus from control to control, in the
        //  order the controls are laid out now (FR-059).
        RefreshFocusOrder (nullptr);
        isHandled = GetFocusManager().HandleKey (ev.shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
    }
    else if (ev.kind == DxuiKeyEventKind::Down && GetFocusManager().GetFocusedControl() != nullptr && (ev.vk == VK_ESCAPE || ev.vk == VK_F6))
    {
        //  Escape and F6 return the keys from the focused control to the views.
        GetFocusManager().SetFocused (nullptr);
        isHandled = ev.vk == VK_ESCAPE;
    }
    else if (GetFocusManager().GetFocusedControl() != nullptr && GetFocusManager().GetFocusedControl()->OnKey (ev))
    {
        //  A focused control takes Enter, Space and its arrows first.
        isHandled = true;
    }
    else if (m_editing->OnKey (ev))
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
    else if (ev.kind == DxuiKeyEventKind::Down && !ev.alt && m_session.context.hasDisk)
    {
        isHandled = true;

        if (ev.ctrl)
        {
            switch (ev.vk)
            {
                case VK_LEFT:  m_session.model.PanBy ({  s_kPanStep, 0.0 }); break;
                case VK_RIGHT: m_session.model.PanBy ({ -s_kPanStep, 0.0 }); break;
                case VK_UP:    m_session.model.PanBy ({ 0.0,  s_kPanStep }); break;
                case VK_DOWN:  m_session.model.PanBy ({ 0.0, -s_kPanStep }); break;
                case 'A':      SelectAll();                          break;
                case 'C':      if (ev.shift) { CopySector(); } else { Copy(); } break;
                case 'F':      m_findPart->OpenFindBar();            break;
                case 'G':      OpenGoTo();                           break;
                default:       isHandled = false;                    break;
            }
        }
        else if (ev.vk == VK_ESCAPE && !ev.shift)
        {
            m_session.model.ClearSelection();
            OnSelection();
        }
        else if (ev.shift && (m_session.keyTarget == KeyTarget::Nibbles || m_session.keyTarget == KeyTarget::SectorData) &&
                 (ev.vk == VK_LEFT || ev.vk == VK_RIGHT || ev.vk == VK_UP || ev.vk == VK_DOWN))
        {
            ExtendSelection (ev.vk);
        }
        else if (m_session.keyTarget == KeyTarget::Strip && IsStripKey (ev.vk))
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
                case '0':          case VK_NUMPAD0:  m_session.model.Fit();                                    break;
                case VK_UP:    m_session.model.SelectQuarterTrack (qt - 1);                           OnSelection(); break;
                case VK_DOWN:  m_session.model.SelectQuarterTrack (qt + 1);                           OnSelection(); break;
                case VK_PRIOR: m_session.model.SelectQuarterTrack ((qt - 1) / kPerTrack * kPerTrack); OnSelection(); break;
                case VK_NEXT:  m_session.model.SelectQuarterTrack ((qt / kPerTrack + 1) * kPerTrack); OnSelection(); break;
                case VK_LEFT:  StepSector (-1, false); break;
                case VK_RIGHT: StepSector ( 1, false); break;
                case VK_HOME:  StepSector (-1, true);  break;
                case VK_F3:    m_findPart->StepFind (ev.shift ? -1 : 1); break;
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
    else if (m_platterView != nullptr && (m_platterView->IsDragging() || (IsInside (m_platterView->GetBounds(), clientPx) && !m_session.model.IsAtFit())))
    {
        cursor = IDC_SIZEALL;
    }

    return cursor;
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
    const DiskCopy *   copy     = m_session.analysis.copy.get();
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
        else if (m_session.scheduler->IsPending (slot) || slot >= static_cast<int> (m_session.analysis.tracks.size()) || m_session.analysis.tracks[slot] == nullptr)
        {
            renderer.SetRing (qt, PlatterRingState::Pending, nullptr);
        }
        else if (m_session.changedSlots.contains (slot) || m_session.levels[slot] == nullptr)
        {
            if (m_session.levels[slot] == nullptr)
            {
                auto  levels = std::make_shared<PlatterRenderer::Levels>();

                PlatterCells::BuildCells  (*m_session.analysis.tracks[slot], cells);
                PlatterCells::BuildLevels (cells, *levels);
                m_session.levels[slot] = levels;

                if (m_session.analysis.tracks[slot]->framed.isFlux)
                {
                    auto  timing = std::make_shared<PlatterRenderer::Levels>();

                    FluxTiming::BuildDeviations (*m_session.analysis.tracks[slot], cells);
                    FluxTiming::BuildLevels     (cells, *timing);
                    m_session.timingLevels[slot] = timing;
                }
            }

            renderer.SetRing (qt, PlatterRingState::Data, m_session.levels[slot], m_session.timingLevels[slot]);
        }
    }

    m_session.changedSlots.clear();
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
    if (m_zoomLabel != nullptr && m_session.scaler.GetDpi() != 0 && GetToolSig() != m_toolSig)
    {
        Layout (m_boundsDip, m_session.hostScaler);
    }

    if (m_zoomLabel != nullptr)
    {
        m_zoomLabel->SetText    (std::format (L"{:.0f}{}", m_session.model.GetZoom(), s_kpszMultiplyX));
        m_fit->SetEnabled       (!m_session.model.IsAtFit());
        m_topDown->SetEnabled   (m_session.model.IsTilted());
        m_zoomOut->SetEnabled   (!m_session.model.IsAtFit());
        m_zoomOut->SetVisible   (m_session.context.hasDisk);
        m_decodeButton->SetVisible (m_session.context.hasDisk);
        m_exportButton->SetVisible (m_session.context.hasDisk);
        m_goToButton->SetVisible   (m_session.context.hasDisk);
        m_findButton->SetVisible   (m_session.context.hasDisk);
        m_copySector->SetVisible   (m_session.context.hasDisk && !m_session.isDiskGroup && m_session.trackTab == kTabSectorData);
        m_editing->SetShown        (m_session.context.hasDisk && !m_session.isDiskGroup && m_session.trackTab == kTabSectorData);
        m_editing->Update();
        m_copySector->SetEnabled   (m_session.model.GetSector() != nullptr && m_session.model.GetSector()->dataField >= 0);
        m_compareButton->SetVisible   (m_session.context.hasDisk || m_session.comparison.IsComparing());
        m_followCheck->SetVisible     (m_hostLink->IsHeadShown());
        m_goToHead->SetVisible        (m_hostLink->IsHeadShown());
        m_goToHead->SetEnabled        (m_session.headState != nullptr && m_session.headState->hasDisk);
        m_diffsCheck->SetVisible      (m_session.comparison.IsComparing() && m_session.context.hasDisk);
        m_comparisonPart->Update();
        m_zoomIn->SetVisible    (m_session.context.hasDisk);
        m_fit->SetVisible       (m_session.context.hasDisk);
        m_topDown->SetVisible   (m_session.context.hasDisk);
        m_zoomLabel->SetVisible (m_session.context.hasDisk);
        m_hintLabel->SetVisible    (false);
        m_legend->SetVisible       (false);
        m_legendButton->SetVisible (m_session.context.hasDisk);
        m_files->SyncSelection();
        m_stripOut->SetVisible     (m_session.context.hasDisk);
        m_stripIn->SetVisible      (m_session.context.hasDisk);
        m_stripWhole->SetVisible   (m_session.context.hasDisk);
        m_stripWhole->SetEnabled   (m_session.model.GetStripSpan() < 1.0);
        m_stripOut->SetEnabled     (m_session.model.GetStripSpan() < 1.0);
        m_stripReadout->SetVisible (m_session.context.hasDisk);
        m_stripReadout->SetText    (m_stripView->GetReadout() + ((m_session.model.GetTrack() != nullptr && m_session.model.GetNibbleCount() > 0)
                                    ? L"    " + InspectorText::FormatSelection (*m_session.model.GetTrack(), m_session.model.GetFirstNibble(), m_session.model.GetNibbleCount()) : L""));
        m_stripHint->SetVisible    (false);
        m_alignmentCheck->SetVisible (m_session.context.hasDisk);
        m_filesCheck->SetVisible     (m_session.context.hasDisk);
        m_readsCheck->SetVisible     (m_session.context.hasDisk && m_session.IsWindowDiskA());
        m_modeTabs->SetVisible   (m_session.context.hasDisk);
        m_rangeDown->SetVisible  (m_session.context.hasDisk && m_session.context.isTimingMode);
        m_rangeUp->SetVisible    (m_session.context.hasDisk && m_session.context.isTimingMode);
        m_rangeLabel->SetVisible (m_session.context.hasDisk && m_session.context.isTimingMode);
        m_rangeLabel->SetText    (std::wstring (s_kpszPlusMinus) + InspectorFormat::FormatPercent (m_session.context.timingRange).substr (1));
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
    const TrackAnalysis *   track  = m_session.model.GetTrack();
    const AnalyzedSector *  sector = m_session.model.GetSector();
    int                     nibble = m_session.model.GetFirstNibble();



    if (nibble < 0 && track != nullptr && sector != nullptr)
    {
        nibble = track->fields[sector->addressField].firstNibble;
    }

    m_nibblesTab->ScrollTo (nibble);
    SyncTables();

    //  A new sector the user chooses anywhere but on the platter starts a
    //  flight to it. One chosen on the platter is in view there, and one
    //  chosen by Follow head starts none.
    if (m_session.context.selectionSource == m_platterView)
    {
        m_session.model.StopFlight();
    }
    else if (!m_hostLink->IsHeadSelecting() && sector != nullptr && (m_session.model.GetQuarterTrack() != m_flownQuarterTrack || m_session.model.GetSectorIndex() != m_flownSector))
    {
        m_session.model.StartFlight (InspectorMarquee::GetClockMs());
    }

    m_flownQuarterTrack = m_session.model.GetQuarterTrack();
    m_flownSector       = m_session.model.GetSectorIndex();

    //  A selection made by hand ends Follow head (FR-067).
    if (!m_hostLink->IsHeadSelecting() && m_hostLink->IsFollowingHead())
    {
        m_hostLink->OnHandSelection();
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
    m_session.trackTab    = tab;
    m_session.isDiskGroup = false;
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
    const std::array<InspectorView *, 8>  views     = { m_platterView, m_stripView, m_stripB, m_sectorRow, m_byteView, m_nibblesTab, m_fluxTab, m_headerView };



    for (InspectorView * view : views)
    {
        if (tip.empty() && view != nullptr && view->IsVisible() && IsInside (view->GetBounds(), pointPx) && view->GetTooltip (pointPx, tip, anchor))
        {
            view->GetTooltipSwatches (pointPx, swatches);
            isInstant = view == m_platterView || view == m_stripView || view == m_stripB;
        }
    }

    //  The File map grid's tip appears at once, as the platter's does.
    if (tip.empty() && m_files->GetGridTip (pointPx, tip, anchor, swatches))
    {
        isInstant = true;
    }

    if (tip.empty() && m_session.context.hasDisk)
    {
        tip = GetButtonTip (pointPx, anchor);
    }

    //  Only slot 6's drives are inspected (FR-060).
    if (tip.empty() && m_session.host != nullptr && m_session.host->HasOtherDiskController() && IsInside (m_driveTabs->GetBounds(), pointPx))
    {
        tip    = L"Slot 6 drives only";
        anchor = m_driveTabs->GetBounds();
    }

    if (tip.empty() && PtInRect (&m_helpPx, pointPx))
    {
        tip    = L"Open the walkthrough";
        anchor = m_helpPx;
    }

    //  A write-protected disk's causes follow its name (FR-065).
    if (tip.empty() && m_session.context.hasDisk && PtInRect (&m_fileNamePx, pointPx))
    {
        tip    = m_session.GetFileName();
        tip   += m_session.IsWriteProtected() ? L"\nWrite-protected: " + InspectorText::FormatWriteProtect (m_session.context.drive->writeProtect) : L"";
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
    const TrackAnalysis *  track = m_session.model.GetTrack();
    vector<int>            order = (track != nullptr) ? SectorRowView::GetPassingOrder (*track) : vector<int>();
    int                    count = static_cast<int> (order.size());
    int                    at    = 0;



    if (count > 0)
    {
        at = static_cast<int> (std::find (order.begin(), order.end(), m_session.model.GetSectorIndex()) - order.begin());
        at = isToEnd ? (delta < 0 ? 0 : count - 1) : (at + delta + count) % count;

        m_session.model.SelectSector (m_session.model.GetQuarterTrack(), order[at]);
        OnSelection();
    }
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
    m_session.diskTab     = tab;
    m_session.isDiskGroup = true;
    ShowTabContent();

    if (m_session.scaler.GetDpi() != 0)
    {
        Layout (m_boundsDip, m_session.hostScaler);
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
    bool  isTrack = !m_session.isDiskGroup;



    m_byteView->SetVisible    (isTrack && m_session.trackTab == kTabSectorData);
    m_nibblesTab->SetVisible  (isTrack && m_session.trackTab == kTabNibbles);
    m_fieldsTab->SetVisible   (isTrack && m_session.trackTab == kTabFields);
    m_fluxTab->SetVisible     (isTrack && m_session.trackTab == kTabFluxTiming);
    m_tracksTab->SetVisible   (!isTrack && m_session.diskTab == kTabTracks);
    m_findingsTab->SetVisible (!isTrack && m_session.diskTab == kTabFindings);
    m_imageTab->SetVisible    (!isTrack && m_session.diskTab == kTabImage);
    m_diffsTab->SetVisible    (!isTrack && m_session.diskTab == kTabDifferences);
    m_findResults->SetVisible (!isTrack && m_session.diskTab == kTabFindResults);
    m_files->Show (!isTrack && m_session.diskTab == kTabFileMap);
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
    const TrackAnalysis *  track = m_session.model.GetTrack();
    int                    qt    = m_session.model.GetQuarterTrack();



    if (m_session.isTablesDirty)
    {
        vector<std::wstring>  columns = InspectorTables::GetTrackColumns();
        vector<TableRow>      rows    = m_session.context.hasDisk ? InspectorTables::BuildTracks (m_session.analysis, m_session.context.findMarks, &m_session.edits, &m_session.appliedSlots, m_session.context.drive) : vector<TableRow>();

        //  While comparing, each quarter track's verdict (FR-121).
        if (m_session.comparison.IsComparing())
        {
            ComparisonText::AddVerdictColumn (columns, rows, m_session.comparison.GetResult(), m_session.comparison.IsWhollyBusy() || !m_session.comparison.HasResult(), m_session.comparison.GetComparing());
        }

        if (m_session.comparison.IsComparing() != m_isTracksCompared)
        {
            m_tracksTab->SetColumns (columns);
            m_isTracksCompared = m_session.comparison.IsComparing();
        }

        m_tracksTab->SetRows    (std::move (rows));
        m_diffsTab->Refresh();
        m_tracksTab->SetCaption (m_session.context.hasDisk && InspectorTables::IsAlignmentNoteShown (m_session.analysis)
                                     ? L"This image's INFO says its tracks were not imaged in sync, so their alignment to one another was not kept"
                                     : L"");
        m_findingsTab->Refresh();

        //  A disk with a volume the map reads opens on the Files overlay, the
        //  most telling view of it; the user's choice stands after that.
        if (!m_isFilesDefaulted && m_session.GetFileMap() != nullptr && m_session.GetFileMap()->notMapped == NotMappedReason::None)
        {
            m_isFilesDefaulted               = true;
            m_session.context.isFilesOverlay = true;
            m_filesCheck->SetChecked (true);
        }

        m_imageTab->SetRows (m_session.context.hasDisk ? InspectorTables::BuildImage (m_session.analysis.image, m_session.context.drive != nullptr ? m_session.analysis.copy.get() : nullptr, m_session.context.drive)
                                               : vector<TableRow>());
        m_files->RefreshFileList();

        //  Casso does not analyze 3.5" disks; such a WOZ opens on the Image
        //  tab, and every other view is covered by a note (FR-054).
        m_session.is35 = m_session.context.hasDisk && m_session.analysis.image.isWoz && m_session.analysis.image.info.diskType == WozLoader::kDiskType35;

        if (m_session.is35)
        {
            ShowDiskTab (kTabImage);
        }

        Layout (m_boundsDip, m_session.hostScaler);
    }

    if (m_session.isTablesDirty || qt != m_fieldsOf || track != m_fieldsTrack)
    {
        m_fieldsTab->SetRows (m_session.context.hasDisk ? InspectorTables::BuildFields (m_session.analysis, qt) : vector<TableRow>());
        m_fieldsOf    = qt;
        m_fieldsTrack = track;
        SyncTables();
    }

    m_session.isTablesDirty = false;
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
    int  qt     = m_session.model.GetQuarterTrack();
    int  sector = m_session.model.GetSectorIndex();



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
        m_session.model.SelectQuarterTrack (row.quarterTrack);
        track = m_session.model.GetTrack();

        if (row.sectorIndex >= 0)
        {
            m_session.model.SelectSector (row.quarterTrack, row.sectorIndex);
        }
        else if (track != nullptr && row.field >= 0 && row.field < static_cast<int> (track->fields.size()))
        {
            m_session.model.SelectNibbles (row.quarterTrack, track->fields[row.field].firstNibble, track->fields[row.field].nibbleCount);
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



    dialog.Configure (m_session.theme, m_session.analysis.settings, m_session.model.GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack);

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

    dialog.SetTheme (m_session.theme);
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
    if (m_session.context.hasDisk && m_session.analysis.copy != nullptr && (m_session.edits.IsEmpty() ||
        Confirm (L"Decode settings", { L"Pending edits on sectors that no longer decode as editable under the new settings are discarded." }, L"Change settings")))
    {
        m_session.analysis.settings = settings;
        m_session.scheduler->Restart (m_session.analysis.copy, settings);
        m_session.comparison.ApplySettings (settings);
        m_session.comparison.MarkAChanged();
        m_session.isTablesDirty = true;
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
    m_rangeStep                   = std::clamp (m_rangeStep + delta, 0, static_cast<int> (std::size (s_kRanges)) - 1);
    m_session.context.timingRange = s_kRanges[m_rangeStep];
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
//  DiskInspectorWindow::Confirm
//
//  A modal question with the lines given and a button for the action and
//  one for Cancel; true for the action.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::Confirm (const std::wstring & title, const vector<std::wstring> & lines, const std::wstring & action)
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

    dialog.SetTheme (m_session.theme);
    result = dialog.TranslateResult (dialog.ShowModalDialog (dialog.GetDefaultCommandId()));

Error:
    return result == kYes;
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



    if (m_session.comparison.IsComparing())
    {
        shown.push_back (kTabDifferences);
    }

    if (!m_session.find.IsEmpty())
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
    auto         at    = std::find (shown.begin(), shown.end(), m_session.diskTab);



    return !m_session.isDiskGroup ? m_session.trackTab : kTrackTabCount + static_cast<int> ((at != shown.end()) ? at - shown.begin() : 0);
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



    dialog.Configure (m_session.theme, m_session.analysis, m_session.model.GetQuarterTrack(), m_goToKind);

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

    dialog.SetTheme (m_session.theme);
    dialog.ShowModalDialog (IDOK);

    m_goToKind = dialog.GetKind();
    target     = dialog.GetTarget();

    if (dialog.IsChosen() && target.firstNibble >= 0)
    {
        m_session.model.SelectNibbles (target.quarterTrack, target.firstNibble, 1);
        OnSelection();
    }
    else if (dialog.IsChosen() && target.sectorIndex >= 0)
    {
        m_session.model.SelectSector (target.quarterTrack, target.sectorIndex);
        OnSelection();
    }
    else if (dialog.IsChosen())
    {
        m_session.model.SelectQuarterTrack (target.quarterTrack);
        OnSelection();
    }

Error:
    return;
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



    if (m_findPart->IsBarAt (pointPx))
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
    else if (m_files->IsListAt (pointPx))
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
    if (m_session.keyTarget == KeyTarget::SectorData)
    {
        m_session.model.SelectAllBytes();
    }
    else if (m_session.keyTarget == KeyTarget::Nibbles || m_session.keyTarget == KeyTarget::Strip)
    {
        m_session.model.SelectAllNibbles();
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
    const TrackAnalysis *  track  = m_session.model.GetTrack();
    bool                   isByte = m_session.keyTarget == KeyTarget::SectorData;
    int                    row    = isByte ? SectorByteView::kBytesPerRow : std::max (m_nibblesTab->GetPerRow(), 1);
    int                    first  = isByte ? m_session.model.GetFirstByte()  : m_session.model.GetFirstNibble();
    int                    count  = isByte ? m_session.model.GetByteCount()  : m_session.model.GetNibbleCount();
    int                    anchor = isByte ? m_session.model.GetByteAnchor() : m_session.model.GetNibbleAnchor();
    int                    last   = isByte ? DiskFieldFormat::kSectorBytes - 1 : (track != nullptr ? static_cast<int> (track->framed.nibbles.size()) - 1 : -1);
    int                    end    = (first == anchor) ? first + count - 1 : first;
    int                    step   = (vk == VK_LEFT) ? -1 : (vk == VK_RIGHT) ? 1 : (vk == VK_UP) ? -row : row;



    if (first >= 0 && count > 0 && last >= 0)
    {
        if (isByte)
        {
            m_session.model.ExtendBytes (std::clamp (end + step, 0, last));
        }
        else
        {
            m_session.model.ExtendNibbles (std::clamp (end + step, 0, last));
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
    const TrackAnalysis *   track  = m_session.model.GetTrack();
    const AnalyzedSector *  sector = m_session.model.GetSector();
    std::wstring            text;



    switch (m_session.keyTarget)
    {
        case KeyTarget::Fields:     text = m_fieldsTab->GetSelectedText();   break;
        case KeyTarget::Tracks:     text = m_tracksTab->GetSelectedText();   break;
        case KeyTarget::Findings:   text = m_findingsTab->GetSelectedText(); break;
        case KeyTarget::Image:      text = m_imageTab->GetSelectedText();    break;
        case KeyTarget::FileList:   text = m_files->GetFileList().GetSelectedText(); break;
        case KeyTarget::Differences: text = m_diffsTab->GetSelectedText();   break;
        case KeyTarget::FindResults: text = m_findResults->GetSelectedText(); break;
        case KeyTarget::FluxTiming: text = m_fluxTab->GetHistogramText();    break;

        case KeyTarget::Strip:
        case KeyTarget::Nibbles:
            text = (track != nullptr && m_session.model.GetNibbleCount() > 0) ? InspectorClipboard::FormatNibbles (*track, m_session.model.GetFirstNibble(), m_session.model.GetNibbleCount())
                                                                      : std::wstring();
            break;

        case KeyTarget::SectorData:
            if (track != nullptr && sector != nullptr && sector->dataField >= 0 && m_session.model.GetByteCount() > 0)
            {
                std::span<const Byte>  bytes = std::span<const Byte> (track->fields[sector->dataField].data.bytes).subspan (m_session.model.GetFirstByte(), m_session.model.GetByteCount());

                text = m_session.model.IsTextColumn() ? InspectorClipboard::FormatText (bytes) : InspectorClipboard::FormatHex (bytes);
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
        (void) m_session.clipboard.SetText (GetHwnd(), text);
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
    const TrackAnalysis *   track  = m_session.model.GetTrack();
    const AnalyzedSector *  sector = m_session.model.GetSector();



    if (track != nullptr && sector != nullptr && sector->dataField >= 0)
    {
        (void) m_session.clipboard.SetText (GetHwnd(), InspectorClipboard::FormatHexDump (track->fields[sector->dataField].data.bytes));
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



    image = (m_session.analysis.copy != nullptr) ? std::filesystem::path (TextEncoding::Utf8ToWide (m_session.analysis.copy->fileName)) : std::filesystem::path (L"Disk");
    dialog.Configure (m_session.theme, m_session.analysis, m_session.model.GetQuarterTrack(), m_session.model.GetSectorIndex(), image.stem().wstring());

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

    dialog.SetTheme (m_session.theme);
    dialog.ShowModalDialog (IDOK);
    BAIL_OUT_IF (!dialog.IsChosen(), S_OK);

    for (const std::wstring & note : dialog.GetRequest().notes)
    {
        list += note + L"\n";
    }

    isGoingOn = list.empty() || DxuiMessageBox (GetHwnd(), m_session.theme, (L"The export will include these as noted:\n\n" + list + L"\nExport anyway?").c_str(),
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
        (void) DxuiMessageBox (GetHwnd(), m_session.theme, (L"The export could not be written to " + target.wstring() + L".").c_str(), L"Export", MB_OK | MB_ICONWARNING);
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
    sector->isEnabled    = [this] () { return m_session.model.GetSector() != nullptr && m_session.model.GetSector()->dataField >= 0; };
    save->label          = L"Export...";
    save->dispatch       = [this] () { OpenExport(); };

    items.push_back (DxuiPopupMenuItem::ForCommand (copy));

    if (m_session.keyTarget == KeyTarget::SectorData)
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
        { m_topDown,         L"See the platter from above, not tilted",                         L"The platter is not tilted" },
        { m_alignmentCheck,  L"Mark where sector 0 and the longest sync start on each track",   nullptr },
        { m_filesCheck,      L"Color each sector's data field by its role in the file map",     nullptr },
        { m_readsCheck,      L"Mark each quarter track the drive has read since the disk was inserted", nullptr },
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
        { m_diffsCheck,      L"Mark the quarter tracks that differ",                             nullptr },
    };


    RECT          modes = m_modeTabs->GetBounds();
    std::wstring  tip   = m_editing->GetTip (pointPx, outAnchorPx);



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

    if (tip.empty())
    {
        tip = m_files->GetTip (pointPx, outAnchorPx);
    }

    if (tip.empty())
    {
        tip = m_comparisonPart->GetTip (pointPx, outAnchorPx);
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
        at = (order[k].first == m_session.keyTarget) ? k : at;
    }

    for (k = 1; k < count; k++)
    {
        size_t        index = static_cast<size_t> (at + step * k + count * k) % order.size();
        const auto &  next  = (index < order.size()) ? order[index] : order[0];

        if (next.second != nullptr && next.second->IsVisible())
        {
            m_session.keyTarget = next.first;
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



    switch (m_session.keyTarget)
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
                             static_cast<float> (area.bottom - area.top), theme.ForegroundMuted(), m_session.scaler.ToPxf (InspectorView::kTextDip),
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
    return m_session.is35 && (IsInside (m_platterAreaPx, pointPx) || IsInside (m_trackAreaPx, pointPx) || IsInside (m_stripAreaPx, pointPx) ||
                      (!IsImageShown() && IsInside (m_diskContentPx, pointPx)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsImageShown
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsImageShown() const
{
    return m_session.isDiskGroup && m_session.diskTab == kTabImage;
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
    bool              hasDisk = m_session.context.hasDisk;



    if (hasDisk)
    {
        //  The range's room is reserved in Structure mode too, where its
        //  controls are hidden, so a mode switch doesn't move any control.
        items.push_back ({ ToolId::Modes, scaler.ToPx (2 * s_kModeTabDip) });
        items.push_back ({ ToolId::Range, scaler.ToPx (s_kRangeDip) });
    }

    if (m_hostLink->IsHeadShown())
    {
        items.push_back ({ ToolId::Follow,   scaler.ToPx (s_kFollowDip) });
        items.push_back ({ ToolId::GoToHead, scaler.ToPx (s_kGoToHeadDip) });
    }

    if (hasDisk)
    {
        items.push_back ({ ToolId::GoTo, scaler.ToPx (s_kFindButtonDip) });
        items.push_back ({ ToolId::Find, scaler.ToPx (s_kFindButtonDip) });
    }

    if (hasDisk || m_session.comparison.IsComparing())
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
    int   inset  = scaler.ToPx (s_kMarginDip) / 2;
    int   button = scaler.ToPx (s_kButtonDip);
    RECT  r      = item.rect;
    RECT  inner  = { r.left, r.top + inset, r.right, r.bottom - inset };



    switch (item.id)
    {
        case ToolId::Modes:
            m_modeTabs->Layout (inner, scaler);
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



    for (const ToolItem & item : GetToolItems (m_session.scaler))
    {
        sig |= 1u << static_cast<int> (item.id);
    }

    return sig;
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
    params.backgroundArgb   = m_session.theme->BackgroundElevated();
    params.renderContent    = [this] (IDxuiPainter & painter, IDxuiTextRenderer & text)
                              {
                                  m_legend->Layout ({ 0, 0, m_session.scaler.ToPx (PlatterLegendView::kWidthDip), m_session.scaler.ToPx (m_legend->GetHeightDip()) }, m_session.scaler);
                                  m_legend->Paint  (painter, text, *m_session.theme);
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

    inOut.isTimingMode  = m_session.context.isTimingMode;
    inOut.isDiskGroup   = m_session.isDiskGroup;
    inOut.trackTab      = m_session.trackTab;
    inOut.diskTab       = (m_session.diskTab == kTabDifferences) ? kTabTracks : m_session.diskTab;
    inOut.timingRange   = m_session.context.timingRange;
    inOut.isAlignment   = m_alignmentCheck->IsChecked();
    inOut.isFiles       = m_session.context.isFilesOverlay;
    inOut.isReads       = m_session.context.isReadsOverlay;
    inOut.zoom          = m_zoom;
    inOut.tilt          = m_session.model.GetTilt();
    inOut.lastDrive     = m_session.drive;

    for (IInspectorComponent * component : m_components)
    {
        component->GetPrefs (inOut);
    }
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

    m_session.context.isTimingMode   = prefs.isTimingMode;
    m_session.context.isFilesOverlay = prefs.isFiles;
    m_session.context.isReadsOverlay = prefs.isReads;
    m_isFilesDefaulted               = prefs.w > 0;
    m_isFilesSaved                   = prefs.w > 0;
    m_session.trackTab               = std::clamp (prefs.trackTab, 0, kTrackTabCount - 1);
    m_session.diskTab                = std::clamp (prefs.diskTab,  0, kDiskTabCount  - 1);
    m_session.diskTab                = (m_session.diskTab == kTabDifferences) ? kTabTracks : m_session.diskTab;

    //  The timing range step nearest the saved range.
    for (step = 0; step < static_cast<int> (std::size (s_kRanges)); step++)
    {
        m_rangeStep = (std::abs (s_kRanges[step] - prefs.timingRange) < std::abs (s_kRanges[m_rangeStep] - prefs.timingRange)) ? step : m_rangeStep;
    }

    m_session.context.timingRange = s_kRanges[m_rangeStep];

    m_modeTabs->SetSelected  (prefs.isTimingMode ? 1 : 0);
    m_alignmentCheck->SetChecked (prefs.isAlignment);
    m_platterView->SetAlignmentShown (prefs.isAlignment);
    m_filesCheck->SetChecked (prefs.isFiles);
    m_readsCheck->SetChecked (prefs.isReads);
    m_followCheck->SetChecked (prefs.isFollowHead);

    for (IInspectorComponent * component : m_components)
    {
        component->ApplyPrefs (prefs);
    }

    SetZoom (prefs.zoom);
    m_session.model.SetTilt (prefs.tilt);

    if (prefs.isDiskGroup)
    {
        ShowDiskTab (m_session.diskTab);
    }
    else
    {
        ShowTrackTab (m_session.trackTab);
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
    float          rim     = std::min (m_platterAreaPx.right - m_platterAreaPx.left, m_platterAreaPx.bottom - m_platterAreaPx.top) / 2.0f - m_session.scaler.ToPxf (kInsetDip);
    float          line    = m_session.scaler.ToPxf (1.5f);
    bool           isNoCtl = m_session.headState != nullptr && !m_session.headState->hasController;
    bool           isOff   = m_session.host != nullptr && !m_session.host->IsDriveAttached (m_session.drive);
    std::wstring   why     = isNoCtl ? std::wstring (L"This machine has no Disk II controller in slot 6") :
                             isOff   ? std::format (L"Drive {} is not attached", m_session.drive + 1) : std::format (L"No disk in drive {}", m_session.drive + 1);
    double         a       = 0.0;
    double         b       = 0.0;
    int            k       = 0;
    int            dashes  = 0;



    //  The same dash length on both circles.
    for (float radius : { rim, rim * PlatterRenderer::kInnerFraction })
    {
        dashes = std::max (8, static_cast<int> (kTwoPi * radius / (2.0f * m_session.scaler.ToPxf (kDashDip))));

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
                     theme.ForegroundMuted(), m_session.scaler.ToPxf (InspectorView::kTextDip + 1.0f), DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
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
