#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/Source/SourcePathList.h"
#include "Config/WindowPlacementProfile.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/FlagsDialog.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/SymbolDescriptions.h"
#include "Cassque/CassquePromptDialog.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/Panes/TracePaneController.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Ui/Debugger/Panes/BasicProgramController.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/ConsolePanes.h"
#include "Ui/Debugger/ViewMenuRows.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/FindWidgets.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/Panes/HeatMapPane.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/MemoryWindows.h"
#include "Ui/Debugger/Panes/OperandTipController.h"
#include "Ui/Debugger/Panes/UndoListPanes.h"
#include "Ui/Debugger/Panes/WatchesPane.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Ui/Debugger/HeatMapRangeBarCommands.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"
#include "Ui/Debugger/UndoBarCommands.h"





static constexpr LPCWSTR  s_kpszWindowTitle = L"Debugger";
static constexpr LPCWSTR  s_kpszClassName   = L"CassoDebuggerWindow";





////////////////////////////////////////////////////////////////////////////////
//
//  Widen
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring Widen (const std::string & text)
{
    return TextEncoding::NarrowToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::DebuggerWindow
//
//  Here, where the panes and bars it owns are complete; the header only
//  declares most of them.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerWindow::DebuggerWindow() :
    m_operandTips (std::make_unique<OperandTipController> (static_cast<IOperandTipHost &> (*this), static_cast<const ICodeViews &> (*this))),
    m_tips        (std::make_unique<TooltipCoordinator> (static_cast<ITooltipSources &> (*this))),
    m_gutter      (std::make_unique<DisassemblyGutter> (static_cast<IDebuggerPaneHost &> (*this), static_cast<const ICodeViews &> (*this),
                                                        [this] (int view) { ApplyCodeView (view); })),
    m_floating    (std::make_unique<FloatingPanes> (static_cast<IFloatingPanesHost &> (*this))),
    m_colorKeys   (std::make_unique<PaneColorKeys> (static_cast<IColorKeysHost &> (*this)))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::~DebuggerWindow
//
////////////////////////////////////////////////////////////////////////////////

DebuggerWindow::~DebuggerWindow()
{
    //  The operand tip's popup goes back to its window before the window goes.
    m_operandTips->Hide();
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebuggerWindow::Create (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDebuggerWindowHost * host, bool activate)
{
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;



    BAIL_OUT_IF (IsCreated(), S_OK);

    m_theme         = theme;
    m_emulatorTheme = theme;
    m_host          = host;
    m_hInstance = hInstance;

    params.title                    = s_kpszWindowTitle;
    params.hInstance                = hInstance;
    //  A PEER OF THE EMULATOR WINDOW, NOT AN OWNED ONE. An owned window is
    //  z-locked above its owner forever: the debugger could never be put
    //  behind Casso, which is what a second window on one screen is for. It
    //  still opens beside it, which is what the placement anchor is.
    params.ownerHwnd                = nullptr;
    params.placementAnchorHwnd      = hwndOwner;
    params.initialSizeDip           = { kPreferredWidthDip, kPreferredHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = false;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.classNameOverride        = s_kpszClassName;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    ApplyKeyScheme        (GetSavedKeyScheme());
    ApplySavedPlacement();

    ApplyTheme (m_host != nullptr ? m_host->GetDebuggerTheme() : std::string());
    //  A window opened by the launch, rather than by the user, leaves the
    //  foreground and the keys where Windows put them.
    Show (activate);

    //  Where it opened is the baseline a close compares against, so a window
    //  the user never moved writes nothing and leaves the file to whoever did.
    m_openedRect = DxuiWindowFrame::GetVisibleRect (GetHwnd());
    WindowTrace::LogWindow ("create.actual", "debugger", GetHwnd(), "after Show");
    m_placed = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnCreate()
{
    m_menuBar           = CreateChild<DxuiMenuBar>   ();
    m_commandBar        = CreateChild<DxuiToolbar>   ();
    m_timelineBar       = CreateChild<DxuiToolbar>   ();
    CreateStatusBar();

    CreatePanes();

    //  Last, so its strips and the drop overlay paint over the panes.
    m_dockSite = CreateChild<DxuiDockSite>();
    m_dockSite->SetPaneGap (DxuiDockSite::kPaneGapDip, DxuiDockSite::kPaneMarginDip);

    //  After even the site, so a box being edited in place, such as a
    //  breakpoint's or a heat map range's name, is drawn over its row.
    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->CreateOverlays();
    }

    ConfigureWidgets();
    ConfigureDockSite();
    m_colorKeys->Create (m_panes);

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->OnDocked();
    }

    //  The text size the user last left, once every pane it sizes exists.
    if (m_host != nullptr)
    {
        ApplyTextZoom ((float) m_host->GetDebuggerTextZoomPercent() / DebuggerStatusText::kPercent);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureWidgets
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureWidgets()
{
    ConfigureMenuBar();
    ConfigureCommandBar();


    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->Configure();
    }

    SetAcceptsDroppedFiles  (true);

    for (DxuiListView * list : GetLists())
    {
        MakeDense (list);
    }

    //  Every list but the disassemblies is in the proportional face, as
    //  Visual Studio's Breakpoints, Watch and Call Stack windows are. Each
    //  column is aligned on its own, so a column lines up in either face.
    //  The event logs' fitted columns are measured from the labels the panes
    //  note (EventLogPane::NoteFitTexts).
    for (DxuiListView * list : GetTextLists())
    {
        list->SetMonospace (false);
    }

    //  A disassembly opens with its glyph margin for the breakpoints and the
    //  PC's arrow, as Visual Studio's does, so its columns start at the list's
    //  left rather than at the pane's text inset.
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        m_code->GetCodeList (view)->SetPaneTextInset (false);
    }


    //  The console's find widget is made now; a source document's the first
    //  time find opens there. Hidden until find opens.
    (void) m_finds->Activate (DebuggerLayout::kConsole);

    m_focusMgr.Attach   (this);
    m_focusMgr.SetTheme (m_theme);
    m_focusMgr.Rebuild();
    m_focusMgr.SetFocused (m_consolePane->GetCommandLine().GetBox());

    //  A list's column widths, once the user sizes one, are saved with the
    //  project; the event logs' are in their own options.
    for (DxuiListView * list : GetLists())
    {
        if (!HasOwnColumnWidths (list))
        {
            list->SetOnColumnResized ([this, list] (int, int)
            {
                m_columnWidths[GetPaneOfControl (list)].clear();
                SaveColumnWidths();
            });
        }
    }

    RestoreWindowState (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureCommandBar
//
//  The strip of commands across the top. Its entries carry the key schemes'
//  own ids, so a click and the key beside it in the tip run the same command.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureCommandBar()
{
    DebuggerCommands::Handlers  handlers;



    handlers.dispatch  = [this] (int id) { RunCommandBarEntry (id); };
    handlers.isEnabled = [this] (int id) { return IsCommandBarEntryEnabled (id); };
    handlers.isChecked = [this] (int id) { return id == DebuggerCommands::kTrace && m_snapshot != nullptr && m_snapshot->trace.isOn; };
    handlers.isDark    = [this]          { return IsDarkTheme(); };

    m_commands = std::make_unique<DebuggerCommands> (std::move (handlers));

    m_commandBar->SetTextRenderer (GetTextRenderer());
    m_commandBar->SetPopupHost    (GetPopupHost());
    m_tooltip.SetPopupHost        (GetPopupHost());
    m_tooltip.SetTheme            (*m_theme);
    m_tooltip.SetMonospace        (false);
    m_tooltip.ApplyVisualStudioLook();
    m_commandBar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
    m_commandBar->EnableSeeMore   (s_kpszMdl2More, L"See more");
    m_commandBar->SetGrabHandle   (true);
    m_commandBar->SetEntries      (m_commands->BuildEntries());

    ConfigureCommandBarHost();
    ConfigureTimeline();

    //  The two toolbars lay out together, in bands they can share. The
    //  timeline joins first: places saved before they could share a band
    //  had the timeline against the edge and the command bar inside it.
    m_timelineHost.JoinGroup (m_toolbarDocks);
    m_barHost.JoinGroup      (m_toolbarDocks);

    SetWindowMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMenuState
//
//  What the drop-downs show: the command mode, and each panel and whether it
//  is open.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerWindow::GetMenuState() const
{
    std::string  state;



    if (m_snapshot == nullptr)
    {
        return state;
    }

    state = CommandModeNames::GetName (m_snapshot->mode);

    for (const DebuggerViewSnapshot::PanelInfo & panel : m_snapshot->panels)
    {
        state += std::format ("|{}={}", panel.id, panel.open ? 1 : 0);
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunCommandBarEntry
//
//  A click on the strip runs what the same key would: the key schemes' own
//  actions go through the mapped-command path, and the bar's own entries are
//  handled here.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunCommandBarEntry (int id)
{
    if (id == DebuggerCommands::kShowNext)
    {
        ShowCode (std::nullopt);
        return;
    }

    if (id == DebuggerCommands::kTrace)
    {
        RunAction (DebuggerActions::GetTraceToggle (m_snapshot != nullptr && m_snapshot->trace.isOn, GetMode()));
        return;
    }

    if (id == DebuggerCommands::kReverseContinue || id == DebuggerCommands::kGoLive)
    {
        RunAction (DebuggerActions::GetReverse ((id == DebuggerCommands::kGoLive) ? DebugVerb::GoLive : DebugVerb::ReverseGo, GetMode()));
        return;
    }

    if (id == DebuggerCommands::kLoadFile)
    {
        LoadFileIntoMemory();
        return;
    }

    (void) OnMappedCommand (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsCommandBarEntryEnabled
//
//  A running machine has no state to step through, so everything that acts
//  on a stopped one is off while it runs, and Pause is off while it is
//  already stopped. Run to Cursor needs a line to run to as well. The
//  choices that only change what the window shows stay live throughout.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsCommandBarEntryEnabled (int id) const
{
    bool  paused = m_snapshot != nullptr && m_snapshot->isPaused;



    if (id == DebuggerCommands::kRun)
    {
        return paused;
    }

    //  A replay runs with the machine stopped, and Pause stops it.
    if (id == DebuggerCommands::kPause)
    {
        return !paused || (m_host != nullptr && m_host->IsReplayingHistory());
    }

    //  A load writes memory, which BLOAD does only with the machine stopped.
    if (id == DebuggerCommands::kStepInto || id == DebuggerCommands::kStepOver ||
        id == DebuggerCommands::kStepOut  || id == DebuggerCommands::kShowNext ||
        id == DebuggerCommands::kRunFrame || id == DebuggerCommands::kLoadFile)
    {
        return paused;
    }

    if (id == DebuggerCommands::kRunToCursor)
    {
        return paused && m_code->GetCodeList (m_code->GetActive())->GetSelectedRow() >= 0;
    }

    //  Moving through history needs a stopped machine and a recording, and
    //  going live needs the machine to be behind live.
    if (id == DebuggerCommands::kStepBackInto || id == DebuggerCommands::kStepBackOver ||
        id == DebuggerCommands::kStepBackOut  || id == DebuggerCommands::kReverseContinue)
    {
        return paused && m_snapshot->history.isRecording;
    }

    if (id == DebuggerCommands::kGoLive)
    {
        return paused && m_snapshot->history.isBehindLive;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetLists
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> DebuggerWindow::GetLists() const
{
    std::vector<DxuiListView *>  lists;



    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        std::ranges::copy (pane->GetLists(), std::back_inserter (lists));
    }

    return lists;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTextLists
//
//  Every list but the disassemblies, whose columns are code laid out as
//  Visual Studio lays out its own. Each column is aligned on its own, so a
//  column lines up in either face.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> DebuggerWindow::GetTextLists() const
{
    std::vector<DxuiListView *>  lists;



    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        if (pane->HasTextLists())
        {
            std::ranges::copy (pane->GetLists(), std::back_inserter (lists));
        }
    }

    return lists;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MakeDense
//
//  One setting for every pane, so the panes read as one listing rather than
//  seven lists that happen to share a window.
//
//  A pane narrower than its columns scrolls sideways, as Visual Studio's do:
//  the columns fit their contents, so without the scroll whatever was right
//  of the pane's edge -- the code pane's annotations, the call stack's Found
//  by -- could not be seen at all.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MakeDense (DxuiListView * list)
{
    list->SetShowHeader              (true);
    list->SetMonospace               (true);
    list->SetFontSizeDip             (kPaneFontDip);
    list->SetRowHeightDip            (kPaneRowDip);
    list->SetHeaderHeightDip         (kPaneHeaderDip);
    list->SetCellPaddingDip          (kPanePadDip, kPanePadDip);
    list->SetPreciseAutoFit          (true);
    list->SetRefitOnSetRows          (true);
    list->SetHorizontalScrollEnabled (true);
    list->SetPaneTextInset           (true);

    //  The arrows, the page keys, Home and End move the selection, and Ctrl+C
    //  copies it, so F9, run to cursor and the edit keys can act on a row
    //  chosen without the mouse. A keyboard move is reported as a click is,
    //  so the pane's record of the selection, which each snapshot restores,
    //  follows it.
    list->SetKeyboardColumnNav       (true);
    list->SetKeyboardMoveNotify      (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::StepTextZoom
//
//  Each size is a whole number of ten-percentage-point steps, so a step past
//  either end is lost rather than stored as a clamped fraction, and the same
//  number of steps back always returns to the size they left.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::StepTextZoom (int steps)
{
    int  smallest = (int) std::lround (kMinTextZoom / kTextZoomStep);
    int  largest  = (int) std::lround (kMaxTextZoom / kTextZoomStep);
    int  step     = (int) std::lround (m_textZoom / kTextZoomStep);



    step = std::clamp (step + steps, smallest, largest);
    ApplyTextZoom ((float) step * kTextZoomStep);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyTextZoom
//
//  A size change, not a zoom of the window: every content pane's text and
//  rows grow or shrink together, floating ones included since they are the
//  same controls, and the caption, the command bar and every other Casso
//  window keep their size. The code pane refits its line count at the next
//  layout. The size is saved through the host, so it survives the window.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyTextZoom (float zoom)
{
    m_textZoom = std::clamp (zoom, kMinTextZoom, kMaxTextZoom);

    if (m_host != nullptr)
    {
        m_host->SetDebuggerTextZoomPercent ((int) std::lround (m_textZoom * DebuggerStatusText::kPercent));
    }

    for (DxuiListView * list : GetLists())
    {
        list->SetFontSizeDip     (kPaneFontDip * m_textZoom);
        list->SetRowHeightDip    ((int) std::lround (kPaneRowDip * m_textZoom));
        list->SetHeaderHeightDip ((int) std::lround (kPaneHeaderDip * m_textZoom));
        list->ResetAutoFit();
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->SetTextZoom (m_textZoom);
    }


    //  The status bar's zoom and its slider follow a change from the keys or
    //  the wheel as well as from the slider itself.
    m_zoomSlider.SetValue (m_textZoom * DebuggerStatusText::kPercent);
    UpdateStatusBar();

    LayoutWidgets();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPressTargets
//
//  Every control a mouse press is offered to before the lists.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetPressTargets() const
{
    std::vector<IDxuiControl *>  targets;



    targets.push_back (m_consolePane->GetCommandLine().GetBox());

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        std::ranges::copy (pane->GetPressTargets(), std::back_inserter (targets));
    }

    std::ranges::copy (m_finds->GetPressTargets(), std::back_inserter (targets));

    return targets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyUndoHistories
//
//  A different machine clears every memory window's undo and the watch
//  pane's, since the addresses they would restore belonged to the old one,
//  unless the histories were restored for the new machine since the last
//  change.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyUndoHistories()
{
    if (m_snapshot->machine != m_machine)
    {
        m_machine = m_snapshot->machine;

        if (!m_isHistoryRestored)
        {
            m_memoryGroup->ClearHistories();
            m_watchesPane->GetHistory().Clear();
        }

        m_isHistoryRestored = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSourceSlotOf
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::GetSourceSlotOf (const std::wstring & pane) const
{
    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        if (pane == DebuggerLayout::GetSourcePaneId (slot))
        {
            return slot;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NoteViewFocus
//
//  Steps follow the view: the source pane steps by source line, the
//  disassembly by instruction, in the words of the session's mode. Sent only
//  when it changes, so a click does not fill the console.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::NoteViewFocus (bool isSource)
{
    if (m_host == nullptr || m_snapshot == nullptr || !m_snapshot->source.has_value())
    {
        return;
    }

    if (m_snapshot->source->stepBySource != isSource)
    {
        RunAction (DebuggerActions::GetSourceStepping (isSource, m_snapshot->mode));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NoteTabFocus
//
//  A press on a pane's tab counts as a press inside that pane: the pane takes
//  the focus, so its group draws the focus outline, and a disassembly or
//  source tab sets the step mode.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::NoteTabFocus (POINT pointDip)
{
    RECT            tab    = {};
    std::wstring    tip;
    std::wstring    pane   = m_dockSite->GetTabAt (pointDip, tab, tip);
    int             slot   = GetSourceSlotOf (pane);
    IDxuiControl  * target = nullptr;



    if (pane.empty())
    {
        return;
    }

    if (GetPaneOfFocus() != pane)
    {
        target = (pane == DebuggerLayout::kConsole) ? m_consolePane->GetCommandLine().GetBox() : nullptr;

        for (IDxuiControl * part : GetPaneControls (pane))
        {
            target = (target != nullptr) ? target : FindFirstFocusable (part);
        }

        if (target != nullptr)
        {
            SetFocusedControl (target);
        }
    }

    if (slot >= 0)
    {
        m_sources->SetActive (slot);
        NoteViewFocus (true);
        return;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            m_code->SetActive (view);
            NoteViewFocus (false);
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FindFirstFocusable
//
//  The first focusable control in a pane's tree, the pane's content first.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DebuggerWindow::FindFirstFocusable (IDxuiControl * node)
{
    IDxuiControl  * found = nullptr;



    if (node == nullptr)
    {
        return nullptr;
    }

    if (node->IsFocusable() && node->IsEnabled())
    {
        return node;
    }

    for (size_t i = 0; i < node->GetChildCount() && found == nullptr; i++)
    {
        found = FindFirstFocusable (node->GetChild (i));
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFilesDropped
//
//  A debug or symbol file loads as SYM LOAD would, and a source opens as File
//  > Open source file opens it. Any other file loads as symbols when it reads
//  as a symbol file; otherwise it is shown as text, or as a hex dump when it
//  is binary. One that cannot be read goes to SYM LOAD, which reports why.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnFilesDropped (const std::vector<std::wstring> & paths)
{
    SourceLookup  lookup;
    int           index  = -1;



    if (paths.empty() || m_host == nullptr)
    {
        return false;
    }

    for (const std::wstring & path : paths)
    {
        DroppedFiles::Kind     kind    = DroppedFiles::GetKind (path);
        DroppedFiles::Opening  opening = DroppedFiles::Opening::Symbols;

        if (kind == DroppedFiles::Kind::Source)
        {
            m_sources->OpenSourcePath (path);
            continue;
        }

        if (kind == DroppedFiles::Kind::Other)
        {
            lookup  = m_host->MatchDroppedDebuggerSource ({}, path, std::string(), index);
            opening = lookup.path.empty() ? DroppedFiles::Opening::Symbols : DroppedFiles::GetOpening (lookup.text);
        }

        if (opening == DroppedFiles::Opening::Symbols)
        {
            RunAction (DebuggerActions::GetLoadSymbols (SourcePathList::WideToUtf8 (path), GetMode()));
            continue;
        }

        m_sources->OpenLooseFile (path, opening == DroppedFiles::Opening::Hex ? DroppedFiles::FormatHexDump (lookup.text) : lookup.text, false);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsSymbolFile
//
//  A debug file or one of the symbol tables SYM LOAD reads, by extension.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsSymbolFile (const std::wstring & path)
{
    return DroppedFiles::GetKind (path) == DroppedFiles::Kind::Symbols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFocusedBox
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextInput * DebuggerWindow::GetFocusedBox() const
{
    IDxuiControl  * focused = GetFocused();



    if (focused != nullptr && focused == m_consolePane->GetCommandLine().GetBox())
    {
        return m_consolePane->GetCommandLine().GetBox();
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        for (DxuiTextInput * box : pane->GetTextBoxes())
        {
            if (focused != nullptr && focused == box)
            {
                return box;
            }
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunToCursor
//
//  Casso's own G, in Casso mode whatever the console's dialect.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunToCursor (Word address)
{
    RunAction (DebuggerActions::GetRunToCursor (address));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunAction
//
//  A control's action, which the session runs directly (FR-135).
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunAction (const DebuggerAction & action)
{
    if (m_host != nullptr)
    {
        m_host->RunDebuggerAction (action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMode
//
//  The session's mode as the last snapshot gave it, which an action's echo
//  is written in.
//
////////////////////////////////////////////////////////////////////////////////

CommandMode DebuggerWindow::GetMode() const
{
    return (m_snapshot != nullptr) ? m_snapshot->mode : CommandMode::AppleWin;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSavedKeyScheme
//
//  A name this build does not know reads as the default.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerKeyScheme DebuggerWindow::GetSavedKeyScheme() const
{
    DebuggerKeyScheme  scheme = DebuggerKeySchemes::kDefault;



    if (m_host != nullptr && !DebuggerKeySchemes::TryParse (m_host->GetDebuggerKeyScheme(), scheme))
    {
        scheme = DebuggerKeySchemes::kDefault;
    }

    return scheme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyTheme
//
//  The window's own theme, or the emulator's for the empty name. The system
//  themes take the accent from Windows each time one is chosen, as Casso
//  Explorer's do, and then the debugger's own tooltip and pane colors. A
//  test gives the system colors in place of Windows', so nothing is read
//  from the machine. The Theme drop-down is rebuilt for the check it holds.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyTheme (const std::string & name)
{
    const DxuiWindowsThemeColors::SystemColors &  system = m_systemColorsForTest.has_value() ? *m_systemColorsForTest
                                                                                            : DxuiWindowsThemeColors::Instance().GetSystemColors();



    m_themeName = name;

    m_lightTheme.ApplySystemColors (system);
    m_darkTheme.ApplySystemColors  (system);

    DebuggerThemes::ApplyOwnColors (m_lightTheme, m_darkTheme);

    if (m_emulatorTheme != nullptr)
    {
        m_theme = &DebuggerThemes::Choose (name, *m_emulatorTheme, m_lightTheme, m_darkTheme, m_ownTheme);
    }

    SetTheme            (m_theme);
    m_focusMgr.SetTheme (m_theme);

    if (m_theme != nullptr)
    {
        m_tooltip.SetTheme (*m_theme);
    }

    m_floating->ApplyTheme (m_theme);

    if (m_barHost.GetFloatWindow() != nullptr)
    {
        m_barHost.GetFloatWindow()->SetTheme (m_theme);
    }

    if (m_timelineHost.GetFloatWindow() != nullptr)
    {
        m_timelineHost.GetFloatWindow()->SetTheme (m_theme);
    }

    if (m_commandBar != nullptr)
    {
        SetWindowMenus();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyKeyScheme
//
//  The menu bar is rebuilt too, since its rows carry the keys of the scheme
//  that was in force when they were built.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyKeyScheme (DebuggerKeyScheme scheme)
{
    const DxuiKeyMap &  map = DebuggerKeySchemes::GetMap (scheme);



    m_keyScheme = scheme;
    SetKeyMap (&map);

    m_floating->ApplyKeyMap (&map);

    if (m_barHost.GetFloatWindow() != nullptr)
    {
        m_barHost.GetFloatWindow()->SetKeyMap (&map);
    }

    if (m_timelineHost.GetFloatWindow() != nullptr)
    {
        m_timelineHost.GetFloatWindow()->SetKeyMap (&map);
    }

    if (m_commands != nullptr)
    {
        m_commands->ApplyKeyScheme (scheme);
    }

    if (m_menuBar != nullptr)
    {
        SetWindowMenus();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnMappedCommand
//
//  Every action is the command its button sends, except Pause, which is the
//  channel's pause. A cursor action with no line to act on still consumes the
//  key, so it does not fall through to anything else.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnMappedCommand (int commandId)
{
    DebuggerKeySchemes::Action     action = (DebuggerKeySchemes::Action) commandId;
    std::optional<DebuggerAction>  taken;
    int                            row    = GetCodeLineOfRow (m_code->GetActive(), m_code->GetCodeList (m_code->GetActive())->GetSelectedRow());
    DebuggerViewSnapshot           active;



    //  The bookmark keys (FR-189).
    if (action == DebuggerKeySchemes::Action::ToggleBookmark || action == DebuggerKeySchemes::Action::NextBookmark ||
        action == DebuggerKeySchemes::Action::PreviousBookmark)
    {
        if (m_snapshot != nullptr && action == DebuggerKeySchemes::Action::ToggleBookmark)
        {
            ToggleBookmarkAtCursor();
        }
        else if (m_snapshot != nullptr)
        {
            GoToBookmark (action == DebuggerKeySchemes::Action::PreviousBookmark);
        }

        return true;
    }

    //  A pane's key opens it, or brings it forward, and focuses it.
    if (!DebuggerKeySchemes::GetPaneOfAction (action).empty())
    {
        ShowPane  (DebuggerKeySchemes::GetPaneOfAction (action));
        FocusPane (DebuggerKeySchemes::GetPaneOfAction (action));
        return true;
    }

    //  A pane's own use of an action while it has the focus, as a memory
    //  window's Find, which searches memory, and F3 and Shift+F3, which walk
    //  the matches.
    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        if (pane->OnMappedAction (action, GetFocused()))
        {
            return true;
        }
    }

    //  Find acts on the window, not the machine.
    if (action == DebuggerKeySchemes::Action::Find)
    {
        OpenFind();
        return true;
    }

    if (action == DebuggerKeySchemes::Action::FindNext || action == DebuggerKeySchemes::Action::FindPrevious)
    {
        FindInPane (action == DebuggerKeySchemes::Action::FindNext);
        return true;
    }

    if (action == DebuggerKeySchemes::Action::Pause)
    {
        if (m_host != nullptr)
        {
            TryStopReplay();
            m_host->PauseDebugger();
        }

        return true;
    }

    //  The cursor actions read the disassembly view last used.
    if (m_snapshot != nullptr)
    {
        active      = *m_snapshot;
        active.code = GetCodeLines (m_code->GetActive());
    }

    taken = DebuggerActions::GetForKey (action, (m_snapshot != nullptr) ? &active : nullptr, row, GetMode());

    if (taken.has_value())
    {
        RunAction (*taken);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteBoxKey
//
//  A key-down in a focused text box that the box does not keep goes to the
//  scheme first. A memory window and the Address box count as boxes that are
//  never empty: every character typed into the one is an edit, and the
//  other is no command line. When the scheme took a Space, the character WM_CHAR delivers
//  for it is swallowed, or an empty box that stepped would be left holding a
//  space. Returns true when the key was decided here.
//
//  In GSSquared mode the empty command line has GSSquared's own keys first:
//  Space and F10 step and Return resumes, whatever the scheme. In Monitor
//  mode Return on the empty command line is the Monitor's own empty line,
//  which shows the next row of bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteBoxKey (const DxuiKeyEvent & ev, bool & handled)
{
    DxuiTextInput                               * box           = GetFocusedBox();
    bool                                          inMemory      = std::ranges::any_of (m_panes, [this] (const std::unique_ptr<IDebuggerPane> & pane) { return pane->TakesTyping (GetFocused()); });
    bool                                          decided       = false;
    CommandMode                                   mode          = (m_snapshot != nullptr) ? m_snapshot->mode : CommandMode::AppleWin;
    std::optional<DebuggerKeySchemes::Action>     consoleAction;
    bool                                          boxEmpty      = false;
    bool                                          boxKeeps      = false;



    //  While the line assembler takes the box's lines, Return and Space are
    //  the box's, so an empty line ends the assembly.
    if (ev.kind == DxuiKeyEventKind::Down && box != nullptr && box == m_consolePane->GetCommandLine().GetBox() &&
        DebuggerViewState::DoesAssemblerKeepKey (m_snapshot.get(), ev.vk, ev.ctrl, ev.alt))
    {
        return false;
    }

    consoleAction = (ev.kind == DxuiKeyEventKind::Down && box != nullptr && box == m_consolePane->GetCommandLine().GetBox())
                  ? DebuggerViewState::GetConsoleKeyAction (mode, ev.vk, ev.ctrl, ev.alt, ev.shift, box->GetText().empty())
                  : std::nullopt;

    //  Only the command line steps from an empty line; the Address box is no
    //  command line, so it keeps Space and Return however empty it is.
    boxEmpty = box != nullptr && box == m_consolePane->GetCommandLine().GetBox() && box->GetText().empty() &&
               !DebuggerViewState::DoesConsoleKeepKey (mode, ev.vk, ev.ctrl, ev.alt);

    boxKeeps = DebuggerKeySchemes::DoesBoxKeepKey (ev.vk, ev.ctrl, ev.alt, true, boxEmpty,
                                                   box != nullptr && box == m_consolePane->GetCommandLine().GetBox() && mode == CommandMode::Monitor);

    if (consoleAction.has_value())
    {
        OnMappedCommand ((int) *consoleAction);
        m_swallowSpace = ev.vk == VK_SPACE;
        handled        = true;
        decided        = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Char)
    {
        decided        = m_swallowSpace && ev.vk == L' ';
        handled        = decided;
        m_swallowSpace = false;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && (box != nullptr || inMemory) && !boxKeeps && RouteMappedKey (ev))
    {
        m_swallowSpace = ev.vk == VK_SPACE;
        handled        = true;
        decided        = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && (box != nullptr || inMemory) && boxKeeps && ev.vk != VK_TAB &&
             !(box != nullptr && ev.vk == VK_RETURN))
    {
        //  A key the box keeps stays with it even when the box does nothing
        //  with the key-down, so it never reaches the scheme after all.
        (void) GetFocused()->OnKey (ev);
        handled = true;
        decided = true;
    }

    return decided;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UpdateTopLayer
//
//  Each open find widget floats over its pane's text. The page's text is
//  drawn after all of the page's fills, so a widget painted with the page
//  would have its pane's text showing through its plate; it goes in the top
//  layer, whose fills cover the page's text. A slid-out pane lies over the
//  others, so then only its own widget joins it, painted after its controls.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UpdateTopLayer()
{
    std::vector<IDxuiControl *>  layer;



    if (!m_slidPane.empty())
    {
        layer = GetPaneControls (m_slidPane);
    }

    std::ranges::copy (m_finds->GetLayerControls (m_slidPane), std::back_inserter (layer));

    SetTopLayer (std::move (layer));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnWindowClose
//
//  Closing the window closes the debugger: the host closes the channel, so
//  attached clients receive `closing`. The window is hidden rather than
//  destroyed and is shown again when the debugger reopens.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnWindowClose()
{
    SavePlacementIfMoved();
    SaveFocusedPane();

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->OnWindowClose();
    }

    Hide();

    m_floating->HideAll();

    m_barHost.Hide();
    m_timelineHost.Hide();

    //  OnMouseLeave clears the hover with the preview; with the hover set,
    //  the next frame's Sync opens the preview again for that cell.
    m_timelineStrip.OnMouseLeave();

    if (m_host != nullptr && m_host->GetHistoryThumbnails() != nullptr)
    {
        m_host->GetHistoryThumbnails()->SetVisible (false);
    }

    if (m_host != nullptr)
    {
        m_host->OnDebuggerWindowClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_widthDip  = std::max (1, (int) (boundsDip.right  - boundsDip.left));
    m_heightDip = std::max (1, (int) (boundsDip.bottom - boundsDip.top));
    m_scaler    = scaler;

    LayoutWidgets();

    //  The first layout shows which panes are in front, so the keys can go
    //  to one of them.
    if (m_isOpeningFocusDue)
    {
        m_isOpeningFocusDue = false;
        PlaceOpeningFocus();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::LayoutWidgets
//
//  Controls across the top and the memory bar across the bottom stay where
//  they are; the dock site between them arranges every pane.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::LayoutWidgets()
{
    auto  px       = [this] (int dip) { return m_scaler.ToPx (dip); };
    int   boxH     = px (30);
    int   width    = m_widthDip;
    int   height   = m_heightDip;
    int   captionH = GetCaptionHeightPx();
    int   menuH    = DxuiMenuBar::GetStripHeightPx (m_scaler.GetDpi());
    //  FLUSH UNDER THE CAPTION, AND THE PANES FLUSH UNDER IT. The strip
    //  carries its own margin around its buttons; padding it as well put
    //  body color between the caption and the buttons, which reads as a gap
    //  the bottom edge does not have.
    int   rowY     = captionH;
    int   top      = 0;
    int   bottom   = height;
    RECT  area     = {};



    if (m_code == nullptr || m_dockSite == nullptr)
    {
        return;
    }

    //  The strip takes the row, less the room the flags need at its end: it
    //  drops its labels one at a time and finally into See more, so it never
    //  runs off the edge. The renderer it measures and draws its icons with
    //  arrives with the backend, after the window was built.
    //  The menu bar first, under the caption, as Visual Studio's is.
    m_menuBar->SetTextRendererForMeasure (GetTextRenderer());
    m_menuBar->SetHostClientRect         (RECT { 0, 0, width, height });
    m_menuBar->Layout                    (RECT { 0, rowY, width, rowY + menuH }, m_scaler);

    rowY += menuH;

    //  The command bar takes a band along the edge it is docked to, sharing
    //  it with the dock site's auto-hide tabs, and the panes fill the rest;
    //  floating, the panes have the whole area.
    m_tooltip.SetDpi          (m_scaler.GetDpi());
    m_tooltip.SetViewportSize (width, height);

    //  The status bar along the bottom edge; everything else ends above it.
    bottom = PlaceStatusBar (width, height);

    //  The history timeline and the command bar take their bands, across the
    //  top under the menu bar by default, the timeline against the menu bar,
    //  and the panes have the rest, sharing the command bar's band for their
    //  auto-hidden tabs when it is the innermost and the timeline is not in it.
    area = m_toolbarDocks.Layout (RECT { 0, rowY, width, bottom }, RECT { 0, 0, width, height }, m_scaler);
    top  = area.top;

    //  The site takes the whole area; its own margin, in the gap color, keeps
    //  the panes off the window's edges and the toolbars' bands.
    m_dockSite->Layout (RECT { area.left, top, area.right, area.bottom }, m_scaler);

    m_code->UpdateLines();
    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->PlaceBar();
    }

    m_colorKeys->Place();
    m_finds->Place();
    ClipPaneControls();

    //  A pane moved or sized under the operand tip moves its cell.
    m_operandTips->Check (m_operandTips->GetCell().view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceOpeningFocus
//
//  When the window opens, the keys go back to the pane that had them when it
//  closed: its tab comes forward and its main control takes the focus. With
//  none saved, or one that no longer shows, the console comes forward and its
//  command line takes the focus. If the console is closed or slid away at an
//  edge, they go to the first pane's main control in the tab order: a list, a
//  memory view or a text view, never a text box in a pane's toolbar.
//
//  A saved pane that opens only after the machine's first snapshot -- a
//  second disassembly view, a device panel -- takes the focus when it shows,
//  if the user has not clicked or typed in the window by then.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceOpeningFocus()
{
    std::wstring  saved   = GetSavedFocusPane();
    bool          isReady = IsFocusPaneReady (saved);



    if (!saved.empty() && !isReady && !m_floating->Contains (saved))
    {
        m_openingFocus.Defer();
    }

    FocusPane (isReady ? saved : std::wstring (DebuggerLayout::kConsole));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceLateFocus
//
//  After each snapshot while the opening focus waits on a saved pane that had
//  not opened yet.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceLateFocus()
{
    std::wstring                  saved;
    OpeningFocusDeferral::Action  action = OpeningFocusDeferral::Action::None;



    if (m_openingFocus.IsPending())
    {
        saved  = GetSavedFocusPane();
        action = m_openingFocus.OnSnapshot (IsFocusPaneReady (saved), IsRestoringViews());
    }

    if (action == OpeningFocusDeferral::Action::Place)
    {
        FocusPane (saved);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSavedFocusPane
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetSavedFocusPane() const
{
    return (m_host != nullptr) ? SourcePathList::Utf8ToWide (m_host->GetDebuggerFocusedPane()) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsFocusPaneReady
//
//  A docked pane on screen with a control to take the keys.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsFocusPaneReady (const std::wstring & pane) const
{
    return !pane.empty() && !m_floating->Contains (pane) && IsPaneShown (pane) && GetMainControl (pane) != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FocusPane
//
//  The pane's tab comes forward and its main control takes the focus, or the
//  first main control in the tab order when that one cannot.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::FocusPane (const std::wstring & pane)
{
    IDxuiControl  * target = nullptr;
    IDxuiControl  * each   = nullptr;
    bool            isMain = false;
    size_t          count  = 0;



    if (IsPaneShown (pane) && !m_dockSite->GetPaneLayout().IsAutoHidden (pane))
    {
        m_dockSite->ActivatePane (pane);
        LayoutWidgets();
    }

    m_focusMgr.Rebuild();
    count = m_focusMgr.GetTabOrderCount();

    target = GetMainControl (pane);
    target = (target != nullptr && target->IsVisible() && IsRoutable (target)) ? target : nullptr;

    for (size_t i = 0; i < count && target == nullptr; i++)
    {
        each   = m_focusMgr.GetTabOrderAt (i);
        isMain = dynamic_cast<DxuiListView *> (each) != nullptr || dynamic_cast<DxuiHexView *> (each) != nullptr || dynamic_cast<DxuiTextView *> (each) != nullptr;
        target = (isMain && each->IsVisible()) ? each : nullptr;
    }

    if (target != nullptr)
    {
        m_focusMgr.SetFocused (target);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMainControl
//
//  The control in a pane that takes the keys when the pane is given the
//  focus: the console's command line, or else the pane's list, memory view or
//  text view. A pane with none of those gives its last control; an unknown
//  pane gives none.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DebuggerWindow::GetMainControl (const std::wstring & pane) const
{
    std::vector<IDxuiControl *>  controls = GetPaneControls (pane);



    if (pane == DebuggerLayout::kConsole)
    {
        return m_consolePane->GetCommandLine().GetBox();
    }

    for (IDxuiControl * each : controls)
    {
        if (dynamic_cast<DxuiListView *> (each) != nullptr || dynamic_cast<DxuiHexView *> (each) != nullptr || dynamic_cast<DxuiTextView *> (each) != nullptr)
        {
            return each;
        }
    }

    return controls.empty() ? nullptr : controls.back();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveFocusedPane
//
//  The pane holding the focused control, kept so the window gives it the
//  keys when it opens again. A control in no pane, the toolbar's, saves none.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveFocusedPane()
{
    std::wstring  pane = GetPaneOfControl (m_focusMgr.GetFocusedControl());



    if (m_host != nullptr)
    {
        m_host->SetDebuggerFocusedPane (SourcePathList::WideToUtf8 (pane));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneHost
//
//  The window a pane's controls are in: its floating window, or this one.
//
////////////////////////////////////////////////////////////////////////////////

DxuiWindow * DebuggerWindow::GetPaneHost (const std::wstring & pane)
{
    DxuiDockedWindow  * window = m_floating->Find (pane);



    return (window != nullptr) ? (DxuiWindow *) window : (DxuiWindow *) this;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMenuHost
//
//  Where a menu opened by the event being routed shows: the floating window
//  the event came from, or this one.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHwndSource * DebuggerWindow::GetMenuHost() const
{
    DxuiHwndSource  * host = m_floating->GetRoutedPopupHost();



    return (host != nullptr) ? host : GetPopupHost();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetRoutedTooltip
//
//  The tooltip of the window the event being routed came from, since a
//  tooltip shows only over the window that owns it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTooltip & DebuggerWindow::GetRoutedTooltip()
{
    DxuiTooltip  * tip = m_floating->GetRoutedTip();



    return (tip != nullptr) ? *tip : m_tooltip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBarRoutingPane
//
//  The routing pane under which a pane's bar takes the mouse: the pane's own
//  while it floats, and none while it is in this window.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetBarRoutingPane (const std::wstring & pane) const
{
    return m_floating->GetBarRoutingPane (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClipPaneControls
//
//  Each pane's controls paint inside the pane, so nothing a control draws
//  past its edge lands on a neighbor. The pane's info button is left out:
//  it is in the pane's title bar, outside the body, and PaneColorKeys::Place
//  clips it to its own place.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ClipPaneControls()
{
    for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
    {
        IDxuiControl  * content = GetPaneContent (pane);
        RECT            clip    = {};



        if (content == nullptr)
        {
            continue;
        }

        clip = content->GetBounds();

        SetChildClip (content, clip);

        for (IDxuiControl * control : GetOwnControls (pane))
        {
            SetChildClip (control, clip);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneMinimum
//
//  A framed pane's least height: a tool window's title bar, its tab band
//  and the lines round them, over the least height the frame's parts fit in
//  at its width. A pane of one control has no minimum.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DebuggerWindow::GetPaneMinimum (const std::wstring & pane) const
{
    const DebuggerPaneFrame  * frame  = dynamic_cast<const DebuggerPaneFrame *> (m_dockSite->GetPaneContent (pane));
    RECT                       bounds = {};
    long                       chrome = 0;



    if (frame == nullptr)
    {
        return SIZE {};
    }

    bounds = frame->GetBounds();
    chrome = m_scaler.ToPx (DxuiTabGroup::kTitleDip + DxuiTabGroup::kStripDip) + 2 * DxuiPaneMetrics::GetLinePx (m_scaler);

    return SIZE { 0, chrome + frame->GetMinimumHeightPx ((int) (bounds.right - bounds.left), m_scaler) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ReadSavedLayout
//
//  The saved layout text, with the closed panes read into m_closedPanes from
//  their own setting. A layout an older build saved carries them on a line
//  ahead of the text instead; that line is moved to the setting once.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::ReadSavedLayout()
{
    std::wstring            savedText;
    std::wstring            layoutText;
    std::set<std::wstring>  legacy;
    bool                    hasHeatMapOptions = false;



    m_closedPanes.clear();

    if (m_host == nullptr)
    {
        return {};
    }

    savedText  = SourcePathList::Utf8ToWide (m_host->GetDebuggerLayout());
    layoutText = DebuggerLayout::TakeClosedPanes (savedText, legacy);

    DebuggerLayout::ReadClosedPanes (SourcePathList::Utf8ToWide (m_host->GetDebuggerClosedPanes()), m_closedPanes);

    //  The heat map opens by default only in the default layout. A layout
    //  saved before it did so had it closed, and no build that opened it by
    //  default has yet saved its options.
    hasHeatMapOptions = !m_host->GetDebuggerHeatMapOptions().empty();

    if (!layoutText.empty() && !hasHeatMapOptions)
    {
        m_closedPanes.insert (DebuggerLayout::kHeatMap);
        m_host->SetDebuggerClosedPanes (SourcePathList::WideToUtf8 (DebuggerLayout::ClosedPanesToText (m_closedPanes)));
    }


    m_barHost.SetDock      (DxuiToolbarDock::FromText (SourcePathList::Utf8ToWide (m_host->GetDebuggerCommandBarDock())));
    m_timelineHost.SetDock (DxuiToolbarDock::FromText (SourcePathList::Utf8ToWide (m_host->GetDebuggerTimelineDock())));

    if (layoutText != savedText)
    {
        m_closedPanes.insert (legacy.begin(), legacy.end());

        m_host->SetDebuggerClosedPanes (SourcePathList::WideToUtf8 (DebuggerLayout::ClosedPanesToText (m_closedPanes)));
        m_host->SetDebuggerLayout      (SourcePathList::WideToUtf8 (layoutText));
    }

    return layoutText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureDockSite
//
//  Every pane goes to the site under its layout id. The source pane and the
//  console are frames over several controls; the rest are one control each.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureDockSite()
{
    //  A pane's toolbar is a tool window's compact strip, as Visual Studio's.
    constexpr int   kMemoryBarDip = DxuiToolbar::kCompactBandDp;
    auto            boxHeight     = [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (30); };
    auto            barHeight     = [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (kMemoryBarDip); };
    auto            bandHeight    = [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (HistoryBand::GetHeightDip (kPaneRowDip)); };
    std::wstring    savedText;
    DxuiPaneLayout  restored;
    InputFilter     dragFilter;



    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->AddToDock (*m_dockSite);
    }

    m_dockSite->SetShownFn    ([this] (const std::wstring & pane) { return IsPaneShown (pane); });

    //  The site lays a pane out no shorter than its bar and bands, so a sash
    //  dragged over it stops before its controls run past its edge.
    m_dockSite->SetMinSizeFn  ([this] (const std::wstring & pane) { return GetPaneMinimum (pane); });

    //  Source and Disassembly are documents, the rest tool windows, each with
    //  a title bar whose menu is the pane's Dock To menu (FR-084).
    m_dockSite->SetDocumentFn  ([this] (const std::wstring & pane) { return IsDocumentPane (pane); });
    m_dockSite->SetOnPaneMenu  ([this] (const std::wstring & pane, POINT clientPx) { ShowDockToMenu (pane, clientPx); });
    m_dockSite->SetOnClosePane ([this] (const std::wstring & pane) { ClosePane (pane); },
                                [this] (const std::wstring & pane) { return CanClosePane (pane); });

    //  A pane slid out from an edge lies over the others, so its controls are
    //  painted above the page.
    m_dockSite->SetOnSlid ([this] (const std::wstring & pane)
    {
        m_slidPane = pane;
        UpdateTopLayer();
    });
    savedText = ReadSavedLayout();
    std::erase_if (m_closedPanes, [this] (const std::wstring & pane) { return !IsFixedPane (pane); });

    restored = DebuggerLayout::Restore (savedText);
    restored.PlaceOnMonitors (GetMonitors());
    m_dockSite->SetPaneLayout (restored);
    m_floating->MarkSyncNeeded();

    m_dockSite->SetOnFloatRequested ([this] (const std::wstring & pane, POINT clientPx) { m_floating->RequestFloat (pane, clientPx); });
    m_dockSite->SetOnTearOff        ([this] (const std::wstring & pane, POINT clientPx) { m_floating->TearOff      (pane, clientPx); });

    //  Every change the user makes is saved as it happens, so a crash or a
    //  closed emulator loses nothing.
    m_dockSite->SetOnChanged ([this]
    {
        m_floating->MarkSyncNeeded();
        SaveLayout();
    });

    //  A pane drag is canceled when the window loses the mouse with the
    //  button still down. The window releases the capture itself as the
    //  button comes up, with the button already up, which is no loss.
    dragFilter.onMouseLost = [this] (UINT)
    {
        m_dockSite->OnDragMouseLost ((GetKeyState (VK_LBUTTON) & 0x8000) != 0);
    };

    AddInputFilter (m_dockSite, std::move (dragFilter));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsPaneShown
//
//  The source pane shows while a debug file is loaded, a memory window while
//  it is open, a device panel while its device is present and its panel open
//  (FR-044), and the heat map's ranges once they have been opened; the rest
//  show until their close buttons close them.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsPaneShown (const std::wstring & pane) const
{
    bool  isOpened = false;



    if (m_closedPanes.contains (pane))
    {
        return false;
    }

    if (FindPane (pane) != nullptr && FindPane (pane)->TryGetOpened (isOpened))
    {
        return isOpened;
    }

    if (DebuggerLayout::IsOpenedLater (pane))
    {
        return m_basicOpened.contains (pane);
    }


    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PaintTopLayer
//
//  The slid-out pane over the page: its background, its controls, then its
//  title bar and outline; then the status bar's zoom popup over all of it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PaintTopLayer (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    //  Under a slid-out pane, which lies over the disassembly and the source.
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews + SourceDocuments::kMaxDocuments; view++)
    {
        PaintBranchArrow (painter, view);
    }

    m_dockSite->PaintSlidUnder (painter, theme);
    DxuiWindow::PaintTopLayer  (painter, text, theme);
    m_dockSite->PaintSlidOver  (painter, text, theme);
    PaintZoomPopup             (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HasTopLayer
//
//  A slid-out pane, the status bar's zoom popup, or the PC on a branch with
//  its arrow to draw. The top
//  layer is a second flush of the frame, so it is asked for only then.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::HasTopLayer() const
{
    if (DxuiWindow::HasTopLayer() || m_zoomOpen)
    {
        return true;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        for (const DebuggerViewSnapshot::CodeLine & line : GetCodeLines (view))
        {
            //  The PC's branch, or the one a step just took to the PC, which
            //  can be out of the lines built for the view.
            if (m_code->IsOpen (view) && line.isCurrent && (line.target.has_value() || m_snapshot->branchedFrom.has_value()))
            {
                return true;
            }
        }
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        const SourceDocumentPane  & document = m_sources->GetDocument (slot);

        if (document.IsShown() && document.GetModel().IsActive() && m_snapshot != nullptr &&
            m_snapshot->source.has_value() && m_snapshot->source->pcTarget.has_value())
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PaintDragLayer
//
//  A pane drag's drop preview and guides, over every pane and its pictures,
//  the heat map's included, and over the top layer.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PaintDragLayer (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme)
{
    if (m_dockSite != nullptr)
    {
        m_dockSite->PaintDragLayer (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HasDragLayer
//
//  While a pane is dragged in this window and its marks are not drawn in the
//  overlay above the floating windows.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::HasDragLayer() const
{
    return m_dockSite != nullptr && m_dockSite->HasDragLayer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsDocumentPane
//
//  The Disassembly views and the source documents are documents; every
//  other pane is a tool window (FR-084).
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsDocumentPane (const std::wstring & pane) const
{
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            return true;
        }
    }

    return GetSourceSlotOf (pane) >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CanClosePane
//
//  Every pane can close, since the View menu opens any of them again.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::CanClosePane (const std::wstring &) const
{
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsFixedPane
//
//  A pane the window always holds: the first Disassembly view, the first
//  memory window and the tool windows. Closing one only hides it; the
//  others close for real and open again from the snapshot.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsFixedPane (const std::wstring & pane) const
{
    std::string  diagnosticsId;



    for (int view = 1; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            return false;
        }
    }

    if (FindPane (pane) != nullptr && !FindPane (pane)->IsFixed())
    {
        return false;
    }

    return GetSourceSlotOf (pane) < 0 && !DebuggerLayout::TryGetDiagnosticsId (pane, diagnosticsId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetViewMenuPanes
//
//  What the View menu lists, in its order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DebuggerWindow::GetViewMenuPanes() const
{
    return ViewMenuRows::GetPanes (m_panes, m_snapshot.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowPane
//
//  A View menu choice: a closed pane opens where the layout still keeps it,
//  docked or floating, and the pane comes forward. A pane the snapshot opens
//  comes forward once the snapshot shows it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowPane (const std::wstring & pane)
{
    //  A pane turned on opens docked and shown, not slid in at an edge.
    if (!IsPaneShown (pane) && m_dockSite->GetPaneLayout().IsAutoHidden (pane))
    {
        (void) m_dockSite->EditPaneLayout().DockBack (pane);
    }

    m_closedPanes.erase (pane);

    //  A pane closed until first shown, such as the heat map's ranges, is
    //  opened from then on.
    if (FindPane (pane) != nullptr)
    {
        FindPane (pane)->Open();
    }

    //  A BASIC or tool pane is added to the open views the same way.
    if (DebuggerLayout::IsOpenedLater (pane))
    {
        m_basicOpened.insert (pane);
    }

    if (!IsPaneShown (pane))
    {
        if (FindPane (pane) != nullptr)
        {
            (void) FindPane (pane)->RequestOpen();
        }
    }

    m_floating->MarkSyncNeeded();
    m_dockSite->Relayout();
    m_pendingShowPane = pane;
    ShowPendingPane();
    SetWindowMenus();
    SaveLayout();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowPendingPane
//
//  The pane the View menu chose comes forward once it shows: its tab is
//  selected, an auto-hidden one slides out, and a floating one's window is
//  raised.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowPendingPane()
{
    std::wstring  pane = m_pendingShowPane;



    if (pane.empty() || !IsPaneShown (pane))
    {
        return;
    }

    m_pendingShowPane.clear();
    m_dockSite->ActivatePane (pane);

    m_floating->Show (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClosePane
//
//  A close button on a document tab or a tool window's title bar, carried
//  out as the pane's own Close menu item would. A fixed pane hides, keeping
//  its place in the layout, until the View menu shows it again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ClosePane (const std::wstring & pane)
{
    int  slot = GetSourceSlotOf (pane);



    if (IsFixedPane (pane))
    {
        m_closedPanes.insert (pane);
        m_floating->MarkSyncNeeded();
        m_dockSite->Relayout();
        SetWindowMenus();
        SaveLayout();
        Invalidate();
        return;
    }

    if (slot >= 0)
    {
        m_sources->Close (slot);
        return;
    }

    if (m_host == nullptr)
    {
        return;
    }

    if (FindPane (pane) != nullptr && FindPane (pane)->RequestClose())
    {
        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClosePaneOfFocus
//
//  Closes the pane holding the focus, when there is one and it can close.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ClosePaneOfFocus()
{
    std::wstring  pane = GetPaneOfFocus();



    if (pane.empty() || !CanClosePane (pane))
    {
        return false;
    }

    ClosePane (pane);
    Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneOfFocus
//
//  The pane holding the focused control, including the parts of a frame.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneOfFocus() const
{
    IDxuiControl  * focused = GetFocused();
    std::wstring    owner;



    if (!m_floating->GetRoutingPane().empty())
    {
        return m_floating->GetRoutingPane();
    }

    if (focused == nullptr)
    {
        return L"";
    }

    if (focused == m_consolePane->GetOutput().GetView() || focused == m_consolePane->GetCommandLine().GetBox() || GetPaneOfControl (focused) == DebuggerLayout::kConsole)
    {
        return DebuggerLayout::kConsole;
    }

    //  A pane docked as a frame -- a disassembly view, the call stack, a
    //  memory window -- holds its focusable controls inside the frame, so
    //  the dock site, which knows only the frame, cannot place them.
    owner = GetPaneOfControl (focused);

    if (!owner.empty())
    {
        return owner;
    }

    return m_dockSite->GetPaneOf (focused);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowDockToMenu
//
//  The menu of a pane's tab or title bar, as Visual Studio's: the pane's own
//  actions, then Dock, Dock in tab group, Auto hide, Move to new window (from
//  a tab), All to new window and Close. The one chosen runs through the site like a drop would.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowDockToMenu (const std::wstring & pane, POINT clientPx)
{
    RECT                                 tab     = {};
    std::wstring                         tip;
    bool                                 fromTab = m_floating->GetRoutingPane().empty() && !m_dockSite->GetTabAt (clientPx, tab, tip).empty();
    std::vector<DxuiDockSite::MenuItem>  items   = m_dockSite->GetPaneMenu (pane, fromTab);
    std::vector<DxuiPopupMenuItem>       menu;
    DxuiHwndSource                     * host    = GetMenuHost();
    int                                  slot    = GetSourceSlotOf (pane);



    //  A floating pane's menu opens in its own window, where the click was.
    if (items.empty() || host == nullptr)
    {
        return;
    }

    m_menuCommands.clear();
    SetWindowMenus();

    //  A disassembly view that does not follow the PC can take it over.
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane != DebuggerLayout::GetCodePaneId (view) || m_snapshot == nullptr)
        {
            continue;
        }

        if (m_snapshot->followView != view)
        {
            m_menuCommands.push_back (DebuggerMenuCommand::Make (L"Follow PC", false, [this, view] { if (m_host != nullptr) { m_host->SetDebuggerFollowView (view); } }));
            menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
        }

        if (!menu.empty())
        {
            menu.push_back (DxuiPopupMenuItem::ForSeparator());
        }
    }

    //  A source document's own items.
    if (slot >= 0 && m_sources->IsOpen (slot))
    {
        std::vector<DxuiPopupMenuItem>  syntax;
        SourcePane                    * source = &m_sources->GetDocument (slot).GetModel();



        //  Whose grammar colors the document, for a file its text misleads.
        for (SourceSyntax::Assembler each : { SourceSyntax::Assembler::Any, SourceSyntax::Assembler::As65,
                                              SourceSyntax::Assembler::Merlin, SourceSyntax::Assembler::Ca65 })
        {
            m_menuCommands.push_back (DebuggerMenuCommand::Make (SourceSyntax::GetAssemblerLabel (each), source->GetAssemblerChoice() == each, [this, source, each]
            {
                source->SetAssembler (each);

                if (m_snapshot != nullptr)
                {
                    source->Apply (*m_snapshot);
                }
            }));

            syntax.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
        }

        m_menuCommands.push_back (DebuggerMenuCommand::Make (L"Syntax", false, [] {}));
        menu.push_back (DxuiPopupMenuItem::ForSubmenu (m_menuCommands.back(), std::move (syntax)));
        menu.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    for (const DxuiDockSite::MenuItem & item : items)
    {
        if (item.label.empty())
        {
            menu.push_back (DxuiPopupMenuItem::ForSeparator());
            continue;
        }

        m_menuCommands.push_back (DebuggerMenuCommand::Make (item.label, false, [action = item.action] { if (action) { (void) action(); } }));
        m_menuCommands.back()->accelerator = item.accelerator;
        m_menuCommands.back()->isEnabled   = [enabled = item.enabled] { return enabled; };
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    DxuiContextMenu::Show (*host, clientPx.x, clientPx.y, std::move (menu));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowContentMenu
//
//  The actions on what was right-clicked: a line, a breakpoint, a watch, a
//  byte, a heat map cell. Reports false when the point is not on the pane's
//  content, which leaves it to the pane's own menu.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ShowContentMenu (const std::wstring & pane, POINT clientPx)
{
    std::vector<std::pair<std::wstring, std::function<void()>>>  items;
    std::vector<DxuiPopupMenuItem>                               menu;
    int                                                          row    = -1;
    RECT                                                         bounds = {};



    if (m_snapshot == nullptr || GetMenuHost() == nullptr)
    {
        return false;
    }

    //  The console's two parts are text: the command line edits, the output
    //  only copies.
    if (GetPaneOfControl (m_consolePane->GetOutput().GetView()) == pane && m_consolePane->GetCommandLine().GetBox()->IsVisible() && DxuiDockSite::Contains (m_consolePane->GetCommandLine().GetBox()->GetBounds(), clientPx))
    {
        SetFocusedControl (m_consolePane->GetCommandLine().GetBox());
        ShowEditMenu (m_consolePane->GetCommandLine().GetBox(), clientPx, {});
        return true;
    }

    if (GetPaneOfControl (m_consolePane->GetOutput().GetView()) == pane && m_consolePane->GetOutput().GetView()->IsVisible() && DxuiDockSite::Contains (m_consolePane->GetOutput().GetView()->GetBounds(), clientPx))
    {
        ShowEditMenu (m_consolePane->GetOutput().GetView(), clientPx, { { L"Clear", [this] { m_consolePane->GetOutput().Clear(); } } });
        return true;
    }

    for (DxuiListView * list : GetLists())
    {
        bounds = list->GetBounds();

        if (GetPaneOfControl (list) != pane || !list->IsVisible() || !DxuiDockSite::Contains (bounds, clientPx))
        {
            continue;
        }

        if (ShowColumnMenu (list, clientPx))
        {
            return true;
        }

        row = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);

        //  As in Explorer: a right-click on a selected row does not change
        //  the selection, so Copy copies every selected row; on any other
        //  row it selects that row alone.
        if (row >= 0 && !list->IsRowSelected (row))
        {
            list->SetSelectedRow (row);
        }

        AddListMenuItems (list, row, GetColumnAt (list, clientPx.x - bounds.left), items);
    }

    if (FindPane (pane) != nullptr)
    {
        FindPane (pane)->AddContentMenuItems (clientPx, items);
    }

    if (items.empty())
    {
        return false;
    }

    m_menuCommands.clear();
    SetWindowMenus();

    for (auto & [label, action] : items)
    {
        m_menuCommands.push_back (DebuggerMenuCommand::Make (label, false, action));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    DxuiContextMenu::Show (*GetMenuHost(), clientPx.x, clientPx.y, std::move (menu));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowColumnMenu
//
//  On the headings of a list whose columns can be chosen, a check for each
//  column. False when the point is not on the headings or the list has no
//  such menu.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ShowColumnMenu (DxuiListView * list, POINT clientPx)
{
    std::vector<std::shared_ptr<DxuiCommand>>  commands;
    std::vector<DxuiPopupMenuItem>             menu;
    RECT                                       bounds   = list->GetBounds();



    if (!list->IsHeaderShown() || clientPx.y - bounds.top >= list->GetHeaderHeightPx())
    {
        return false;
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        (void) pane->AddColumnMenuItems (list, commands);
    }

    if (commands.empty())
    {
        return false;
    }

    m_menuCommands.clear();
    SetWindowMenus();

    for (const std::shared_ptr<DxuiCommand> & command : commands)
    {
        m_menuCommands.push_back (command);
        menu.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    DxuiContextMenu::Show (*GetMenuHost(), clientPx.x, clientPx.y, std::move (menu));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowEditMenu
//
//  Cut, Copy, Paste and Select all, as far as the control does them, each
//  dimmed when it has nothing to act on; then the extra rows after a line.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowEditMenu (IDxuiControl * control, POINT clientPx, std::vector<std::pair<std::wstring, std::function<void()>>> extra)
{
    static constexpr std::tuple<DxuiStandardCommand, const wchar_t *, const wchar_t *>  s_kEdits[] =
    {
        { DxuiStandardCommand::Cut,       L"Cut",        L"Ctrl+X" },
        { DxuiStandardCommand::Copy,      L"Copy",       L"Ctrl+C" },
        { DxuiStandardCommand::Paste,     L"Paste",      L"Ctrl+V" },
        { DxuiStandardCommand::SelectAll, L"Select all", L"Ctrl+A" },
    };
    std::vector<DxuiPopupMenuItem>  menu;
    bool                            enabled = false;



    m_menuCommands.clear();
    SetWindowMenus();

    for (const auto & [command, label, accelerator] : s_kEdits)
    {
        if (!control->QueryCommand (command, enabled))
        {
            continue;
        }

        m_menuCommands.push_back (DebuggerMenuCommand::Make (label, false, [this, control, command] { (void) control->InvokeCommand (command); Invalidate(); }));
        m_menuCommands.back()->accelerator = accelerator;
        m_menuCommands.back()->isEnabled   = [control, command] { bool on = false; return control->QueryCommand (command, on) && on; };
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    if (!extra.empty())
    {
        menu.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    for (auto & [label, action] : extra)
    {
        m_menuCommands.push_back (DebuggerMenuCommand::Make (label, false, action));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    DxuiContextMenu::Show (*GetMenuHost(), clientPx.x, clientPx.y, std::move (menu));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetColumnAt
//
//  The columns start after the list's pane lead, so their edges are counted
//  from there; a point in the lead is in the first column.
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::GetColumnAt (const DxuiListView * list, int xPx)
{
    int  right = list->GetPaneLeadPx() - list->GetLeftPx();



    for (size_t c = 0; c < list->GetColumnCount(); c++)
    {
        right += list->GetColumnEffectiveWidthPx (c);

        if (xPx < right)
        {
            return (int) c;
        }
    }

    return (int) list->GetColumnCount() - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddShowInMemory
//
//  "Show <what> in Memory N" for every memory pane open, each resolving the
//  same Go to text in its own pane.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddShowInMemory (const std::wstring & what, const std::string & goTo, std::vector<std::pair<std::wstring, std::function<void()>>> & items)
{
    for (MemoryWindowPane * window : m_memoryGroup->GetOpenWindows())
    {
        int  id = window->GetWindowId();

        items.push_back ({ std::format (L"Show {} in Memory {}", what, id), [this, id, goTo]
        {
            if (m_host != nullptr)
            {
                m_host->GoToDebuggerMemory (id, goTo);
            }
        } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddListMenuItems
//
//  A list pane's menu, for the row under the pointer where there is one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddListMenuItems (DxuiListView * list, int row, int column, std::vector<std::pair<std::wstring, std::function<void()>>> & items)
{
    const DebuggerViewSnapshot  & s    = *m_snapshot;
    auto                          copy = [this, list] { DxuiClipboard::SetText (GetHwnd(), list->GetSelectionText()); };



    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        if (pane->AddMenuItems (list, row, items))
        {
            return;
        }
    }

}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RenderFrame
//
//  Once per UI frame: take whatever the CPU thread published, then repaint
//  when that brought something, when something is moving, or after
//  kIdlePaintMs, which keeps a caret blinking. Painting every frame, and
//  twice when a snapshot came in, cost a fifth of a core in a release build.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RenderFrame()
{
    std::shared_ptr<const DebuggerViewSnapshot>  snapshot;
    std::vector<std::string>                     console;
    int64_t                                      now         = (int64_t) GetTickCount64();
    bool                                         hasUpdate   = false;
    bool                                         isAnimating = GetCapture() != nullptr;



    if (!IsCreated() || m_host == nullptr)
    {
        return;
    }

    SaveUndoHistories();

    if (m_host->TakeDebuggerProjectState())
    {
        ReloadProjectState();
    }

    if (m_host->TakeDebuggerUpdate (snapshot, console))
    {
        hasUpdate = true;

        if (snapshot != nullptr)
        {
            TakeSnapshot (std::move (snapshot));
        }

        AppendConsole (console);
    }

    m_consolePane->GetCommandLine().RefreshGhost();
    UpdateStatusBar();

    for (DxuiListView * list : GetLists())
    {
        list->Tick (now);
    }

    SyncHeatMapRecording();

    //  Focus moves by click, key and command alike, so the group the user is
    //  working in is found once a frame rather than at each of them.
    ShowFocusAccent();

    //  A disassembly view's height changes with a window resize, a sash drag,
    //  a tab brought forward, a pane slid out or floated, a text-size change,
    //  and each of those has been missed in turn. Measured once a frame, the
    //  view fills whatever room it has; only a change is sent.
    m_code->UpdateLines();

    //  A drop-down slides open on ticks its host supplies. Without them the
    //  menu stayed at the first frame of its reveal, a sliver under the
    //  entry, and Panels, Dialect and Keys looked as if they did nothing.
    //  The content menus are the same.
    for (DxuiToolbar * strip : { m_commandBar })
    {
        if (strip != nullptr && strip->WantsTick())
        {
            strip->TickMenus (now);
            isAnimating = true;
        }
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        for (DxuiToolbar * strip : pane->GetBars())
        {
            if (strip != nullptr && strip->WantsTick())
            {
                strip->TickMenus (now);
                isAnimating = true;
            }
        }
    }

    if (m_menuBar->WantsTick())
    {
        m_menuBar->TickMenus (now);
        isAnimating = true;
    }

    //  A menu dismissed by a click outside closes in its own popup, so the
    //  theme it previewed is put back here.
    EndThemePreview();

    if (GetPopupHost() != nullptr && GetPopupHost()->GetContextMenu().WantsTick())
    {
        GetPopupHost()->GetContextMenu().Tick (now);
        isAnimating = true;
    }

    if (m_tooltip.WantsTick())
    {
        m_tooltip.Tick (now);
        isAnimating = true;
    }

    m_operandTips->FollowPointer();
    m_floating->Tick (now);

    ApplyLogDeltas();

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        isAnimating = pane->Tick (now) || isAnimating;
    }

    isAnimating = m_timelineScrub.IsScrubbing() || isAnimating;

    m_floating->Sync();
    m_barHost.Sync();
    SyncCommandBarFloatTip();
    SyncTimeline();
    m_floating->CarryTornOff();
    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->PlaceBar();
    }

    m_colorKeys->Place();
    m_finds->Place();
    ClipPaneControls();

    m_sources->FollowMarkedLines();


    m_floating->PollCaptionDrags();

    if (!IsFramePaintDue (hasUpdate, isAnimating, now, m_framePaintedAt))
    {
        return;
    }

    m_framePaintedAt = now;

    m_floating->InvalidateAll();

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsFramePaintDue
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsFramePaintDue (
    bool     hasUpdate,
    bool     isAnimating,
    int64_t  nowMs,
    int64_t  paintedAtMs)
{
    return hasUpdate || isAnimating || nowMs - paintedAtMs >= kIdlePaintMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RequestPaint
//
//  The window paints once the messages waiting are handled, not now, so
//  input that comes in a burst is all taken before the paint that shows it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RequestPaint()
{
    HWND  hwnd = GetHwnd();



    if (hwnd != nullptr)
    {
        InvalidateRect (hwnd, nullptr, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TakeSnapshot
//
//  A new snapshot from the machine, applied as each frame applies one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::TakeSnapshot (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
{
    m_snapshot = std::move (snapshot);
    ApplySnapshot();
    m_code->ApplyHistory (m_snapshot->history);

    PlaceLateFocus();

    //  At a stop, focus moves from the console to the disassembly unless the
    //  console received the last input; at a stop in a running BASIC program,
    //  to the program pane, where the steps are BASIC's.
    if (m_stopFocus.OnSnapshot (m_snapshot->isPaused) && GetPaneOfFocus() == DebuggerLayout::kConsole)
    {
        FocusPane (m_basicProgram->IsStop() ? std::wstring (DebuggerLayout::kBasicProgram) : DebuggerLayout::GetCodePaneId (m_code->GetActive()));
    }

    //  The drop-downs carry the mode and the panels they were built with, so
    //  they are rebuilt when either changes.
    if (GetMenuState() != m_menuState)
    {
        m_menuState = GetMenuState();
        SetWindowMenus();
        SetConsoleBarMenus();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowMemoryAddress
//
//  A cell clicked on the heat map: the memory window last used goes to its
//  address if it is in view, or else the first that is. With none in view
//  the window last used goes there and its tab comes forward, which puts the
//  heat map behind it when they share a group, as they do by default.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowMemoryAddress (Word address)
{
    MemoryWindowPane  * pane = m_memoryGroup->GetActive();



    if (!pane->GetView()->IsVisible())
    {
        for (MemoryWindowPane * open : m_memoryGroup->GetOpenWindows())
        {
            if (open->GetView()->IsVisible())
            {
                pane = open;
                break;
            }
        }
    }

    pane->GetModel().GoTo (address);

    if (!pane->GetView()->IsVisible())
    {
        m_dockSite->ActivatePane (pane->GetId());
        m_dockSite->Relayout();
        SyncHeatMapRecording();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SyncHeatMapRecording
//
//  The machine records for the heat map only while its pane is open and on
//  screen, not behind another tab; the host is told when that changes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SyncHeatMapRecording()
{
    bool  isWanted        = IsPaneShown (DebuggerLayout::kHeatMap) && m_heatMapPane->GetView()->IsVisible();
    bool  isColumnsWanted = IsAnyHeatColumnOn();



    if (m_host == nullptr)
    {
        return;
    }

    if (isColumnsWanted != m_isHeatColumnsRecording)
    {
        m_isHeatColumnsRecording = isColumnsWanted;
        m_host->SetDebuggerHeatColumnsShown (isColumnsWanted);
    }

    if (isWanted == m_isHeatMapRecording)
    {
        return;
    }

    m_isHeatMapRecording = isWanted;
    m_host->SetDebuggerHeatMapShown (isWanted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsAnyHeatColumnOn
//
//  Show heat in the disassembly views, which the source and BASIC views
//  follow, or Heat in a memory window.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsAnyHeatColumnOn() const
{
    return (m_code != nullptr && m_code->GetOptions().IsOn (DisassemblyOptions::Option::Heat)) ||
           (m_memoryGroup != nullptr && m_memoryGroup->IsAnyHeatOn());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatPalette
//
//  Code in the disassembly's instruction color, reads in the annotation
//  green and writes in the changed red, from the theme's cold gray, over the
//  page the panes are drawn on. The PC is outlined in the disassembly's PC
//  yellow and a breakpoint in its breakpoint red, and a symbol tints toward
//  the symbol color; the rest of the overlays take the theme's heat colors.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::Palette DebuggerWindow::GetHeatPalette() const
{
    DebuggerTextColors::Set  colors  = GetTextColors();
    HeatMapView::Palette     palette;



    palette.background    = (m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000;
    palette.cold          = colors.heatCold;
    palette.execute       = colors.syntax.mnemonic;
    palette.read          = colors.annotation;
    palette.write         = colors.changed;
    palette.pc            = GetPcMarkerArgb();
    palette.stack         = colors.heatStack;
    palette.breakpoint    = GetBreakpointArgb();
    palette.readWatch     = colors.heatReadWatch;
    palette.writeWatch    = colors.heatWriteWatch;
    palette.symbol        = colors.syntax.symbol;
    palette.selfModifying = colors.heatSelfModifying;
    palette.unwritten     = colors.heatUnwritten;
    palette.changed       = colors.heatChanged;

    return palette;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySnapshot
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySnapshot()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    bool                                          restoring = IsRestoringViews();



    m_code->Apply();

    rows.clear();

    UpdateChanges();

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->Apply (*m_snapshot);
    }


    ApplyUndoHistories();
    m_sources->Apply();
    KeepOpenViews();
    ShowPendingPane();

    //  CODE, DATA or CONSOLE: the pane comes forward, even from an edge. The
    //  keys stay where they were, so the next command can be typed; CONSOLE
    //  alone brings them to its command line.
    //  A CODE typed before the window opened is not brought forward again.
    if (m_snapshot->showPaneSerial != m_shownPaneSerial && restoring)
    {
        m_shownPaneSerial = m_snapshot->showPaneSerial;
    }

    if (m_snapshot->showPaneSerial != m_shownPaneSerial)
    {
        m_shownPaneSerial = m_snapshot->showPaneSerial;
        m_dockSite->ActivatePane (m_snapshot->showPane);

        if (m_snapshot->showPane == DebuggerLayout::kConsole && IsRoutable (m_consolePane->GetCommandLine().GetBox()))
        {
            SetFocusedControl (m_consolePane->GetCommandLine().GetBox());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::KeepOpenViews
//
//  The first snapshot reopens whatever was open when Casso last closed; each
//  later one saves the set when it changes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::KeepOpenViews()
{
    std::string                   text;
    DebuggerViewState::OpenViews  views;
    auto                          nameOf = [this] (int fileId)
    {
        std::string  name;

        if (m_snapshot->source.has_value())
        {
            for (const DebugSourceFile & record : m_snapshot->source->files)
            {
                name = (record.id == fileId) ? record.name : name;
            }
        }

        return name;
    };



    if (m_host == nullptr)
    {
        return;
    }

    //  The source documents open, each at the line at its top (FR-113).
    text  = DebuggerViewState::FormatOpenViews (*m_snapshot);
    text += m_sources->FormatOpen (nameOf);
    text += BreakpointColumns::FormatShown (m_breakpointsPane->GetShown());
    text += DebuggerLayout::FormatBasicOpened (m_basicOpened);

    if (!text.empty() && text.front() == ' ')
    {
        text.erase (0, 1);
    }

    if (!m_openViewsRestored)
    {
        m_openViewsRestored = true;
        m_openViewsSaved    = m_host->GetDebuggerOpenViews();
        m_openViewsSettling = m_openViewsSaved.empty() ? 0 : kSettlingSnapshots;
        views               = DebuggerViewState::ParseOpenViews (m_openViewsSaved);
        m_basicOpened       = DebuggerLayout::ParseBasicOpened (m_openViewsSaved);

        m_sources->SetSaved         (SourceDocuments::Parse (m_openViewsSaved));
        m_breakpointsPane->SetShown (BreakpointColumns::ParseShown (m_openViewsSaved));

        for (int view = 1; view < DebuggerViewState::kMaxCodeViews; view++)
        {
            if (views.code[(size_t) view].has_value())
            {
                m_host->SetDebuggerCodeTop (*views.code[(size_t) view], view);
            }
        }

        if (views.follow != 0)
        {
            m_host->SetDebuggerFollowView (views.follow);
        }

        for (int window = 2; window <= DebuggerViewState::kMaxMemoryWindows; window++)
        {
            if (views.memory[(size_t) (window - 1)].has_value())
            {
                m_host->SetDebuggerMemoryWindow (window, views.memory[(size_t) (window - 1)]);
            }
        }

        //  The panels reopen through the window's own action, quietly: the
        //  console shows only what the user did.
        for (const std::string & panel : views.panels)
        {
            DebuggerAction  reopen = DebuggerActions::GetPanel (panel, true, GetMode());

            reopen.quiet = true;
            RunAction (reopen);
        }

        return;
    }

    //  Until the reopened views show up, the set on screen is the one the
    //  window started with, not a choice the user made. Once they have, what
    //  is on screen is taken as the starting point WITHOUT being written: a
    //  view reopened on its address can land a line away from where it was
    //  saved, and writing that back would walk it further every restart.
    if (m_openViewsSettling > 0)
    {
        m_openViewsSettling = (text == m_openViewsSaved) ? 0 : m_openViewsSettling - 1;

        if (m_openViewsSettling == 0)
        {
            m_openViewsSaved = text;
        }

        return;
    }

    if (text != m_openViewsSaved)
    {
        m_openViewsSaved = text;
        m_host->SetDebuggerOpenViews (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetUndoLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetUndoLabel (bool redo, const std::wstring & text)
{
    std::wstring  verb = redo ? L"Redo" : L"Undo";



    return text.empty() ? verb : verb + L" \"" + text + L"\"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UpdateChanges
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UpdateChanges()
{
    StopChanges::Values  values;



    for (const DebuggerViewSnapshot::RegisterRow & reg : m_snapshot->registers)
    {
        values["R:" + reg.name] = reg.value;
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : m_snapshot->watches)
    {
        values[std::format ("W:{}", watch.id)] = watch.value;
    }

    for (const DebuggerViewSnapshot::AutoWatchLine & line : m_snapshot->autoWatches)
    {
        values["A:" + line.key] = line.value;
    }

    for (const auto & entry : m_snapshot->stack)
    {
        values[std::format ("S:{:04X}", entry.address)] = std::format ("{:02X}", entry.value);
    }

    m_stopChanges.Update (m_snapshot->isPaused, std::move (values));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsRestoringViews
//
//  True from the window's first snapshot until the views it reopens have
//  settled: what opens then was open when the window closed, so it does not
//  take the front of its tab group from the tab saved there.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsRestoringViews() const
{
    return !m_openViewsRestored || m_openViewsSettling > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AppendConsole
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AppendConsole (const std::vector<std::string> & lines)
{
    m_consolePane->GetOutput().Append (lines);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetRegisterByte
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Byte> DebuggerWindow::GetRegisterByte (const std::string & name) const
{
    Word  value = 0;



    for (const DebuggerViewSnapshot::RegisterRow & reg : m_snapshot->registers)
    {
        if (reg.name == name && !DebuggerViewState::IsReadOnlyRegister (name) && TextDialog::TryParseHexWord (Widen (reg.value), value) && value <= 0xFF)
        {
            return (Byte) value;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsAnyMenuOpen
//
//  Whether the menu bar, a toolbar's drop-down or a context menu is open,
//  in this window or a floating one.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsAnyMenuOpen() const
{
    DxuiHwndSource  * popups = GetPopupHost();
    bool              open   = false;



    open = (m_menuBar != nullptr && m_menuBar->IsOpen()) || (popups != nullptr && popups->GetContextMenu().IsVisible());

    for (const DxuiToolbar * bar : { m_commandBar })
    {
        open = open || (bar != nullptr && bar->IsMenuOpen());
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        for (const DxuiToolbar * bar : pane->GetBars())
        {
            open = open || (bar != nullptr && bar->IsMenuOpen());
        }
    }

    open = open || m_floating->IsAnyMenuOpen();

    return open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UpdateTooltip
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UpdateTooltip (POINT clientPx)
{
    m_tips->Update (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCommandBarTip
//
//  Only for the window the command bar is in.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DebuggerWindow::GetCommandBarTip (POINT clientPx, RECT & anchor) const
{
    bool  isBarWindow = m_floating->GetRoutingPane() == (m_barHost.IsFloating() ? std::wstring (kBarFloatKey) : std::wstring());



    if (!isBarWindow || m_commandBar == nullptr || !m_commandBar->IsVisible())
    {
        return nullptr;
    }

    return m_commandBar->GetTooltipAt (clientPx.x, clientPx.y, anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FindOperandTipView
//
//  The disassembly view under a point in the window the event being routed
//  came from, or -1: one slid out over the panes ahead of any under it.
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::FindOperandTipView (POINT clientPx) const
{
    std::wstring  slid   = (m_floating->GetRoutingPane().empty() && m_dockSite != nullptr) ? m_dockSite->GetSlidPane() : std::wstring();
    int           found  = -1;
    bool          isSlid = false;



    for (int each = 0; each < DebuggerViewState::kMaxCodeViews && !isSlid; each++)
    {
        DxuiListView  * code   = m_code->GetCodeList (each);
        bool            isOver = code != nullptr && IsRoutable (code) && code->IsVisible() && DxuiDockSite::Contains (code->GetBounds(), clientPx);

        if (isOver)
        {
            found  = each;
            isSlid = !slid.empty() && slid == DebuggerLayout::GetCodePaneId (each);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryFindOperandTipRow
//
//  The disassembly view and row whose operand and result lie under a point.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryFindOperandTipRow (
    POINT    clientPx,
    int    & view,
    int    & row) const
{
    HRESULT          hr     = S_OK;
    DxuiListView   * list   = nullptr;
    RECT             bounds = {};
    bool             isOver = false;



    view = FindOperandTipView (clientPx);
    row  = -1;
    list = (view >= 0) ? m_code->GetCodeList (view) : nullptr;
    CBR (list != nullptr);

    bounds = list->GetBounds();
    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    isOver = row >= 0 && GetColumnAt (list, clientPx.x - bounds.left) == (int) kCodeOperandColumn;

Error:
    return isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsOperandTipCovered
//
//  Whether anything lying over the disassembly meets the tip or the pointer:
//  the zoom popup, another pane slid out over it, a color key held open, a
//  find widget, or a window lying over this one, such as a floating pane,
//  command bar or timeline. The tip is a window of its own, shown over
//  everything drawn in this one and over the windows it owns, so it would
//  show over each of them; and a pointer on one of them is on it rather than
//  on the cell, though a color key lets it through to the panes.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsOperandTipCovered (
    int                       view,
    POINT                     clientPx,
    const RECT              & tipRect,
    const OperandTipScreen  & screen) const
{
    bool  isMain    = m_floating->GetRoutingPane().empty();
    bool  isCovered = false;
    RECT  pinned    = {};



    //  The zoom popup and a slid pane lie over this window's panes only.
    isCovered = isMain && m_zoomOpen && Meets (m_zoomRect, clientPx, tipRect);

    isCovered = isCovered || (isMain && m_dockSite != nullptr && !m_dockSite->GetSlidPane().empty() &&
                              m_dockSite->GetSlidPane() != DebuggerLayout::GetCodePaneId (view) &&
                              Meets (m_dockSite->GetSlidRect(), clientPx, tipRect));

    isCovered = isCovered || (m_colorKeys->TryGetPinnedRect (pinned) && Meets (pinned, clientPx, tipRect));

    isCovered = isCovered || m_finds->Overlaps (clientPx, tipRect);

    for (const RECT & over : screen.overWindows)
    {
        isCovered = isCovered || Meets (over, clientPx, tipRect);
    }

    return isCovered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::Meets
//
//  Whether something lying at `over` holds the pointer or overlaps the tip.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::Meets (
    const RECT  & over,
    POINT         clientPx,
    const RECT  & tipRect)
{
    RECT  both = {};



    return DxuiDockSite::Contains (over, clientPx) || IntersectRect (&both, &over, &tipRect) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetChromeTipAt
//
//  A tab's tip in this window, then a pane's title-bar buttons from the
//  dock site of the window the mouse is in: this one's, or the floating
//  window's the event was routed from.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetChromeTipAt (POINT clientPx, RECT & anchor) const
{
    const DxuiDockSite  * site = m_floating->GetRoutingPane().empty() ? m_dockSite : m_floating->FindSite (m_floating->GetRoutingPane());
    std::wstring          text;



    if (m_floating->GetRoutingPane().empty() && m_dockSite != nullptr && !m_dockSite->GetTabAt (clientPx, anchor, text).empty() && !text.empty())
    {
        return text;
    }

    if (site == nullptr)
    {
        return std::wstring();
    }

    return site->GetTitleButtonTipAt (clientPx, anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTipLists
//
//  The lists in the window the event being routed came from.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> DebuggerWindow::GetTipLists() const
{
    std::vector<DxuiListView *>  lists;



    for (DxuiListView * each : GetLists())
    {
        if (each != nullptr && IsRoutable (each))
        {
            lists.push_back (each);
        }
    }

    return lists;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetGraphicTip
//
//  Over a device panel's disk head, what its colors mean; over the status
//  bar's history section, what its meter's do.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetGraphicTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    if (m_floating->GetRoutingPane().empty() && m_statusBar != nullptr && m_statusBar->IsVisible() &&
        m_statusBar->FindFieldAt (clientPx) == (int) kStatusHistory &&
        DebuggerStatusText::GetHistoryFill (m_snapshot != nullptr ? m_snapshot->history : HistoryStatus()) >= 0.0f)
    {
        anchor = m_statusBar->GetFieldRect (kStatusHistory);
        text   = ColorLegend::GetHistoryMeterTip();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetColorPalette
//
//  Every color the legend shows, as the panes draw them in the theme in
//  force.
//
////////////////////////////////////////////////////////////////////////////////

ColorLegend::Palette DebuggerWindow::GetColorPalette() const
{
    ColorLegend::Palette  palette;



    palette.text       = GetTextColors();
    palette.background = (m_theme != nullptr) ? m_theme->ContentBackground()  : 0xFF000000;
    palette.muted      = (m_theme != nullptr) ? m_theme->ForegroundMuted()    : 0xFF808080;
    palette.disabled   = (m_theme != nullptr) ? m_theme->ForegroundDisabled() : 0xFF808080;
    palette.accent     = (m_theme != nullptr) ? m_theme->Accent()             : 0xFF3C8CE6;
    palette.flash      = (m_theme != nullptr) ? DiskHeadView::GetFlashColor (*m_theme) : 0xFFFFFFFF;
    palette.pcMarker   = GetPcMarkerArgb();
    palette.breakpoint = GetBreakpointArgb();
    palette.meterEmpty = DebuggerStatusText::kEmptyArgb;
    palette.meterFull  = DebuggerStatusText::kFullArgb;

    return palette;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsDarkTheme
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsDarkTheme() const
{
    uint32_t  bg    = (m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000;
    uint32_t  luma  = ((bg >> 16) & 0xFF) * 299 + ((bg >> 8) & 0xFF) * 587 + (bg & 0xFF) * 114;



    return luma < 128 * 1000;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBreakpointArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetBreakpointArgb() const
{
    //  A warm red that holds its own against a blue ground, as Visual Studio
    //  Code's breakpoint red does; a pure red goes purple beside blue.
    return IsDarkTheme() ? 0xFFF4524D : 0xFFD1242F;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBreakpointIcon
//
//  The breakpoint's dot, filled when enabled and a ring when not. Rebuilt
//  when the theme changes the color.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerWindow::GetBreakpointIcon (bool enabled)
{
    uint32_t  argb = GetBreakpointArgb();



    if (argb != m_breakpointIconArgb || m_breakpointIcons[0] == nullptr)
    {
        m_breakpointIconArgb = argb;
        m_breakpointIcons[0] = GutterGlyph::MakeDot (argb, false);
        m_breakpointIcons[1] = GutterGlyph::MakeDot (argb, true);
    }

    return m_breakpointIcons[enabled ? 1 : 0];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHoverBreakpointIcon
//
//  The gray dot under the pointer in the breakpoint column, in the theme's
//  muted text color, where a click would set a breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerWindow::GetHoverBreakpointIcon()
{
    uint32_t  argb = (m_theme != nullptr) ? m_theme->ForegroundMuted() : 0xFF808080;



    if (argb != m_hoverBreakpointArgb || m_hoverBreakpointIcon == nullptr)
    {
        m_hoverBreakpointArgb = argb;
        m_hoverBreakpointIcon = GutterGlyph::MakeDot (argb, true);
    }

    return m_hoverBreakpointIcon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPcIcon
//
//  The PC's arrow in the glyph margin, over the line's dot when it has one,
//  as Visual Studio draws its arrow over a breakpoint. A dot not seen before
//  gets its own image, and a few are kept: the enabled and disabled dots,
//  the gray one under the pointer, and the arrow alone; one more means a
//  dot from an earlier theme, so the old ones are dropped.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerWindow::GetPcIcon (const std::shared_ptr<const DxuiIconImage> & dot)
{
    constexpr size_t                      kMaxPcIcons = 4;
    uint32_t                              argb        = GetPcMarkerArgb();
    std::shared_ptr<const DxuiIconImage>  icon;



    if (argb != m_pcIconArgb)
    {
        m_pcIconArgb = argb;
        m_pcIcons.clear();
    }

    for (const PcIcon & each : m_pcIcons)
    {
        icon = (each.dot == dot) ? each.icon : icon;
    }

    if (icon == nullptr)
    {
        if (m_pcIcons.size() >= kMaxPcIcons)
        {
            m_pcIcons.clear();
        }

        icon = GutterGlyph::MakeMarked (dot, GutterGlyph::MakeArrow (argb));

        m_pcIcons.push_back ({ dot, icon });
    }

    return icon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPcMarkerArgb
//
//  Visual Studio's yellow, which reads on either background.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetPcMarkerArgb() const
{
    return DisassemblyCells::GetPcMarkerArgb ((m_theme != nullptr) ? m_theme->ContentBackground() : 0xFF000000);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPcRowArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetPcRowArgb() const
{
    return GetTextColors().pcRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetNavigatedRowArgb
//
//  Visual Studio's green for a frame a call stack brought into view.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetNavigatedRowArgb() const
{
    return GetTextColors().navigatedRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTargetRowArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetTargetRowArgb() const
{
    return GetTextColors().targetRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTextColors
//
//  Every text color the panes draw, made readable against the theme's
//  content background, with every color of the theme's own: its ROM, I/O and
//  memory map colors among them.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerTextColors::Set DebuggerWindow::GetTextColors() const
{
    if (m_theme != nullptr)
    {
        return m_textColors.GetFor (*m_theme);
    }

    return DebuggerTextColors::Make (0xFF000000, 0xFFFFFFFF, 0xFFC0C0C0, 0u);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSyntaxColors
//
//  Visual Studio's code colors for the theme's darkness; see DebuggerTextColors.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Colors DebuggerWindow::GetSyntaxColors() const
{
    return GetTextColors().syntax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetResultArgb
//
//  The theme's color for an operand's result; a theme that gives none gets a
//  cyan for its darkness.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetResultArgb() const
{
    return GetTextColors().result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetAnnotationArgb
//
//  The comment green of Visual Studio's editor.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetAnnotationArgb() const
{
    return GetTextColors().annotation;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetChangedArgb
//
//  Visual Studio's red for a value that changed since the last stop.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetChangedArgb() const
{
    return GetTextColors().changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ForwardToList
//
//  A list takes coordinates relative to itself.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ForwardToList (DxuiListView * list, const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  local  = ev;
    RECT            bounds = list->GetBounds();



    local.positionDip = { ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top };
    return list->OnMouse (local);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OfferPress
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OfferPress (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled)
{
    if (!handled && control != nullptr && IsRoutable (control) && control->IsVisible() && control->OnMouse (ev))
    {
        SetFocusedControl (control);
        handled = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteCommandBarMouse
//
//  The strip takes the pointer over itself and everything while one of its
//  menus is open, so a click meant for a menu row never reaches the pane
//  behind it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteCommandBarMouse (const DxuiMouseEvent & ev)
{
    int   x    = ev.positionDip.x;
    int   y    = ev.positionDip.y;
    RECT  bars = m_commandBar->GetBounds();
    bool  over = x >= bars.left && x < bars.right && y >= bars.top && y < bars.bottom;



    if (!over && !m_commandBar->IsMenuOpen())
    {
        m_commandBar->OnToolbarMouseLeave();
        return false;
    }

    //  The tip follows the hover and goes the moment a press opens a menu,
    //  as Casso Explorer's toolbar tips do.
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        UpdateTooltip (ev.positionDip);
        return m_commandBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        GetRoutedTooltip().HideImmediate();
        return m_commandBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_commandBar->OnToolbarLButtonUp (x, y);

    default:
        return m_commandBar->IsMenuOpen();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureCommandBarHost
//
//  The command bar docks and floats through a Dxui toolbar host; the
//  debugger supplies its preferences, its tooltips and its frames.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureCommandBarHost()
{
    m_barHost.Attach (this, m_commandBar, m_dockSite, m_hInstance);

    m_barHost.SetOnLayout     ([this] { LayoutWidgets(); });
    m_barHost.SetOnFloatMouse ([this] (const DxuiMouseEvent & ev) { return RouteFloatingBarMouse (ev); });
    m_barHost.SetOnDragStart  ([this] { GetRoutedTooltip().HideImmediate(); });

    m_barHost.SetOnSave ([this] (const std::wstring & text)
    {
        if (m_host != nullptr)
        {
            m_host->SetDebuggerCommandBarDock (SourcePathList::WideToUtf8 (text));
        }
    });

    m_barHost.SetOnFloatCreated ([this] (DxuiToolbarWindow & window)
    {
        std::unique_ptr<DxuiTooltip>  tip = std::make_unique<DxuiTooltip>();

        window.SetTheme  (m_theme);
        window.SetKeyMap (&DebuggerKeySchemes::GetMap (m_keyScheme));

        tip->SetPopupHost (window.GetPopupHost());
        tip->SetTheme     (*m_theme);
        tip->SetDpi       (GetDpiForWindow (window.GetHwnd()));
        tip->SetMonospace (false);
        tip->ApplyVisualStudioLook();
        m_floating->SetTip (kBarFloatKey, std::move (tip));
    });

    m_barHost.SetOnFloatChanged ([this]
    {
        if (!m_barHost.IsFloating())
        {
            m_floating->EraseTip (kBarFloatKey);
        }

        m_focusMgr.Rebuild();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SyncCommandBarFloatTip
//
//  Once a frame, while the command bar floats: its window's tooltip sized
//  to that window, and ticked.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SyncCommandBarFloatTip()
{
    DxuiToolbarWindow  * window = m_barHost.GetFloatWindow();
    DxuiTooltip        * tip    = m_floating->FindTip (kBarFloatKey);
    RECT                 client = {};



    if (window == nullptr || tip == nullptr)
    {
        return;
    }

    if (GetClientRect (window->GetHwnd(), &client))
    {
        tip->SetDpi          (GetDpiForWindow (window->GetHwnd()));
        tip->SetViewportSize (client.right - client.left, client.bottom - client.top);
    }

    if (tip->WantsTick())
    {
        tip->Tick ((int64_t) GetTickCount64());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteFloatingBarMouse
//
//  Input over the floating bar's window, less its grab handle, which that
//  window takes itself, routed as the docked bar's is, with the window's
//  own tooltip.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteFloatingBarMouse (const DxuiMouseEvent & ev)
{
    FloatingPanes::RouteScope  scope (*m_floating, kBarFloatKey);



    return RouteCommandBarMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnMouse (const DxuiMouseEvent & ev)
{
    int           x       = ev.positionDip.x;
    int           y       = ev.positionDip.y;
    bool          lbDown  = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;
    bool          handled = false;
    std::wstring  pane;



    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_openingFocus.OnUserInput();
        m_stopFocus.OnInput ((m_floating->GetRoutingPane().empty() ? m_dockSite->GetPaneAt (ev.positionDip) : m_floating->GetRoutingPane()) == DebuggerLayout::kConsole);
        m_lastPressPx = POINT { x, y };
    }

    //  A pane's drag takes every event until the button comes up, wherever
    //  the pointer is, so the menu bar, toolbars and status bar it crosses
    //  neither take its moves nor show their tips. Off the panes, its cross
    //  and its preview go at once.
    if (m_floating->GetRoutingPane().empty() && m_dockSite->HasPointerDrag())
    {
        GetRoutedTooltip().HideImmediate();
        (void) m_dockSite->OnMouse (ev);
        return true;
    }

    //  A disassembly row's operand and result that the pane cuts off show
    //  whole over their cell while the pointer is on it. This runs ahead of
    //  everything that lies over the panes, so IsOperandTipCovered tests for
    //  each of them. A move event records whether the left button was down
    //  when the pointer moved; that, not the button's state at this moment,
    //  marks a drag over the cell.
    m_operandTips->Update (ev, ev.button == DxuiMouseButton::Left);

    //  A pane's own box being edited, such as a breakpoint's name, a watch, a
    //  register, a heat map range or its set's name.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteEditMouse (ev))
        {
            return true;
        }
    }

    //  A pane's own use of the mouse ahead of every other control, such as
    //  Ctrl+wheel over the heat map, which zooms the map rather than the text.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteEarlyMouse (ev))
        {
            return true;
        }
    }

    //  Ctrl+wheel sizes the panes' text as Ctrl+Plus and Ctrl+Minus do, over
    //  any pane, before a view that would take the wheel for itself.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal && ev.wheelDelta != 0.0f)
    {
        StepTextZoom (ev.wheelDelta > 0.0f ? 1 : -1);
        return true;
    }

    //  The status bar's zoom popup lies over everything while it is open, and
    //  the status bar owns its band.
    if (m_floating->GetRoutingPane().empty() && RouteStatusBarMouse (ev))
    {
        return true;
    }

    //  The menu bar first, then the command bar: each owns its strip and
    //  whatever menu it has open.
    if (m_floating->GetRoutingPane().empty() && RouteMenuBarMouse (ev))
    {
        return true;
    }

    if (m_floating->GetRoutingPane().empty() && !m_timelineHost.IsFloating() && RouteTimelineMouse (ev))
    {
        return true;
    }

    if (m_floating->GetRoutingPane().empty() && !m_barHost.IsFloating() && RouteCommandBarMouse (ev))
    {
        return true;
    }

    //  The panes' info buttons first: they sit at the ends of the toolbars.
    if (m_colorKeys->RouteMouse (ev))
    {
        return true;
    }

    //  Then each pane's on the pane interface.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteBarMouse (ev))
        {
            return true;
        }
    }

    //  A press on a disassembly or source tab sets the step mode as a press
    //  inside that pane does.
    if (m_floating->GetRoutingPane().empty() && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        NoteTabFocus (ev.positionDip);
    }

    //  A click on a branch arrow goes to where it points, and the next back.
    if (m_floating->GetRoutingPane().empty() && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left &&
        ClickBranchArrow (ev.positionDip))
    {
        return true;
    }

    //  Then the site: its strips, its sashes and a drag in progress lie over
    //  the panes. A tip up as a drag starts goes, as none shows during one.
    if (m_floating->GetRoutingPane().empty() && m_dockSite->OnMouse (ev))
    {
        if (m_dockSite->IsDragging())
        {
            GetRoutedTooltip().HideImmediate();
        }

        return true;
    }

    //  A pane slid out from an edge lies over others, so its area is its own.
    if (m_floating->GetRoutingPane().empty() && !m_dockSite->GetSlidPane().empty() && ev.kind != DxuiMouseEventKind::Move &&
        DxuiDockSite::Contains (m_dockSite->GetSlidRect(), ev.positionDip))
    {
        return m_floating->RouteMouse (m_dockSite->GetSlidPane(), ev);
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        pane = m_floating->GetRoutingPane().empty() ? m_dockSite->GetPaneAt (ev.positionDip) : m_floating->GetRoutingPane();

        //  Over a pane's content, the content's own menu (FR-084); over its
        //  tab or title, the menu of what can be done with the pane.
        if (!pane.empty() && !ShowContentMenu (pane, ev.positionDip))
        {
            ShowDockToMenu (pane, ev.positionDip);
        }

        return true;
    }

    //  The find widget floats over its pane's text, so a press on it is the
    //  widget's, not the text's.
    if (!m_finds->IsOverPlate (ev.positionDip) &&
        (m_sources->RouteMouse (ev) || m_consolePane->GetOutput().RouteMouse (ev, m_consolePane->GetCommandLine().GetBox())))
    {
        return true;
    }

    //  What a pane draws, such as the heat map and the flame graph.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteContentMouse (ev))
        {
            return true;
        }
    }

    for (DxuiListView * list : GetLists())
    {
        if (IsRoutable (list) && list->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
        {
            ForwardToList (list, ev);

            //  A click on a register's value edits it once the click is
            //  over, so the list is not left mid-press under the box.
            if (ev.kind == DxuiMouseEventKind::Up)
            {
                NoteListReleased (list, ev.positionDip);
            }

            return true;
        }
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        //  A floating window's tips show from its own tooltip.
        UpdateTooltip (ev.positionDip);
        m_gutter->Hover (ev.positionDip);
        m_gutter->Drag  (ev.positionDip);

        if (IsRoutable (m_consolePane->GetCommandLine().GetBox()))
        {
            m_consolePane->GetCommandLine().GetBox()->SetMouseHover (x, y);
        }

        for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
        {
            for (DxuiTextInput * box : each->GetTextBoxes())
            {
                if (IsRoutable (box))
                {
                    box->SetMouseHover (x, y);
                }
            }
        }

        //  Each find widget moves with its pane.
        m_finds->OnMouseMove (ev, lbDown);

        return true;

    case DxuiMouseEventKind::Down:
        if (ev.button != DxuiMouseButton::Left)
        {
            return true;
        }

        if (m_gutter->Click (ev))
        {
            return true;
        }

        for (IDxuiControl * control : GetPressTargets())
        {
            OfferPress (control, ev, handled);
        }

        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (!handled && IsRoutable (list) && list->IsVisible() && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                handled = ForwardToList (list, ev);
                SetFocusedControl (list);
                handled = true;

                if (m_code->GetViewOf (list) >= 0)
                {
                    m_code->SetActive (m_code->GetViewOf (list));
                    NoteViewFocus (false);
                }
            }
        }

        return true;

    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left && m_gutter->Drop (ev))
        {
            return true;
        }

        //  Only to what is shown: the disassembly views' radios share a rect,
        //  and a press on the one in front must not reach those behind it.
        for (IDxuiControl * control : GetPressTargets())
        {
            if (IsRoutable (control) && control->IsVisible())
            {
                control->OnMouse (ev);
            }
        }

        //  A release goes to the list UNDER THE POINTER, the same as a press. A
        //  list mid-drag already had it above, whatever the pointer is over.
        //  Sending it to every list reached the ones hidden behind a tab as
        //  well: the Stack and Call Stack panes share a rect, so a click on the
        //  fifth Stack row released onto the fifth Call Stack row -- which
        //  activated it and moved the code pane to that frame's call site.
        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (IsRoutable (list) && list->IsVisible() &&
                x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                ForwardToList (list, ev);
                NoteListReleased (list, ev.positionDip);
            }
        }

        return true;

    case DxuiMouseEventKind::Wheel:
        //  The code pane holds only the lines it shows, so the wheel scrolls
        //  the disassembly itself, through all of memory (FR-073).
        for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
        {
            DxuiListView  * code = m_code->GetCodeList (view);

            if (IsRoutable (code) && code->IsVisible() && DxuiDockSite::Contains (code->GetBounds(), ev.positionDip) && m_host != nullptr)
            {
                m_host->ScrollDebuggerCode ((int) std::lround (-ev.wheelDelta * (float) code->GetWheelLinesPerNotch()), view);
                return true;
            }
        }

        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (IsRoutable (list) && list->IsVisible() && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                ForwardToList (list, ev);
            }
        }

        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneControls
//
//  The window's children that make up a pane, which move together when it
//  floats.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetPaneControls (const std::wstring & pane) const
{
    std::vector<IDxuiControl *>  controls = GetOwnControls (pane);
    ColorKeyButton             * key      = GetColorKey (pane);



    if (key != nullptr && !controls.empty())
    {
        controls.push_back (key);
    }

    return controls;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetOwnControls
//
//  A pane's controls less its info button.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetOwnControls (const std::wstring & pane) const
{

    if (FindPane (pane) != nullptr)
    {
        return FindPane (pane)->GetControls();
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneContent
//
//  The one control a dock site places for a pane: its frame, or the pane's
//  only control.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DebuggerWindow::GetPaneContent (const std::wstring & pane) const
{
    std::vector<IDxuiControl *>  controls;



    if (FindPane (pane) != nullptr)
    {
        return FindPane (pane)->GetContent();
    }

    controls = GetPaneControls (pane);
    return controls.empty() ? nullptr : controls.front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneTitle (const std::wstring & pane) const
{
    if (pane == DebuggerLayout::kCode)
    {
        return L"Disassembly";
    }

    return (FindPane (pane) != nullptr) ? FindPane (pane)->GetTitle() : pane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneOfControl
//
//  Empty for a control that belongs to no pane: the toolbar, which never
//  leaves this window. The memory bar and its box belong to the memory pane
//  they sit in.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneOfControl (const IDxuiControl * control) const
{
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        std::wstring  shared = each->GetPaneOfSharedControl (control);



        if (!shared.empty())
        {
            return shared;
        }
    }

    //  A find widget is the pane's it searches, and goes with it.
    if (!m_finds->GetPaneOfControl (control).empty())
    {
        return m_finds->GetPaneOfControl (control);
    }

    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        for (const IDxuiControl * part : GetPaneControls (each->GetId()))
        {
            if (part == control)
            {
                return each->GetId();
            }
        }
    }

    for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
    {
        for (const IDxuiControl * part : GetPaneControls (pane))
        {
            if (part == control)
            {
                return pane;
            }
        }
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsRoutable
//
//  Whether an event now being routed may reach a control: an event from this
//  window reaches the controls still here, one from a floating window only
//  the controls of its pane.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsRoutable (const IDxuiControl * control) const
{
    return m_floating->IsRoutable (GetPaneOfControl (control));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFocused
//
//  A floating window keeps its own focus: the control last pressed in it.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DebuggerWindow::GetFocused() const
{
    return m_floating->IsRoutedToFloat() ? m_floating->GetRoutedFocus() : m_focusMgr.GetFocusedControl();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetFocusedControl (IDxuiControl * control)
{
    if (!m_floating->IsRoutedToFloat())
    {
        m_focusMgr.SetFocused (control);
        return;
    }

    m_floating->SetRoutedFocus (control);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetRoutingHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND DebuggerWindow::GetRoutingHwnd() const
{
    HWND  hwnd = m_floating->GetRoutedHwnd();



    return (hwnd != nullptr) ? hwnd : GetHwnd();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMonitors
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPaneLayout::Monitor> DebuggerWindow::GetMonitors()
{
    std::vector<DxuiPaneLayout::Monitor>  monitors;



    EnumDisplayMonitors (nullptr, nullptr,
        [] (HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL
        {
            MONITORINFOEXW            info = {};
            DxuiPaneLayout::Monitor   entry;

            info.cbSize = sizeof (info);

            if (GetMonitorInfoW (monitor, &info))
            {
                entry.key       = info.szDevice;
                entry.workDip   = info.rcWork;
                entry.isPrimary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
                reinterpret_cast<std::vector<DxuiPaneLayout::Monitor> *> (data)->push_back (entry);
            }

            return TRUE;
        },
        reinterpret_cast<LPARAM> (&monitors));

    return monitors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFloatsSynced
//
//  The floating windows match the layout again: the focus moves to a
//  control that an event can be routed to.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnFloatsSynced()
{
    IDxuiControl  * focused = nullptr;



    m_dockSite->Relayout();
    m_focusMgr.Rebuild();

    focused = m_focusMgr.GetFocusedControl();

    if (focused == nullptr || !IsRoutable (focused))
    {
        m_focusMgr.SetFocused (IsRoutable (m_consolePane->GetCommandLine().GetBox()) ? (IDxuiControl *) m_consolePane->GetCommandLine().GetBox() : m_code->GetCodeList (0));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFloatFocusChanged
//
//  The accent is drawn on the window that got the focus last, so it remains
//  on a float when another application gets the focus.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnFloatFocusChanged (const std::wstring & pane, bool focused)
{
    m_accentOwner.OnFloatFocusChanged (pane, focused);
    ShowFocusAccent();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnWindowFocusChanged
//
//  The window, main or floating, that took the keyboard focus last shows the
//  accent (see FocusAccentOwner): this one taking it takes the accent back
//  from a floating pane. Losing it to another application changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnWindowFocusChanged (bool focused)
{
    //  Input in another window comes next, so the console did not receive
    //  the last of it; a click or key routed back from a floating console
    //  marks it again.
    if (!focused)
    {
        m_stopFocus.OnInput (false);

        //  The pointer can move off the timeline while another window is
        //  active, and the strip then gets no OnMouseLeave, so it is called
        //  here.
        m_timelineStrip.OnMouseLeave();
    }

    m_accentOwner.OnMainFocusChanged (focused);
    ShowFocusAccent();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowFocusAccent
//
//  Of this window and the floating ones, the one that took the focus last
//  shows the accent. The focus messages run this as well as each frame, so
//  the window losing the accent repaints without it in the same message
//  rather than at its next frame paint, up to kIdlePaintMs later.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowFocusAccent()
{
    if (m_dockSite == nullptr || m_theme == nullptr)
    {
        return;
    }

    m_dockSite->SetFocusedPane (m_accentOwner.GetFocusedFloat().empty() ? GetPaneOfFocus() : std::wstring());

    m_floating->SetFocusedLooks (m_accentOwner.GetFocusedFloat(), *m_theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CloseFloatingPane
//
//  The pane closes, keeping its floating place, so the View menu opens it
//  there again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CloseFloatingPane (const std::wstring & pane)
{
    ClosePane (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneLayout
//
////////////////////////////////////////////////////////////////////////////////

const DxuiPaneLayout & DebuggerWindow::GetPaneLayout() const
{
    return m_dockSite->GetPaneLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditPaneLayout
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayout & DebuggerWindow::EditPaneLayout()
{
    return m_dockSite->EditPaneLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFloatDocked
//
//  A pane's floating window is about to go, its controls back in this one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnFloatDocked (const std::wstring & pane, DxuiDockedWindow * window)
{
    m_accentOwner.OnFloatDocked (pane);

    //  And an operand tip up in it.
    m_operandTips->OnPopupHostClosed (window->GetPopupHost());

    //  A control several panes share, as the memory bar is, may be sitting
    //  in this pane, and would go with the window.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        each->OnFloatClosed (window);
    }

    //  And each find widget, in the pane it searches.
    m_finds->OnFloatClosed (window);

    //  The site's strips and drop zones are drawn over the panes, so the site
    //  is the last child.
    {
        std::unique_ptr<IDxuiControl>  site = DetachChild (m_dockSite);

        (void) AttachChild (std::move (site));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveLayout
//
//  Each floating window's place is read back first, since moving or sizing
//  one is not a layout operation.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveLayout()
{
    DxuiPaneLayout &  layout = m_dockSite->EditPaneLayout();



    m_floating->SaveRects (layout);

    if (m_host != nullptr)
    {
        m_host->SetDebuggerLayout      (SourcePathList::WideToUtf8 (layout.ToText()));
        m_host->SetDebuggerClosedPanes (SourcePathList::WideToUtf8 (DebuggerLayout::ClosedPanesToText (m_closedPanes)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySavedPlacement
//
//  The window opens where the user last left it on this monitor arrangement.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySavedPlacement()
{
    RECT  visible = {};
    RECT  placed  = {};



    if (m_host == nullptr || !m_host->TryGetDebuggerPlacement (visible))
    {
        WindowTrace::Log ("restore.miss", "debugger", "nothing saved for this monitor arrangement");
        return;
    }

    //  What was saved is the frame the user sees. The window rect around it
    //  is this window's own invisible border wider, and only this window can
    //  say how wide that is.
    placed = DxuiWindowFrame::ToWindowRect (GetHwnd(), visible);

    WindowTrace::LogRect ("restore.hit", "debugger", visible, "the visible rect that was saved");

    //  TWICE, ON PURPOSE. Landing on a monitor of a different scale makes
    //  Windows send WM_DPICHANGED and resize the window by the ratio of the
    //  two: a rect saved at 120dpi came back on a 144dpi screen exactly 1.2x
    //  too big. The first call takes that rescale, and the border is measured
    //  again afterwards because it scales with the window.

    SetWindowPos (GetHwnd(), nullptr, placed.left, placed.top,
                  placed.right - placed.left, placed.bottom - placed.top,
                  SWP_NOZORDER | SWP_NOACTIVATE);

    placed = DxuiWindowFrame::ToWindowRect (GetHwnd(), visible);

    SetWindowPos (GetHwnd(), nullptr, placed.left, placed.top,
                  placed.right - placed.left, placed.bottom - placed.top,
                  SWP_NOZORDER | SWP_NOACTIVATE);

    WindowTrace::LogWindow ("restore.applied", "debugger", GetHwnd(), "window rect after placing");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnWindowPlaced
//
//  The user finished dragging or sizing the window, so where it is now is
//  where it should open next time. A close catches the moves that never
//  reach this hook.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnWindowPlaced()
{
    SavePlacementIfMoved();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SavePlacementIfMoved
//
//  A placement is written only when the window is somewhere other than where
//  it opened. That keeps the rule the user asked for -- two instances fight
//  over the file only if both were moved -- and it holds for the ways a
//  window moves WITHOUT the OS drag loop, which is the only thing
//  OnWindowPlaced can see: a snap from the keyboard, or an arrangement made
//  and then closed.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SavePlacementIfMoved()
{
    RECT  rect = {};



    //  Until the window has been shown and its opening rect taken, the size
    //  and move events of its own creation have nothing to compare against.
    //  A minimized window is parked far off the desktop -- x=-32000 -- and
    //  that is not a placement anyone chose. The log caught one being
    //  written as the arrangement for the whole monitor set.
    if (!m_placed || m_host == nullptr || !IsCreated() || IsIconic (GetHwnd()))
    {
        WindowTrace::Log ("save.skipped", "debugger", "not placed yet, or no host");
        return;
    }

    rect = DxuiWindowFrame::GetVisibleRect (GetHwnd());

    if (!WindowPlacementProfile::IsPlaceableRect (rect))
    {
        WindowTrace::LogRect ("save.refused", "debugger", rect, "not a rect anyone put a window at");
        return;
    }

    if (EqualRect (&rect, &m_openedRect))
    {
        WindowTrace::LogRect ("save.unmoved", "debugger", rect, "where it opened, so the file is left alone");
        return;
    }

    WindowTrace::LogRect ("save", "debugger", rect, "opened at x=" + std::to_string (m_openedRect.left) +
                          " y=" + std::to_string (m_openedRect.top));

    m_openedRect = rect;
    m_host->SetDebuggerPlacement (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteDockKey
//
//  Docking from the keyboard (FR-042): Shift+F10 or the menu key opens Dock To
//  for the focused pane, and Alt+Shift with an arrow moves it into the group
//  that way.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteDockKey (const DxuiKeyEvent & ev)
{
    std::wstring     pane    = GetPaneOfFocus();
    IDxuiControl   * focused = GetFocused();
    RECT             bounds  = {};
    DxuiDockSide     side    = DxuiDockSide::Left;



    if (pane.empty())
    {
        return false;
    }

    //  The menu opens at the focused control of the window the key came from,
    //  a floating pane's own when it came from one.
    if (ev.vk == VK_APPS || (ev.vk == VK_F10 && ev.shift && !ev.ctrl && !ev.alt))
    {
        bounds = (focused != nullptr) ? focused->GetBounds() : RECT {};
        ShowDockToMenu (pane, POINT { bounds.left, bounds.top });
        return true;
    }

    if (!ev.alt || !ev.shift || ev.ctrl)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_LEFT:  side = DxuiDockSide::Left;   break;
    case VK_RIGHT: side = DxuiDockSide::Right;  break;
    case VK_UP:    side = DxuiDockSide::Top;    break;
    case VK_DOWN:  side = DxuiDockSide::Bottom; break;
    default:       return false;
    }

    (void) m_dockSite->MovePaneByArrow (pane, side);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnKey
//
//  Enter in a box submits it. The text input has no submit event of its own,
//  so the window catches Enter before the focused box would ignore it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnKey (const DxuiKeyEvent & ev)
{
    IDxuiControl  * focused = GetFocused();
    bool            handled = false;



    if (ev.kind == DxuiKeyEventKind::Down || ev.kind == DxuiKeyEventKind::Char)
    {
        m_openingFocus.OnUserInput();
        m_stopFocus.OnInput (GetPaneOfFocus() == DebuggerLayout::kConsole);
    }

    //  A key can scroll, step or resize what the operand tip lies over, so
    //  the tip goes; the next move of the pointer brings it back.
    if (ev.kind == DxuiKeyEventKind::Down)
    {
        m_operandTips->Hide();
    }

    //  Escape cancels a pane's drag, as in Visual Studio, and every pane
    //  stays where it was.
    if (m_floating->GetRoutingPane().empty() && m_dockSite->OnDragKey (ev))
    {
        return true;
    }

    //  The second key of a chord goes to the key scheme before any box or
    //  list, so the 1 of Ctrl+Alt+M, 1 opens Memory 1 rather than typing,
    //  and the character it would type goes too.
    if (ev.kind == DxuiKeyEventKind::Down && IsKeyPrefixPending())
    {
        m_swallowChordChar = ev.vk != VK_CONTROL && ev.vk != VK_MENU && ev.vk != VK_SHIFT;
        (void) RouteMappedKey (ev);
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Char && m_swallowChordChar)
    {
        m_swallowChordChar = false;
        return true;
    }

    //  The menu bar takes the keys while one of its menus is open, and Alt
    //  with a letter opens the menu it marks.
    if (RouteMenuBarKey (ev, handled))
    {
        return handled;
    }

    //  A pane's color key, while it is up, closes on Escape.
    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && m_colorKeys->IsShown())
    {
        m_colorKeys->Hide();
        return true;
    }

    //  The zoom popup, while open, takes Escape to close and the keys its
    //  slider moves by.
    if (m_zoomOpen && ev.kind == DxuiKeyEventKind::Down)
    {
        if (ev.vk == VK_ESCAPE)
        {
            CloseZoomPopup();
            return true;
        }

        if (m_zoomSlider.OnKey (ev))
        {
            Invalidate();
            return true;
        }
    }

    //  While a replay runs, Escape stops it where it has reached.
    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && TryStopReplay())
    {
        return true;
    }

    //  A pane's own box being edited, such as a breakpoint's name, a watch, a
    //  register or a heat map range, takes every key; and a pane's list with
    //  the focus takes its own keys, as F2 and Ctrl+Z in the registers, stack,
    //  watches and breakpoints.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteEditKey (ev, focused))
        {
            return true;
        }
    }

    //  Shift+Esc closes the pane that has the focus, as in Visual Studio.
    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && ev.shift && !ev.ctrl && !ev.alt && ClosePaneOfFocus())
    {
        return true;
    }

    if (m_finds->RouteKey (ev, handled))
    {
        return handled;
    }

    if (RouteBoxKey (ev, handled))
    {
        return handled;
    }

    if (ev.kind == DxuiKeyEventKind::Down && RouteDockKey (ev))
    {
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN)
    {
        for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
        {
            if (each->SubmitTextBox (dynamic_cast<DxuiTextInput *> (focused)))
            {
                return true;
            }
        }
    }

    //  A floating window types into its own focused control.
    if (ev.kind == DxuiKeyEventKind::Char && !m_floating->GetRoutingPane().empty())
    {
        return focused != nullptr && focused->OnKey (ev);
    }

    //  Only the box this window's focus is on types. A box focused in a
    //  floating window keeps its own focused flag after a click back here.
    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return IsTextBox (focused) && focused->OnKey (ev);
    }

    //  Ctrl+Plus, Ctrl+Minus and Ctrl+0 size the panes' text (FR-083).
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt)
    {
        switch (ev.vk)
        {
        case VK_OEM_PLUS:  case VK_ADD:       StepTextZoom (+1);                 return true;
        case VK_OEM_MINUS: case VK_SUBTRACT:  StepTextZoom (-1);                 return true;
        case '0':          case VK_NUMPAD0:   ApplyTextZoom (1.0f);              return true;
        default:                                                                 break;
        }
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = (focused != nullptr) && focused->OnKey (ev);

        //  Ctrl+C and Ctrl+A copy and select in a pane whose own keys do not
        //  take them, as its Edit menu does.
        if (!handled && focused != nullptr && ev.ctrl && !ev.alt && !ev.shift && (ev.vk == 'C' || ev.vk == 'A'))
        {
            handled = focused->InvokeCommand (ev.vk == 'C' ? DxuiStandardCommand::Copy : DxuiStandardCommand::SelectAll);
        }

        if (!handled && ev.vk == VK_TAB && m_floating->GetRoutingPane().empty())
        {
            //  Panes show, hide and move between one Tab and the next, so the
            //  order is taken from the layout as it stands now.
            m_focusMgr.Rebuild();
            m_focusMgr.HandleKey (ev.shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
            handled = true;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DebuggerWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  sash = (m_dockSite != nullptr) ? m_dockSite->GetCursorForPoint (clientPx) : nullptr;



    if (sash != nullptr)
    {
        return sash;
    }

    //  The find widget floats over its pane's text: the I-beam over its box,
    //  and the arrow over its buttons and the rest of it, not the text's
    //  I-beam beneath.
    if (LPCWSTR cursor = m_finds->GetCursor (clientPx); cursor != nullptr)
    {
        return cursor;
    }

    for (DxuiListView * list : GetLists())
    {
        if (!list->IsVisible() || !IsRoutable (list))
        {
            continue;
        }

        RECT     bounds = list->GetBounds();
        POINT    local  = { clientPx.x - bounds.left, clientPx.y - bounds.top };
        LPCWSTR  cursor = list->GetCursorForPoint (local);

        if (cursor != nullptr)
        {
            return cursor;
        }
    }

    return DxuiWindow::GetCursorForPoint (clientPx);
}
