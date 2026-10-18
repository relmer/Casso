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

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->ConfigureLists();
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
    if (m_stopFocus.OnSnapshot (m_snapshot->isPaused) && IsCommandPane (GetPaneOfFocus()) && !GetStopFocusPane().empty())
    {
        FocusPane (GetStopFocusPane());
    }

    //  The drop-downs carry the mode and the panels they were built with, so
    //  they are rebuilt when either changes.
    if (GetMenuState() != m_menuState)
    {
        m_menuState = GetMenuState();
        SetWindowMenus();
        m_consolePane->GetModeBar().SetMenus();
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
//  DebuggerWindow::ApplySnapshot
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySnapshot()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    bool                                          restoring = IsRestoringViews();
    IDebuggerPane                               * shown     = nullptr;



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
        shown = FindPane (m_snapshot->showPane);

        if (shown != nullptr && shown->GetMainControl() != nullptr && IsRoutable (shown->GetMainControl()))
        {
            SetFocusedControl (shown->GetMainControl());
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





