#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorWindow.h"
#include "Ui/DiskInspector/InspectorPlacement.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Ui/DiskInspector/CompareDialog.h"
#include "Ui/DiskInspector/DecodeSettingsDialog.h"
#include "Ui/DiskInspector/Views/DifferencesView.h"
#include "Ui/DiskInspector/Views/IssuesView.h"
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
#include "Ui/DiskInspector/Components/InspectorCommandSet.h"
#include "Ui/DiskInspector/Components/InspectorWindowLayout.h"
#include "Ui/DiskInspector/Components/ScreenNotes.h"
#include "Ui/DiskInspector/Components/WalkthroughPart.h"
#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/Components/InspectorTooltips.h"
#include "Ui/DiskInspector/Components/SelectionPart.h"
#include "Ui/DiskInspector/Components/ViewTables.h"
#include "Ui/DiskInspector/Components/InspectorMenus.h"
#include "Ui/DiskInspector/Components/InspectorToolbar.h"
#include "Ui/DiskInspector/Components/PlatterCorners.h"
#include "Ui/DiskInspector/Components/StatusBand.h"
#include "Ui/DiskInspector/Components/StripControls.h"
#include "Ui/DiskInspector/Components/TrackColumn.h"
#include "Ui/DiskInspector/EditBar.h"
#include "Ui/DiskInspector/FindBar.h"
#include "Ui/DiskInspector/Views/FluxView.h"
#include "Ui/DiskInspector/GoToDialog.h"
#include "Ui/DiskInspector/Views/NibblesView.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/Views/SectorsView.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"
#include "Ui/DiskInspector/TrackStripView.h"
#include "Ui/DiskInspector/ZoomStops.h"





static constexpr LPCWSTR  s_kpszWindowTitle = L"Disk inspector";
static constexpr LPCWSTR  s_kpszClassName   = L"CassoDiskInspector";

//  A command added to the system menu: below SC_SIZE, low four bits clear.
static constexpr UINT     s_kDefaultSizeCommand = 0x0010;

static constexpr LPCWSTR  s_kpszHint        = L"Scroll to zoom, drag to pan, double-click to fit";
static constexpr LPCWSTR  s_kpszStripHint   = L"Scroll to zoom, drag to select, Shift+drag to pan, double-click to select a field";
static constexpr LPCWSTR  s_kpszNoDisk      = L"No disk";
static constexpr LPCWSTR  s_kpszAnalyzing   = L"Analyzing";

static constexpr int      s_kToolbarDip     = 40;
static constexpr int      s_kHintRowDip     = 20;
static constexpr int      s_kTabWidthDip    = 96;
static constexpr int      s_kFileNameDip    = 260;
static constexpr float    s_kChipPadDip     = 8.0f;
static constexpr float    s_kChipGapDip     = 6.0f;
static constexpr float    s_kChipHeightDip  = 22.0f;
static constexpr float    s_kChipTextDip    = 12.0f;
static constexpr float    s_kAnalyzingDip   = 90.0f;
static constexpr int      s_kStatusDip      = 28;
static constexpr double   s_kCharWidthDip   = 6.6;
static constexpr int      s_kMinTableDip    = 200;
static constexpr int      s_kDecodeButtonDip = 132;
static constexpr int      s_kExportButtonDip = 80;
static constexpr int      s_kCopySectorDip   = 100;
static constexpr int      s_kEditSectorDip   = 72;
static constexpr int      s_kFindButtonDip   = 60;
static constexpr int      s_kCompareDip      = 122;
static constexpr int      s_kFollowDip       = 110;
static constexpr int      s_kGoToHeadDip     = 94;
static constexpr int      s_kMinFileNameDip  = 200;
static constexpr int      s_kFocusGapDip    = 3;
static constexpr int      s_kModeTabDip     = 76;
static constexpr int      s_kRangeDip       = 110;





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

HRESULT DiskInspectorWindow::Create (HINSTANCE hInstance, HWND hwndOwner, const DxuiTheme * theme, IDiskInspectorHost * host, bool activate)
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
    //  Not owned, so the main window can cover it. Windows chooses where it
    //  opens, and it is sized to fit the main window's work area.
    params.ownerHwnd                = nullptr;
    params.placementAnchorHwnd      = hwndOwner;
    params.initialSizeDip           = { kOpeningWidthDip, kOpeningHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::Standard;
    params.classNameOverride        = s_kpszClassName;
    params.createNoActivate         = !activate;
    params.placement                = DxuiWindowPlacement::Default;
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
    m_zoomNote.SetTheme (*m_session.theme);
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
//  DiskInspectorWindow::OpenWalkthrough
//
//  The walkthrough from its first step (FR-153).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenWalkthrough()
{
    m_walkthrough->Open();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenRepair
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenRepair (const std::set<std::wstring> & chosen)
{
    m_dialogs->OpenRepair (chosen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowFinding
//
//  The Issues tab, with the finding selected as a click selects it, and the
//  keys on the list.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowFinding (int finding)
{
    ShowDiskTab (kTabFindings);

    m_session.keyTarget = KeyTarget::Findings;
    m_session.views.issues->ShowFinding (finding);
    Invalidate();
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
    m_statusBand->SetHint (note);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ReloadDisk
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ReloadDisk()
{
    if (m_isFileGone)
    {
        m_isFileGone = false;
        SetStatusNote (std::wstring());
    }

    m_hostLink->RequestReread();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ChangeTheme
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ChangeTheme (const DxuiTheme * theme)
{
    m_session.theme = theme;
    SetTheme (theme);
    m_tooltip.SetTheme (*theme);
    m_zoomNote.SetTheme (*theme);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::NoteFileGone
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::NoteFileGone()
{
    m_isFileGone = true;
    SetStatusNote (L"The file is no longer there.");
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
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::NotifyDiskStarted (const DiskChange & change)
{
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

bool DiskInspectorWindow::ConfirmDiscardEdits (const std::wstring & action, std::function<void()> afterSave)
{
    return m_editing->ConfirmDiscardEdits (action, std::move (afterSave));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RenderFrame
//
//  Once per host frame: replies from the host, then analysis results, then
//  a repaint. The palette is resolved each frame, so a theme change shows
//  without reopening (FR-077). A hidden or minimized window isn't drawn.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::RenderFrame()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!IsCreated() || !IsWindowVisible (GetHwnd()) || IsIconic (GetHwnd()), S_OK);

    m_session.context.palette = DiskInspectorPalette::Resolve (*m_session.theme);
    m_tooltip.SetTheme (*m_session.theme);
    m_zoomNote.SetTheme (*m_session.theme);

    m_hostLink->TakeReplies();
    m_hostLink->FollowHostDisk();
    m_hostLink->TakeResults();

    if (std::function<void()> action = m_editing->TakeDueAction(); action != nullptr)
    {
        action();
    }

    m_comparisonPart->TakeComparison();
    m_comparisonPart->SyncSideB();
    m_hostLink->TickHead();
    m_hostLink->UpdateRings();
    UpdateControls();
    m_tables->RefreshTables();

    //  While the File map tab is up, the selected file's sectors are marked
    //  in the platter and the strip as in the grid (FR-090).
    m_session.context.isFileHighlighted = m_session.isDiskGroup && m_session.diskTab == kTabFileMap && m_session.context.selectedFile >= 0;

    //  The picker's width matches its label, measured as it is drawn, and the
    //  sector row's height its row count for the track selected.
    m_session.isLayoutStale = m_session.isLayoutStale || m_trackColumn->IsLayoutStale();

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
    m_zoomNote.Tick    (static_cast<int64_t> (GetTickCount64()));

    //  The window's own context menu opens with an animation each frame
    //  advances, as the main window's does.
    if (GetPopupHost() != nullptr && GetPopupHost()->GetContextMenu().WantsTick())
    {
        GetPopupHost()->GetContextMenu().Tick (static_cast<int64_t> (GetTickCount64()));
    }

    m_session.views.tracks->Tick   (static_cast<int64_t> (GetTickCount64()));
    m_session.views.issues->Tick (static_cast<int64_t> (GetTickCount64()));

    for (IInspectorComponent * component : m_components)
    {
        component->Tick (static_cast<int64_t> (GetTickCount64()));
    }

    Invalidate();


    //  Repair reads the image file, so the copy is enough without its
    //  analysis. The flag is cleared first, as the dialog is modal and
    //  frames go on under it.
    if (m_isRepairWanted && m_session.analysis.copy != nullptr)
    {
        m_isRepairWanted = false;
        OpenRepair ({});
    }

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
    m_commandSet = std::make_unique<InspectorCommandSet> (m_session, GetHost(), static_cast<IInspectorFrame &> (*this), static_cast<IInspectorParts &> (*this), m_commands);
    m_commandSet->Build();

    m_hostLink    = std::make_unique<HostLink> (m_session, GetHost());
    m_toolbar     = std::make_unique<InspectorToolbar> (m_session, GetHost(), m_commands);
    m_toolbar->Create();
    m_session.views.platter = CreateChild<PlatterView> (m_session.context);
    m_trackColumn = std::make_unique<TrackColumn> (m_session, GetHost());
    m_trackColumn->Create();
    m_session.views.header  = &m_trackColumn->GetHeader();
    m_session.views.sectorRow   = &m_trackColumn->GetSectorRow();
    m_session.views.strip   = CreateChild<TrackStripView>  (m_session.context);
    m_session.views.stripB      = CreateChild<TrackStripView>  (m_session.contextB);
    m_stripControls = std::make_unique<StripControls> (m_session, GetHost(), *m_session.views.strip);
    m_stripControls->Create();
    m_session.views.sectors    = CreateChild<SectorsView>  (m_session.context);
    m_session.views.nibbles  = CreateChild<NibblesView>      (m_session.context);
    m_session.views.flux     = CreateChild<FluxView>   (m_session.context);
    m_copySector   = CreateChild<DxuiButton> (L"Copy sector");
    m_session.views.tracks   = CreateChild<InspectorTableView> (m_session.context);
    m_session.views.issues = CreateChild<IssuesView>     (m_session.context);
    m_session.views.image    = CreateChild<InspectorTableView> (m_session.context);
    m_files       = std::make_unique<FilesView> (m_session, GetHost());
    m_files->Create();
    m_session.views.files = m_files.get();
    m_session.views.diffs    = CreateChild<DifferencesView> (m_session.context, m_session.comparison);
    m_comparisonPart = std::make_unique<ComparisonPart> (m_session, GetHost(), *m_session.views.diffs, *m_session.views.stripB, *m_files);
    m_comparisonPart->Create();
    m_findPart    = std::make_unique<FindPart> (m_session, GetHost());
    m_findPart->Create();
    m_session.views.findResults = &m_findPart->GetResults();
    m_editing     = std::make_unique<EditingPart> (m_session, GetHost(), *m_session.views.sectors);
    m_editing->Create();
    m_notes       = std::make_unique<ScreenNotes> (m_session);
    m_walkthrough = std::make_unique<WalkthroughPart> (m_session);
    m_dialogs     = std::make_unique<InspectorDialogs> (m_session, GetHost());
    m_tables      = std::make_unique<ViewTables> (m_session, GetHost());
    m_selection   = std::make_unique<SelectionPart> (m_session, GetHost(), *m_dialogs, *m_findPart, *m_stripControls);

    m_session.views.tracks->SetColumns  (InspectorTables::GetTrackColumns());
    m_session.views.image->SetColumns   (InspectorTables::GetImageColumns());
    m_session.views.tracks->SetOnSelect   ([this] (const TableRow & row) { m_tables->SelectFromRow (row); });
    m_session.views.issues->SetOnSelect ([this] (const TableRow & row) { m_tables->SelectFromRow (row); });
    m_session.views.issues->SetOnRepair ([this] (const std::set<std::wstring> & chosen) { OpenRepair (chosen); });
    m_session.views.image->SetGrouped   (true);
    m_session.views.image->SetWrapsText (true);
    m_session.views.image->SetOnSelect  ([this] (const TableRow & row) { m_tables->SelectFromRow (row); });
    m_session.context.edits = &m_session.edits;

    //  The sector row and the sector's bytes are each one stop for Tab; the
    //  arrows then step through the sectors or the bytes.
    m_session.views.sectorRow->SetFocusable (true);
    m_session.views.sectors->SetFocusable   (true);
    m_session.views.sectorRow->SetOnFocus   ([this] () { m_session.keyTarget = KeyTarget::SectorRow; });
    m_session.views.sectors->SetOnFocus     ([this] () { m_session.keyTarget = KeyTarget::SectorData; });
    m_copySector->SetOnClick ([this] () { m_selection->CopySector(); });
    m_session.views.stripB->SetSide (ComparisonSession::kSideB);

    m_corners     = std::make_unique<PlatterCorners> (m_session, GetHost(), *m_session.views.platter);
    m_corners->Create();
    m_menus       = std::make_unique<InspectorMenus> (m_session, GetHost(), m_commands);
    m_menus->Create();

    //  The status band's zoom is the last control, so its slider, which opens
    //  over the views, is drawn on top and has the pointer there.
    m_statusBand  = std::make_unique<StatusBand> (m_session, GetHost());
    m_statusBand->Create();

    m_components = { m_hostLink.get(), m_menus.get(), m_toolbar.get(), m_corners.get(), m_trackColumn.get(), m_stripControls.get(), m_dialogs.get(), m_files.get(), m_editing.get(), m_comparisonPart.get(), m_findPart.get(), m_statusBand.get() };

    m_tooltips = std::make_unique<InspectorTooltips> (m_session, m_tooltip, InspectorTooltips::Sources { m_corners.get(), m_files.get(), m_copySector, &m_components });

    m_tooltip.SetPopupHost  (GetPopupHost());
    m_zoomNote.SetPopupHost (GetPopupHost());

    //  The find bar's layer, as its fill has to cover the views' text, which
    //  is drawn after every fill in the window's pass.
    GetPopupHost()->SetOverlayHooks ([this] () { return m_session.isFindOpen; },
                                     [this] (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) { m_findPart->PaintBar (painter, text, theme); });

    m_session.diskTab = kTabTracks;
    ShowTrackTab (kTabSectorData);

    //  Tab and Shift+Tab walk the window's controls (FR-059).
    GetFocusManager().SetTheme (GetTheme());
    GetFocusManager().Attach   (this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateTitle
//
//  "Disk inspector - " and the file's path, with a lock before it while the
//  disk is write-protected (FR-007, FR-065), a red dot after the path while
//  edits are pending, and the machine and drive while the disk is mounted.
//  The caption has a "Read as CP/M" chip after the title while the disk is
//  read as CP/M by the user's choice.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateTitle()
{
    std::wstring  title  = s_kpszWindowTitle;
    size_t        markAt = std::wstring::npos;
    std::wstring  path   = (m_session.analysis.copy != nullptr) ? TextEncoding::Utf8ToWide (m_session.analysis.copy->fileName) : std::wstring();
    bool          isCpm  = m_session.context.hasDisk && m_session.analysis.isCpmChosen;



    if (m_session.context.hasDisk)
    {
        title = InspectorText::FormatWindowTitle (path.empty() ? m_session.GetFileName() : path, m_session.IsWriteProtected(), !m_session.edits.IsEmpty(),
                                                  m_session.host->GetMountDescription(), markAt);
    }

    if (title != m_title || markAt != m_titleMark)
    {
        m_title     = title;
        m_titleMark = markAt;
        SetTitle (title, markAt);
    }

    if (isCpm != m_isTitleCpm)
    {
        m_isTitleCpm = isCpm;
        SetTitleChips (isCpm ? std::vector<DxuiTitleChip> { InspectorText::MakeReadAsCpmTitleChip() } : std::vector<DxuiTitleChip>());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnSysCommand
//
//  On "Default size", the zoom returns to 100% and the window to the size it
//  opens at, about its center on the monitor it is on.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DiskInspectorWindow::OnSysCommand (UINT command)
{
    HRESULT            hr     = S_OK;
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    BAIL_OUT_IF (command != s_kDefaultSizeCommand || GetPopupHost() == nullptr, S_OK);

    result = DxuiMessageResult::Handled;
    SetZoom (1.0);
    hr     = GetPopupHost()->RestoreInitialPlacement();
    CHR (hr);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RestoreDefaultSize
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RestoreDefaultSize()
{
    (void) OnSysCommand (s_kDefaultSizeCommand);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnCopyData
//
//  A WM_COPYDATA for the host, such as Casso's reply to an edit Casso
//  Explorer saved (FR-105).
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DiskInspectorWindow::OnCopyData (WPARAM sender, LPARAM data)
{
    const COPYDATASTRUCT  * copy = reinterpret_cast<const COPYDATASTRUCT *> (data);



    (void) sender;

    return (copy != nullptr && m_session.host->HandleCopyData (*copy)) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnWindowClose
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnWindowClose()
{
    //  The window stays open when the user cancels, and after Save until
    //  the save succeeds (FR-108).
    if (ConfirmDiscardEdits (L"Closing the disk inspector", [hwnd = GetHwnd()] () { PostMessageW (hwnd, WM_CLOSE, 0, 0); }))
    {
        m_tooltip.HideImmediate();
        m_zoomNote.HideImmediate();
        m_corners->HideLegend();
        m_session.host->SetHeadStateWanted (false);
        Hide();
        m_session.host->OnInspectorClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Layout
//
//  The menu bar along the top and the status band along the bottom at the
//  window's scale, and the rest at the zoom: every size and font multiplied
//  by it (FR-154).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    DxuiDpiScaler  zoomed;
    int            menus   = m_menus->GetHeightPx (scaler);
    int            status  = m_statusBand->GetHeightPx (scaler);
    RECT           content = { boundsDip.left, boundsDip.top + menus, boundsDip.right, boundsDip.bottom - status };



    m_session.hostScaler = scaler;
    zoomed.SetDpi (static_cast<UINT> (std::lround (scaler.GetDpi() * m_zoom)));

    SetBounds (boundsDip);
    m_menus->Layout      ({ boundsDip.left, boundsDip.top, boundsDip.right, content.top }, scaler);
    m_statusBand->Layout ({ boundsDip.left, content.bottom, boundsDip.right, boundsDip.bottom }, scaler);
    LayoutContent (content, zoomed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::LayoutContent
//
//  The frames from InspectorWindowLayout, then the track column's header,
//  sector row, view selector and the chosen view's content, with the edit
//  bar above the Sector data tab.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::LayoutContent (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    InspectorWindowLayout::Inputs  in;
    InspectorWindowLayout::Frames  f;
    int                            margin = scaler.ToPx (InspectorWindowLayout::kMarginDip);
    int                            width  = boundsDip.right - boundsDip.left;
    int                            y      = 0;
    RECT                           right  = {};



    in.bounds        = boundsDip;
    in.splitFraction = m_splitFraction;
    in.toolbarPx     = m_toolbar->GetHeightPx (scaler);
    in.isComparing   = m_session.comparison.IsComparing();
    in.minColumnPx   = m_corners->GetMinWidthPx (scaler);
    in.minTrackPx    = m_trackColumn->GetMinWidthPx (scaler);
    in.cornerBandPx  = m_corners->GetBottomBandPx (scaler);
    in.noticePx      = m_comparisonPart->GetNoticeHeightPx (width - 2 * margin, scaler);
    f                = InspectorWindowLayout::Plan (in, scaler);
    right            = f.trackColumn;

    m_session.scaler = scaler;
    m_tooltip.SetDpi (scaler.GetDpi());
    m_tooltip.SetViewportSize (width, boundsDip.bottom - boundsDip.top);
    m_zoomNote.SetDpi (scaler.GetDpi());
    m_zoomNote.SetViewportSize (width, boundsDip.bottom - boundsDip.top);
    m_session.context.scenePx = boundsDip;

    //  The walkthrough's toolbar step covers the menu bar too.
    m_session.layout.toolbar = { boundsDip.left, boundsDip.top - m_menus->GetHeightPx (m_session.hostScaler), boundsDip.right, f.toolbar.bottom };
    m_toolbar->Layout  (f.toolbar, scaler);
    m_findPart->Layout (f.findArea, scaler);
    m_splitterPx = f.splitter;

    m_comparisonPart->Layout        (f.compareBar, f.notice, scaler);
    m_session.views.platter->Layout (f.platter, scaler);

    m_session.layout.platterArea   = f.platterArea;
    m_session.layout.platterColumn = f.platterColumn;
    m_corners->Layout (m_session.layout.platterColumn, scaler);

    //  The track column.
    y = m_trackColumn->Layout (right, scaler);
    m_session.layout.trackArea = { f.trackArea.left, f.trackArea.top, f.trackArea.right, m_trackColumn->GetBoundsPx().bottom };

    m_session.layout.diskContent = { right.left, y, right.right, right.bottom };

    for (IDxuiControl * content : std::initializer_list<IDxuiControl *> { m_session.views.nibbles, m_session.views.flux, m_session.views.tracks, m_session.views.issues, m_session.views.image, m_session.views.diffs, m_session.views.findResults })
    {
        content->Layout (m_session.layout.diskContent, scaler);
    }

    //  The edit bar is placed by the Sector data tab, under its header.
    m_session.views.sectors->Layout ({ right.left, y, right.right, right.bottom }, scaler);

    m_files->Layout (m_session.layout.diskContent, scaler);

    //  "Edit" and the "Copy sector" icon at the end of the Sectors header's
    //  first line, as block mode has Edit and Copy block in its title row, with
    //  room reserved for them there.
    m_copySector->Layout       ({ right.right - scaler.ToPx (s_kCopySectorDip), y, right.right, y + scaler.ToPx (SectorsView::kHeaderDip) + margin / 2 }, scaler);
    m_editing->LayoutEditButton ({ right.right - scaler.ToPx (s_kCopySectorDip) - margin / 2 - scaler.ToPx (s_kEditSectorDip), y,
                                   right.right - scaler.ToPx (s_kCopySectorDip) - margin / 2, y + scaler.ToPx (SectorsView::kHeaderDip) + margin / 2 }, scaler);
    m_session.views.sectors->SetHeaderReserve (scaler.ToPx (s_kCopySectorDip + s_kEditSectorDip) + margin * 3 / 2);

    m_session.views.strip->Layout  (f.strip, scaler);
    m_session.views.stripB->Layout (f.stripB, scaler);

    //  One shimmer crosses both strips of a comparison, a single light.
    LONG  lightEnd = (f.stripB.bottom > f.stripB.top) ? f.stripB.bottom : f.strip.top;

    m_session.views.strip->SetLightRows  (f.strip.top, lightEnd);
    m_session.views.stripB->SetLightRows (f.strip.top, lightEnd);
    m_stripControls->Layout        (f.stripControls, scaler);

    m_session.layout.stripArea = f.stripArea;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  focus = {};



    SyncEditFocus();

    painter.FillRect (static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top),
                      static_cast<float> (m_boundsDip.right - m_boundsDip.left), static_cast<float> (m_boundsDip.bottom - m_boundsDip.top),
                      theme.Background());

    painter.FillRect (static_cast<float> (m_splitterPx.left), static_cast<float> (m_splitterPx.top),
                      static_cast<float> (m_splitterPx.right - m_splitterPx.left), static_cast<float> (m_splitterPx.bottom - m_splitterPx.top),
                      theme.Divider());

    for (IInspectorComponent * component : m_components)
    {
        component->PaintUnder (painter, text, theme);
    }

    DxuiWindow::Paint (painter, text, theme);

    for (IInspectorComponent * component : m_components)
    {
        component->PaintOver (painter, text, theme);
    }

    if (m_session.is35)
    {
        m_notes->PaintNot35 (painter, text, theme);
    }

    //  A file that could not be opened says why where the platter would be.
    if (!m_session.context.hasDisk && !m_session.openError.empty())
    {
        text.DrawString (m_session.openError.c_str(), static_cast<float> (m_session.layout.platterArea.left), static_cast<float> (m_session.layout.platterArea.top),
                         static_cast<float> (m_session.layout.platterArea.right - m_session.layout.platterArea.left), static_cast<float> (m_session.layout.platterArea.bottom - m_session.layout.platterArea.top),
                         theme.ErrorForeground(), m_session.scaler.ToPxf (InspectorView::kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                         DxuiTextVAlign::Center, DxuiFontWeight::Normal, true);
    }

    if (!m_session.context.hasDisk && m_session.openError.empty() && m_session.pendingRequest == 0)
    {
        m_notes->PaintNoDisk (painter, text, theme);
    }

    //  The view the keys act on, outlined once a key has been used, unless a
    //  control or the find bar has the focus (FR-059).
    if (m_isFocusShown && m_session.keyTarget != KeyTarget::FindBar && m_session.context.hasDisk && !m_session.is35 && IsViewFocused())
    {
        focus = m_selection->GetKeyTargetBounds();
        InflateRect (&focus, m_session.scaler.ToPx (s_kFocusGapDip), m_session.scaler.ToPx (s_kFocusGapDip));
        painter.OutlineRect (static_cast<float> (focus.left), static_cast<float> (focus.top), static_cast<float> (focus.right - focus.left),
                             static_cast<float> (focus.bottom - focus.top), m_session.scaler.ToPxf (2.0f), theme.FocusRing());
    }

    m_walkthrough->Paint (painter, text, theme, m_boundsDip);
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
    if (m_walkthrough->OnWalkthroughMouse (ev))
    {
        return true;
    }

    if (m_selection->OnHistoryButton (ev))
    {
        return true;
    }

    //  The find bar floats over the views, so it has the pointer first, unless
    //  a drag that started under it is passing over.
    if (!m_isSplitting && !m_session.views.platter->IsDragging() && m_findPart->OnBarMouse (ev))
    {
        m_isFocusShown            = m_isFocusShown && ev.kind != DxuiMouseEventKind::Down;
        m_session.context.hoverPx  = POINT { SHRT_MIN, SHRT_MIN };
        m_session.contextB.hoverPx = m_session.context.hoverPx;
        m_tooltip.RequestHide (static_cast<int64_t> (GetTickCount64()));
        m_tooltips->ClearText();

        return true;
    }

    POINT  p           = ev.positionDip;
    int    width       = m_boundsDip.right - m_boundsDip.left;
    bool   isCovered   = m_notes->IsCovered (p) && ev.kind != DxuiMouseEventKind::Leave;
    bool   isHandled   = isCovered;
    bool   isComponent = false;



    //  The wheel zooms the File map grid over it, as it does the platter
    //  and the strip, with Shift and Ctrl left to pan it (FR-157); Ctrl and the
    //  wheel elsewhere, or the wheel over the zoom control, zoom the window (FR-154).
    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.shift && !ev.ctrl && !ev.wheelHorizontal && m_files->IsOverGrid (p))
    {
        m_files->ZoomGrid (ZoomStops::GetWheelSteps (ev.wheelDelta), p);
        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && ((ev.ctrl && !m_files->IsOverGrid (p)) || m_statusBand->IsOverZoom (p)))
    {
        SetZoom (DxuiZoomControl::StepZoom (m_zoom, (ev.wheelDelta > 0) ? 1 : -1));
        isHandled = true;
    }

    //  Over a 3.5" disk's notes, the views under them take nothing.
    if (!isCovered && ev.kind == DxuiMouseEventKind::Down)
    {
        m_session.keyTarget = m_selection->GetKeyTarget (p);
        m_isFocusShown      = false;
        GetFocusManager().SetFocused (nullptr);
        m_findPart->OnPress();
    }

    if (!isCovered && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right && m_session.context.hasDisk &&
        (m_session.keyTarget == KeyTarget::SectorData || m_session.keyTarget == KeyTarget::Nibbles || m_session.keyTarget == KeyTarget::Tracks))
    {
        m_selection->ShowContextMenu (p);
        isHandled = true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && PtInRect (&m_splitterPx, p))
    {
        m_isSplitting = true;
        isHandled     = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Move && m_isSplitting && width > 0)
    {
        m_splitFraction = std::clamp (static_cast<double> (p.x - m_boundsDip.left) / width, 0.0, 1.0);
        Layout (m_boundsDip, m_session.hostScaler);

        //  Past a column's least width, the splitter stops; the fraction is
        //  where it stopped, so a drag back moves it at once.
        m_splitFraction = static_cast<double> (m_splitterPx.left - m_boundsDip.left) / width;
        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && m_isSplitting)
    {
        m_isSplitting = false;
        isHandled     = true;
    }

    //  Every event is passed to every component, so each component's hover
    //  follows the pointer; a press or release that a component takes is not
    //  passed on to the controls.
    for (IInspectorComponent * component : m_components)
    {
        isComponent = component->OnMouse (ev) || isComponent;
    }

    if (!isHandled && !(isComponent && ev.kind != DxuiMouseEventKind::Move))
    {
        isHandled = DxuiWindow::OnMouse (ev);
    }

    //  A right drag on the platter follows the pointer outside the window.
    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right && m_session.views.platter->IsDragging())
    {
        SetCapture (GetHwnd());
    }

    isHandled = isHandled || isComponent;

    //  The views draw their hover from where the pointer is (FR-148), and
    //  the platter draws none under its corner groups (FR-163).
    m_session.context.hoverPx = (ev.kind == DxuiMouseEventKind::Leave || m_corners->Contains (p)) ? POINT { SHRT_MIN, SHRT_MIN }
                              : (ev.kind == DxuiMouseEventKind::Move ? p : m_session.context.hoverPx);
    m_session.contextB.hoverPx = m_session.context.hoverPx;

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        m_tooltips->Update (p);
        m_statusBand->SetHint (!m_session.context.hasDisk ? m_session.statusNote : (IsInside (m_session.layout.platterArea, p) && !m_corners->Contains (p)) ? s_kpszHint
                               : IsInside (m_session.layout.stripArea, p) ? s_kpszStripHint : m_session.statusNote);
    }
    else if (ev.kind == DxuiMouseEventKind::Down || (ev.kind == DxuiMouseEventKind::Wheel && !ev.ctrl && !m_statusBand->IsOverZoom (p)) || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_tooltip.RequestHide (static_cast<int64_t> (GetTickCount64()));
        m_tooltips->ClearText();
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SyncEditFocus
//
//  Starting to edit moves the keys from any control to the sector's bytes,
//  and the bytes are drawn focused only while the keys go to them.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SyncEditFocus()
{
    bool            isEditing = m_session.views.sectors->IsEditing();
    IDxuiControl *  focused   = GetFocusManager().GetFocusedControl();



    if (isEditing && !m_wasEditing)
    {
        GetFocusManager().SetFocused (m_session.views.sectors);
        m_session.keyTarget = KeyTarget::SectorData;
    }
    else if ((focused == m_session.views.sectorRow && m_session.keyTarget != KeyTarget::SectorRow) ||
             (focused == m_session.views.sectors && m_session.keyTarget != KeyTarget::SectorData))
    {
        //  F6 or a click moved the keys to another view.
        GetFocusManager().SetFocused (nullptr);
    }

    m_wasEditing = isEditing;
    m_session.views.sectors->SetHasKeys   (m_session.keyTarget == KeyTarget::SectorData && IsViewFocused());
    m_session.views.sectorRow->SetHasKeys (m_session.keyTarget == KeyTarget::SectorRow && IsViewFocused() && m_isFocusShown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsViewFocused
//
//  Whether the keys go to the views: no control has the focus, or a view
//  that is a stop for Tab has it.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsViewFocused() const
{
    const IDxuiControl *  focused = const_cast<DiskInspectorWindow *> (this)->GetFocusManager().GetFocusedControl();



    return focused == nullptr || focused == m_session.views.sectorRow || focused == m_session.views.sectors;
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
    bool  isHandled = false;
    bool  wasFind   = m_session.keyTarget == KeyTarget::FindBar;



    if (m_walkthrough->OnWalkthroughKey (ev))
    {
        return true;
    }

    //  The menus take Alt, F10 and every key while one is open (FR-158);
    //  keys go to the find bar while it has the focus, all but the window's
    //  zoom and its Ctrl+F and Ctrl+G.
    if (m_menus->OnKey (ev) || m_findPart->OnKey (ev))
    {
        isHandled = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && !ev.ctrl && !ev.alt && !ev.shift && m_session.views.sectors->IsEditing())
    {
        //  Escape stops editing, wherever the focus is.
        GetFocusManager().SetFocused (nullptr);
        m_editing->ToggleEditing();
        isHandled = true;
    }
    else if (IsViewFocused() && m_editing->OnKey (ev))
    {
        //  While editing, the Sector data tab has the keys unless a control
        //  has the focus: typing, Ctrl+Z, Ctrl+Y, Ctrl+V, and Tab from the
        //  hex column to the text column (FR-097).
        isHandled = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_TAB && !ev.ctrl && !ev.alt)
    {
        //  Tab and Shift+Tab move the focus from control to control, in the
        //  order the controls are laid out now (FR-059).
        RefreshFocusOrder (nullptr);
        isHandled = GetFocusManager().HandleKey (ev.shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
    }
    else if (ev.kind == DxuiKeyEventKind::Down && !IsViewFocused() && (ev.vk == VK_ESCAPE || ev.vk == VK_F6))
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
    else
    {
        isHandled = m_commandSet->RunKey (ev);
    }

    m_isFocusShown = (m_isFocusShown || isHandled) && !wasFind && m_session.keyTarget != KeyTarget::FindBar;

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
    else if (m_session.views.platter != nullptr && (m_session.views.platter->IsDragging() || (IsInside (m_session.views.platter->GetBounds(), clientPx) && !m_corners->Contains (clientPx) && !m_session.model.IsAtFit())))
    {
        cursor = IDC_SIZEALL;
    }
    else if (m_statusBand->IsOverLink (clientPx))
    {
        cursor = IDC_HAND;
    }
    else if (DxuiWindow::GetCursorForPoint (clientPx) != nullptr)
    {
        //  A child's cursor: the hand over a link, a column's resize arrows.
        cursor = DxuiWindow::GetCursorForPoint (clientPx);
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateControls
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateControls()
{
    if (m_corners != nullptr)
    {
        UpdateTitle();
        m_copySector->SetVisible   (m_session.context.hasDisk && !m_session.isDiskGroup && m_session.trackTab == kTabSectorData);
        m_editing->SetShown        (m_session.context.hasDisk && !m_session.isDiskGroup && m_session.trackTab == kTabSectorData);
        m_copySector->SetEnabled   (m_session.model.GetSector() != nullptr && m_session.model.GetSector()->dataField >= 0);
        m_files->SyncSelection();

        for (IInspectorComponent * component : m_components)
        {
            component->Update();
        }
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
    int                     field  = -1;



    //  A selection outside any sector, as in a data field with no address
    //  field, is flown to by the field it starts in.
    for (size_t i = 0; track != nullptr && sector == nullptr && nibble >= 0 && field < 0 && i < track->fields.size(); i++)
    {
        field = (nibble >= track->fields[i].firstNibble && nibble < track->fields[i].firstNibble + track->fields[i].nibbleCount) ? static_cast<int> (i) : -1;
    }

    if (nibble < 0 && track != nullptr && sector != nullptr)
    {
        nibble = track->fields[sector->addressField].firstNibble;
    }

    m_session.views.nibbles->ScrollTo (nibble);
    m_tables->SyncTables();

    //  A new sector the user chooses anywhere but on the platter starts a
    //  flight to it. One chosen on the platter is in view there, and one
    //  chosen by Follow head starts none.
    if (m_session.context.selectionSource == m_session.views.platter)
    {
        m_session.model.StopFlight();
    }
    else if (!m_hostLink->IsHeadSelecting() && sector != nullptr && (m_session.model.GetQuarterTrack() != m_flownQuarterTrack || m_session.model.GetSectorIndex() != m_flownSector))
    {
        m_session.model.StartFlight (InspectorMarquee::GetClockMs());
    }
    else if (!m_hostLink->IsHeadSelecting() && field >= 0 && (m_session.model.GetQuarterTrack() != m_flownQuarterTrack || field != m_flownField))
    {
        m_session.model.StartFlight (InspectorMarquee::GetClockMs());
    }

    m_selection->RecordSelection (m_hostLink->IsHeadSelecting());
    m_flownQuarterTrack = m_session.model.GetQuarterTrack();
    m_flownSector       = m_session.model.GetSectorIndex();
    m_flownField        = field;

    //  A selection made by hand ends Follow head (FR-067).
    if (!m_hostLink->IsHeadSelecting() && m_hostLink->IsFollowingHead())
    {
        m_hostLink->OnHandSelection();
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

    if (tab == kTabSectorData)
    {
        m_session.views.sectors->ScrollToSelected();
    }

    ShowTabContent();
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



    m_session.views.sectors->SetVisible    (isTrack && m_session.trackTab == kTabSectorData);
    m_session.views.nibbles->SetVisible  (isTrack && m_session.trackTab == kTabNibbles);
    m_session.views.flux->SetVisible     (isTrack && m_session.trackTab == kTabFluxTiming);
    m_session.views.tracks->SetVisible   (!isTrack && m_session.diskTab == kTabTracks);
    m_session.views.issues->SetVisible (!isTrack && m_session.diskTab == kTabFindings);
    m_session.views.image->SetVisible    (!isTrack && m_session.diskTab == kTabImage);
    m_session.views.diffs->SetVisible    (!isTrack && m_session.diskTab == kTabDifferences);
    m_session.views.findResults->SetVisible (!isTrack && m_session.diskTab == kTabFindResults);
    m_files->Show (!isTrack && m_session.diskTab == kTabFileMap);
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
    return m_dialogs->Confirm (title, lines, action);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::GetPrefs (DiskInspectorPrefs & inOut) const
{
    InspectorPlacement::Get (GetHwnd(), inOut);

    inOut.zoom         = m_zoom;
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
//  The saved position only with isPositioned, for the first inspector
//  window open (InspectorPlacement).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ApplyPrefs (const DiskInspectorPrefs & prefs, bool isPositioned)
{
    InspectorPlacement::Apply (GetHwnd(), prefs, isPositioned);

    for (IInspectorComponent * component : m_components)
    {
        component->ApplyPrefs (prefs);
    }

    SetZoom (prefs.zoom);
    m_session.model.SetTilt (prefs.tilt);
}




