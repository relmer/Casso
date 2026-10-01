#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/Source/SourcePathList.h"
#include "Core/WindowTrace.h"
#include "Core/DxuiWindowFrame.h"
#include "Config/WindowPlacementProfile.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Widgets/DxuiContextMenu.h"
#include "Ui/Debugger/FlagsDialog.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/SymbolDescriptions.h"
#include "Cassque/CassquePromptDialog.h"
#include "Core/DxuiClipboard.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Theme/DxuiWindowsThemeColors.h"





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
//  TryParseHexWord
//
//  Hex with or without a `$` or `0x` in front. The window's small boxes take an
//  address the way a person types one, and a stray prefix is not an error.
//
////////////////////////////////////////////////////////////////////////////////

static bool TryParseHexWord (std::wstring text, Word & value)
{
    uint32_t  parsed = 0;



    while (!text.empty() && iswspace (text.front())) { text.erase (0, 1); }
    while (!text.empty() && iswspace (text.back()))  { text.pop_back(); }

    if (!text.empty() && text[0] == L'$')
    {
        text.erase (0, 1);
    }
    else if (text.size() > 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X'))
    {
        text.erase (0, 2);
    }

    if (text.empty() || text.size() > 4)
    {
        return false;
    }

    for (wchar_t c : text)
    {
        if (!iswxdigit (c))
        {
            return false;
        }

        parsed = parsed * 16 + (uint32_t) (iswdigit (c) ? c - L'0' : (towupper (c) - L'A' + 10));
    }

    value = (Word) parsed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::~DebuggerWindow
//
////////////////////////////////////////////////////////////////////////////////

DebuggerWindow::~DebuggerWindow()
{
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebuggerWindow::Create (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDebuggerWindowHost * host)
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
    Show();

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
    m_codeList          = CreateChild<DxuiListView>  ();
    m_codeLists[0]      = m_codeList;

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        m_codeLists[(size_t) view] = (view == 0) ? m_codeList : CreateChild<DxuiListView>();
    }

    m_registerList      = CreateChild<DxuiListView>  ();
    m_breakpointList    = CreateChild<DxuiListView>  ();
    m_watchList         = CreateChild<DxuiListView>  ();
    m_stackList         = CreateChild<DxuiListView>  ();
    m_callStackList     = CreateChild<DxuiListView>  ();
    m_callStackButton   = CreateChild<DxuiButton>    (L"Hybrid");
    m_consoleView       = CreateChild<DxuiTextView>  ();
    m_traceList         = CreateChild<DxuiListView>  ();
    m_traceHint         = CreateChild<DxuiLabel>     ();
    m_commandBox        = CreateChild<DxuiTextInput> ();
    m_consoleBar        = CreateChild<DxuiToolbar>   ();
    //  The memory bar before its Address box, so the box paints over the strip.
    m_memoryBar         = CreateChild<DxuiToolbar>   ();
    m_memoryBox         = CreateChild<DxuiTextInput> ();
    m_breakpointBar     = CreateChild<DxuiToolbar>   ();

    //  All four windows exist from the start; the ones not open are hidden.
    for (int id = 1; id <= DebuggerViewState::kMaxMemoryWindows; id++)
    {
        DxuiHexView  * view = CreateChild<DxuiHexView>();

        m_memoryPanes[(size_t) (id - 1)] = std::make_unique<MemoryPane> (
            id, view,
            [this] (int window, Word first)          { if (m_host != nullptr) { m_host->SetDebuggerMemoryWindow (window, first); } },
            [this] (const DebuggerActionBuilder & build) { RunAction (build (GetMode())); },
            [this] (const std::string & line)        { AppendConsole ({ line }); });

        view->SetVisible (id == 1);
        m_memoryOpen[(size_t) (id - 1)] = (id == 1);
    }

    m_tracePane = std::make_unique<TracePane> (
        m_traceList,
        [this] (std::optional<uint64_t> first) { if (m_host != nullptr) { m_host->SetDebuggerTraceTop (first); } });

    //  A document per source file, each shown only while it holds one (FR-054).
    for (SourceDocument & document : m_sourceDocs)
    {
        document.view   = CreateChild<DxuiTextView>();
        document.banner = CreateChild<DxuiActionBanner>();
        document.bar    = CreateChild<DxuiToolbar>();
        document.pane   = std::make_unique<SourcePane> (
            document.view, document.banner,
            [this] (const DebugSourceFile & record, const std::wstring & path, const std::string & key)
            {
                return (m_host != nullptr) ? m_host->FindDebuggerSource (record, path, key) : SourceLookup();
            },
            [this] (const DebuggerActionBuilder & build) { RunAction (build (GetMode())); },
            [this] (Word address)             { ShowCode (address); });

        document.pane->SetOnToggleBody ([this] { ToggleMacroBody(); });
        document.view->SetGutter       (kGutterColumnDip, kBreakpointIconDip);
        document.view->SetVisible   (false);
        document.banner->SetVisible (false);
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        m_sourceDocs[(size_t) slot].pane->SetOnLoadSymbols ([this, slot] { LoadSymbolsFor (slot); });
    }

    m_callStackPane = std::make_unique<CallStackPane> (
        m_callStackList, m_callStackButton,
        [this] (const DebuggerActionBuilder & build) { RunAction (build (GetMode())); },
        [this] (Word address)             { ShowCode (address); });

    //  Every device panel the window can place; each shows while its device
    //  is present and its panel open.
    for (const DebuggerLayout::DiagnosticsPanel & panel : DebuggerLayout::GetDiagnosticsPanels())
    {
        DxuiListView  * list   = CreateChild<DxuiListView>();
        MemoryMapBar  * map    = CreateChild<MemoryMapBar>();
        DiskHeadView  * head   = CreateChild<DiskHeadView>();
        MeterBar      * meters = CreateChild<MeterBar>();

        m_diagPanes.push_back (std::make_unique<DiagnosticsPane> (panel.id, panel.title, list, map, head, meters));
        list->SetVisible (false);
    }


    //  Last, so its strips and the drop overlay paint over the panes.
    m_dockSite = CreateChild<DxuiDockSite>();

    //  After even the site, so a watch being edited is drawn over its row.
    m_watchEditor = CreateChild<DxuiTextInput>();
    m_watchEditor->SetVisible      (false);
    m_watchEditor->SetOverText     (true);
    m_watchEditor->SetHwnd         (GetHwnd());
    m_watchEditor->SetMaxLength    (64);

    ConfigureWidgets();
    ConfigureDockSite();
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
    ConfigureConsoleBar();
    ConfigureSourceBars();

    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        pane->Configure();
    }

    ConfigureMemoryBar();
    ConfigureBreakpointBar();

    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        pane->Configure (GetHwnd());
    }

    for (SourceDocument & document : m_sourceDocs)
    {
        document.pane->Configure (GetHwnd());
    }

    m_callStackPane->Configure();
    SetAcceptsDroppedFiles  (true);

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        ConfigureCodeList (view);
    }

    m_breakpointList->SetActivateOnDoubleClick (true);
    m_registerList->SetActivateOnDoubleClick   (true);
    m_stackList->SetActivateOnDoubleClick      (true);
    m_watchList->SetActivateOnDoubleClick      (true);

    //  Double-clicking a watch edits the cell under the pointer in place: the
    //  expression on the left, the value on the right (FR-096).
    m_watchList->SetOnActivateRow ([this] (int row)
    {
        RECT  value  = {};
        RECT  list   = m_watchList->GetBounds();
        int   column = 0;

        if (m_watchList->GetCellTextRectPx (row, 1, value))
        {
            column = (m_lastPressPx.x - list.left >= value.left) ? 1 : 0;
        }

        BeginWatchEdit (row, column);
    });

    //  Double-clicking a stack byte shows it in memory. The pane lists the
    //  stack newest first, so its rows run backwards through STACK's.
    m_stackList->SetOnActivateRow ([this] (int row)
    {
        if (m_snapshot != nullptr && row >= 0 && row < (int) m_snapshot->stack.size())
        {
            GetActiveMemoryPane()->GoTo (m_snapshot->stack[m_snapshot->stack.size() - 1 - (size_t) row].address);
        }
    });

    //  Double-clicking PC shows the PC, as Show Next Statement does; P and S
    //  open an editor for the flags or the stack pointer.
    m_registerList->SetOnActivateRow ([this] (int row)
    {
        if (m_snapshot != nullptr && row >= 0 && row < (int) m_snapshot->registers.size())
        {
            EditRegister (m_snapshot->registers[(size_t) row].name);
        }
    });

    m_registerList->SetColumns   ({ { L"Reg",         0, false, DxuiTextHAlign::Left },
                                    { L"Value",       0, false, DxuiTextHAlign::Left },
                                    { L"",            0, false, DxuiTextHAlign::Left } });
    SetBreakpointColumns();
    //  The value runs to the pane's edge, as Visual Studio's does, so the
    //  Automatic and Watches headings span the pane rather than the columns.
    m_watchList->SetColumns      ({ { L"Watch",       0, false, DxuiTextHAlign::Left },
                                    { L"Value",       0, true,  DxuiTextHAlign::Left } });
    m_stackList->SetColumns      ({ { L"Stack",       0, false, DxuiTextHAlign::Left },
                                    { L"Value",       0, false, DxuiTextHAlign::Left } });
    //  THE CONSOLE IS TEXT, NOT A LIST: no columns or rows to pick, a
    //  selection that runs through the text as an editor's does, and Ctrl+C
    //  to copy it.
    m_consoleView->SetOwnerWindow (GetHwnd());
    m_consoleView->SetFollowEnd (true);

    //  Activating a breakpoint shows its address; its checkbox turns it on
    //  and off, as Visual Studio's Breakpoints window does.
    m_breakpointList->SetOnActivateRow ([this] (int row)
    {
        const DebuggerViewSnapshot::BreakpointLine  * bp = GetBreakpointOfRow (row);

        if (bp != nullptr)
        {
            ShowCode (bp->address);
        }
    });

    m_breakpointList->SetOnCheckToggled ([this] (int row, size_t, bool checked)
    {
        const DebuggerViewSnapshot::BreakpointLine  * bp = GetBreakpointOfRow (row);

        if (bp != nullptr)
        {
            RunBreakpointStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEnableBreakpoint (bp->id, checked, GetMode()) }, {} });
        }
    });

    //  A click on a heading sorts by that column, and a second click on the
    //  same one turns the order around.
    m_breakpointList->SetOnSortColumn ([this] (int column) { SortBreakpoints ((BreakpointColumns::Column) column); });

    for (DxuiListView * list : GetLists())
    {
        MakeDense (list);
    }

    m_tracePane->Configure();
    m_traceHint->SetText (TracePane::GetKeyHint());

    for (DxuiTextInput * box : { m_commandBox, m_memoryBox })
    {
        box->SetHwnd      (GetHwnd());
        box->SetMaxLength (256);
    }

    ConfigureFindBar();

    m_commandBox->SetPrompt      (GetPromptText (CommandMode::AppleWin));
    m_commandBox->SetPlaceholder (L"Command (Enter to run, HELP for help)");
    m_memoryBox->SetPlaceholder  (L"Address: 0300, PC, (3E),Y");

    m_focusMgr.Attach   (this);
    m_focusMgr.SetTheme (m_theme);
    m_focusMgr.Rebuild();
    m_focusMgr.SetFocused (m_commandBox);
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

    m_commands = std::make_unique<DebuggerCommands> (std::move (handlers));

    m_commandBar->SetTextRenderer (GetTextRenderer());
    m_commandBar->SetPopupHost    (GetPopupHost());
    m_tooltip.SetPopupHost        (GetPopupHost());
    m_tooltip.SetTheme            (*m_theme);
    m_tooltip.SetMonospace        (true);
    m_commandBar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
    m_commandBar->EnableSeeMore   (s_kpszMdl2More, L"See more");
    m_commandBar->SetGrabHandle   (true);
    m_commandBar->SetEntries      (m_commands->BuildEntries());

    SetWindowMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MakeMenuCommand
//
//  A command for one row of a drop-down: its own label, whether it is the
//  one in force, and what choosing it runs.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> DebuggerWindow::MakeMenuCommand (const std::wstring & label, bool checked, std::function<void()> chosen)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->label     = label;
    command->isChecked = [checked] { return checked; };
    command->dispatch  = std::move (chosen);

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureMemoryBar
//
//  Visual Studio's memory window bar: the Address box, Refresh, Columns and
//  the grouping. View > Memory opens another window. Typing into the bytes or
//  POKE at the console writes memory, so the bar has no poke box; a window
//  closes from its tab.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureMemoryBar()
{
    MemoryBarCommands::Handlers  handlers;



    handlers.dispatch  = [this] (int id) { RunMemoryBarEntry (id); };
    handlers.getLabel  = [this] (int id) { return GetMemoryBarLabel (id); };

    m_memoryCommands = std::make_unique<MemoryBarCommands>  (std::move (handlers));
    m_addressEntry   = std::make_unique<MemoryAddressEntry> (m_memoryBox);

    m_addressEntry->SetTooltips (L"Address: a hex address, a register, a symbol or an expression such as (3E),Y; Enter goes there",
                                 L"Addresses entered before");

    m_memoryBar->SetTextRenderer (GetTextRenderer());
    m_memoryBar->SetPopupHost    (GetPopupHost());
    m_memoryBar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
    m_memoryBar->SetCompact      (true);
    m_memoryBar->EnableSeeMore   (s_kpszMdl2More, L"See more");
    m_memoryBar->SetEntries      (m_memoryCommands->BuildEntries (m_addressEntry.get()));
    m_memoryBar->SetVisible      (false);

    SetMemoryBarMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetMemoryBarMenus
//
//  The Address box's history, newest first, and the Columns and grouping
//  choices with the active window's checked. Rebuilt whenever what they list
//  changes, since the rows carry the state they were built with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetMemoryBarMenus()
{
    std::vector<DxuiPopupMenuItem>  history;
    std::vector<DxuiPopupMenuItem>  columns;
    std::vector<DxuiPopupMenuItem>  grouping;
    MemoryPane                    * pane     = GetActiveMemoryPane();



    for (const std::wstring & entered : m_memoryHistory)
    {
        history.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (entered, false, [this, entered]
        {
            m_memoryBox->SetText (entered);
            SubmitMemoryBox();
        })));
    }

    for (int choice : MemoryBarCommands::GetColumnChoices())
    {
        columns.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (MemoryBarCommands::GetColumnsLabel (choice),
                                                                            pane != nullptr && pane->GetColumns() == choice,
                                                                            [this, choice] { ChooseMemoryColumns (choice); })));
    }

    for (int choice : MemoryBarCommands::GetGroupingChoices())
    {
        grouping.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (MemoryBarCommands::GetGroupingName (choice),
                                                                             pane != nullptr && pane->GetGrouping() == choice,
                                                                             [this, choice] { ChooseMemoryGrouping (choice); })));
    }

    m_memoryBar->SetDropDownItems (MemoryBarCommands::kAddress,  std::move (history));
    m_memoryBar->SetDropDownItems (MemoryBarCommands::kColumns,  std::move (columns));
    m_memoryBar->SetDropDownItems (MemoryBarCommands::kGrouping, std::move (grouping));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMemoryBarLabel
//
//  What the drop-downs show: the choice in force in the active window.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetMemoryBarLabel (int id) const
{
    MemoryPane  * pane = GetActiveMemoryPane();



    if (pane == nullptr)
    {
        return std::wstring();
    }

    if (id == MemoryBarCommands::kColumns)
    {
        return L"Columns: " + MemoryBarCommands::GetColumnsLabel (pane->GetColumns());
    }

    if (id == MemoryBarCommands::kGrouping)
    {
        return MemoryBarCommands::GetGroupingLabel (pane->GetGrouping());
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunMemoryBarEntry
//
//  The drop-downs open from the strip; a choice from one comes back through
//  ChooseMemoryColumns or ChooseMemoryGrouping.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunMemoryBarEntry (int id)
{
    switch (id)
    {
    case MemoryBarCommands::kAddress:
        SetFocusedControl (m_memoryBox);
        break;

    case MemoryBarCommands::kRefresh:
        GetActiveMemoryPane()->Refresh();
        break;

    default:
        break;
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ChooseMemoryColumns
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ChooseMemoryColumns (int columns)
{
    GetActiveMemoryPane()->SetColumns (columns);
    SetMemoryBarMenus();
    LayoutWidgets();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ChooseMemoryGrouping
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ChooseMemoryGrouping (int grouping)
{
    GetActiveMemoryPane()->SetGrouping (grouping);
    SetMemoryBarMenus();
    LayoutWidgets();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddMemoryHistory
//
//  Newest first, each address once, as Visual Studio's Address box keeps it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddMemoryHistory (const std::wstring & text)
{
    constexpr size_t  kMaxHistory = 16;
    std::wstring      entered     = text;



    std::erase (m_memoryHistory, entered);
    m_memoryHistory.insert (m_memoryHistory.begin(), entered);

    if (m_memoryHistory.size() > kMaxHistory)
    {
        m_memoryHistory.resize (kMaxHistory);
    }

    SetMemoryBarMenus();
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

    if (id == DebuggerCommands::kPause)
    {
        return !paused;
    }

    if (id == DebuggerCommands::kStepInto || id == DebuggerCommands::kStepOver ||
        id == DebuggerCommands::kStepOut  || id == DebuggerCommands::kShowNext)
    {
        return paused;
    }

    if (id == DebuggerCommands::kRunToCursor)
    {
        return paused && m_codeLists[(size_t) m_activeCode]->GetSelectedRow() >= 0;
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
    std::vector<DxuiListView *>  lists = { m_codeLists[1], m_codeLists[2], m_codeLists[3], m_codeList, m_registerList, m_breakpointList, m_watchList, m_stackList, m_callStackList,
                                           m_traceList };



    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        lists.push_back (pane->GetList());
    }

    return lists;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPromptText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPromptText (CommandMode mode)
{
    switch (mode)
    {
    case CommandMode::Monitor:   return L"Monitor>";
    case CommandMode::GSSquared: return L"GSSquared>";
    case CommandMode::WinDbg:    return L"WinDbg>";
    case CommandMode::Casso:     return L"Casso>";
    default:                     return L"AppleWin>";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHelpCommand
//
//  How each dialect asks for help: Monitor mode reaches HELP through its
//  slash, as it reaches every AppleWin command.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetHelpCommand (CommandMode mode)
{
    switch (mode)
    {
    case CommandMode::Monitor:   return L"/HELP";
    case CommandMode::GSSquared: return L"help";
    case CommandMode::WinDbg:    return L".help";
    default:                     return L"HELP";
    }
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
//  layout.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyTextZoom (float zoom)
{
    m_textZoom = std::clamp (zoom, kMinTextZoom, kMaxTextZoom);

    for (DxuiListView * list : GetLists())
    {
        list->SetFontSizeDip     (kPaneFontDip * m_textZoom);
        list->SetRowHeightDip    ((int) std::lround (kPaneRowDip * m_textZoom));
        list->SetHeaderHeightDip ((int) std::lround (kPaneHeaderDip * m_textZoom));
        list->ResetAutoFit();
    }

    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        pane->GetView()->SetZoom (m_textZoom);
    }

    for (SourceDocument & document : m_sourceDocs)
    {
        document.view->SetZoom (m_textZoom);
    }

    m_consoleView->SetZoom (m_textZoom);

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



    targets.insert (targets.end(), { m_callStackButton, m_commandBox, m_memoryBox });

    for (IDxuiControl * control : GetFindControls())
    {
        targets.push_back (control);
    }

    return targets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetOpenMemoryPanes
//
//  In window order, which is the order they are laid out.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<MemoryPane *> DebuggerWindow::GetOpenMemoryPanes() const
{
    std::vector<MemoryPane *>  open;



    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        if (pane != nullptr && m_memoryOpen[(size_t) (pane->GetId() - 1)])
        {
            open.push_back (pane.get());
        }
    }

    return open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetActiveMemoryPane
//
//  The window last clicked in, which the memory box and the grouping button
//  act on; the first window until another is clicked.
//
////////////////////////////////////////////////////////////////////////////////

MemoryPane * DebuggerWindow::GetActiveMemoryPane() const
{
    return (m_activePane != nullptr) ? m_activePane : m_memoryPanes[0].get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFocusedMemoryPane
//
////////////////////////////////////////////////////////////////////////////////

MemoryPane * DebuggerWindow::GetFocusedMemoryPane() const
{
    IDxuiControl  * focused = GetFocused();



    for (MemoryPane * pane : GetOpenMemoryPanes())
    {
        if (focused != nullptr && focused == pane->GetView())
        {
            return pane;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyMemoryWindows
//
//  The snapshot says which windows are open: each one it holds is shown and
//  given its bytes, and the rest are hidden. A different machine clears every
//  window's undo, since the addresses it would restore belonged to the old
//  one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyMemoryWindows()
{
    std::array<bool, DebuggerViewState::kMaxMemoryWindows>  open    = {};
    bool                                                    changed = false;



    //  A Go to the CPU thread resolved, once.
    if (m_snapshot->goTo.has_value() && m_snapshot->goTo->serial != m_goToSerial)
    {
        const DebuggerViewSnapshot::GoTo & goTo = *m_snapshot->goTo;

        m_goToSerial = goTo.serial;

        if (goTo.address.has_value() && goTo.window >= 1 && goTo.window <= DebuggerViewState::kMaxMemoryWindows)
        {
            m_memoryPanes[(size_t) (goTo.window - 1)]->GoTo (*goTo.address);
        }
        else
        {
            AppendConsole ({ std::format ("Go to: \"{}\" is not an address, a register or a 6502 operand.", goTo.text) });
        }
    }

    m_registerHistory.OnSnapshot (m_snapshot->isPaused, m_snapshot->pc);
    m_stackHistory.OnSnapshot    (m_snapshot->isPaused, m_snapshot->pc);

    if (m_snapshot->machine != m_machine)
    {
        m_machine = m_snapshot->machine;

        //  Every undo record holds the old machine's values and addresses,
        //  the watch pane's as much as the memory windows'.
        for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
        {
            pane->ClearHistory();
        }

        m_watchHistory.Clear();
    }

    for (const DebuggerViewSnapshot::MemoryWindow & window : m_snapshot->memoryWindows)
    {
        if (window.id >= 1 && window.id <= DebuggerViewState::kMaxMemoryWindows)
        {
            open[(size_t) (window.id - 1)] = true;
            m_memoryPanes[(size_t) (window.id - 1)]->SetChangedColor (GetChangedArgb());

            //  Bytes are edited while the machine is stopped, as Visual
            //  Studio's memory window allows: a running machine would
            //  overwrite the edit, or be changed under the code using it.
            if (m_memoryPanes[(size_t) (window.id - 1)]->GetView()->IsEditable() != m_snapshot->isPaused)
            {
                m_memoryPanes[(size_t) (window.id - 1)]->GetView()->SetEditable (m_snapshot->isPaused);
            }

            m_memoryPanes[(size_t) (window.id - 1)]->Apply (window);
        }
    }

    for (size_t i = 0; i < m_memoryPanes.size(); i++)
    {
        //  A window opened comes to the front of its tab group.
        if (m_memoryOpen[i] != open[i])
        {
            m_memoryOpen[i] = open[i];
            changed         = true;

            if (open[i])
            {
                (void) m_dockSite->EditPaneLayout().Activate (DebuggerLayout::GetMemoryPaneId ((int) i + 1));
            }
        }

        if (!open[i] && m_activePane == m_memoryPanes[i].get())
        {
            m_activePane = nullptr;
        }
    }

    if (changed)
    {
        m_dockSite->Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySource
//
//  The documents take the snapshot, and the layout changes when a document or
//  its banner comes or goes, or a banner's text or actions change: a new
//  action has no place until it is laid out. Another debug file closes every
//  document, since a file's id belongs to the debug file it came from.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySource()
{
    const std::optional<DebuggerViewSnapshot::SourceState> & source    = m_snapshot->source;
    std::wstring                                             loadedFor;
    bool                                                     relayout  = false;



    if (source.has_value())
    {
        loadedFor = source->debugFilePath + SourcePathList::Utf8ToWide (source->programKey);
    }

    if (loadedFor != m_sourceLoadedFor)
    {
        m_sourceLoadedFor = loadedFor;
        m_documents.Clear();
        m_looseSources.clear();
        m_pcPlace         = { -1, 0 };
        m_macroLevel      = 0;
    }

    if (source.has_value())
    {
        m_macroLevel = SourcePane::GetMacroLevel (*source, m_macroLevel);

        RestoreSourceDocuments();
        FollowPcSource();
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        SourceDocument             & document    = m_sourceDocs[(size_t) slot];
        bool                         shown       = false;
        bool                         bannerShown = false;
        bool                         holdsPc     = false;
        std::wstring                 bannerKey;
        std::wstring                 title;
        DxuiTabGroup::LeadingMark    mark;

        document.pane->SetStyle    ({ GetPcMarkerArgb(), GetPcRowArgb(), GetBreakpointIcon (true), GetBreakpointIcon (false), GetSyntaxColors(),
                                       (m_theme != nullptr) ? m_theme->ForegroundMuted() : 0u, GetResultArgb(),
                                       GetSyntaxColors().GetBlended ((m_theme != nullptr) ? m_theme->ContentBackground() : 0u) });
        document.pane->SetShowCode (m_showSourceCode);
        document.pane->SetFile     (m_documents.GetFileId (slot));
        document.pane->SetMacroLevel (m_macroLevel);
        document.pane->Apply       (*m_snapshot);

        //  Each document's tab is titled with its file's name.
        title = L"Source";

        if (source.has_value())
        {
            for (const DebugSourceFile & record : source->files)
            {
                title = (record.id == m_documents.GetFileId (slot)) ? SourcePathList::Utf8ToWide (record.name) : title;
            }
        }

        if (m_looseSources.contains (m_documents.GetFileId (slot)))
        {
            title = fs::path (m_looseSources.at (m_documents.GetFileId (slot))).filename().wstring();
        }

        if (title != document.title)
        {
            document.title = title;
            m_dockSite->SetTitle (DebuggerLayout::GetSourcePaneId (slot), title);
        }

        bannerKey   = document.banner->GetText() + (document.banner->GetAction (0) != nullptr ? document.banner->GetAction (0)->GetAccessibleName() : L"");
        shown       = document.pane->IsActive();
        bannerShown = shown && document.pane->HasBanner();

        if (shown != document.shown)
        {
            document.shown       = shown;
            document.bannerShown = bannerShown;
            document.bannerKey   = bannerKey;
            relayout             = true;
        }
        else if (bannerShown != document.bannerShown || bannerKey != document.bannerKey)
        {
            document.bannerShown = bannerShown;
            document.bannerKey   = bannerKey;
            document.frame->Relayout();
        }

        //  The document the PC is in carries the PC's own marker ahead of its
        //  title, as the following disassembly view's tab does (FR-113).
        holdsPc = shown && source.has_value() && m_documents.GetFileId (slot) == source->fileId;

        if (holdsPc)
        {
            mark = DxuiTabGroup::LeadingMark { s_kpszTriangleRight, DxuiTheme::kMonoFace, GetPcMarkerArgb() };
        }

        m_dockSite->SetLeadingMark (DebuggerLayout::GetSourcePaneId (slot), mark);
        m_dockSite->SetTabTip      (DebuggerLayout::GetSourcePaneId (slot), holdsPc ? L"The PC is in this file" : L"");
    }

    if (relayout)
    {
        m_dockSite->Relayout();
    }

    //  A source opened before its debug file loaded, shown again against it.
    if (source.has_value() && !m_pendingLooseSource.empty())
    {
        std::wstring  pending = std::move (m_pendingLooseSource);

        m_pendingLooseSource.clear();
        ShowDroppedSource (pending);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FollowPcSource
//
//  At each stop at a new place, the file the PC is in comes to the front,
//  opened if it is not open yet -- the body's file while the body is shown.
//  A running machine moves nothing, so the documents do not churn under it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::FollowPcSource()
{
    const DebuggerViewSnapshot::SourceState & source = *m_snapshot->source;
    std::pair<int, int>                       place  = SourcePane::GetPlace (source, SourcePane::GetMacroLevel (source, m_macroLevel));



    if (!m_snapshot->isPaused || place.first < 0 || place == m_pcPlace)
    {
        return;
    }

    m_pcPlace = place;
    OpenSourceDocument (place.first, 0, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenSourceDocument
//
//  The file's document, opened if it is not, brought to the front of its
//  group when asked, and scrolled so `line` is at its top when one is given.
//  The PC's file is never the one closed to make room.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenSourceDocument (int fileId, int line, bool activate)
{
    int  keep = m_snapshot != nullptr && m_snapshot->source.has_value() ? m_snapshot->source->fileId : -1;
    int  slot = m_documents.Open (fileId, keep);



    if (slot < 0)
    {
        return;
    }

    m_sourceDocs[(size_t) slot].pane->SetFile (fileId);

    if (line > 0)
    {
        m_sourceDocs[(size_t) slot].pane->SetTopSourceLine (line);
    }

    if (activate)
    {
        (void) m_dockSite->EditPaneLayout().Activate (DebuggerLayout::GetSourcePaneId (slot));
        m_activeSource = slot;
        m_dockSite->Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CloseSourceDocument
//
//  Only that file's document goes (FR-113). The PC's place is kept, so a
//  document closed while the PC is in it stays closed until the next stop
//  somewhere else in that file opens it again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CloseSourceDocument (int slot)
{
    if (!m_documents.IsOpen (slot))
    {
        return;
    }

    m_documents.Close (slot);
    m_sourceDocs[(size_t) slot].pane->SetFile (-1);

    if (m_snapshot != nullptr)
    {
        ApplySource();
        KeepOpenViews();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RestoreSourceDocuments
//
//  The documents open when the debugger last closed, once a debug file with
//  files of their names is loaded; until then they wait. A saved file that
//  is gone reopens with its not-found notice.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RestoreSourceDocuments()
{
    const DebuggerViewSnapshot::SourceState & source = *m_snapshot->source;



    if (m_pendingSourceDocs.empty() || source.files.empty())
    {
        return;
    }

    for (const SourceDocuments::Saved & saved : m_pendingSourceDocs)
    {
        for (const DebugSourceFile & record : source.files)
        {
            if (record.name == saved.name)
            {
                OpenSourceDocument (record.id, saved.line, false);
            }
        }
    }

    m_pendingSourceDocs.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleMacroBody
//
//  One choice for every document: the body's file comes forward, or the
//  invocation's again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleMacroBody()
{
    if (m_snapshot != nullptr && m_snapshot->source.has_value())
    {
        m_macroLevel = SourcePane::GetNextMacroLevel (m_macroLevel, SourcePane::GetMacroLevel (*m_snapshot->source, -1));
    }

    m_pcPlace = { -1, 0 };

    if (m_snapshot != nullptr && m_snapshot->source.has_value())
    {
        ApplySource();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowSourceLine
//
//  A disassembly line was selected: its source line is selected in its
//  file's document, opened behind the others if it is not open (FR-054).
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowSourceLine (int fileId, int line)
{
    int  slot = m_documents.Find (fileId);



    if (fileId < 0 || line <= 0)
    {
        return;
    }

    if (slot < 0)
    {
        OpenSourceDocument (fileId, 0, false);
        ApplySource();
        slot = m_documents.Find (fileId);
    }

    if (slot >= 0)
    {
        m_sourceDocs[(size_t) slot].pane->ShowLine (fileId, line);
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
//  DebuggerWindow::GetSourceSlotAt
//
//  The shown document whose text or banner is under a point, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::GetSourceSlotAt (POINT atDip) const
{
    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        const SourceDocument & document = m_sourceDocs[(size_t) slot];

        if (document.shown && document.view->IsVisible() &&
            (DxuiDockSite::Contains (document.view->GetBounds(), atDip) || DxuiDockSite::Contains (document.banner->GetBounds(), atDip)))
        {
            return slot;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteSourceMouse
//
//  The banner's actions, then the text: a click moves the code pane to the
//  line, a second click on the same place within the double-click time
//  toggles the line's breakpoint, and a drag selects text as it does in any
//  text view.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteSourceMouse (const DxuiMouseEvent & ev)
{
    POINT             at       = ev.positionDip;
    int               slot     = GetSourceSlotAt (at);
    uint64_t          now      = GetTickCount64();
    bool              isDouble = false;
    bool              inside   = false;
    bool              onBanner = false;
    SourceDocument  * document = nullptr;



    //  A drag or a press on a scrollbar keeps going to the document it began
    //  in, wherever the pointer goes.
    for (int each = 0; each < SourceDocuments::kMaxDocuments; each++)
    {
        if (m_sourceDocs[(size_t) each].view->IsInteracting())
        {
            slot = each;
        }
    }

    if (slot < 0)
    {
        return false;
    }

    document = &m_sourceDocs[(size_t) slot];

    inside   = DxuiDockSite::Contains (document->view->GetBounds(),   at);
    onBanner = DxuiDockSite::Contains (document->banner->GetBounds(), at);

    //  A document behind another tab takes no input.
    if (!document->shown || !document->view->IsVisible() || !IsRoutable (document->view))
    {
        return false;
    }

    if (document->bannerShown && (onBanner || ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Move))
    {
        (void) document->banner->OnMouse (ev);

        //  An action changes what the pane shows while the machine is paused,
        //  when no snapshot is coming to show it.
        if (onBanner && ev.kind == DxuiMouseEventKind::Up && m_snapshot != nullptr)
        {
            ApplySource();
        }

        if (onBanner && ev.kind == DxuiMouseEventKind::Down)
        {
            return true;
        }
    }

    if (document->view->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
    {
        (void) document->view->OnMouse (ev);
        return true;
    }

    if (!inside || (ev.kind != DxuiMouseEventKind::Down && ev.kind != DxuiMouseEventKind::Wheel))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        isDouble = (now - m_sourceClickMs) <= (uint64_t) GetDoubleClickTime() &&
                   std::abs (at.x - m_sourceClickAt.x) <= GetSystemMetrics (SM_CXDOUBLECLK) &&
                   std::abs (at.y - m_sourceClickAt.y) <= GetSystemMetrics (SM_CYDOUBLECLK);

        m_sourceClickMs = isDouble ? 0 : now;
        m_sourceClickAt = at;

        if (isDouble)
        {
            document->pane->OnDoubleClick (at);
        }
        else
        {
            document->pane->OnClick (at);
        }

        m_activeSource = slot;
        SetFocusedControl (document->view);
        NoteViewFocus (true);
    }

    (void) document->view->OnMouse (ev);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteConsoleMouse
//
//  The console's text takes a press or the wheel inside it, and everything
//  while a selection drag or its scrollbar is under way.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteConsoleMouse (const DxuiMouseEvent & ev)
{
    if (!m_consoleView->IsVisible() || !IsRoutable (m_consoleView))
    {
        return false;
    }

    if (m_consoleView->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
    {
        (void) m_consoleView->OnMouse (ev);
        return true;
    }

    if (!DxuiDockSite::Contains (m_consoleView->GetBounds(), ev.positionDip) ||
        (ev.kind != DxuiMouseEventKind::Down && ev.kind != DxuiMouseEventKind::Wheel) ||
        (ev.kind == DxuiMouseEventKind::Down && ev.button != DxuiMouseButton::Left))
    {
        return false;
    }

    //  A press still selects in the output, but typing goes to the command
    //  line, as it does in a terminal.
    if (ev.kind == DxuiMouseEventKind::Down)
    {
        SetFocusedControl (m_commandBox);
    }

    (void) m_consoleView->OnMouse (ev);
    return true;
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
        m_host->RunDebuggerCommand (DebuggerViewState::GetSourceStepLine (isSource, m_snapshot->mode));
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
        target = (pane == DebuggerLayout::kConsole) ? m_commandBox : nullptr;

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
        m_activeSource = slot;
        NoteViewFocus (true);
        return;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            m_activeCode = view;
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
            OpenSourcePath (path);
            continue;
        }

        if (kind == DroppedFiles::Kind::Other)
        {
            lookup  = m_host->MatchDroppedDebuggerSource ({}, path, std::string(), index);
            opening = lookup.path.empty() ? DroppedFiles::Opening::Symbols : DroppedFiles::GetOpening (lookup.text);
        }

        if (opening == DroppedFiles::Opening::Symbols)
        {
            m_host->RunDebuggerCommandInMode (std::format ("SYM LOAD \"{}\"", SourcePathList::WideToUtf8 (path)), CommandMode::AppleWin);
            continue;
        }

        OpenLooseFile (path, opening == DroppedFiles::Opening::Hex ? DroppedFiles::FormatHexDump (lookup.text) : lookup.text, false);
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
//  DebuggerWindow::OpenSourcePath
//
//  With a debug file loaded, the source opens in the document for the record
//  it matches. With none, a debug or symbol file beside it is loaded first,
//  and the source shows meanwhile with no symbols; once that debug file is
//  in, the source is matched against it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenSourcePath (const std::wstring & path)
{
    SourceLookup  lookup;
    int           index  = -1;
    bool          loaded = m_snapshot != nullptr && m_snapshot->source.has_value();



    if (m_host == nullptr)
    {
        return;
    }

    if (loaded)
    {
        ShowDroppedSource (path);
        return;
    }

    for (const std::wstring & beside : DroppedFiles::GetSymbolFilesBeside (path))
    {
        if (m_host->DoesDebuggerFileExist (beside))
        {
            m_host->RunDebuggerCommandInMode (std::format ("SYM LOAD \"{}\"", SourcePathList::WideToUtf8 (beside)), CommandMode::AppleWin);
            m_pendingLooseSource = path;
            break;
        }
    }

    lookup = m_host->MatchDroppedDebuggerSource ({}, path, std::string(), index);
    OpenLooseFile (path, lookup.text, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenLooseFile
//
//  A file with no debug file record, in a document of its own titled with its
//  name. The same file opened again goes to the document it is in.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenLooseFile (const std::wstring & path, const std::string & text, bool isSource)
{
    int  fileId = -1;
    int  slot   = -1;



    for (const auto & [id, open] : m_looseSources)
    {
        fileId = (_wcsicmp (open.c_str(), path.c_str()) == 0) ? id : fileId;
    }

    if (fileId < 0)
    {
        fileId = m_nextLooseId++;
        m_looseSources[fileId] = path;
    }

    OpenSourceDocument (fileId, 0, true);
    slot = m_documents.Find (fileId);

    if (slot < 0)
    {
        return;
    }

    m_sourceDocs[(size_t) slot].pane->ShowLoose (path, text, isSource);

    if (m_snapshot != nullptr)
    {
        ApplySource();
    }

    SetWindowMenus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsLooseSource
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsLooseSource (int slot) const
{
    return m_looseSources.contains (m_documents.GetFileId (slot));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::LoadSymbolsFor
//
//  A source document's Load symbols button: the user picks the debug or
//  symbol file, and the source is matched against it once it loads.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::LoadSymbolsFor (int slot)
{
    auto  found = m_looseSources.find (m_documents.GetFileId (slot));



    OpenSymbolFile (found != m_looseSources.end() ? found->second : std::wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleSourceCode
//
//  The rows of instructions each source line assembled to, listed under it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleSourceCode()
{
    m_showSourceCode = !m_showSourceCode;

    if (m_snapshot != nullptr)
    {
        ApplySource();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowDroppedSource
//
//  The document for the record the file matches; a file that matches none
//  opens on its own, saying it has no symbols.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowDroppedSource (const std::wstring & path)
{
    const DebuggerViewSnapshot::SourceState & source = *m_snapshot->source;
    SourceLookup                              lookup;
    int                                       index  = -1;



    lookup = m_host->MatchDroppedDebuggerSource (source.files, path, source.programKey, index);

    if (index < 0 || index >= (int) source.files.size())
    {
        OpenLooseFile (path, lookup.text, true);
        return;
    }

    OpenSourceDocument (source.files[(size_t) index].id, 0, true);

    m_sourceDocs[(size_t) m_activeSource].pane->ShowDropped (lookup, index);
    ApplySource();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteMemoryMouse
//
//  A drag in a window keeps its events until it ends; a press or the wheel goes
//  to the window under it, and a press makes it the active one. Releases reach
//  every window, so none is left thinking a drag is still on.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteMemoryMouse (const DxuiMouseEvent & ev)
{
    POINT  at = ev.positionDip;



    for (MemoryPane * pane : GetOpenMemoryPanes())
    {
        DxuiHexView  * view   = pane->GetView();
        RECT           bounds = view->GetBounds();
        bool           inside = view->IsVisible() && at.x >= bounds.left && at.x < bounds.right && at.y >= bounds.top && at.y < bounds.bottom;

        if (!IsRoutable (view))
        {
            continue;
        }

        if (view->IsDragging() && ev.kind != DxuiMouseEventKind::Down)
        {
            (void) view->OnMouse (ev);
            return true;
        }

        if (inside && (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel))
        {
            (void) view->OnMouse (ev);

            if (ev.kind == DxuiMouseEventKind::Down)
            {
                SetFocusedControl (view);
                m_activePane = pane;
            }

            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Up)
        {
            (void) view->OnMouse (ev);
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFocusedBox
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextInput * DebuggerWindow::GetFocusedBox() const
{
    IDxuiControl  * focused = GetFocused();



    for (DxuiTextInput * box : { m_commandBox, m_memoryBox })
    {
        if (focused != nullptr && focused == box)
        {
            return box;
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
//  Explorer's do. The Theme drop-down is rebuilt for the check it carries.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyTheme (const std::string & name)
{
    const DxuiWindowsThemeColors::SystemColors &  system = DxuiWindowsThemeColors::Instance().GetSystemColors();



    m_themeName = name;

    m_lightTheme.ApplySystemColors (system);
    m_darkTheme.ApplySystemColors  (system);

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

    for (const auto & entry : m_floats)
    {
        entry.second->SetTheme (m_theme);
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

    for (const auto & entry : m_floats)
    {
        entry.second->SetKeyMap (&map);
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
    int                            row    = m_codeLists[(size_t) m_activeCode]->GetSelectedRow();
    DebuggerViewSnapshot           active;



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
            m_host->PauseDebugger();
        }

        return true;
    }

    //  The cursor actions read the disassembly view last used.
    if (m_snapshot != nullptr)
    {
        active      = *m_snapshot;
        active.code = GetCodeLines (m_activeCode);
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
//  scheme first. A memory window counts as a box that is never empty, since
//  every character typed into it is an edit. When the scheme took a Space, the character WM_CHAR delivers
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
    bool                                          inMemory      = GetFocusedMemoryPane() != nullptr;
    bool                                          decided       = false;
    CommandMode                                   mode          = (m_snapshot != nullptr) ? m_snapshot->mode : CommandMode::AppleWin;
    std::optional<DebuggerKeySchemes::Action>     consoleAction;
    bool                                          boxEmpty      = false;
    bool                                          boxKeeps      = false;



    //  While the line assembler takes the box's lines, Return and Space are
    //  the box's, so an empty line ends the assembly.
    if (ev.kind == DxuiKeyEventKind::Down && box != nullptr && box == m_commandBox &&
        DebuggerViewState::DoesAssemblerKeepKey (m_snapshot.get(), ev.vk, ev.ctrl, ev.alt))
    {
        return false;
    }

    consoleAction = (ev.kind == DxuiKeyEventKind::Down && box != nullptr && box == m_commandBox)
                  ? DebuggerViewState::GetConsoleKeyAction (mode, ev.vk, ev.ctrl, ev.alt, ev.shift, box->GetText().empty())
                  : std::nullopt;

    boxEmpty = box != nullptr && box->GetText().empty() &&
               !(box == m_commandBox && DebuggerViewState::DoesConsoleKeepKey (mode, ev.vk, ev.ctrl, ev.alt));

    boxKeeps = DebuggerKeySchemes::DoesBoxKeepKey (ev.vk, ev.ctrl, ev.alt, true, boxEmpty,
                                                   box != nullptr && box == m_commandBox && mode == CommandMode::Monitor);

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
//  DebuggerWindow::RouteFindKey
//
//  While the find bar has the keys, Enter finds the next match, Shift+Enter
//  the one before, and Escape closes the bar. Whatever else a box keeps --
//  letters, Space, editing keys -- stays in the find box, so typing a search
//  never steps the machine; function keys and chords reach the scheme, which
//  is how F3 and Shift+F3 work from here. Returns true when the key was
//  decided here.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteFindKey (const DxuiKeyEvent & ev, bool & handled)
{
    IDxuiControl  * focused = GetFocused();
    std::wstring    owner   = GetFindPaneOfControl (focused);
    bool            isDown  = ev.kind == DxuiKeyEventKind::Down;



    //  The keys act on the widget they are in, whichever pane's it is.
    if (owner.empty())
    {
        return false;
    }

    ActivateFind (owner);

    if (!m_findOpen)
    {
        return false;
    }

    if (isDown && ev.vk == VK_ESCAPE)
    {
        CloseFind();
        handled = true;
        return true;
    }

    if (focused != m_findBox)
    {
        return false;
    }

    if (isDown && ev.vk == VK_RETURN)
    {
        FindInPane (!ev.shift);
        handled = true;
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Char)
    {
        //  Enter and Escape arrive as characters too, after the key-down
        //  above has acted on them; each character has its key's code.
        handled = (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE) || m_findBox->OnKey (ev);
        return true;
    }

    //  Up and Down step through the pane's find history.
    if (isDown && (ev.vk == VK_UP || ev.vk == VK_DOWN) && !ev.ctrl && !ev.alt)
    {
        (void) StepFindHistory ((ev.vk == VK_UP) ? -1 : 1);
        handled = true;
        return true;
    }

    if (isDown && ev.vk != VK_TAB && DebuggerKeySchemes::DoesBoxKeepKey (ev.vk, ev.ctrl, ev.alt, true, false, false))
    {
        (void) m_findBox->OnKey (ev);
        handled = true;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureFindBar
//
//  The console's find widget is made now; a source document's is made the
//  first time find opens there. Hidden until find opens.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureFindBar()
{
    ActivateFind (DebuggerLayout::kConsole);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreateFindWidget
//
//  A pane's own find widget: a box for the text, whether case matters,
//  whether only whole words count, a button each way, and a line saying
//  what the last search found. Each control acts on its own pane's widget,
//  so two panes can each have theirs open.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreateFindWidget (const std::wstring & pane)
{
    FindState  & state = m_findStates[pane];



    if (state.box != nullptr)
    {
        return;
    }

    //  The widget floats over its pane's text, so it comes after every
    //  pane's controls: its plate, then what sits on the plate.
    state.plate       = CreateChild<FindWidgetPlate> ();
    state.chevron     = CreateChild<DxuiButton>      (s_kpszChevronRight);
    state.box         = CreateChild<DxuiTextInput>   ();
    state.caseButton  = CreateChild<DxuiButton>      (L"Aa");
    state.wordButton  = CreateChild<DxuiButton>      (L"ab");
    state.regexButton = CreateChild<DxuiButton>      (L".*");
    state.status      = CreateChild<DxuiLabel>       ();
    state.prevButton  = CreateChild<DxuiButton>      (s_kpszUpArrow);
    state.nextButton  = CreateChild<DxuiButton>      (s_kpszDownArrow);
    state.selButton   = CreateChild<DxuiButton>      (s_kpszIdenticalTo);
    state.closeButton = CreateChild<DxuiButton>      (s_kpszMultiplyX);

    state.box->SetHwnd        (GetHwnd());
    state.box->SetMaxLength   (256);
    state.box->SetPlaceholder ((pane == DebuggerLayout::kConsole) ? L"Find in the console" : L"Find in the source");

    //  Each option is a toggle, shown emphasized while on, as Visual Studio
    //  Code's find bar shows them.
    state.caseButton->SetOnClick  ([this, pane] { ActivateFind (pane); SetFindOptions (!m_findMatchCase, m_findWholeWord, m_findRegex); SetFocusedControl (m_findBox); });
    state.wordButton->SetOnClick  ([this, pane] { ActivateFind (pane); SetFindOptions (m_findMatchCase, !m_findWholeWord, m_findRegex); SetFocusedControl (m_findBox); });
    state.regexButton->SetOnClick ([this, pane] { ActivateFind (pane); SetFindOptions (m_findMatchCase, m_findWholeWord, !m_findRegex); SetFocusedControl (m_findBox); });

    //  A button press takes the keys, so each gives them back to the box
    //  and the next Enter searches again.
    state.prevButton->SetOnClick  ([this, pane] { ActivateFind (pane); FindInPane (false); SetFocusedControl (m_findBox); });
    state.nextButton->SetOnClick  ([this, pane] { ActivateFind (pane); FindInPane (true);  SetFocusedControl (m_findBox); });
    state.closeButton->SetOnClick ([this, pane] { ActivateFind (pane); CloseFind(); });

    state.selButton->SetOnClick ([this, pane] { ActivateFind (pane); SetFindInSelection (!m_findInSelection); SetFocusedControl (m_findBox); });

    //  The chevron opens replace in Visual Studio Code. The console and the
    //  source are read-only, so it is shown, as the widget shows it, but off.
    state.chevron->SetEnabled (false);

    state.plate->SetVisible  (false);
    state.box->SetVisible    (false);
    state.status->SetVisible (false);

    for (DxuiButton * button : { state.chevron, state.caseButton, state.wordButton, state.regexButton, state.prevButton, state.nextButton, state.selButton, state.closeButton })
    {
        button->SetVisible (false);
    }

    //  The site's strips and drop zones paint over the panes, and a watch
    //  being edited over the site, so both stay last.
    for (IDxuiControl * last : { (IDxuiControl *) m_dockSite, (IDxuiControl *) m_watchEditor })
    {
        std::unique_ptr<IDxuiControl>  owned = (last != nullptr) ? DetachChild (last) : nullptr;

        if (owned != nullptr)
        {
            (void) AttachChild (std::move (owned));
        }
    }

    m_focusMgr.Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ActivateFind
//
//  Makes the pane's find widget the one the m_find members hold, making it
//  first if the pane has none yet. The widget left keeps its own state.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ActivateFind (const std::wstring & pane)
{
    if (pane == m_findPane && m_findBox != nullptr)
    {
        return;
    }

    if (m_findBox != nullptr)
    {
        SaveFindState();
    }

    CreateFindWidget (pane);

    m_findPane = pane;
    LoadFindState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindPaneOfControl
//
//  The pane whose find widget holds the control; empty when none does.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetFindPaneOfControl (const IDxuiControl * control) const
{
    if (control == nullptr)
    {
        return {};
    }

    for (const auto & [pane, state] : m_findStates)
    {
        const IDxuiControl * parts[] = { state.plate, state.chevron, state.box, state.caseButton, state.wordButton, state.regexButton,
                                         state.status, state.prevButton, state.nextButton, state.selButton, state.closeButton };

        if (std::ranges::find (parts, control) != std::end (parts))
        {
            return pane;
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsFindOpenIn
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsFindOpenIn (const std::wstring & pane) const
{
    auto  it = m_findStates.find (pane);



    if (pane == m_findPane)
    {
        return m_findOpen;
    }

    return it != m_findStates.end() && it->second.open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindBoxOf
//
//  Null for a pane find has never opened in.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextInput * DebuggerWindow::GetFindBoxOf (const std::wstring & pane) const
{
    auto  it = m_findStates.find (pane);



    return (it != m_findStates.end()) ? it->second.box : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindControls
//
//  The find bar's controls that take a press or the keys; its status line
//  takes neither.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetFindControls() const
{
    return { m_findBox, m_findCaseButton, m_findWordButton, m_findRegexButton, m_findPrevButton, m_findNextButton, m_findSelectionButton, m_findCloseButton };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetFindBarVisible
//
//  Through each control's own type: a button keeps a shown flag of its own
//  beside the base's.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetFindBarVisible (bool shown)
{
    m_findPlate->SetVisible   (shown);
    m_findBox->SetVisible     (shown);
    m_findStatus->SetVisible  (shown);

    for (DxuiButton * button : { m_findChevronButton, m_findCaseButton, m_findWordButton, m_findRegexButton, m_findPrevButton, m_findNextButton, m_findSelectionButton, m_findCloseButton })
    {
        button->SetVisible (shown);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindTarget
//
//  The pane find opens in: the one with the keys when it is text to search
//  (the console or a source document), the one the bar is in while the keys
//  are in the bar, and the console otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetFindTarget() const
{
    std::wstring  pane = GetPaneOfFocus();



    if (pane == DebuggerLayout::kConsole || GetSourceSlotOf (pane) >= 0)
    {
        return pane;
    }

    return DebuggerLayout::kConsole;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindView
//
//  The text the find bar searches: its pane's.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView * DebuggerWindow::GetFindView() const
{
    int  slot = GetSourceSlotOf (m_findPane);



    return (slot >= 0) ? m_sourceDocs[(size_t) slot].view : m_consoleView;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveFindState
//
//  Keeps what the m_find members hold as the active pane's widget's.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveFindState()
{
    FindState  & state = m_findStates[m_findPane];



    state.host        = m_findBarHost;
    state.open        = m_findOpen;
    state.matchCase   = m_findMatchCase;
    state.wholeWord   = m_findWholeWord;
    state.isRegex     = m_findRegex;
    state.inSelection = m_findInSelection;
    state.historyAt   = m_findHistoryAt;
    state.statusText  = m_findStatusText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::LoadFindState
//
//  Points the m_find members at the active pane's own widget: its controls,
//  whether it is open, its options and its count.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::LoadFindState()
{
    const FindState  & state = m_findStates[m_findPane];



    m_findPlate           = state.plate;
    m_findChevronButton   = state.chevron;
    m_findBox             = state.box;
    m_findCaseButton      = state.caseButton;
    m_findWordButton      = state.wordButton;
    m_findRegexButton     = state.regexButton;
    m_findStatus          = state.status;
    m_findPrevButton      = state.prevButton;
    m_findNextButton      = state.nextButton;
    m_findSelectionButton = state.selButton;
    m_findCloseButton     = state.closeButton;
    m_findBarHost         = state.host;
    m_findOpen            = state.open;
    m_findMatchCase       = state.matchCase;
    m_findWholeWord       = state.wholeWord;
    m_findRegex           = state.isRegex;
    m_findInSelection     = state.inSelection;
    m_findHistoryAt       = state.historyAt;
    m_findStatusText      = state.statusText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RecordFindHistory
//
//  The text just searched for goes to the end of its pane's history, moved
//  there if it was already in it, and the oldest goes once there are more
//  than kFindHistoryMax.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RecordFindHistory()
{
    std::vector<std::wstring>  & history = m_findStates[m_findPane].history;
    const std::wstring         & text    = m_findBox->GetText();



    if (text.empty())
    {
        return;
    }

    std::erase (history, text);
    history.push_back (text);

    if (history.size() > kFindHistoryMax)
    {
        history.erase (history.begin());
    }

    m_findHistoryAt = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindHistory
//
//  Oldest first.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DebuggerWindow::GetFindHistory (const std::wstring & pane) const
{
    auto  it = m_findStates.find (pane);



    return (it != m_findStates.end()) ? it->second.history : std::vector<std::wstring>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::StepFindHistory
//
//  Up and Down in the find box, as in Visual Studio Code's: a step back
//  (-1) shows the text searched for before, a step forward (+1) the one
//  after, and stepping forward past the newest empties the box. Returns
//  false with no history to step through.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::StepFindHistory (int step)
{
    const std::vector<std::wstring>  & history = m_findStates[m_findPane].history;
    int                                count   = (int) history.size();
    int                                at      = (m_findHistoryAt < 0) ? count : m_findHistoryAt;



    if (count == 0)
    {
        return false;
    }

    at = std::clamp (at + step, 0, count);

    m_findHistoryAt = (at == count) ? -1 : at;
    m_findBox->SetText ((at == count) ? std::wstring() : history[(size_t) at]);
    m_findBox->SelectAll();

    Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetFindInSelection
//
//  On, the search keeps to what is selected in the pane's text when it is
//  turned on, as Visual Studio Code's does; with nothing selected it stays
//  the whole text. Shown emphasized while on.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetFindInSelection (bool on)
{
    DxuiTextView  * view = GetFindView();



    m_findInSelection = on;
    m_findSelectionButton->SetEmphasis (on);

    if (view != nullptr && on)
    {
        view->SetFindScopeToSelection();
    }
    else if (view != nullptr)
    {
        view->ClearFindScope();
    }

    m_findStatusText.clear();
    m_findStatus->SetText (m_findStatusText);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindFrame
//
//  The frame of the find bar's pane, which lays out its slot.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerPaneFrame * DebuggerWindow::GetFindFrame() const
{
    int  slot = GetSourceSlotOf (m_findPane);



    return (slot >= 0) ? m_sourceDocs[(size_t) slot].frame.get() : m_consoleFrame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MoveFindBar
//
//  Moves the find bar's controls into another window, as the memory bar
//  moves. Focus on one of them stays behind in the window it leaves, so it
//  goes to the command line there.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MoveFindBar (DxuiWindow * to)
{
    DxuiWindow                   * from     = m_findBarHost != nullptr ? m_findBarHost : this;
    std::vector<IDxuiControl *>    controls = GetFindControls();



    if (to == from)
    {
        return;
    }

    //  The plate goes first, so what sits on it paints over it.
    controls.insert    (controls.begin(), { m_findPlate, m_findChevronButton });
    controls.push_back (m_findStatus);

    for (IDxuiControl * control : controls)
    {
        if (m_focusMgr.GetFocusedControl() == control)
        {
            m_focusMgr.SetFocused (m_commandBox);
        }

        for (auto it = m_floatFocus.begin(); it != m_floatFocus.end(); )
        {
            if (it->second == control)
            {
                control->OnFocusChanged (false);
                it = m_floatFocus.erase (it);
                continue;
            }

            ++it;
        }

        std::unique_ptr<IDxuiControl>  owned = from->DetachChild (control);

        if (owned != nullptr)
        {
            (void) to->AttachChild (std::move (owned));
        }
    }

    m_findBarHost = to;

    //  The site's strips and drop zones paint over the panes, so it stays
    //  this window's last child.
    if (to == this)
    {
        std::unique_ptr<IDxuiControl>  site = DetachChild (m_dockSite);

        (void) AttachChild (std::move (site));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceFindBar
//
//  The widget floats at the top right of its pane's text, clear of the
//  scrollbar, as Visual Studio Code's does: the chevron, the box with the
//  three option toggles inside its right end, the count, the arrows, find
//  in selection and Close. It narrows with a narrow pane, the box giving up
//  the room first. With find closed, or the pane out of sight, it hides.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceFindBar()
{
    std::wstring               active = m_findPane;
    std::vector<std::wstring>  panes;



    if (m_findBox == nullptr)
    {
        return;
    }

    for (const auto & [pane, state] : m_findStates)
    {
        if (state.box != nullptr)
        {
            panes.push_back (pane);
        }
    }

    //  Each pane's widget is placed over its own pane, open or not.
    for (const std::wstring & pane : panes)
    {
        ActivateFind (pane);
        PlaceActiveFindBar();
    }

    ActivateFind (active);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceActiveFindBar
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceActiveFindBar()
{
    constexpr int    kButtonDip = 22;
    auto             px         = [this] (int dip) { return m_scaler.ToPx (dip); };
    DxuiTextView   * view       = GetFindView();
    bool             paneThere  = m_findPane == DebuggerLayout::kConsole || GetSourceSlotOf (m_findPane) >= 0;
    bool             shown      = m_findOpen && paneThere && view != nullptr && view->IsVisible();
    int              button     = px (kButtonDip);
    RECT             area       = {};
    RECT             plate      = {};
    int              top        = 0;
    int              bottom     = 0;
    int              x          = 0;
    int              right      = 0;
    int              boxRight   = 0;



    if (m_findBox == nullptr)
    {
        return;
    }

    SetFindBarVisible (shown);

    if (!shown)
    {
        return;
    }

    //  The widget is the pane's, so it goes with the pane into a floating
    //  window.
    MoveFindBar (GetPaneHost (m_findPane));

    area         = view->GetBounds();
    plate.right  = area.right - px (kFindWidgetScrollDip);
    plate.left   = (std::max) (area.left + px (4), plate.right - px (kFindWidgetWidthDip));
    plate.top    = area.top;
    plate.bottom = area.top + px (kFindWidgetHeightDip);

    m_findPlate->Layout (plate, m_scaler);

    top    = plate.top    + px (4);
    bottom = plate.bottom - px (4);
    x      = plate.left   + px (2);
    right  = plate.right  - px (4);

    //  From the right: Close, find in selection, the arrows and the count.
    m_findCloseButton->Layout     (RECT { right - button, top, right, bottom }, m_scaler);  right -= button + px (2);
    m_findSelectionButton->Layout (RECT { right - button, top, right, bottom }, m_scaler);  right -= button;
    m_findNextButton->Layout      (RECT { right - button, top, right, bottom }, m_scaler);  right -= button;
    m_findPrevButton->Layout      (RECT { right - button, top, right, bottom }, m_scaler);  right -= button + px (4);
    m_findStatus->Layout          (RECT { right - px (kFindCountDip), top, right, bottom }, m_scaler);
    right -= px (kFindCountDip) + px (4);

    //  From the left: the chevron, then the box, with the toggles inside it.
    m_findChevronButton->Layout (RECT { x, top, x + px (16), bottom }, m_scaler);  x += px (16) + px (2);

    boxRight = (std::max) (x + button * 3, right);

    m_findBox->Layout         (RECT { x, top, boxRight, bottom }, m_scaler);
    m_findRegexButton->Layout (RECT { boxRight - px (2) - button,     top + px (2), boxRight - px (2),              bottom - px (2) }, m_scaler);
    m_findWordButton->Layout  (RECT { boxRight - px (2) - button * 2, top + px (2), boxRight - px (2) - button,     bottom - px (2) }, m_scaler);
    m_findCaseButton->Layout  (RECT { boxRight - px (2) - button * 3, top + px (2), boxRight - px (2) - button * 2, bottom - px (2) }, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenFind
//
//  Opens the find bar of the pane with the keys -- the console or a source
//  document -- and brings that pane forward with the keys in the box, its
//  text selected so typing replaces it. A selection in the pane's text on
//  one line becomes the text to find, as it does in Visual Studio. Each pane
//  has its own widget, so one open in another pane stays open.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenFind()
{
    OpenFindIn (GetFindTarget());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenFindIn
//
//  Opens the find bar in the pane given, as the search button on that
//  pane's toolbar does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenFindIn (const std::wstring & target)
{
    std::wstring  saved    = m_routingPane;
    std::wstring  selected;



    //  Each pane keeps its own widget, text, options and history.
    ActivateFind (target);

    selected = GetFindView()->GetSelectionText();

    m_findBox->SetPlaceholder ((m_findPane == DebuggerLayout::kConsole) ? L"Find in the console" : L"Find in the source");

    if (!selected.empty() && selected.find (L'\n') == std::wstring::npos)
    {
        m_findBox->SetText (selected);
    }

    m_findOpen = true;

    if (m_dockSite != nullptr)
    {
        m_dockSite->ActivatePane (m_findPane);
    }

    if (GetFindFrame() != nullptr)
    {
        GetFindFrame()->Relayout();
    }

    PlaceFindBar();
    m_findBox->SelectAll();

    //  A floating pane keeps its own focus.
    m_routingPane = m_floats.contains (m_findPane) ? m_findPane : std::wstring();
    SetFocusedControl (m_findBox);
    m_routingPane = saved;

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CloseFind
//
//  The match stays selected in the pane's text; the keys go back to the
//  command line from the console's bar, and to the text from a source
//  document's.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CloseFind()
{
    std::wstring  saved = m_routingPane;



    m_findOpen = false;
    m_findStatusText.clear();
    m_findStatus->SetText (m_findStatusText);

    if (GetFindFrame() != nullptr)
    {
        GetFindFrame()->Relayout();
    }

    PlaceFindBar();

    m_routingPane = m_floats.contains (m_findPane) ? m_findPane : std::wstring();
    SetFocusedControl ((m_findPane == DebuggerLayout::kConsole) ? (IDxuiControl *) m_commandBox : (IDxuiControl *) GetFindView());
    m_routingPane = saved;

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetFindOptions
//
//  Match case, match whole word and regular expression, each shown
//  emphasized while on. With none on, the text is found as a plain
//  substring. The last count no longer holds, so it is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetFindOptions (bool matchCase, bool wholeWord, bool isRegex)
{
    m_findMatchCase = matchCase;
    m_findWholeWord = wholeWord;
    m_findRegex     = isRegex;

    m_findCaseButton->SetEmphasis  (matchCase);
    m_findWordButton->SetEmphasis  (wholeWord);
    m_findRegexButton->SetEmphasis (isRegex);

    m_findStatusText.clear();
    m_findStatus->SetText (m_findStatusText);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FindInPane
//
//  With nothing to find yet, this opens the bar to ask for it. Otherwise the
//  next or previous match is selected in the find bar's pane, whether or not
//  the bar is open, as F3 finds again in Visual Studio after its find is
//  closed.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::FindInPane (bool forward)
{
    std::wstring              target = GetFindTarget();
    std::wstring              needle;
    DxuiTextView::FindResult  result = DxuiTextView::FindResult::NotFound;
    int                       index  = 0;
    int                       count  = 0;



    //  F3 searches the text with the keys, if it can; from elsewhere, the
    //  pane whose widget is open.
    if (target != m_findPane && (!m_findOpen || GetPaneOfFocus() == target))
    {
        ActivateFind (target);
    }

    needle = m_findBox->GetText();

    if (needle.empty())
    {
        OpenFind();
        return;
    }

    RecordFindHistory();

    result           = GetFindView()->SelectMatch (needle, m_findMatchCase, m_findWholeWord, m_findRegex, forward, index, count);
    m_findStatusText = GetFindStatusText (result, index, count);
    m_findStatus->SetText (m_findStatusText);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindStatusText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetFindStatusText (DxuiTextView::FindResult result, int index, int count)
{
    if (result == DxuiTextView::FindResult::NotFound || count == 0)
    {
        return L"No results";
    }

    return std::format (L"{} of {}", index, count);
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

    Hide();

    for (const auto & entry : m_floats)
    {
        entry.second->Hide();
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
    int   pad      = px (8);
    //  The strip gets the thickness the toolbar is drawn for. Given less, it
    //  keeps its fixed margin and shrinks the buttons instead, which leaves
    //  a squeezed hover pill floating in air.
    int   buttonH  = px (DxuiToolbar::GetBandDip());
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
    int   barY     = 0;
    int   x        = pad;



    if (m_codeList == nullptr || m_dockSite == nullptr)
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

    //  The command bar takes a band along the edge it is docked to, at its
    //  place along that edge, as long as its entries need or the edge
    //  allows; the panes fill the rest.
    m_commandBar->SetTextRenderer   (GetTextRenderer());
    m_commandBar->SetHostClientRect (RECT { 0, 0, width, height });
    m_commandBar->SetVertical       (m_barDock.IsVertical());

    {
        int   natural = m_commandBar->GetNaturalLengthPx (m_scaler);
        int   edgeLen = m_barDock.IsVertical() ? height - rowY : width - pad * 2;
        int   length  = (std::min) (natural, edgeLen);
        int   offset  = CommandBarDock::ClampOffset (px (m_barDock.offsetDip), edgeLen, length);
        int   left    = pad;
        int   right   = width - pad;
        RECT  bar     = {};

        top       = rowY;
        barY      = height - pad;
        m_barArea = RECT { 0, rowY, width, height };

        switch (m_barDock.edge)
        {
        case CommandBarDock::Edge::Top:
            bar  = RECT { pad + offset, rowY, pad + offset + length, rowY + buttonH };
            top  = rowY + buttonH;
            break;

        case CommandBarDock::Edge::Bottom:
            bar  = RECT { pad + offset, height - buttonH, pad + offset + length, height };
            barY = height - buttonH;
            break;

        case CommandBarDock::Edge::Left:
            bar  = RECT { 0, rowY + offset, buttonH, rowY + offset + length };
            left = buttonH;
            break;

        case CommandBarDock::Edge::Right:
            bar   = RECT { width - buttonH, rowY + offset, width, rowY + offset + length };
            right = width - buttonH;
            break;
        }

        m_commandBar->Layout (bar, m_scaler);
        m_tooltip.SetDpi          (m_scaler.GetDpi());
        m_tooltip.SetViewportSize (width, height);

        m_dockSite->Layout (RECT { left, top, right, barY }, m_scaler);
    }

    UpdateCodeLines();
    PlaceMemoryBar();
    PlaceBreakpointBar();
    PlaceConsoleBar();
    PlaceSourceBars();
    PlaceFindBar();
    ClipPaneControls();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UpdateCodeLines
//
//  Each disassembly view holds as many lines as it has room for, so a pane is
//  full whatever height it is dragged to. Called every frame; only a change
//  is sent, since the count crosses to the CPU thread, which rebuilds the
//  snapshot. A view out of sight measures nothing and keeps its count until
//  it is shown.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UpdateCodeLines()
{
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        int  fits = m_codeLists[(size_t) view]->GetVisibleRowCapacity();

        if (m_codeOpen[(size_t) view] && fits > 0 && fits != m_codeLinesSentTo[(size_t) view] && m_host != nullptr)
        {
            m_codeLinesSentTo[(size_t) view] = fits;
            m_host->SetDebuggerCodeLines (fits, view);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceMemoryBar
//
//  The memory bar sits in the bar of the memory pane it acts on: the active
//  one when it is shown here, or else the first memory pane shown. With none
//  shown in this window, it hides.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceMemoryBar()
{
    DebuggerPaneFrame  * bar    = nullptr;
    MemoryPane         * owner  = GetActiveMemoryPane();
    DxuiWindow         * host   = nullptr;
    RECT                 slot   = {};
    auto                 isHere = [this] (size_t i)
    {
        return m_memoryBars[i] != nullptr && m_memoryBars[i]->IsVisible();
    };



    if (owner != nullptr && isHere ((size_t) (owner->GetId() - 1)))
    {
        bar             = m_memoryBars[(size_t) (owner->GetId() - 1)].get();
        m_memoryBarPane = DebuggerLayout::GetMemoryPaneId (owner->GetId());
    }

    for (size_t i = 0; bar == nullptr && i < m_memoryBars.size(); i++)
    {
        if (isHere (i))
        {
            bar             = m_memoryBars[i].get();
            m_memoryBarPane = DebuggerLayout::GetMemoryPaneId ((int) i + 1);
        }
    }

    //  The bar is the pane's, so it goes with the pane into a floating
    //  window; with no pane to sit in, it waits in this one.
    host = (bar != nullptr) ? GetPaneHost (m_memoryBarPane) : this;
    MoveMemoryBar (host);

    if (bar == nullptr)
    {
        m_memoryBarPane.clear();
    }

    m_memoryBar->SetVisible (bar != nullptr);

    //  A hidden box keeps no focus, or it would go on taking the keys; they
    //  go back to the command line, as they do when the find bar closes.
    m_memoryBox->SetVisible (bar != nullptr);

    if (bar == nullptr && m_focusMgr.GetFocusedControl() == m_memoryBox)
    {
        m_focusMgr.SetFocused (m_commandBox);
    }

    if (bar == nullptr)
    {
        return;
    }

    //  The strip lays out its own entries, the Address box among them, and
    //  moves what does not fit into its "..." menu.
    slot = bar->GetBounds();

    m_memoryBar->SetTextRenderer   (host->GetTextRenderer());
    m_memoryBar->SetPopupHost      (host->GetPopupHost());
    m_memoryBar->SetHostClientRect (host->GetBounds());
    m_memoryBar->Layout            (slot, m_scaler);

    host->SetChildClip (m_memoryBar, slot);
    host->SetChildClip (m_memoryBox, slot);
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
    auto  found = m_floats.find (pane);



    return (found != m_floats.end()) ? (DxuiWindow *) found->second.get() : (DxuiWindow *) this;
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
    auto  found = m_floats.find (m_routingPane);



    return (!m_routingPane.empty() && found != m_floats.end()) ? found->second->GetPopupHost() : GetPopupHost();
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
    auto  found = m_floatTips.find (m_routingPane);



    return (!m_routingPane.empty() && found != m_floatTips.end()) ? *found->second : m_tooltip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TickFloats
//
//  Each floating window's context menu and tooltip run on the ticks this
//  window supplies, as its own do; without them a menu stays at the first
//  frame of its reveal, one line high.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::TickFloats (int64_t now)
{
    RECT  client = {};



    for (const auto & [pane, window] : m_floats)
    {
        DxuiHwndSource  * host = window->GetPopupHost();
        auto              tip  = m_floatTips.find (pane);

        if (host != nullptr && host->GetContextMenu().WantsTick())
        {
            host->GetContextMenu().Tick (now);
        }

        if (tip == m_floatTips.end())
        {
            continue;
        }

        if (window->GetHwnd() != nullptr && GetClientRect (window->GetHwnd(), &client))
        {
            tip->second->SetDpi          (GetDpiForWindow (window->GetHwnd()));
            tip->second->SetViewportSize (client.right - client.left, client.bottom - client.top);
        }

        if (tip->second->WantsTick())
        {
            tip->second->Tick (now);
        }
    }
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
    return (!pane.empty() && m_floats.contains (pane)) ? pane : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MoveMemoryBar
//
//  Moves the memory bar and its Address box into another window. Focus on
//  the box stays behind in the window it leaves, so it goes to the command
//  line there, as it does when the box hides.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MoveMemoryBar (DxuiWindow * to)
{
    DxuiWindow  * from = m_memoryBarHost != nullptr ? m_memoryBarHost : this;



    if (to == from)
    {
        return;
    }

    if (m_focusMgr.GetFocusedControl() == m_memoryBox)
    {
        m_focusMgr.SetFocused (m_commandBox);
    }

    for (auto it = m_floatFocus.begin(); it != m_floatFocus.end(); )
    {
        if (it->second == m_memoryBox)
        {
            m_memoryBox->OnFocusChanged (false);
            it = m_floatFocus.erase (it);
            continue;
        }

        ++it;
    }

    for (IDxuiControl * control : { (IDxuiControl *) m_memoryBar, (IDxuiControl *) m_memoryBox })
    {
        std::unique_ptr<IDxuiControl>  owned = from->DetachChild (control);

        if (owned != nullptr)
        {
            (void) to->AttachChild (std::move (owned));
        }
    }

    m_memoryBarHost = to;

    //  The site's strips and drop zones paint over the panes, so it stays
    //  this window's last child.
    if (to == this)
    {
        std::unique_ptr<IDxuiControl>  site = DetachChild (m_dockSite);

        (void) AttachChild (std::move (site));
    }

    m_focusMgr.Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClipPaneControls
//
//  Each pane's controls paint inside the pane, so nothing a control draws
//  past its edge lands on a neighbor.
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

        for (IDxuiControl * control : GetPaneControls (pane))
        {
            SetChildClip (control, clip);
        }
    }
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



    m_closedPanes.clear();

    if (m_host == nullptr)
    {
        return {};
    }

    savedText  = SourcePathList::Utf8ToWide (m_host->GetDebuggerLayout());
    layoutText = DebuggerLayout::TakeClosedPanes (savedText, legacy);

    DebuggerLayout::ReadClosedPanes (SourcePathList::Utf8ToWide (m_host->GetDebuggerClosedPanes()), m_closedPanes);

    m_barDock = CommandBarDock::FromText (SourcePathList::Utf8ToWide (m_host->GetDebuggerCommandBarDock()));

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
    std::wstring    savedText;
    DxuiPaneLayout  restored;



    //  A source document is its toolbar and its banner over its text. Its
    //  find widget floats over the text (PlaceFindBar).
    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        SourceDocument  & document = m_sourceDocs[(size_t) slot];
        SourceDocument  * each     = &document;
        std::wstring      id       = DebuggerLayout::GetSourcePaneId (slot);

        document.frame    = std::make_unique<DebuggerPaneFrame> (L"Source");
        document.barSlot  = std::make_unique<DebuggerPaneFrame> (L"Source commands");
        document.frame->AddPart (document.barSlot.get(), barHeight);
        document.frame->AddPart (document.banner,
                                 [each] (int width, const DxuiDpiScaler & scaler) { return (int) each->banner->GetPreferredHeightPx ((float) width, scaler); },
                                 [each] { return each->bannerShown; });
        document.frame->AddPart (document.view);
    }

    m_consoleFrame = std::make_unique<DebuggerPaneFrame> (L"Console");
    //  The console's toolbar, which holds its dialect, is a place held at the
    //  pane's top, which PlaceConsoleBar fills.
    m_consoleBarSlot = std::make_unique<DebuggerPaneFrame> (L"Console commands");
    m_consoleFrame->AddPart (m_consoleBarSlot.get(), barHeight);

    m_consoleFrame->AddPart (m_consoleView);
    m_consoleFrame->AddPart (m_commandBox, boxHeight);
    m_consoleFrame->SetBottomMarginDip (kPanePadDip + 2);

    m_callStackFrame = std::make_unique<DebuggerPaneFrame> (L"Call stack");
    //  The pane always shows hybrid; CALLS MODE picks another (FR-068), so
    //  the button that cycled them is not shown.
    m_callStackButton->SetVisible (false);
    m_callStackFrame->AddPart (m_callStackList);

    //  The trace pane lists its keys above its rows.
    m_traceFrame = std::make_unique<DebuggerPaneFrame> (L"Trace");
    m_traceFrame->AddPart (m_traceHint, [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (kTraceHintDip); });
    m_traceFrame->AddPart (m_traceList);

    //  The breakpoints pane is its toolbar over its rows (FR-119). The bar is a
    //  place held at the pane's top, which PlaceBreakpointBar fills.
    m_breakpointSlot  = std::make_unique<DebuggerPaneFrame> (L"Breakpoint commands");
    m_breakpointFrame = std::make_unique<DebuggerPaneFrame> (L"Breakpoints");
    m_breakpointFrame->AddPart (m_breakpointSlot.get(), barHeight);
    m_breakpointFrame->AddPart (m_breakpointList);

    //  A disassembly view is a frame over its lines.
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        m_codeFrames[(size_t) view] = std::make_unique<DebuggerPaneFrame> (std::format (L"Disassembly {}", view + 1));
        m_codeFrames[(size_t) view]->AddPart (m_codeLists[(size_t) view]);

        m_dockSite->AddPane (DebuggerLayout::GetCodePaneId (view), std::format (L"Disassembly {}", view + 1),
                             m_codeFrames[(size_t) view].get());
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        m_dockSite->AddPane (DebuggerLayout::GetSourcePaneId (slot), L"Source", m_sourceDocs[(size_t) slot].frame.get());
    }

    m_dockSite->AddPane (DebuggerLayout::kConsole,     L"Console",     m_consoleFrame.get());
    m_dockSite->AddPane (DebuggerLayout::kRegisters,   L"Registers",   m_registerList);
    m_dockSite->AddPane (DebuggerLayout::kBreakpoints, L"Breakpoints", m_breakpointFrame.get());
    m_dockSite->AddPane (DebuggerLayout::kWatches,     L"Watches",     m_watchList);
    m_dockSite->AddPane (DebuggerLayout::kStack,       L"Stack",       m_stackList);
    m_dockSite->AddPane (DebuggerLayout::kCallStack,   L"Call stack",  m_callStackFrame.get());
    m_dockSite->AddPane (DebuggerLayout::kTrace,       L"Trace",       m_traceFrame.get());

    //  A memory pane is its command bar over its bytes (FR-089). The bar is a
    //  place held at the pane's top; the controls, shared by every memory
    //  pane, are put in the bar of the one they act on (PlaceMemoryBar).
    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        size_t  i = (size_t) (pane->GetId() - 1);

        m_memoryBars[i]   = std::make_unique<DebuggerPaneFrame> (L"Memory commands");
        m_memoryFrames[i] = std::make_unique<DebuggerPaneFrame> (std::format (L"Memory {}", pane->GetId()));
        m_memoryFrames[i]->AddPart (m_memoryBars[i].get(), barHeight);
        m_memoryFrames[i]->AddPart (pane->GetView());

        m_dockSite->AddPane (DebuggerLayout::GetMemoryPaneId (pane->GetId()),
                             std::format (L"Memory {}", pane->GetId()), m_memoryFrames[i].get());
    }

    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        m_dockSite->AddPane (DebuggerLayout::GetDiagnosticsPaneId (pane->GetId()), pane->GetTitle(), pane->GetFrame());
    }

    m_dockSite->SetShownFn    ([this] (const std::wstring & pane) { return IsPaneShown (pane); });

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
        SetTopLayer (pane.empty() ? std::vector<IDxuiControl *>() : GetPaneControls (pane));
    });
    savedText = ReadSavedLayout();
    std::erase_if (m_closedPanes, [this] (const std::wstring & pane) { return !IsFixedPane (pane); });

    restored = DebuggerLayout::Restore (savedText);
    restored.PlaceOnMonitors (GetMonitors());
    m_dockSite->SetPaneLayout (restored);
    m_syncFloats = true;

    m_dockSite->SetOnFloatRequested ([this] (const std::wstring & pane, POINT clientPx) { RequestFloat (pane, clientPx); });
    m_dockSite->SetOnTearOff        ([this] (const std::wstring & pane, POINT clientPx) { TearOffPane  (pane, clientPx); });

    //  Every change the user makes is saved as it happens, so a crash or a
    //  closed emulator loses nothing.
    m_dockSite->SetOnChanged ([this]
    {
        m_syncFloats = true;
        SaveLayout();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsPaneShown
//
//  The source pane shows while a debug file is loaded, a memory window while
//  it is open, and a device panel while its device is present and its panel
//  open (FR-044); the rest show until their close buttons close them.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsPaneShown (const std::wstring & pane) const
{
    std::string  diagnosticsId;



    if (m_closedPanes.contains (pane))
    {
        return false;
    }

    if (GetSourceSlotOf (pane) >= 0)
    {
        return m_sourceDocs[(size_t) GetSourceSlotOf (pane)].shown;
    }

    for (int view = 1; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            return m_codeOpen[(size_t) view];
        }
    }

    if (DebuggerLayout::TryGetDiagnosticsId (pane, diagnosticsId))
    {
        return m_diagOpen.contains (diagnosticsId);
    }

    for (const std::unique_ptr<MemoryPane> & memory : m_memoryPanes)
    {
        if (pane == DebuggerLayout::GetMemoryPaneId (memory->GetId()))
        {
            return m_memoryOpen[(size_t) (memory->GetId() - 1)];
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PaintTopLayer
//
//  The slid-out pane over the page: its background, its controls, then its
//  title bar and outline.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PaintTopLayer (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    //  Under a slid-out pane, which lies over the disassembly and the source.
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews + (int) m_sourceDocs.size(); view++)
    {
        PaintBranchArrow (painter, view);
    }

    m_dockSite->PaintSlidUnder (painter, theme);
    DxuiWindow::PaintTopLayer  (painter, text, theme);
    m_dockSite->PaintSlidOver  (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HasTopLayer
//
//  A slid-out pane, or the PC on a branch with its arrow to draw. The top
//  layer is a second flush of the frame, so it is asked for only then.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::HasTopLayer() const
{
    if (DxuiWindow::HasTopLayer())
    {
        return true;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        for (const DebuggerViewSnapshot::CodeLine & line : GetCodeLines (view))
        {
            if (m_codeOpen[(size_t) view] && line.isCurrent && line.target.has_value())
            {
                return true;
            }
        }
    }

    for (const SourceDocument & document : m_sourceDocs)
    {
        if (document.shown && document.pane->IsActive() && m_snapshot != nullptr &&
            m_snapshot->source.has_value() && m_snapshot->source->pcTarget.has_value())
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBranchArrow
//
//  Where the PC's branch, jump or call arrow lies in a view, in pixels, and
//  where it goes. Either end scrolled out of view runs the line to that edge
//  of the rows, so the arrow stays drawn while any part of it crosses them.
//  A view past the code views is a source document's, whose pane places it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::GetBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken) const
{
    static constexpr size_t                              s_kInstructionColumn = 5;
    DxuiListView                                       * list                 = (view < DebuggerViewState::kMaxCodeViews) ? m_codeLists[(size_t) view] : nullptr;
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines                = GetCodeLines (view);
    int                                                  current              = -1;
    int                                                  target               = -1;
    int                                                  firstRow             = 0;
    int                                                  endRow               = 0;
    RECT                                                 bounds               = {};
    RECT                                                 column               = {};
    RECT                                                 cell                 = {};
    float                                                scale                = 1.0f;
    float                                                top                  = 0.0f;
    float                                                bottom               = 0.0f;
    int                                                  rowPx                = 0;



    if (view >= DebuggerViewState::kMaxCodeViews)
    {
        const SourceDocument & document = m_sourceDocs[(size_t) (view - DebuggerViewState::kMaxCodeViews)];

        return document.shown && IsRoutable (document.view) && document.pane->GetBranchArrow (input, goesTo, isTaken);
    }

    if (list == nullptr || !m_codeOpen[(size_t) view] || !list->IsVisible() || !IsRoutable (list))
    {
        return false;
    }

    for (size_t i = 0; i < lines.size(); i++)
    {
        current = lines[i].isCurrent ? (int) i : current;
    }

    firstRow = list->GetTopRow();

    if (current < 0 || !lines[(size_t) current].target.has_value() ||
        !list->GetCellTextRectPx (firstRow, s_kInstructionColumn, column))
    {
        return false;
    }

    goesTo  = *lines[(size_t) current].target;
    isTaken = lines[(size_t) current].isTargetTaken;

    for (size_t i = 0; i < lines.size(); i++)
    {
        target = (lines[i].address == goesTo) ? (int) i : target;
    }

    bounds = list->GetBounds();
    rowPx  = column.bottom - column.top;
    scale  = (float) rowPx / (float) (std::max) (1, list->GetRowHeightDip());
    endRow = firstRow + (std::min) (list->GetVisibleRowCapacity(), list->GetRowCount() - firstRow);
    top    = (float) (bounds.top + column.top);
    bottom = top + (float) ((endRow - firstRow) * rowPx);

    input.mnemonicX     = (float) (bounds.left + column.left);
    input.isTargetBelow = (target >= 0) ? target > current : goesTo > lines[(size_t) current].address;
    input.edgeY         = input.isTargetBelow ? bottom : top;
    input.sourceEdgeY   = input.isTargetBelow ? top : bottom;
    input.marginPx     *= scale;
    input.stubPx       *= scale;
    input.radiusPx     *= scale;
    input.headPx       *= scale;

    //  A source out of view counts only while the line runs on across the
    //  rows: above them toward a target below, or below toward one above.
    if (current < firstRow || current >= endRow)
    {
        bool  above = current < firstRow;

        if (above != input.isTargetBelow ||
            (target >= 0 && (above ? target < firstRow : target >= endRow)))
        {
            return false;
        }
    }
    else if (list->GetCellTextRectPx (current, s_kInstructionColumn, cell))
    {
        input.sourceY = (float) (bounds.top + (cell.top + cell.bottom) / 2);
    }

    if (target >= 0 && list->GetCellTextRectPx (target, s_kInstructionColumn, cell))
    {
        input.targetY = (float) (bounds.top + (cell.top + cell.bottom) / 2);
    }

    //  Scrolled sideways past the mnemonics, there is no room for it.
    return input.mnemonicX - input.marginPx - input.stubPx >= (float) bounds.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PaintBranchArrow
//
//  The arrow from GetBranchArrow, in the PC marker's color, or in the
//  disabled color for a branch the flags as they stand will not take.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PaintBranchArrow (IDxuiPainter & painter, int view)
{
    BranchArrow::Input   input;
    BranchArrow::Result  arrow;
    Word                 goesTo  = 0;
    bool                 isTaken = true;
    uint32_t             argb    = 0;
    float                width   = 1.5f;



    if (!GetBranchArrow (view, input, goesTo, isTaken))
    {
        return;
    }

    argb  = isTaken ? GetPcMarkerArgb() : ((m_theme != nullptr) ? m_theme->ForegroundDisabled() : 0xFF808080);
    width = 1.5f * input.radiusPx / BranchArrow::Input().radiusPx;
    arrow = BranchArrow::Build (input);

    for (const BranchArrow::Segment & segment : arrow.segments)
    {
        painter.DrawLine (segment.x0, segment.y0, segment.x1, segment.y1, width, argb);
    }

    if (arrow.hasHead)
    {
        painter.FillConvexQuad (arrow.head[0], arrow.head[1], arrow.head[2], arrow.head[3],
                                arrow.head[4], arrow.head[5], arrow.head[4], arrow.head[5], argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClickBranchArrow
//
//  A double-click on a view's branch arrow goes to where it points, as Go to
//  would. The second press within the system's double-click time and
//  distance of the first counts as the double-click.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ClickBranchArrow (POINT pointPx)
{
    BranchArrow::Input  input;
    Word                goesTo  = 0;
    bool                isTaken = true;
    DWORD               now     = GetTickCount();
    bool                isPair  = false;



    for (int view = 0; view < DebuggerViewState::kMaxCodeViews + (int) m_sourceDocs.size(); view++)
    {
        input = BranchArrow::Input();

        if (!GetBranchArrow (view, input, goesTo, isTaken) ||
            !BranchArrow::HitTest (input, (float) pointPx.x, (float) pointPx.y))
        {
            continue;
        }

        isPair = m_arrowPressTick != 0 && now - m_arrowPressTick <= GetDoubleClickTime() &&
                 std::abs (pointPx.x - m_arrowPressPx.x) <= GetSystemMetrics (SM_CXDOUBLECLK) &&
                 std::abs (pointPx.y - m_arrowPressPx.y) <= GetSystemMetrics (SM_CYDOUBLECLK);

        m_arrowPressTick = isPair ? 0 : now;
        m_arrowPressPx   = pointPx;

        if (isPair)
        {
            m_activeCode = (view < DebuggerViewState::kMaxCodeViews) ? view : m_activeCode;
            ShowCode (goesTo);
        }

        return isPair;
    }

    m_arrowPressTick = 0;

    return false;
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

    for (int window = 2; window <= DebuggerViewState::kMaxMemoryWindows; window++)
    {
        if (pane == DebuggerLayout::GetMemoryPaneId (window))
        {
            return false;
        }
    }

    return GetSourceSlotOf (pane) < 0 && !DebuggerLayout::TryGetDiagnosticsId (pane, diagnosticsId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetViewMenuPanes
//
//  What the View menu lists, in its order: the Disassembly views and the
//  open source documents, then the tool windows, then the device panels of
//  the machine's devices.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DebuggerWindow::GetViewMenuPanes() const
{
    std::vector<std::wstring>  panes;



    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        panes.push_back (DebuggerLayout::GetCodePaneId (view));
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        if (m_documents.IsOpen (slot))
        {
            panes.push_back (DebuggerLayout::GetSourcePaneId (slot));
        }
    }

    panes.push_back (DebuggerLayout::kRegisters);
    panes.push_back (DebuggerLayout::kStack);
    panes.push_back (DebuggerLayout::kCallStack);

    for (int window = 1; window <= DebuggerViewState::kMaxMemoryWindows; window++)
    {
        panes.push_back (DebuggerLayout::GetMemoryPaneId (window));
    }

    panes.push_back (DebuggerLayout::kBreakpoints);
    panes.push_back (DebuggerLayout::kWatches);
    panes.push_back (DebuggerLayout::kTrace);
    panes.push_back (DebuggerLayout::kConsole);

    if (m_snapshot != nullptr)
    {
        for (const DebuggerViewSnapshot::PanelInfo & panel : m_snapshot->panels)
        {
            panes.push_back (DebuggerLayout::GetDiagnosticsPaneId (panel.id));
        }
    }

    return panes;
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
    std::string  diagnosticsId;



    m_closedPanes.erase (pane);

    if (!IsPaneShown (pane))
    {
        for (int view = 1; view < DebuggerViewState::kMaxCodeViews; view++)
        {
            if (pane == DebuggerLayout::GetCodePaneId (view) && m_host != nullptr)
            {
                m_host->SetDebuggerCodeAddress ((m_snapshot != nullptr) ? m_snapshot->pc : (Word) 0, view);
            }
        }

        for (int window = 2; window <= DebuggerViewState::kMaxMemoryWindows; window++)
        {
            if (pane == DebuggerLayout::GetMemoryPaneId (window) && m_host != nullptr)
            {
                m_host->SetDebuggerMemoryWindow (window, m_memoryPanes[0]->GetTopAddress());
            }
        }

        if (DebuggerLayout::TryGetDiagnosticsId (pane, diagnosticsId))
        {
            RunAction (DebuggerActions::GetPanel (diagnosticsId, true, GetMode()));
        }
    }

    m_syncFloats = true;
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

    if (m_floats.contains (pane) && m_floats[pane]->GetHwnd() != nullptr)
    {
        m_floats[pane]->Show (true);
    }
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
    std::string  diagnosticsId;
    int          slot = GetSourceSlotOf (pane);



    if (IsFixedPane (pane))
    {
        m_closedPanes.insert (pane);
        m_syncFloats = true;
        m_dockSite->Relayout();
        SetWindowMenus();
        SaveLayout();
        Invalidate();
        return;
    }

    if (slot >= 0)
    {
        CloseSourceDocument (slot);
        return;
    }

    if (DebuggerLayout::TryGetDiagnosticsId (pane, diagnosticsId))
    {
        RunAction (DebuggerActions::GetPanel (diagnosticsId, false, GetMode()));
        return;
    }

    if (m_host == nullptr)
    {
        return;
    }

    for (int view = 1; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            m_host->CloseDebuggerCodeView (view);
            return;
        }
    }

    for (int window = 2; window <= DebuggerViewState::kMaxMemoryWindows; window++)
    {
        if (pane == DebuggerLayout::GetMemoryPaneId (window))
        {
            m_host->SetDebuggerMemoryWindow (window, std::nullopt);
            return;
        }
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



    if (!m_routingPane.empty())
    {
        return m_routingPane;
    }

    if (focused == nullptr)
    {
        return L"";
    }

    if (focused == m_consoleView || focused == m_commandBox || GetPaneOfControl (focused) == DebuggerLayout::kConsole)
    {
        return DebuggerLayout::kConsole;
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        if (focused == m_sourceDocs[(size_t) slot].view)
        {
            return DebuggerLayout::GetSourcePaneId (slot);
        }
    }

    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        if (focused == pane->GetList())
        {
            return DebuggerLayout::GetDiagnosticsPaneId (pane->GetId());
        }
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
//  a tab), All to new window and Close, with the Dock To choices of FR-042 in
//  a submenu. The one chosen runs through the site like a drop would.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowDockToMenu (const std::wstring & pane, POINT clientPx)
{
    RECT                                 tab     = {};
    std::wstring                         tip;
    bool                                 fromTab = m_routingPane.empty() && !m_dockSite->GetTabAt (clientPx, tab, tip).empty();
    std::vector<DxuiDockSite::MenuItem>  items   = m_dockSite->GetPaneMenu (pane, fromTab);
    std::vector<DxuiDockSite::MenuItem>  dockTo  = m_dockSite->GetDockToMenu (pane);
    std::vector<DxuiPopupMenuItem>       menu;
    std::vector<DxuiPopupMenuItem>       sides;
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
            m_menuCommands.push_back (MakeMenuCommand (L"Follow PC", false, [this, view] { if (m_host != nullptr) { m_host->SetDebuggerFollowView (view); } }));
            menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
        }

        if (!menu.empty())
        {
            menu.push_back (DxuiPopupMenuItem::ForSeparator());
        }
    }

    //  A source document's own items.
    if (slot >= 0 && m_documents.IsOpen (slot))
    {
        std::vector<DxuiPopupMenuItem>  syntax;
        SourcePane                    * source = m_sourceDocs[(size_t) slot].pane.get();



        //  Whose grammar colors the document, for a file its text misleads.
        for (SourceSyntax::Assembler each : { SourceSyntax::Assembler::Any, SourceSyntax::Assembler::As65,
                                              SourceSyntax::Assembler::Merlin, SourceSyntax::Assembler::Ca65 })
        {
            m_menuCommands.push_back (MakeMenuCommand (SourceSyntax::GetAssemblerLabel (each), source->GetAssemblerChoice() == each, [this, source, each]
            {
                source->SetAssembler (each);

                if (m_snapshot != nullptr)
                {
                    source->Apply (*m_snapshot);
                }
            }));

            syntax.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
        }

        m_menuCommands.push_back (MakeMenuCommand (L"Syntax", false, [] {}));
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

        m_menuCommands.push_back (MakeMenuCommand (item.label, false, [action = item.action] { if (action) { (void) action(); } }));
        m_menuCommands.back()->accelerator = item.accelerator;
        m_menuCommands.back()->isEnabled   = [enabled = item.enabled] { return enabled; };
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    //  The keyboard's Dock To choices, each edge and each other group, ahead
    //  of Close.
    for (const DxuiDockSite::MenuItem & item : dockTo)
    {
        if (!m_dockSite->GetPaneLayout().IsDocked (pane) || item.label == L"Auto hide" || item.label == L"Float")
        {
            continue;
        }

        m_menuCommands.push_back (MakeMenuCommand (item.label, false, [action = item.action] { (void) action(); }));
        sides.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    if (!sides.empty() && menu.size() >= 2)
    {
        m_menuCommands.push_back (MakeMenuCommand (L"Dock to", false, [] {}));
        menu.insert (menu.end() - 2, DxuiPopupMenuItem::ForSubmenu (m_menuCommands.back(), std::move (sides)));
    }

    DxuiContextMenu::Show (*host, clientPx.x, clientPx.y, std::move (menu));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowContentMenu
//
//  The actions on what was right-clicked: a line, a breakpoint, a watch, a
//  byte. Reports false when the point is not on the pane's content, which
//  leaves it to the pane's own menu.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ShowContentMenu (const std::wstring & pane, POINT clientPx)
{
    std::vector<std::pair<std::wstring, std::function<void()>>>  items;
    std::vector<DxuiPopupMenuItem>                               menu;
    MemoryPane                                                 * memory = nullptr;
    int                                                          row    = -1;
    RECT                                                         bounds = {};



    if (m_snapshot == nullptr || GetMenuHost() == nullptr)
    {
        return false;
    }

    //  The console's two parts are text: the command line edits, the output
    //  only copies.
    if (GetPaneOfControl (m_consoleView) == pane && m_commandBox->IsVisible() && DxuiDockSite::Contains (m_commandBox->GetBounds(), clientPx))
    {
        SetFocusedControl (m_commandBox);
        ShowEditMenu (m_commandBox, clientPx, {});
        return true;
    }

    if (GetPaneOfControl (m_consoleView) == pane && m_consoleView->IsVisible() && DxuiDockSite::Contains (m_consoleView->GetBounds(), clientPx))
    {
        ShowEditMenu (m_consoleView, clientPx, { { L"Clear", [this] { m_console.clear(); m_consoleView->SetRows ({}); } } });
        return true;
    }

    for (DxuiListView * list : GetLists())
    {
        bounds = list->GetBounds();

        if (GetPaneOfControl (list) != pane || !list->IsVisible() || !DxuiDockSite::Contains (bounds, clientPx))
        {
            continue;
        }

        row = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);

        if (row >= 0)
        {
            list->SetSelectedRow (row);
        }

        AddListMenuItems (list, row, GetColumnAt (list, clientPx.x - bounds.left), items);
    }

    for (MemoryPane * each : GetOpenMemoryPanes())
    {
        if (IsRoutable (each->GetView()) && DxuiDockSite::Contains (each->GetView()->GetBounds(), clientPx) && each->GetView()->IsVisible())
        {
            memory = each;
        }
    }


    if (memory != nullptr)
    {
        m_activePane = memory;
        items.push_back ({ L"Copy",         [memory] { memory->GetView()->CopySelection(); } });
        items.push_back ({ L"Go to...",     [this]   { SetFocusedControl (m_memoryBox); } });
        items.push_back ({ L"Change bytes per value", [memory] { (void) memory->CycleGrouping(); } });

        if (memory->CanUndo())
        {
            items.push_back ({ L"Undo", [this, memory] { UndoMemoryEdit (memory, false); } });
        }

        if (memory->CanRedo())
        {
            items.push_back ({ L"Redo", [this, memory] { UndoMemoryEdit (memory, true); } });
        }
    }

    if (items.empty())
    {
        return false;
    }

    m_menuCommands.clear();
    SetWindowMenus();

    for (auto & [label, action] : items)
    {
        m_menuCommands.push_back (MakeMenuCommand (label, false, action));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
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

        m_menuCommands.push_back (MakeMenuCommand (label, false, [this, control, command] { (void) control->InvokeCommand (command); Invalidate(); }));
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
        m_menuCommands.push_back (MakeMenuCommand (label, false, action));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    DxuiContextMenu::Show (*GetMenuHost(), clientPx.x, clientPx.y, std::move (menu));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetColumnAt
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::GetColumnAt (const DxuiListView * list, int xPx)
{
    int  right = -list->GetLeftPx();



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
    for (MemoryPane * pane : GetOpenMemoryPanes())
    {
        int  id = pane->GetId();

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



    if (GetCodeViewOf (list) >= 0 && row >= 0 && row < (int) GetCodeLines (GetCodeViewOf (list)).size())
    {
        int                                    view = GetCodeViewOf (list);
        const DebuggerViewSnapshot::CodeLine & line = GetCodeLines (view)[(size_t) row];
        Word                                   at   = line.address;

        //  What was right-clicked leads (FR-084): the instruction's operand,
        //  resolved through its addressing mode, or the line's own address;
        //  one entry for each memory pane open.
        if (column >= kCodeInstructionColumn && !line.memoryOperand.empty())
        {
            AddShowInMemory (Widen (line.shownOperand), line.memoryOperand, items);
        }
        else
        {
            AddShowInMemory (std::format (L"${:04X}", at), std::format ("{:04X}", at), items);
        }

        items.push_back ({ s.code[(size_t) row].hasBreakpoint ? L"Remove breakpoint" : L"Insert breakpoint",
                           [this, at] { RunAction (DebuggerActions::GetToggleBreakpoint (*m_snapshot, at, GetMode())); } });
        items.push_back ({ L"Run to cursor",       [this, at] { RunToCursor (at); } });
        items.push_back ({ L"Show next statement", [this]     { ShowCode (std::nullopt); } });
        items.push_back ({ L"Copy",                copy });

        if (view > 0)
        {
            items.push_back ({ std::format (L"Close Disassembly {}", view + 1), [this, view] { if (m_host != nullptr) { m_host->CloseDebuggerCodeView (view); } } });
        }
    }
    else if (list == m_breakpointList && GetBreakpointOfRow (row) != nullptr)
    {
        const DebuggerViewSnapshot::BreakpointLine  bp     = *GetBreakpointOfRow (row);
        int                                         fileId = -1;
        int                                         line   = 0;



        //  Go to source code for one set from source or at an address with a
        //  source line, and Go to disassembly for one at an address, as a
        //  double-click does (FR-119).
        if (BreakpointColumns::TryGetSourcePlace (s, bp, fileId, line))
        {
            items.push_back ({ L"Go to source code", [this, fileId, line] { OpenSourceDocument (fileId, line, true); } });
        }

        if (BreakpointColumns::HasAddress (bp.info))
        {
            items.push_back ({ L"Go to disassembly", [this, bp] { ShowCode (bp.address); } });
        }

        items.push_back ({ bp.enabled ? L"Disable" : L"Enable", [this, bp] { RunBreakpointStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEnableBreakpoint (bp.id, !bp.enabled, GetMode()) }, {} }); } });
        items.push_back ({ L"Remove",                           [this, bp] { RunBreakpointStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetClearBreakpoint (bp.id, GetMode()) }, {} }); } });

        //  Its type and the fields that type needs, in a dialog; the result is
        //  the definition BPEDIT takes, as a person could have typed it
        //  (FR-094).
        items.push_back ({ L"Edit...", [this, bp]
        {
            std::optional<std::string>  definition = BreakpointDialog::Ask (GetHwnd(), m_theme, bp.info);

            if (definition.has_value())
            {
                RunBreakpointStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEditBreakpoint (bp.id, *definition, GetMode()) }, {} });
            }
        } });
    }
    else if (list == m_watchList && row >= 0 && row < (int) m_watchRows.size())
    {
        //  The row map says what the row is: the pane mixes headings,
        //  automatic watches and the user's own, so a row number is not an
        //  index into any one of them.
        const WatchRow  what = m_watchRows[(size_t) row];

        if (what.kind == WatchRowKind::Manual)
        {
            auto  found = std::find_if (s.watches.begin(), s.watches.end(),
                                        [&what] (const DebuggerViewSnapshot::WatchLine & w) { return w.id == what.index; });

            if (found != s.watches.end())
            {
                const DebuggerViewSnapshot::WatchLine  watch = *found;

                AddShowInMemory (std::format (L"${:04X}", watch.address), std::format ("{:04X}", watch.address), items);
                items.push_back ({ L"Remove", [this, watch] { RunAction (DebuggerActions::GetClearWatch (watch.id, GetMode())); } });
            }
        }
        else if (what.kind == WatchRowKind::Automatic && what.index < (int) s.autoWatches.size() &&
                 s.autoWatches[(size_t) what.index].key.starts_with ("M:"))
        {
            std::string  hex = s.autoWatches[(size_t) what.index].key.substr (2);

            AddShowInMemory (L"$" + Widen (hex), hex, items);
        }

        if (what.kind != WatchRowKind::Heading)
        {
            items.push_back ({ L"Copy", copy });
        }

        if (m_watchHistory.CanUndo())
        {
            items.push_back ({ L"Undo", [this] { UndoWatchEdit (false); } });
        }

        if (m_watchHistory.CanRedo())
        {
            items.push_back ({ L"Redo", [this] { UndoWatchEdit (true); } });
        }
    }
    else if (list == m_stackList && row >= 0 && row < (int) s.stack.size())
    {
        Word  at = s.stack[s.stack.size() - 1 - (size_t) row].address;

        AddShowInMemory (std::format (L"${:04X}", at), std::format ("{:04X}", at), items);
        items.push_back ({ L"Copy", copy });

        if (s.isPaused)
        {
            items.push_back ({ L"Edit value...", [this, row] { EditStackByte (row); } });
        }

        if (m_stackHistory.CanUndo())
        {
            items.push_back ({ L"Undo", [this] { UndoStackEdit (false); } });
        }

        if (m_stackHistory.CanRedo())
        {
            items.push_back ({ L"Redo", [this] { UndoStackEdit (true); } });
        }
    }
    else if (list == m_callStackList && row >= 0 && row < (int) CallStackPane::GetRows (s.callStack).size())
    {
        Word  at = CallStackPane::GetRows (s.callStack)[(size_t) row].address;

        items.push_back ({ L"Show call site", [this, at] { ShowCode (at); } });
        items.push_back ({ L"Copy",           copy });
    }
    else if (list == m_registerList && row >= 0 && row < (int) s.registers.size())
    {
        std::string  name = s.registers[(size_t) row].name;

        if (name == "PC")
        {
            items.push_back ({ L"Show in code", [this] { ShowCode (std::nullopt); } });
        }

        AddShowInMemory (Widen (name), name, items);
        items.push_back ({ L"Copy",           copy });

        if (m_registerHistory.CanUndo())
        {
            items.push_back ({ L"Undo", [this] { UndoRegisterEdit (false); } });
        }

        if (m_registerHistory.CanRedo())
        {
            items.push_back ({ L"Redo", [this] { UndoRegisterEdit (true); } });
        }
    }
    else if (list == m_traceList)
    {
        items.push_back ({ L"Copy", copy });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RenderFrame
//
//  Once per UI frame: take whatever the CPU thread published, then repaint.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RenderFrame()
{
    std::shared_ptr<const DebuggerViewSnapshot>  snapshot;
    std::vector<std::string>                     console;
    int64_t                                      now = (int64_t) GetTickCount64();



    if (!IsCreated() || m_host == nullptr)
    {
        return;
    }

    if (m_host->TakeDebuggerUpdate (snapshot, console))
    {
        if (snapshot != nullptr)
        {
            TakeSnapshot (std::move (snapshot));
        }

        AppendConsole (console);
    }

    RefreshCommandGhost();

    for (DxuiListView * list : GetLists())
    {
        list->Tick (now);
    }

    //  Focus moves by click, key and command alike, so the group the user is
    //  working in is found once a frame rather than at each of them.
    m_dockSite->SetFocusedPane (GetPaneOfFocus());

    //  A disassembly view's height changes with a window resize, a sash drag,
    //  a tab brought forward, a pane slid out or floated, a text-size change,
    //  and each of those has been missed in turn. Measured once a frame, the
    //  view fills whatever room it has; only a change is sent.
    UpdateCodeLines();

    //  A drop-down slides open on ticks its host supplies. Without them the
    //  menu stayed at the first frame of its reveal, a sliver under the
    //  entry, and Panels, Dialect and Keys looked as if they did nothing.
    //  The content menus are the same.
    for (DxuiToolbar * strip : { m_commandBar, m_memoryBar, m_breakpointBar, m_consoleBar })
    {
        if (strip->WantsTick())
        {
            strip->TickMenus (now);
        }
    }

    for (SourceDocument & document : m_sourceDocs)
    {
        if (document.bar->WantsTick())
        {
            document.bar->TickMenus (now);
        }
    }

    if (m_menuBar->WantsTick())
    {
        m_menuBar->TickMenus (now);
    }

    //  A menu dismissed by a click outside closes in its own popup, so the
    //  theme it previewed is put back here.
    EndThemePreview();

    if (GetPopupHost() != nullptr && GetPopupHost()->GetContextMenu().WantsTick())
    {
        GetPopupHost()->GetContextMenu().Tick (now);
    }

    if (m_tooltip.WantsTick())
    {
        m_tooltip.Tick (now);
    }

    TickFloats (now);

    for (MemoryPane * pane : GetOpenMemoryPanes())
    {
        pane->FollowScroll();
        (void) pane->GetView()->TickScrollbars (now);
    }

    m_tracePane->FollowScroll();

    SyncFloats();
    CarryTornOffPane();
    PlaceMemoryBar();
    PlaceBreakpointBar();
    PlaceConsoleBar();
    PlaceSourceBars();
    PlaceFindBar();
    ClipPaneControls();

    for (SourceDocument & document : m_sourceDocs)
    {
        document.pane->FollowMarkedLine();
    }


    for (const auto & entry : m_floats)
    {
        entry.second->PollCaptionDrag();
        entry.second->Invalidate();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyCodeView
//
//  One disassembly view's rows: the gutter's breakpoints, the PC's arrow and
//  row wherever the PC is on its lines, the row another pane brought into
//  view, and a branch's destination.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyCodeView (int view)
{
    DxuiListView                                  * list     = m_codeLists[(size_t) view];
    std::vector<std::vector<DxuiListView::Cell>>    rows;
    int                                             current  = -1;
    int                                             selected = -1;
    std::optional<Word>                             target;
    SourceSyntax::Colors                            syntax   = GetSyntaxColors();



    for (const DebuggerViewSnapshot::CodeLine & line : GetCodeLines (view))
    {
        if (line.isCurrent && line.target.has_value())
        {
            target = line.target;
        }
    }

    for (size_t i = 0; i < GetCodeLines (view).size(); i++)
    {
        const DebuggerViewSnapshot::CodeLine & line   = GetCodeLines (view)[i];
        std::vector<DxuiListView::Cell>        cells;
        DxuiListView::Cell                     gutter;
        DxuiListView::Cell                     marker;
        uint32_t                               fill   = 0;

        //  The gutter holds a breakpoint's dot and the next column the PC's
        //  arrow, so a breakpoint on the PC's line shows both.
        if (line.hasBreakpoint)
        {
            gutter.icon = GetBreakpointIcon (line.isEnabled);
        }

        if (line.isCurrent)
        {
            marker.text = s_kpszTriangleRight;
            marker.argb = GetPcMarkerArgb();
            fill        = GetPcRowArgb();
            current     = (int) i;
        }
        else if (view == m_navigatedView && m_navigatedTo.has_value() && *m_navigatedTo == line.address)
        {
            fill = GetNavigatedRowArgb();
        }
        else if (target.has_value() && *target == line.address)
        {
            fill = GetTargetRowArgb();
        }

        cells = { gutter,
                  marker,
                  { std::format (L"{:04X}", line.address) },
                  { Widen (line.bytes) },
                  { Widen (line.label) },
                  { Widen (line.instruction) },
                  GetOperandAndResultCell (line.annotation, line.effect, GetResultArgb()) };

        cells[6].argb = GetAnnotationArgb();
        cells[4].argb = syntax.symbol;

        for (const SourceSyntax::Run & run : SourceSyntax::GetInstructionRuns (cells[5].text))
        {
            cells[5].colorRanges.push_back ({ run.start, run.start + run.length, syntax.Get (run.token) });
        }

        for (DxuiListView::Cell & cell : cells)
        {
            cell.background = fill;
        }

        rows.push_back (std::move (cells));
    }

    list->SetRows (std::move (rows));

    //  The selection is an instruction, not a row: new rows move it to the
    //  row that holds its address, or clear it when none does, so F9 and run
    //  to cursor act on what the user selected.
    if (list->GetSelectedRow() >= 0)
    {
        selected = -1;

        for (size_t i = 0; i < GetCodeLines (view).size(); i++)
        {
            if (m_codeSelected[(size_t) view] == GetCodeLines (view)[i].address)
            {
                selected = (int) i;
                break;
            }
        }

        if (selected != list->GetSelectedRow())
        {
            list->SetSelectedRow (selected);
        }
    }

    if (current >= 0 && list->GetSelectedRow() < 0)
    {
        list->EnsureVisible (current);
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

    m_watchHistory.OnSnapshot (*m_snapshot);

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
//  DebuggerWindow::ApplySnapshot
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySnapshot()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    //  Each disassembly view open, and which follows the PC: once a second
    //  view is open, the follower's tab carries the PC's yellow dot and says
    //  so in its tip. Every view's open state is taken before any dot is set,
    //  so the first view's dot sees a second view opened in this snapshot.
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        bool  open = m_snapshot->codeOpen[(size_t) view];

        if (m_codeOpen[(size_t) view] != open)
        {
            m_codeOpen[(size_t) view] = open;

            //  The keys and the command bar act on the view last used, and a
            //  closed one has nothing to act on, so the first view takes over.
            if (!open && view == m_activeCode)
            {
                m_activeCode = 0;
            }

            if (open && view > 0)
            {
                (void) m_dockSite->EditPaneLayout().Activate (DebuggerLayout::GetCodePaneId (view));
            }

            m_dockSite->Relayout();
            UpdateCodeLines();
        }
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        bool                       open    = m_codeOpen[(size_t) view];
        bool                       follows = GetOpenCodeViewCount() > 1 && m_snapshot->followView == view;
        DxuiTabGroup::LeadingMark  mark;

        //  The tab of the view following the PC carries the PC's own marker:
        //  the same character, face and color as the arrow on the PC's line,
        //  so the two read as one sign.
        if (follows)
        {
            mark = DxuiTabGroup::LeadingMark { s_kpszTriangleRight, DxuiTheme::kMonoFace, GetPcMarkerArgb() };
        }

        m_dockSite->SetLeadingMark (DebuggerLayout::GetCodePaneId (view), mark);
        m_dockSite->SetTabTip      (DebuggerLayout::GetCodePaneId (view), follows ? L"This disassembly follows the PC" : L"");

        if (open)
        {
            ApplyCodeView (view);
        }
    }

    rows.clear();

    UpdateChanges();

    //  THE FLAGS ARE THE P REGISTER written so a person can read it, so they
    //  sit on P's row beside the byte they come from, in the same monospace
    //  face -- where a bit changing moves nothing else. A register that
    //  changed since the last stop is drawn in the changed color (FR-098).
    for (const DebuggerViewSnapshot::RegisterRow & reg : m_snapshot->registers)
    {
        DxuiListView::Cell  value = { Widen (reg.value) };
        DxuiListView::Cell  flags = { reg.name == "P" ? L"Flags: " + Widen (m_snapshot->flags) : L"" };

        if (m_stopChanges.IsChanged ("R:" + reg.name))
        {
            value.argb = GetChangedArgb();
            flags.argb = GetChangedArgb();
        }

        rows.push_back ({ { Widen (reg.name) }, value, flags });
    }

    m_registerList->SetRows (std::move (rows));

    //  The console reads as a command prompt in the dialect in force, with
    //  that dialect's own help command in its hint.
    m_commandBox->SetPrompt      (GetPromptText (m_snapshot->mode));
    m_commandBox->SetPlaceholder (std::format (L"Command (Enter to run, {} for help)", GetHelpCommand (m_snapshot->mode)));

    //  A reply's suggestion is offered once, against the box as it stands.
    m_completion.SetMode (m_snapshot->mode);

    if (m_snapshot->suggestionSerial != m_offeredSuggestionSerial)
    {
        m_offeredSuggestionSerial = m_snapshot->suggestionSerial;
        m_completion.OfferSuggestion (Widen (m_snapshot->suggestion), m_commandBox->GetText());
    }

    m_tracePane->Apply      (m_snapshot->trace);

    ApplyBreakpoints();

    rows.clear();

    //  What the instruction at the PC and the one just executed touch, then
    //  the watches the user added, each section under a heading of its own
    //  (FR-095). A value that changed since the last stop is drawn in the
    //  changed color (FR-098); one the instruction touched and left as it was
    //  is shown plainly, since it was still an input or an output.
    m_watchRows.clear();

    if (!m_snapshot->autoWatches.empty())
    {
        rows.push_back (MakeWatchHeading (L"Automatic"));
        m_watchRows.push_back ({ WatchRowKind::Heading, 0 });

        for (size_t i = 0; i < m_snapshot->autoWatches.size(); i++)
        {
            const DebuggerViewSnapshot::AutoWatchLine & line  = m_snapshot->autoWatches[i];
            DxuiListView::Cell                          name  = { Widen (line.label) };
            DxuiListView::Cell                          value = { Widen (line.value) };

            name.dim = line.isPrevious;

            if (m_stopChanges.IsChanged ("A:" + line.key))
            {
                value.argb = GetChangedArgb();
            }

            rows.push_back ({ name, value });
            m_watchRows.push_back ({ WatchRowKind::Automatic, (int) i });
        }

        rows.push_back (MakeWatchHeading (L"Watches"));
        m_watchRows.push_back ({ WatchRowKind::Heading, 0 });
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : m_snapshot->watches)
    {
        DxuiListView::Cell  name  = { std::format (L"#{} ${:04X}", watch.id, watch.address) };
        DxuiListView::Cell  value = { Widen (watch.value) };

        //  A disabled watch is dimmed, as a disabled breakpoint is.
        name.dim  = !watch.enabled;
        value.dim = !watch.enabled;

        if (m_stopChanges.IsChanged (std::format ("W:{}", watch.id)))
        {
            value.argb = GetChangedArgb();
        }

        rows.push_back ({ name, value });
        m_watchRows.push_back ({ WatchRowKind::Manual, watch.id });
    }

    m_watchList->SetRows (std::move (rows));

    rows.clear();

    //  Newest first, as the call stack lists its frames: STACK reads from
    //  $01FF down, so the pane turns it over.
    for (auto it = m_snapshot->stack.rbegin(); it != m_snapshot->stack.rend(); ++it)
    {
        rows.push_back ({ { std::format (L"${:04X}", it->address) }, { std::format (L"{:02X}", it->value) } });
    }

    m_stackList->SetRows (std::move (rows));
    m_callStackPane->Apply (m_snapshot->callStack);

    ApplyMemoryWindows();
    ApplySource();
    ApplyDiagnostics();
    KeepOpenViews();
    ShowPendingPane();

    //  CODE, DATA or CONSOLE: the pane comes forward, even from an edge. The
    //  keys stay where they were, so the next command can be typed; CONSOLE
    //  alone brings them to its command line.
    if (m_snapshot->showPaneSerial != m_shownPaneSerial)
    {
        m_shownPaneSerial = m_snapshot->showPaneSerial;
        m_dockSite->ActivatePane (m_snapshot->showPane);

        if (m_snapshot->showPane == DebuggerLayout::kConsole && IsRoutable (m_commandBox))
        {
            SetFocusedControl (m_commandBox);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MakeWatchHeading
//
//  A row that divides the watch pane rather than holding a watch: its title
//  in the heading color, on the fill that marks a whole row.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> DebuggerWindow::MakeWatchHeading (const std::wstring & title) const
{
    DxuiListView::Cell  label = { title };
    DxuiListView::Cell  blank = { L"" };



    label.argb       = m_theme->HeadingForeground();
    label.background = m_theme->HoverBackground();
    blank.background = m_theme->HoverBackground();

    return { label, blank };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::BeginWatchEdit
//
//  A box over the cell, holding what the cell shows, all of it selected so
//  typing replaces it -- as Visual Studio's watch window opens one. A heading
//  edits nothing, and an automatic watch's expression is what the
//  instruction touches, so only its value edits.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::BeginWatchEdit (int row, int column)
{
    RECT          cell = {};
    RECT          list = m_watchList->GetBounds();
    WatchRow      what;
    std::wstring  text;



    if (m_snapshot == nullptr || row < 0 || row >= (int) m_watchRows.size())
    {
        return;
    }

    what = m_watchRows[(size_t) row];

    if (what.kind == WatchRowKind::Heading || (what.kind == WatchRowKind::Automatic && column == 0))
    {
        return;
    }

    if (!m_watchList->GetCellTextRectPx (row, (size_t) column, cell))
    {
        return;
    }

    if (what.kind == WatchRowKind::Automatic)
    {
        text = Widen (m_snapshot->autoWatches[(size_t) what.index].value);
    }
    else
    {
        for (const DebuggerViewSnapshot::WatchLine & watch : m_snapshot->watches)
        {
            if (watch.id == what.index)
            {
                text = (column == 0) ? std::format (L"{:04X}", watch.address) : Widen (watch.value);
            }
        }
    }

    OffsetRect (&cell, list.left, list.top);

    m_watchEdit = WatchEdit { row, column, what, {} };

    if (what.kind == WatchRowKind::Automatic)
    {
        m_watchEdit.autoKey = m_snapshot->autoWatches[(size_t) what.index].key;
    }

    //  In the list's own face and size, so the text does not jump when the
    //  box opens over it.
    m_watchEditor->SetTextRenderer (GetTextRenderer());
    m_watchEditor->SetFont         (DxuiTheme::kMonoFace, m_watchList->GetFontSizeDip());
    m_watchEditor->SetText    (text);
    m_watchEditor->Layout     (cell, m_scaler);
    m_watchEditor->SetVisible (true);
    SetFocusedControl         (m_watchEditor);
    m_watchEditor->SelectAll();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EndWatchEdit
//
//  Enter keeps what was typed, Escape leaves the watch as it was, and a
//  click anywhere else keeps it, as a click away from a rename does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EndWatchEdit (bool commit)
{
    std::wstring                 typed = m_watchEditor->GetText();
    std::vector<DebuggerAction>  actions;



    if (m_watchEdit.row < 0)
    {
        return;
    }

    if (commit && m_snapshot != nullptr)
    {
        bool                                        isManual = m_watchEdit.what.kind == WatchRowKind::Manual;
        std::optional<int>                          watchId  = isManual ? std::optional<int> (m_watchEdit.what.index) : std::nullopt;
        std::optional<int>                          autoAt;
        std::optional<DebuggerViewState::WatchUndo> undo;



        //  An automatic watch that is no longer listed takes no edit.
        for (size_t i = 0; !isManual && i < m_snapshot->autoWatches.size(); i++)
        {
            if (m_snapshot->autoWatches[i].key == m_watchEdit.autoKey)
            {
                autoAt = (int) i;
                break;
            }
        }

        actions = DebuggerViewState::GetWatchEditActions (*m_snapshot, watchId, autoAt, m_watchEdit.column,
                                                          TextEncoding::WideToNarrow (typed), GetMode());

        //  Recorded from the snapshot as it stands, BEFORE the edit runs.
        undo = actions.empty() ? std::nullopt : DebuggerViewState::GetWatchUndo (*m_snapshot, watchId, autoAt, m_watchEdit.column, GetMode());

        if (undo.has_value())
        {
            m_watchHistory.Record (std::move (*undo), actions);
        }
    }

    m_watchEdit = WatchEdit {};
    m_watchEditor->SetVisible (false);
    SetFocusedControl         (m_watchList);
    Invalidate();

    for (const DebuggerAction & action : actions)
    {
        RunAction (action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RemoveSelectedWatch
//
//  Delete on a manual watch removes it (FR-096). Automatic watches come and
//  go with the instruction, so there is nothing of theirs to remove.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RemoveSelectedWatch()
{
    int  row = m_watchList->GetSelectedRow();



    if (row >= 0 && row < (int) m_watchRows.size() && m_watchRows[(size_t) row].kind == WatchRowKind::Manual)
    {
        RunAction (DebuggerActions::GetClearWatch (m_watchRows[(size_t) row].index, GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UndoRegisterEdit
//
//  The registers pane's last edit taken back, or with `redo` made again,
//  while the machine is still at the stop it was made at.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UndoRegisterEdit (bool redo)
{
    std::optional<DebuggerAction>  action = redo ? m_registerHistory.TryRedo (GetMode()) : m_registerHistory.TryUndo (GetMode());



    if (action.has_value())
    {
        RunAction (*action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UndoWatchEdit
//
//  The last watch edit, put back, or with `redo` made again. A moved watch
//  is found by being the one whose id did not exist before the move, since
//  the engine numbered it only once the move ran.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UndoWatchEdit (bool redo)
{
    std::optional<std::vector<DebuggerAction>>  actions;



    if (m_snapshot == nullptr)
    {
        return;
    }

    actions = redo ? m_watchHistory.TryRedo() : m_watchHistory.TryUndo (*m_snapshot, GetMode());

    if (!actions.has_value())
    {
        return;
    }

    for (const DebuggerAction & action : *actions)
    {
        RunAction (action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UndoStackEdit
//
//  The stack pane's last edit taken back, or with `redo` made again, while
//  the machine is still at the stop it was made at.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UndoStackEdit (bool redo)
{
    std::optional<DebuggerAction>  action = redo ? m_stackHistory.TryRedo (GetMode()) : m_stackHistory.TryUndo (GetMode());



    if (action.has_value())
    {
        RunAction (*action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UndoMemoryEdit
//
//  A memory window's last edit taken back, or with `redo` made again, while
//  the machine is stopped and the window takes edits.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UndoMemoryEdit (MemoryPane * pane, bool redo)
{
    if (pane == nullptr)
    {
        return;
    }

    (void) (redo ? pane->Redo() : pane->Undo());
    Invalidate();
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

    //  The source documents open, each at the line at its top (FR-113). One
    //  that cannot say, behind another tab, keeps the line it last gave.
    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        int  line = m_sourceDocs[(size_t) slot].pane->GetTopSourceLine();

        if (line > 0)
        {
            m_documents.SetLine (slot, line);
        }
    }

    //  Documents still waiting for their debug file are kept in the text, so
    //  a save before it loads does not lose them.
    text  = DebuggerViewState::FormatOpenViews (*m_snapshot);
    text += m_documents.Format (nameOf);
    text += SourceDocuments::FormatSaved (m_pendingSourceDocs);
    text += BreakpointColumns::FormatShown (m_breakpointShown);

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
        m_pendingSourceDocs = SourceDocuments::Parse (m_openViewsSaved);
        m_breakpointShown   = BreakpointColumns::ParseShown (m_openViewsSaved);

        SetBreakpointColumns();

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

        for (const std::string & panel : views.panels)
        {
            RunAction (DebuggerActions::GetPanel (panel, true, GetMode()));
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

    m_stopChanges.Update (m_snapshot->isPaused, std::move (values));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyDiagnostics
//
//  The snapshot holds a panel for each device whose panel is open. A panel
//  that opens comes to the front of its tab group, and one whose device left
//  the machine is no longer in the snapshot, so it hides.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyDiagnostics()
{
    std::set<std::string>  open;
    bool                   changed = false;



    for (const DiagnosticsSnapshot & diagnostics : m_snapshot->diagnostics)
    {
        DiagnosticsPane  * pane = GetDiagnosticsPane (DebuggerLayout::GetDiagnosticsPaneId (diagnostics.id));

        if (pane == nullptr)
        {
            continue;
        }

        open.insert (diagnostics.id);

        if (pane->Apply (diagnostics))
        {
            pane->GetFrame()->Relayout();
        }
    }

    for (const std::string & id : open)
    {
        if (!m_diagOpen.contains (id))
        {
            (void) m_dockSite->EditPaneLayout().Activate (DebuggerLayout::GetDiagnosticsPaneId (id));
        }
    }

    changed    = (open != m_diagOpen);
    m_diagOpen = std::move (open);

    if (changed)
    {
        m_dockSite->Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetDiagnosticsPane
//
//  Null for a pane that is not a device panel.
//
////////////////////////////////////////////////////////////////////////////////

DiagnosticsPane * DebuggerWindow::GetDiagnosticsPane (const std::wstring & pane) const
{
    std::string  id;



    if (!DebuggerLayout::TryGetDiagnosticsId (pane, id))
    {
        return nullptr;
    }

    for (const std::unique_ptr<DiagnosticsPane> & diagnostics : m_diagPanes)
    {
        if (diagnostics->GetId() == id)
        {
            return diagnostics.get();
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AppendConsole
//
//  Bounded, so a long session does not grow the list without limit; the
//  oldest lines go first.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AppendConsole (const std::vector<std::string> & lines)
{
    std::vector<DxuiTextView::Row>  rows;
    bool                            atEnd   = m_consoleView->IsAtEnd();
    int                             top     = m_consoleView->GetTopLine();
    int                             dropped = 0;



    if (lines.empty())
    {
        return;
    }

    m_console.insert (m_console.end(), lines.begin(), lines.end());

    if ((int) m_console.size() > kConsoleLineLimit)
    {
        dropped = (int) m_console.size() - kConsoleLineLimit;
        m_console.erase (m_console.begin(), m_console.begin() + dropped);
    }

    for (const std::string & line : m_console)
    {
        DxuiTextView::Row  row;

        row.cells = { Widen (line) };
        rows.push_back (std::move (row));
    }

    m_consoleView->SetRows (std::move (rows));

    //  A console scrolled back to read stays on the same text, even as the
    //  oldest lines go; one at its end follows the output.
    m_consoleView->SetTopLine (atEnd ? m_consoleView->GetLineCount() : top - dropped);

    //  Output to a console out of sight marks its tab (FR-041).
    if (m_dockSite != nullptr && !m_consoleView->IsVisible())
    {
        m_dockSite->SetIndicator (DebuggerLayout::kConsole, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SubmitCommandBox
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SubmitCommandBox()
{
    HRESULT                   hr      = S_OK;
    std::wstring              typed   = m_commandBox->GetText();
    std::string               line    = TextEncoding::WideToNarrow (typed);
    std::optional<DebugVerb>  fileVerb;
    FileDialogSpec            spec;
    std::filesystem::path     chosen;
    bool                      picked  = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    //  The mode comes from the last snapshot; one built before a mode change
    //  can only miss a prompt, and the handler then reports the missing name.
    fileVerb = DebuggerViewState::GetMissingFileVerb (line,
                                                      m_snapshot != nullptr ? m_snapshot->mode : CommandMode::AppleWin,
                                                      m_snapshot != nullptr && m_snapshot->isAssembling);

    if (fileVerb.has_value())
    {
        spec.filters = { { L"All files", L"*.*" } };

        hr = (*fileVerb == DebugVerb::ReadFile) ? m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), spec, chosen, picked)
                                                : m_host->GetHostDialogs().PickFileToSave (GetHwnd(), spec, chosen, picked);
        CHR (hr);

        //  Backing out of the picker keeps the line in the box, unrun.
        BAIL_OUT_IF (!picked, S_OK);

        line = DebuggerViewState::GetLineWithFileName (line, TextEncoding::WideToNarrow (chosen.wstring()));
    }

    m_consoleHistory.Add (typed);

    //  Running a command shows its output, however far back the console was
    //  scrolled.
    m_consoleView->SetTopLine (m_consoleView->GetLineCount());

    m_host->RunDebuggerCommand (line);
    m_commandBox->SetText (L"");

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SubmitMemoryBox
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SubmitMemoryBox()
{
    const std::wstring &  text     = m_memoryBox->GetText();
    Word                  address  = 0;
    bool                  isBlank  = std::all_of (text.begin(), text.end(), [] (wchar_t ch) { return iswspace (ch) != 0; });
    std::wstring          upper;



    //  A register's own letter is also a hex digit, so the resolver sees the
    //  register names before the text is read as an address.
    for (wchar_t ch : text)
    {
        if (!iswspace (ch))
        {
            upper += (wchar_t) towupper (ch);
        }
    }

    if (isBlank)
    {
        return;
    }

    AddMemoryHistory (text);

    if (upper != L"A" && TryParseHexWord (text, address))
    {
        GetActiveMemoryPane()->GoTo (address);
    }
    else if (m_host != nullptr)
    {
        m_host->GoToDebuggerMemory (GetActiveMemoryPane()->GetId(), SourcePathList::WideToUtf8 (m_memoryBox->GetText()));
    }
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
        if (reg.name == name && TryParseHexWord (Widen (reg.value), value) && value <= 0xFF)
        {
            return (Byte) value;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditRegister
//
//  P opens the flags a checkbox each; S asks for a new pointer. Either one
//  is written by R, the command a person would type.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EditRegister (const std::string & name)
{
    std::optional<Byte>  value = GetRegisterByte (name);
    Byte                 p     = 0;
    std::wstring         text;
    Word                 typed = 0;



    if (name == "PC")
    {
        ShowCode (std::nullopt);
        return;
    }

    if ((name == "P" || name == "S") && !m_snapshot->isPaused)
    {
        AppendConsole ({ "Pause the machine to edit its registers." });
        return;
    }

    if (name == "P" && value.has_value() && FlagsDialog::Ask (GetHwnd(), m_theme, *value, p))
    {
        m_registerHistory.Record ("P", *value, p, m_snapshot->pc);
        RunAction (DebuggerActions::GetSetRegister ("P", p, GetMode()));
    }
    else if (name == "S" && value.has_value() &&
             CassquePromptDialog::Ask (GetHwnd(), m_theme, L"Stack pointer", L"S, in hex ($00-$FF):", std::format (L"{:02X}", *value), 4, text) &&
             TryParseHexWord (text, typed) && typed <= 0xFF)
    {
        m_registerHistory.Record ("S", *value, (Byte) typed, m_snapshot->pc);
        RunAction (DebuggerActions::GetSetRegister ("S", (Byte) typed, GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditStackByte
//
//  Asks for a new value for a stack byte, written by the byte-entry command a
//  person would type. The pane lists the stack newest first, so its rows run
//  backwards through STACK's.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EditStackByte (int row)
{
    std::wstring  text;
    Word          typed = 0;
    Word          at    = 0;
    Byte          value = 0;



    if (m_snapshot == nullptr || row < 0 || row >= (int) m_snapshot->stack.size())
    {
        return;
    }

    if (!m_snapshot->isPaused)
    {
        AppendConsole ({ "Pause the machine to edit the stack." });
        return;
    }

    at    = m_snapshot->stack[m_snapshot->stack.size() - 1 - (size_t) row].address;
    value = m_snapshot->stack[m_snapshot->stack.size() - 1 - (size_t) row].value;

    if (CassquePromptDialog::Ask (GetHwnd(), m_theme, L"Stack byte", std::format (L"${:04X}, in hex ($00-$FF):", at), std::format (L"{:02X}", value), 4, text) &&
        TryParseHexWord (text, typed) && typed <= 0xFF)
    {
        m_stackHistory.Record (at, value, (Byte) typed, m_snapshot->pc);
        RunAction (DebuggerActions::GetEnterByte (at, (Byte) typed, GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsAnyMenuOpen
//
//  Whether the menu bar, a toolbar's drop-down or a context menu is open.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsAnyMenuOpen() const
{
    DxuiHwndSource  * popups = GetPopupHost();
    bool              open   = false;



    open = (m_menuBar != nullptr && m_menuBar->IsOpen()) || (popups != nullptr && popups->GetContextMenu().IsVisible());

    for (const DxuiToolbar * bar : { m_commandBar, m_memoryBar, m_breakpointBar, m_consoleBar })
    {
        open = open || (bar != nullptr && bar->IsMenuOpen());
    }

    for (const SourceDocument & document : m_sourceDocs)
    {
        open = open || (document.bar != nullptr && document.bar->IsMenuOpen());
    }

    return open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UpdateTooltip
//
//  Over the flags on P's row, what each letter is, one to a line. No tip
//  shows while a menu is open, so none ever lies over its rows.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UpdateTooltip (POINT clientPx)
{
    RECT                 bounds = m_registerList->GetBounds();
    RECT                 cell   = {};
    int                  row    = -1;
    int64_t              now    = (int64_t) GetTickCount64();
    std::optional<Byte>  p;
    std::wstring         text;
    const wchar_t      * barTip = nullptr;
    DxuiTooltip        & tip    = GetRoutedTooltip();



    if (IsAnyMenuOpen())
    {
        tip.HideImmediate();
        return;
    }

    //  The command bar's entries: what each does and its key in the scheme
    //  in force.
    if (m_routingPane.empty() && m_commandBar != nullptr && m_commandBar->IsVisible())
    {
        barTip = m_commandBar->GetTooltipAt (clientPx.x, clientPx.y, cell);
    }

    //  The memory bar's the same, and its Address box's.
    if ((barTip == nullptr || *barTip == L'\0') && m_routingPane == GetBarRoutingPane (m_memoryBarPane) &&
        m_memoryBar != nullptr && m_memoryBar->IsVisible())
    {
        barTip = m_memoryBar->GetTooltipAt (clientPx.x, clientPx.y, cell);
    }

    //  And the breakpoints pane's.
    if ((barTip == nullptr || *barTip == L'\0') && m_routingPane == GetBarRoutingPane (DebuggerLayout::kBreakpoints) &&
        m_breakpointBar != nullptr && m_breakpointBar->IsVisible())
    {
        barTip = m_breakpointBar->GetTooltipAt (clientPx.x, clientPx.y, cell);
    }

    //  And the console's.
    if ((barTip == nullptr || *barTip == L'\0') && m_consoleBar != nullptr && m_consoleBar->IsVisible())
    {
        barTip = m_consoleBar->GetTooltipAt (clientPx.x, clientPx.y, cell);
    }

    //  And each source document's.
    for (int slot = 0; slot < (int) m_sourceDocs.size() && (barTip == nullptr || *barTip == L'\0'); slot++)
    {
        DxuiToolbar  * bar = m_sourceDocs[(size_t) slot].bar;

        if (m_routingPane == GetBarRoutingPane (DebuggerLayout::GetSourcePaneId (slot)) && bar != nullptr && bar->IsVisible())
        {
            barTip = bar->GetTooltipAt (clientPx.x, clientPx.y, cell);
        }
    }

    if (barTip == nullptr || *barTip == L'\0')
    {
        barTip = GetFindBarTip (clientPx, cell);
    }

    if (barTip != nullptr && *barTip != L'\0')
    {
        tip.SetMonospace (false);
        tip.RequestShow  (cell, barTip, now);
        return;
    }

    if (m_snapshot != nullptr && IsRoutable (m_registerList) && m_registerList->IsVisible() && DxuiDockSite::Contains (bounds, clientPx))
    {
        row = m_registerList->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    }

    if (row >= 0 && row < (int) m_snapshot->registers.size() && m_snapshot->registers[(size_t) row].name == "P" &&
        m_registerList->GetCellTextRectPx (row, 2, cell) &&
        clientPx.x - bounds.left >= cell.left && clientPx.x - bounds.left < cell.right)
    {
        p = GetRegisterByte ("P");
    }

    if (p.has_value())
    {
        OffsetRect (&cell, bounds.left, bounds.top);
        tip.SetMonospace (true);
        tip.RequestShow  (cell, FlagsDialog::Describe (*p), now);
        return;
    }

    if (TryGetSymbolTip (clientPx, cell, text))
    {
        tip.SetMonospace (true);
        tip.RequestShow  (cell, text, now);
        return;
    }

    //  A tab's tip is a sentence, not a table, so it reads in the body face.
    if (m_routingPane.empty() && !m_dockSite->GetTabAt (clientPx, cell, text).empty() && !text.empty())
    {
        tip.SetMonospace (false);
        tip.RequestShow  (cell, text, now);
        return;
    }

    tip.RequestHide (now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindBarTip
//
//  What each of the find bar's controls does, as every toolbar control's tip
//  says.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DebuggerWindow::GetFindBarTip (POINT clientPx, RECT & anchor) const
{
    for (const auto & [pane, state] : m_findStates)
    {
        const std::pair<IDxuiControl *, const wchar_t *>  tips[] =
        {
            { state.box,         L"Text to find in this pane"             },
            { state.caseButton,  L"Find only text in the same case"      },
            { state.wordButton,  L"Find only whole words"                },
            { state.regexButton, L"Find by regular expression"           },
            { state.prevButton,  L"Find the previous match"              },
            { state.nextButton,  L"Find the next match"                  },
            { state.selButton,   L"Find in selection"                    },
            { state.chevron,     L"Replace is not available: this text is read-only" },
            { state.closeButton, L"Close the find bar"                   },
        };

        for (const auto & [control, tip] : tips)
        {
            if (control != nullptr && IsRoutable (control) && control->IsVisible() && DxuiDockSite::Contains (control->GetBounds(), clientPx))
            {
                anchor = control->GetBounds();
                return tip;
            }
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetSymbolTip
//
//  Over a name in the code pane -- a line's label, or the symbol an operand
//  is written with -- where it lives and what it is.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetSymbolTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    DxuiListView * list    = nullptr;
    RECT           bounds  = {};
    int            row     = -1;
    int            column  = -1;
    std::string    name;
    Word           address = 0;
    const char   * about   = nullptr;



    for (DxuiListView * code : m_codeLists)
    {
        if (IsRoutable (code) && code->IsVisible() && DxuiDockSite::Contains (code->GetBounds(), clientPx))
        {
            list   = code;
            bounds = code->GetBounds();
        }
    }

    if (m_snapshot == nullptr || list == nullptr)
    {
        return false;
    }

    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    column = GetColumnAt (list, clientPx.x - bounds.left);

    if (row < 0 || row >= (int) GetCodeLines (GetCodeViewOf (list)).size())
    {
        return false;
    }

    const DebuggerViewSnapshot::CodeLine & line = GetCodeLines (GetCodeViewOf (list))[(size_t) row];

    if (column == kCodeInstructionColumn - 1 && !line.label.empty())
    {
        name    = line.label;
        address = line.address;
    }
    else if (column == kCodeInstructionColumn && line.shownOperand != line.memoryOperand)
    {
        for (char ch : line.shownOperand)
        {
            if (std::isalnum ((unsigned char) ch) || ch == '_')
            {
                name += ch;
            }
            else if (!name.empty())
            {
                break;
            }
        }

        for (char ch : line.memoryOperand)
        {
            if (std::isxdigit ((unsigned char) ch))
            {
                address = (Word) ((address << 4) | (Word) std::stoi (std::string (1, ch), nullptr, 16));
            }
            else if (ch == ',' || ch == ')')
            {
                break;
            }
        }
    }

    if (name.empty() || !list->GetCellTextRectPx (row, (size_t) column, anchor))
    {
        return false;
    }

    about = SymbolDescriptions::Find (name);
    text  = std::format (L"{} = ${:04X}", Widen (name), address);

    if (about != nullptr)
    {
        text += L"\n" + Widen (about);
    }

    OffsetRect (&anchor, bounds.left, bounds.top);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureCodeList
//
//  Every disassembly view is set up alike.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureCodeList (int view)
{
    DxuiListView  * list = m_codeLists[(size_t) view];



    //  A code row selected shows its line in the source pane (FR-054).
    list->SetOnSelectionChanged ([this, view] (int row)
    {
        const std::vector<DebuggerViewSnapshot::CodeLine> & lines = GetCodeLines (view);

        m_activeCode                  = view;
        m_codeSelected[(size_t) view] = std::nullopt;

        if (row >= 0 && row < (int) lines.size())
        {
            m_codeSelected[(size_t) view] = lines[(size_t) row].address;
            ShowSourceLine (lines[(size_t) row].sourceFileId, lines[(size_t) row].sourceLine);
        }
    });

    //  Every column fits its contents and none stretches, so a pane is as wide
    //  as what it shows and no wider (FR-026a). The marker column alone has a
    //  set width, since its glyphs are not text a fit could measure.
    list->SetColumns ({ { L"",            kGutterColumnDip, false, DxuiTextHAlign::Left   },
                        { L"",            kMarkerColumnDip, false, DxuiTextHAlign::Center },
                        { L"Address",     0, false, DxuiTextHAlign::Left },
                        { L"Bytes",       0, false, DxuiTextHAlign::Left },
                        { L"Label",       0, false, DxuiTextHAlign::Left },
                        { L"Instruction", 0, false, DxuiTextHAlign::Left },
                        { L"Operand and result", 0, false, DxuiTextHAlign::Left } });

    //  A breakpoint is set from the gutter (see ClickGutter), as in an editor.
    //  The rest of the pane is TEXT (FR-076): a drag selects characters, a
    //  double-click selects the word under the pointer, and Ctrl+C copies
    //  what is selected -- a double-click never touches a breakpoint.
    list->SetActivateOnDoubleClick (true);
    list->SetTextSelection         (true);
    list->SetOwnerWindow           (GetHwnd());

}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCodeViewOf
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::GetCodeViewOf (const IDxuiControl * control) const
{
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (control != nullptr && control == m_codeLists[(size_t) view])
        {
            return view;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetOpenCodeViewCount
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::GetOpenCodeViewCount() const
{
    return (int) std::count (m_codeOpen.begin(), m_codeOpen.end(), true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCodeLines
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DebuggerViewSnapshot::CodeLine> & DebuggerWindow::GetCodeLines (int view) const
{
    static const std::vector<DebuggerViewSnapshot::CodeLine>  kNone;



    if (m_snapshot == nullptr || view < 0 || view >= DebuggerViewState::kMaxCodeViews)
    {
        return kNone;
    }

    return m_snapshot->codeViews[(size_t) view];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowCode
//
//  Moves the code pane to an address another pane chose, and remembers it so
//  the row is marked as the one brought into view. No address follows the PC
//  again, which marks nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowCode (std::optional<Word> address)
{
    //  An address goes to the disassembly view last used; the PC, to the one
    //  following it.
    int  view = address.has_value() ? m_activeCode : ((m_snapshot != nullptr) ? m_snapshot->followView : 0);



    m_navigatedTo   = address;
    m_navigatedView = view;

    //  The view that answers comes to the front, so what was asked for is
    //  looked at rather than drawn on a tab behind another.
    if (m_dockSite != nullptr && m_codeOpen[(size_t) view])
    {
        (void) m_dockSite->EditPaneLayout().Activate (DebuggerLayout::GetCodePaneId (view));
        m_dockSite->Relayout();
    }

    if (m_host != nullptr)
    {
        m_host->SetDebuggerCodeAddress (address, view);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClickGutter
//
//  A press on the first column of the code pane sets or clears the
//  breakpoint on that row. The breakpoints pane turns one on or off with its
//  checkbox instead.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ClickGutter (const DxuiMouseEvent & ev)
{
    int   row = -1;
    RECT  bounds;
    int   lx  = 0;
    int   ly  = 0;



    if (m_snapshot == nullptr)
    {
        return false;
    }

    for (DxuiListView * list : { m_codeLists[0], m_codeLists[1], m_codeLists[2], m_codeLists[3] })
    {
        bounds = list->GetBounds();
        lx     = ev.positionDip.x - bounds.left;
        ly     = ev.positionDip.y - bounds.top;

        if (!IsRoutable (list) || !list->IsVisible() || lx < 0 || ly < 0 || ev.positionDip.x >= bounds.right || ev.positionDip.y >= bounds.bottom)
        {
            continue;
        }

        if (lx + list->GetLeftPx() >= list->GetColumnEffectiveWidthPx (0) + (GetCodeViewOf (list) >= 0 ? list->GetColumnEffectiveWidthPx (1) : 0))
        {
            return false;
        }

        row = list->HitTestRow (lx, ly);

        if (GetCodeViewOf (list) >= 0 && row >= 0 && row < (int) GetCodeLines (GetCodeViewOf (list)).size())
        {
            RunAction (DebuggerActions::GetToggleBreakpoint (*m_snapshot, GetCodeLines (GetCodeViewOf (list))[(size_t) row].address, GetMode()));
            return true;
        }

        return false;
    }

    return false;
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
//  The breakpoint's dot, filled when enabled and a ring when not, drawn as an
//  image so it can be larger than the text beside it. Rebuilt when the theme
//  changes the color.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> DebuggerWindow::GetBreakpointIcon (bool enabled)
{
    static constexpr int    kSize   = 48;
    static constexpr float  kRadius = 21.0f;
    static constexpr float  kRing   = 5.0f;
    uint32_t                argb    = GetBreakpointArgb();



    if (argb != m_breakpointIconArgb || m_breakpointIcons[0] == nullptr)
    {
        m_breakpointIconArgb = argb;

        for (int filled = 0; filled < 2; filled++)
        {
            auto  image = std::make_shared<DxuiIconImage>();

            image->width  = kSize;
            image->height = kSize;
            image->bgraPremul.assign ((size_t) (kSize * kSize), 0u);

            for (int y = 0; y < kSize; y++)
            {
                for (int x = 0; x < kSize; x++)
                {
                    float  d     = std::hypot (x + 0.5f - kSize * 0.5f, y + 0.5f - kSize * 0.5f);
                    float  outer = std::clamp (kRadius - d + 0.5f, 0.0f, 1.0f);
                    float  inner = filled ? 0.0f : std::clamp (kRadius - kRing - d + 0.5f, 0.0f, 1.0f);
                    float  a     = outer - inner;
                    auto   ch    = [a] (uint32_t c) { return (uint32_t) std::lround ((float) (c & 0xFF) * a); };

                    image->bgraPremul[(size_t) (y * kSize + x)] = ((uint32_t) std::lround (a * 255.0f) << 24) |
                                                                  (ch (argb >> 16) << 16) | (ch (argb >> 8) << 8) | ch (argb);
                }
            }

            m_breakpointIcons[filled] = image;
        }
    }

    return m_breakpointIcons[enabled ? 1 : 0];
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
    return IsDarkTheme() ? 0xFFFFE34D : 0xFFD8A800;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPcRowArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetPcRowArgb() const
{
    return IsDarkTheme() ? 0x50C8A000 : 0x60FFE34D;
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
    return IsDarkTheme() ? 0x4A3C8C3C : 0x5096D796;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTargetRowArgb
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerWindow::GetTargetRowArgb() const
{
    uint32_t  accent = (m_theme != nullptr) ? m_theme->Accent() : 0xFF3C8CE6;



    return (accent & 0x00FFFFFFu) | 0x38000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSyntaxColors
//
//  Visual Studio's code colors for the theme's darkness, as the comment color
//  already is: the keyword blue for a mnemonic, the control purple for a
//  directive, the type teal for a symbol, and its number and string colors.
//  A listing's address takes the theme's text color and its bytes the muted
//  one, as the source pane's bytes column has them.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Colors DebuggerWindow::GetSyntaxColors() const
{
    SourceSyntax::Colors  colors;
    bool                  dark   = IsDarkTheme();



    colors.mnemonic  = dark ? 0xFF569CD6 : 0xFF0000FF;
    colors.directive = dark ? 0xFFC586C0 : 0xFFAF00DB;
    colors.symbol    = dark ? 0xFF4EC9B0 : 0xFF2B91AF;
    colors.number    = dark ? 0xFFB5CEA8 : 0xFF098658;
    colors.string    = dark ? 0xFFD69D85 : 0xFFA31515;
    colors.comment   = GetAnnotationArgb();
    colors.address   = (m_theme != nullptr) ? m_theme->Foreground()      : 0u;
    colors.bytes     = (m_theme != nullptr) ? m_theme->ForegroundMuted() : 0u;

    return colors;
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
    if (m_theme != nullptr && m_theme->resultText != 0)
    {
        return m_theme->resultText;
    }

    return IsDarkTheme() ? 0xFF4EC9E0 : 0xFF00727D;
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
    return IsDarkTheme() ? 0xFF57A64A : 0xFF008000;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetOperandAndResultCell
//
//  What an instruction reads, then what it would leave behind, in one cell:
//  the result follows the operand after "Result: ", both in the result
//  color, so it needs no column of its own to push the pane wide.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell DebuggerWindow::GetOperandAndResultCell (const std::string & annotation, const std::string & effect, uint32_t resultArgb)
{
    DxuiListView::Cell  cell;
    std::wstring        result = Widen (effect);



    cell.text = Widen (annotation);

    if (!result.empty())
    {
        result     = SourcePane::kpszResultPrefix + result;
        cell.text += cell.text.empty() ? L"" : L"  ";
        cell.colorRanges.emplace_back ((int) cell.text.size(), (int) (cell.text.size() + result.size()), resultArgb);
        cell.text += result;
    }

    return cell;
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
    return IsDarkTheme() ? 0xFFFF6B68 : 0xFFD00000;
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



    if (RouteCommandBarDrag (ev))
    {
        return true;
    }

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
//  DebuggerWindow::RouteCommandBarDrag
//
//  A press on the command bar's grab handle carries the bar: while the
//  button is down the bar docks to the edge nearest the pointer, at the
//  place that keeps the handle under it, and the release saves that place.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteCommandBarDrag (const DxuiMouseEvent & ev)
{
    POINT           at    = ev.positionDip;
    RECT            bar   = m_commandBar->GetBounds();
    int             along = 0;
    CommandBarDock  dock;



    if (!m_barDragging)
    {
        if (ev.kind != DxuiMouseEventKind::Down || m_commandBar->IsMenuOpen() || !m_commandBar->IsOnGrip (at.x, at.y))
        {
            return false;
        }

        along         = m_barDock.IsVertical() ? at.y - bar.top : at.x - bar.left;
        //  Across the top or bottom the bar starts a margin in from the
        //  window's edge, so the grab point counts it.
        m_barGrab     = POINT { along + m_scaler.ToPx (8), along };
        m_barDragging = true;

        GetRoutedTooltip().HideImmediate();
        return true;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        dock = CommandBarDock::PickForDrop (at, m_barGrab, m_barArea, m_scaler.GetDpi());

        if (!(dock == m_barDock))
        {
            m_barDock = dock;
            LayoutWidgets();
        }

        break;

    case DxuiMouseEventKind::Up:
        m_barDragging = false;

        if (m_host != nullptr)
        {
            m_host->SetDebuggerCommandBarDock (SourcePathList::WideToUtf8 (m_barDock.ToText()));
        }

        break;

    default:
        break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteMemoryBarMouse
//
//  As the command bar's, less the Address box: the box is the window's own
//  control and takes its presses as every box does.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteMemoryBarMouse (const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    RECT  strip = m_memoryBar->GetBounds();
    RECT  box   = m_memoryBox->GetBounds();
    bool  open  = m_memoryBar->IsMenuOpen();
    bool  over  = m_memoryBar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y }) &&
                  !(m_memoryBox->IsVisible() && DxuiDockSite::Contains (box, POINT { x, y }));



    if (!over && !open)
    {
        m_memoryBar->OnToolbarMouseLeave();
        return false;
    }

    //  The right button over the strip opens the pane's own menu.
    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        UpdateTooltip (ev.positionDip);

        return m_memoryBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        //  The rows carry the checks they were built with, and the active
        //  window may have changed since, so they are built again here.
        if (!open)
        {
            SetMemoryBarMenus();
        }

        return m_memoryBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_memoryBar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
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
        m_lastPressPx = POINT { x, y };
    }

    //  A watch being edited takes the pointer while it is over the box; a
    //  press or a wheel anywhere else keeps the edit and lets it through.
    if (m_watchEdit.row >= 0)
    {
        RECT  box = m_watchEditor->GetBounds();

        if (DxuiDockSite::Contains (box, POINT { x, y }))
        {
            m_watchEditor->OnMouse (ev);
            Invalidate();
            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel)
        {
            EndWatchEdit (true);
        }
    }

    //  Ctrl+wheel sizes the panes' text as Ctrl+Plus and Ctrl+Minus do, over
    //  any pane, before a view that would take the wheel for itself.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal && ev.wheelDelta != 0.0f)
    {
        StepTextZoom (ev.wheelDelta > 0.0f ? 1 : -1);
        return true;
    }

    //  The menu bar first, then the command bar: each owns its strip and
    //  whatever menu it has open.
    if (m_routingPane.empty() && RouteMenuBarMouse (ev))
    {
        return true;
    }

    if (m_routingPane.empty() && RouteCommandBarMouse (ev))
    {
        return true;
    }

    //  Then the memory bar, which owns its strip less the Address box, and
    //  whatever menu it has open.
    if (m_routingPane == GetBarRoutingPane (m_memoryBarPane) && RouteMemoryBarMouse (ev))
    {
        return true;
    }

    //  Then the breakpoints pane's toolbar.
    if (m_routingPane == GetBarRoutingPane (DebuggerLayout::kBreakpoints) && RouteBreakpointBarMouse (ev))
    {
        return true;
    }

    //  And the console's.
    if (m_routingPane == GetBarRoutingPane (DebuggerLayout::kConsole) && RouteConsoleBarMouse (ev))
    {
        return true;
    }

    //  And each source document's.
    for (int slot = 0; slot < (int) m_sourceDocs.size(); slot++)
    {
        if (m_routingPane == GetBarRoutingPane (DebuggerLayout::GetSourcePaneId (slot)) && RouteSourceBarMouse (m_sourceDocs[(size_t) slot].bar, ev))
        {
            return true;
        }
    }

    //  A press on a disassembly or source tab sets the step mode as a press
    //  inside that pane does.
    if (m_routingPane.empty() && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        NoteTabFocus (ev.positionDip);
    }

    //  A double-click on a branch arrow goes to where it points.
    if (m_routingPane.empty() && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left &&
        ClickBranchArrow (ev.positionDip))
    {
        return true;
    }

    //  Then the site: its strips, its sashes and a drag in progress lie over
    //  the panes.
    if (m_routingPane.empty() && m_dockSite->OnMouse (ev))
    {
        return true;
    }

    //  A pane slid out from an edge lies over others, so its area is its own.
    if (m_routingPane.empty() && !m_dockSite->GetSlidPane().empty() && ev.kind != DxuiMouseEventKind::Move &&
        DxuiDockSite::Contains (m_dockSite->GetSlidRect(), ev.positionDip))
    {
        return RouteFloatMouse (m_dockSite->GetSlidPane(), ev);
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        pane = m_routingPane.empty() ? m_dockSite->GetPaneAt (ev.positionDip) : m_routingPane;

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
    if (!std::ranges::any_of (m_findStates, [&] (const auto & entry) { const FindWidgetPlate * plate = entry.second.plate; return plate != nullptr && IsRoutable (plate) && plate->IsVisible() && DxuiDockSite::Contains (plate->GetBounds(), ev.positionDip); }) &&
        (RouteMemoryMouse (ev) || RouteSourceMouse (ev) || RouteConsoleMouse (ev)))
    {
        return true;
    }

    for (DxuiListView * list : GetLists())
    {
        if (IsRoutable (list) && list->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
        {
            ForwardToList (list, ev);
            return true;
        }
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        //  A floating window's tips show from its own tooltip.
        UpdateTooltip (ev.positionDip);

        for (DxuiTextInput * box : { m_commandBox, m_memoryBox })
        {
            if (IsRoutable (box))
            {
                box->SetMouseHover (x, y);
            }
        }

        //  Each find widget moves with its pane.
        for (const auto & [findPane, state] : m_findStates)
        {
            if (state.box == nullptr || !IsRoutable (state.box))
            {
                continue;
            }

            state.box->SetMouseHover (x, y);

            for (DxuiButton * button : { state.caseButton, state.wordButton, state.regexButton, state.prevButton, state.nextButton, state.selButton, state.closeButton })
            {
                button->SetMouse (x, y, button->HitTest (x, y) && lbDown);
            }
        }

        //  The call stack's button moves with its pane.
        if (IsRoutable (m_callStackButton))
        {
            m_callStackButton->SetMouse (x, y, m_callStackButton->HitTest (x, y) && lbDown);
        }

        return true;

    case DxuiMouseEventKind::Down:
        if (ev.button != DxuiMouseButton::Left)
        {
            return true;
        }

        if (ClickGutter (ev))
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

                if (GetCodeViewOf (list) >= 0)
                {
                    m_activeCode = GetCodeViewOf (list);
                    NoteViewFocus (false);
                }
            }
        }

        return true;

    case DxuiMouseEventKind::Up:
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
            }
        }

        return true;

    case DxuiMouseEventKind::Wheel:
        //  The code pane holds only the lines it shows, so the wheel scrolls
        //  the disassembly itself, through all of memory (FR-073).
        for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
        {
            DxuiListView  * code = m_codeLists[(size_t) view];

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
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            return { m_codeLists[(size_t) view] };
        }
    }

    if (GetSourceSlotOf (pane) >= 0)
    {
        const SourceDocument & document = m_sourceDocs[(size_t) GetSourceSlotOf (pane)];

        return { document.barSlot.get(), document.bar, document.banner, document.view };
    }

    if (pane == DebuggerLayout::kConsole)     { return { m_consoleBarSlot.get(), m_consoleBar, m_consoleView, m_commandBox }; }
    if (pane == DebuggerLayout::kRegisters)   { return { m_registerList };               }
    if (pane == DebuggerLayout::kBreakpoints) { return { m_breakpointSlot.get(), m_breakpointList, m_breakpointBar }; }
    if (pane == DebuggerLayout::kWatches)     { return { m_watchList, m_watchEditor };   }
    if (pane == DebuggerLayout::kStack)       { return { m_stackList };                  }
    if (pane == DebuggerLayout::kCallStack)   { return { m_callStackButton, m_callStackList }; }
    if (pane == DebuggerLayout::kTrace)       { return { m_traceHint, m_traceList };     }

    for (const std::unique_ptr<MemoryPane> & memory : m_memoryPanes)
    {
        if (pane == DebuggerLayout::GetMemoryPaneId (memory->GetId()))
        {
            return { m_memoryBars[(size_t) (memory->GetId() - 1)].get(), memory->GetView() };
        }
    }

    if (DiagnosticsPane * diagnostics = GetDiagnosticsPane (pane))
    {
        return diagnostics->GetControls();
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



    if (GetSourceSlotOf (pane) >= 0)
    {
        return m_sourceDocs[(size_t) GetSourceSlotOf (pane)].frame.get();
    }

    if (pane == DebuggerLayout::kConsole)
    {
        return m_consoleFrame.get();
    }

    if (pane == DebuggerLayout::kCallStack)
    {
        return m_callStackFrame.get();
    }

    if (pane == DebuggerLayout::kTrace)
    {
        return m_traceFrame.get();
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            return m_codeFrames[(size_t) view].get();
        }
    }

    for (const std::unique_ptr<MemoryPane> & memory : m_memoryPanes)
    {
        if (pane == DebuggerLayout::GetMemoryPaneId (memory->GetId()))
        {
            return m_memoryFrames[(size_t) (memory->GetId() - 1)].get();
        }
    }

    if (DiagnosticsPane * diagnostics = GetDiagnosticsPane (pane))
    {
        return diagnostics->GetFrame();
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
    if (pane == DebuggerLayout::kCode)        { return L"Disassembly"; }
    if (GetSourceSlotOf (pane) >= 0)          { return m_sourceDocs[(size_t) GetSourceSlotOf (pane)].title; }
    if (pane == DebuggerLayout::kConsole)     { return L"Console";     }
    if (pane == DebuggerLayout::kRegisters)   { return L"Registers";   }
    if (pane == DebuggerLayout::kBreakpoints) { return L"Breakpoints"; }
    if (pane == DebuggerLayout::kWatches)     { return L"Watches";     }
    if (pane == DebuggerLayout::kStack)       { return L"Stack";       }
    if (pane == DebuggerLayout::kCallStack)   { return L"Call stack";  }
    if (pane == DebuggerLayout::kTrace)       { return L"Trace";       }

    if (DiagnosticsPane * diagnostics = GetDiagnosticsPane (pane))
    {
        return diagnostics->GetTitle();
    }

    if (pane.starts_with (L"code"))
    {
        return L"Disassembly " + pane.substr (4);
    }

    return pane.starts_with (L"memory") ? L"Memory " + pane.substr (6) : pane;
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
    if (control != nullptr && (control == m_memoryBar || control == m_memoryBox))
    {
        return m_memoryBarPane;
    }

    //  A find widget is the pane's it searches, and goes with it.
    if (!GetFindPaneOfControl (control).empty())
    {
        return GetFindPaneOfControl (control);
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
    std::wstring  pane = GetPaneOfControl (control);



    if (m_routingPane.empty())
    {
        return pane.empty() || !m_floats.contains (pane);
    }

    return pane == m_routingPane;
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
    auto  found = m_floatFocus.find (m_routingPane);



    if (m_routingPane.empty() || !m_floats.contains (m_routingPane))
    {
        return m_focusMgr.GetFocusedControl();
    }

    return (found != m_floatFocus.end()) ? found->second : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetFocusedControl (IDxuiControl * control)
{
    IDxuiControl  * old = GetFocused();



    if (m_routingPane.empty() || !m_floats.contains (m_routingPane))
    {
        m_focusMgr.SetFocused (control);
        return;
    }

    if (old == control)
    {
        return;
    }

    if (old != nullptr)
    {
        old->OnFocusChanged (false);
    }

    m_floatFocus[m_routingPane] = control;

    if (control != nullptr)
    {
        control->OnFocusChanged (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetRoutingHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND DebuggerWindow::GetRoutingHwnd() const
{
    auto  found = m_floats.find (m_routingPane);



    return (found != m_floats.end()) ? found->second->GetHwnd() : GetHwnd();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMonitorKey
//
//  A monitor by its device name, which stays the same while it is attached
//  where it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetMonitorKey (const RECT & rectPx)
{
    HMONITOR        monitor = MonitorFromRect (&rectPx, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW  info    = {};



    info.cbSize = sizeof (info);

    if (monitor == nullptr || !GetMonitorInfoW (monitor, &info))
    {
        return L"";
    }

    return info.szDevice;
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
//  DebuggerWindow::RequestFloat
//
//  A pane dropped outside the dock site, or floated from its menu, floats in
//  a window of the size it had, under the cursor. A floating pane dragged
//  somewhere with no zone stays where it was put.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RequestFloat (const std::wstring & pane, POINT clientPx)
{
    auto            found   = m_floats.find (pane);
    IDxuiControl  * content = GetPaneContent (pane);
    RECT            bounds  = (content != nullptr) ? content->GetBounds() : RECT {};
    int             width   = std::max ((int) (bounds.right - bounds.left), m_scaler.ToPx (320));
    int             height  = std::max ((int) (bounds.bottom - bounds.top), m_scaler.ToPx (220)) + m_scaler.ToPx (60);
    POINT           screen  = clientPx;
    RECT            rect    = {};



    if (found != m_floats.end())
    {
        rect = found->second->GetScreenRect();
    }
    else
    {
        ClientToScreen (GetHwnd(), &screen);
        rect = RECT { screen.x - width / 2, screen.y - m_scaler.ToPx (16), screen.x + width / 2, screen.y - m_scaler.ToPx (16) + height };
    }

    if (m_dockSite->EditPaneLayout().Float (pane, GetMonitorKey (rect), rect))
    {
        m_dockSite->Relayout();
        m_syncFloats = true;
        SaveLayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TearOffPane
//
//  A pane's tab dragged off its strip, or a lone pane's title bar dragged,
//  floats the pane at once under the cursor (FR-125). This window lets the
//  mouse go so the floating window, once the frame makes it, can carry the
//  rest of the drag.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::TearOffPane (const std::wstring & pane, POINT clientPx)
{
    RequestFloat (pane, clientPx);

    if (m_dockSite->GetPaneLayout().IsFloating (pane))
    {
        m_tornOffPane = pane;
        ReleaseCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CarryTornOffPane
//
//  The floating window a pane was just torn off into takes over the drag by
//  its title bar, while the button that started it is still down. The
//  system's move loop then shows this window's drop zones and docks the pane
//  where it is released, as a drag of any floating window does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CarryTornOffPane()
{
    std::wstring  pane       = std::move (m_tornOffPane);
    bool          buttonDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;
    HWND          hwnd       = nullptr;



    m_tornOffPane.clear();

    if (pane.empty() || !buttonDown || !m_floats.contains (pane))
    {
        return;
    }

    hwnd = m_floats[pane]->GetHwnd();

    if (hwnd != nullptr && IsWindowVisible (hwnd))
    {
        PlaceUnderGrab (pane);
        PostMessage (hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceUnderGrab
//
//  A pane torn off by its tab floats with that tab in a strip along the
//  window's bottom, and a pane torn off by its title bar with that title
//  bar; either way the window goes where the cursor holds the spot it
//  pressed, rather than centered on the cursor.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceUnderGrab (const std::wstring & pane)
{
    DxuiDockedWindow  & window  = *m_floats[pane];
    DxuiDockSite      & site    = window.GetSite();
    POINT               grab    = m_dockSite->GetTearOffGrab();
    RECT                grabbed = {};
    RECT                rect    = window.GetScreenRect();
    POINT               anchor  = {};
    POINT               cursor  = {};



    if (m_dockSite->WasTearOffFromTab())
    {
        site.SetCarriedPane (pane);
        grabbed = site.GetCarriedTabRect();
    }
    else if (site.GetGroupCount() > 0)
    {
        grabbed = site.GetGroup (0)->GetTitleRect();
    }

    if (grabbed.right <= grabbed.left || !GetCursorPos (&cursor))
    {
        return;
    }

    anchor = POINT { grabbed.left + grab.x, grabbed.top + grab.y };
    ClientToScreen (window.GetHwnd(), &anchor);
    OffsetRect (&rect, cursor.x - anchor.x, cursor.y - anchor.y);
    window.SetScreenRect (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SyncFloats
//
//  Makes the floating windows match the layout: a window for each floating
//  pane and none for the rest, each shown while this window is and its pane
//  is. Run from the frame, never from inside a floating window's own
//  message, which may be the window about to go.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SyncFloats()
{
    const DxuiPaneLayout &  layout  = m_dockSite->GetPaneLayout();
    bool                    visible = IsWindowVisible (GetHwnd()) != FALSE;
    IDxuiControl          * focused = nullptr;



    if (m_syncFloats)
    {
        m_syncFloats = false;

        for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
        {
            if (layout.IsFloating (pane) && !m_floats.contains (pane))
            {
                FloatControls (pane);
            }
            else if (!layout.IsFloating (pane) && m_floats.contains (pane))
            {
                DockControls (pane);
            }
        }

        m_dockSite->Relayout();
        m_focusMgr.Rebuild();

        focused = m_focusMgr.GetFocusedControl();

        if (focused == nullptr || !IsRoutable (focused))
        {
            m_focusMgr.SetFocused (IsRoutable (m_commandBox) ? (IDxuiControl *) m_commandBox : m_codeList);
        }
    }

    for (const auto & entry : m_floats)
    {
        bool  shown = visible && IsPaneShown (entry.first);

        if ((IsWindowVisible (entry.second->GetHwnd()) != FALSE) != shown)
        {
            if (shown)
            {
                entry.second->Show (false);
            }
            else
            {
                entry.second->Hide();
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FloatControls
//
//  A window for one floating pane, at the place the layout keeps for it,
//  with the pane's controls moved into it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::FloatControls (const std::wstring & pane)
{
    std::unique_ptr<DxuiDockedWindow>  window = std::make_unique<DxuiDockedWindow>();
    DxuiWindow::CreateParams           params;
    RECT                               rect   = {};
    HRESULT                            hr     = S_OK;



    for (const DxuiPaneLayout::Floating & floating : m_dockSite->GetPaneLayout().GetFloating())
    {
        rect = (floating.pane == pane) ? floating.rectDip : rect;
    }

    params.title            = GetPaneTitle (pane);
    params.hInstance        = m_hInstance;
    params.ownerHwnd        = GetHwnd();
    params.initialSizeDip   = { std::max (160L, rect.right - rect.left), std::max (120L, rect.bottom - rect.top) };
    params.minSizeDip       = { 160, 120 };
    params.resizable        = true;
    params.captionStyle     = DxuiCaptionStyle::None;
    params.createNoActivate = true;
    params.toolWindow       = true;

    //  Composited, so the row of a dragged tab can fade over a tab strip.
    params.composited       = true;

    hr = window->Create (params);

    if (FAILED (hr))
    {
        //  With no window to float in, the pane goes back where it was.
        (void) m_dockSite->EditPaneLayout().DockBack (pane);
        return;
    }

    window->SetTheme  (m_theme);
    window->SetKeyMap (&DebuggerKeySchemes::GetMap (m_keyScheme));

    for (IDxuiControl * control : GetPaneControls (pane))
    {
        std::unique_ptr<IDxuiControl>  owned = DetachChild (control);

        if (owned != nullptr)
        {
            (void) window->AttachChild (std::move (owned));
        }
    }

    window->GetSite().AddPane       (pane, GetPaneTitle (pane), GetPaneContent (pane));
    window->GetSite().SetPaneLayout (DxuiPaneLayout::MakeSingle (pane));

    //  The pane's title bar is the window's only one: its menu is the pane's
    //  Dock To menu, opened in this window; its pin docks the pane back; and
    //  its close button closes the pane.
    window->GetSite().SetFloating    ([this] (const std::wstring & p) { DockFloatingPane (p); });
    window->GetSite().SetOnPaneMenu  ([this, pane] (const std::wstring & p, POINT clientPx)
    {
        m_routingPane = pane;
        ShowDockToMenu (p, clientPx);
        m_routingPane.clear();
    });
    window->GetSite().SetOnClosePane ([this] (const std::wstring & p) { CloseFloatingPane (p); },
                                      [this] (const std::wstring & p) { return CanClosePane (p); });

    window->SetOnContentMouse      ([this, pane] (const DxuiMouseEvent & ev) { return RouteFloatMouse (pane, ev); });
    window->SetOnContentKey        ([this, pane] (const DxuiKeyEvent & ev)   { return RouteFloatKey   (pane, ev); });
    window->SetOnMappedCommand     ([this]       (int commandId)             { return OnMappedCommand (commandId); });
    window->SetOnCaptionDrag       ([this, pane] (POINT screen)              { OnFloatDrag (pane, screen, false); });
    window->SetOnCaptionDragEnd    ([this, pane] (POINT screen)              { OnFloatDrag (pane, screen, true);  });
    window->SetOnCaptionDragCancel ([this, pane]                             { DropCarriedTab (pane); SetFloatFade (pane, false); m_dockSite->CancelDrag(); HideDragMarks(); Invalidate(); });
    window->SetOnClosed            ([this, pane]                             { CloseFloatingPane (pane); });
    window->SetOnFilesDropped      ([this] (const std::vector<std::wstring> & paths) { return OnFilesDropped (paths); });
    window->SetAcceptsDroppedFiles (true);

    if (rect.right > rect.left && rect.bottom > rect.top)
    {
        window->SetScreenRect (rect);
    }

    //  A tooltip shows only over the window that owns it, so the pane's
    //  toolbar tips come from one of this window's own.
    {
        std::unique_ptr<DxuiTooltip>  tip = std::make_unique<DxuiTooltip>();

        tip->SetPopupHost (window->GetPopupHost());
        tip->SetTheme     (*m_theme);
        tip->SetDpi       (GetDpiForWindow (window->GetHwnd()));
        m_floatTips[pane] = std::move (tip);
    }

    m_floats[pane] = std::move (window);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetBreakpointColumns
//
//  Every column of FR-117, each shown or not as chosen; Name always shows.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetBreakpointColumns()
{
    std::vector<DxuiListView::Column>  columns;



    for (size_t i = 0; i < BreakpointColumns::kCount; i++)
    {
        DxuiListView::Column  column;

        column.title   = BreakpointColumns::GetHeading ((BreakpointColumns::Column) i);
        column.visible = i == (size_t) BreakpointColumns::Column::Name || m_breakpointShown[i];
        columns.push_back (column);
    }

    m_breakpointList->SetColumns (columns);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyBreakpoints
//
//  A checkbox and the mark the code views' gutter shows ahead of the name,
//  then a cell for each column, in the order the chosen heading sorts them.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyBreakpoints()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    if (m_snapshot == nullptr)
    {
        return;
    }

    if (m_breakpointSort >= 0 && m_breakpointSort < (int) BreakpointColumns::kCount)
    {
        m_breakpointOrder = BreakpointColumns::GetOrder (*m_snapshot, (BreakpointColumns::Column) m_breakpointSort, m_breakpointReverse);
        m_breakpointList->SetSortIndicator (m_breakpointSort, m_breakpointReverse);
    }
    else
    {
        m_breakpointOrder.resize (m_snapshot->breakpoints.size());

        for (size_t i = 0; i < m_breakpointOrder.size(); i++)
        {
            m_breakpointOrder[i] = i;
        }
    }

    for (size_t index : m_breakpointOrder)
    {
        const DebuggerViewSnapshot::BreakpointLine  & bp    = m_snapshot->breakpoints[index];
        BreakpointColumns::Cells                      cells = BreakpointColumns::GetCells (*m_snapshot, bp);
        std::vector<DxuiListView::Cell>               row;

        for (const std::string & text : cells)
        {
            DxuiListView::Cell  cell;

            cell.text = Widen (text);
            cell.dim  = !bp.enabled;
            row.push_back (cell);
        }

        row[(size_t) BreakpointColumns::Column::Name].check = bp.enabled;
        row[(size_t) BreakpointColumns::Column::Name].icon  = GetBreakpointIcon (bp.enabled);
        rows.push_back (std::move (row));
    }

    m_breakpointList->SetRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBreakpointOfRow
//
////////////////////////////////////////////////////////////////////////////////

const DebuggerViewSnapshot::BreakpointLine * DebuggerWindow::GetBreakpointOfRow (int row) const
{
    if (m_snapshot == nullptr || row < 0 || row >= (int) m_breakpointOrder.size())
    {
        return nullptr;
    }

    if (m_breakpointOrder[(size_t) row] >= m_snapshot->breakpoints.size())
    {
        return nullptr;
    }

    return &m_snapshot->breakpoints[m_breakpointOrder[(size_t) row]];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleBreakpointColumn
//
//  Shows or hides a column, and keeps the choice with the open views.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleBreakpointColumn (BreakpointColumns::Column column)
{
    if (column == BreakpointColumns::Column::Name || column >= BreakpointColumns::Column::Count)
    {
        return;
    }

    m_breakpointShown[(size_t) column] = !m_breakpointShown[(size_t) column];
    SetBreakpointColumns();
    ApplyBreakpoints();

    if (m_snapshot != nullptr)
    {
        KeepOpenViews();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SortBreakpoints
//
//  A click on a heading sorts by that column, and a second click on the
//  same one turns the order around.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SortBreakpoints (BreakpointColumns::Column column)
{
    m_breakpointReverse = ((int) column == m_breakpointSort) && !m_breakpointReverse;
    m_breakpointSort    = (int) column;
    ApplyBreakpoints();
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
//  DebuggerWindow::DockFloatingPane
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::DockFloatingPane (const std::wstring & pane)
{
    if (m_dockSite->EditPaneLayout().DockBack (pane))
    {
        m_syncFloats = true;
        SaveLayout();
    }
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
//  DebuggerWindow::DockControls
//
//  The pane's controls come back to this window and its floating window
//  goes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::DockControls (const std::wstring & pane)
{
    std::unique_ptr<DxuiDockedWindow>  window = std::move (m_floats[pane]);



    m_floats.erase     (pane);
    m_floatFocus.erase (pane);

    if (m_floatTips.contains (pane))
    {
        m_floatTips[pane]->HideImmediate();
        m_floatTips.erase (pane);
    }

    for (IDxuiControl * control : GetPaneControls (pane))
    {
        std::unique_ptr<IDxuiControl>  owned = window->DetachChild (control);

        if (owned != nullptr)
        {
            (void) AttachChild (std::move (owned));
        }
    }

    //  The memory bar may be sitting in this pane, and would go with the
    //  window.
    if (m_memoryBarHost == window.get())
    {
        MoveMemoryBar (this);
    }

    //  And each find widget, in the pane it searches.
    {
        std::wstring  active = m_findPane;

        SaveFindState();

        for (auto & [findPane, state] : m_findStates)
        {
            if (state.host == window.get())
            {
                ActivateFind (findPane);
                MoveFindBar  (this);
                SaveFindState();
            }
        }

        ActivateFind (active);
    }

    window.reset();

    //  The site's strips and drop zones paint over the panes, so it stays
    //  the last child.
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



    for (const auto & entry : m_floats)
    {
        RECT  rect = entry.second->GetScreenRect();

        //  A window whose pane just docked is still here until the next frame;
        //  floating it again would undo the dock.
        if (layout.IsFloating (entry.first))
        {
            (void) layout.Float (entry.first, GetMonitorKey (rect), rect);
        }
    }

    if (m_host != nullptr)
    {
        m_host->SetDebuggerLayout      (SourcePathList::WideToUtf8 (layout.ToText()));
        m_host->SetDebuggerClosedPanes (SourcePathList::WideToUtf8 (DebuggerLayout::ClosedPanesToText (m_closedPanes)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteFloatMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteFloatMouse (const std::wstring & pane, const DxuiMouseEvent & ev)
{
    bool  handled = false;



    m_routingPane = pane;
    handled       = OnMouse (ev);
    m_routingPane.clear();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteFloatKey
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteFloatKey (const std::wstring & pane, const DxuiKeyEvent & ev)
{
    bool  handled = false;



    m_routingPane = pane;
    handled       = OnKey (ev);
    m_routingPane.clear();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowDragMarks
//
//  The drop targets of a floating window's drag, drawn in an overlay above
//  the floating windows, which would cover them in this window's own paint.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowDragMarks()
{
    HRESULT  hr     = S_OK;
    RECT     client = {};
    POINT    origin = {};



    BAIL_OUT_IF (m_theme == nullptr, S_OK);

    m_dockSite->SetDragMarksDrawnElsewhere (true);

    GetClientRect  (GetHwnd(), &client);
    ClientToScreen (GetHwnd(), &origin);
    OffsetRect     (&client, origin.x, origin.y);

    hr = m_dragOverlay.Show (GetHwnd(), client, m_dockSite->GetDragMarks (*m_theme));
    CHRA (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HideDragMarks
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::HideDragMarks()
{
    m_dragOverlay.Hide();
    m_dockSite->SetDragMarksDrawnElsewhere (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::DropCarriedTab
//
//  A torn-off pane that is still floating once its drag ends loses the tab
//  strip it was carried by.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::DropCarriedTab (const std::wstring & pane)
{
    auto  found = m_floats.find (pane);



    if (found != m_floats.end() && found->second != nullptr)
    {
        found->second->GetSite().SetCarriedPane (L"");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetFloatFade
//
//  A floating pane dragged over a tab strip fades the row of its own tab or
//  title, so the strip it would drop into shows through.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetFloatFade (const std::wstring & pane, bool on)
{
    auto  found = m_floats.find (pane);



    if (found != m_floats.end() && found->second != nullptr)
    {
        found->second->SetHeaderFade (on);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFloatDrag
//
//  A floating window moved by its title bar shows this window's drop zones
//  under the cursor, and docks the pane on the one it is released over.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnFloatDrag (const std::wstring & pane, POINT screenPx, bool ended)
{
    POINT           client = screenPx;
    RECT            area   = m_dockSite->GetBounds();
    bool            inside = false;
    DxuiMouseEvent  ev;



    ScreenToClient (GetHwnd(), &client);

    if (ended)
    {
        DropCarriedTab (pane);
        SetFloatFade   (pane, false);
    }

    inside = client.x >= area.left && client.x < area.right && client.y >= area.top && client.y < area.bottom;

    if (!ended)
    {
        if (!m_dockSite->IsDragging())
        {
            m_dockSite->BeginDrag (pane);
        }

        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = client;
        (void) m_dockSite->OnMouse (ev);
        SetFloatFade (pane, m_dockSite->GetStripTargetGroup() >= 0);
        ShowDragMarks();
        Invalidate();
        return;
    }

    if (!m_dockSite->IsDragging())
    {
        HideDragMarks();
        return;
    }

    //  Released outside the site, the pane stays floating where it was put;
    //  released over a zone it docks there, which the site reports.
    (void) m_dockSite->EndDrag (inside ? client : POINT { area.right + 1, area.bottom + 1 });
    HideDragMarks();
    Invalidate();

    if (!inside)
    {
        SaveLayout();
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
//  DebuggerWindow::RouteCompletionKey
//
//  The command line's completion keys (FR-128, FR-131): Tab takes the gray
//  suggestion, or completes the command word and cycles through the matches,
//  Shift+Tab backward; Right arrow at the end of the line takes the gray
//  earlier line; F8 steps back through earlier lines starting with the typed
//  text; F7 lists the earlier lines to pick from. Tab in an empty box with
//  nothing to offer still moves the focus. Returns true when the key was
//  used here.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteCompletionKey (const DxuiKeyEvent & ev)
{
    std::wstring  text      = m_commandBox->GetText();
    std::wstring  result;
    bool          atEnd     = m_commandBox->GetSelectionStart() == text.size() && m_commandBox->GetSelectionEnd() == text.size();
    bool          isChanged = false;



    switch (ev.vk)
    {
    case VK_TAB:
        isChanged = (!ev.shift && m_completion.TryAcceptSuggestion (text, result)) || m_completion.TryComplete (text, !ev.shift, result);

        if (!isChanged && text.empty())
        {
            return false;
        }

        break;

    case VK_RIGHT:
        if (ev.shift || !atEnd || !m_completion.TryAcceptHistory (text, m_consoleHistory.GetLines(), result))
        {
            return false;
        }

        isChanged = true;
        break;

    case VK_F8:
        isChanged = !ev.shift && m_completion.TrySearchHistory (text, m_consoleHistory.GetLines(), result);
        break;

    case VK_F7:
        ShowHistoryList();
        return true;

    default:
        return false;
    }

    if (isChanged)
    {
        m_commandBox->SetText (result);
        m_commandBox->SetSelection (result.size(), result.size());
    }

    RefreshCommandGhost();
    Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteTraceKey
//
//  The trace pane's own keys, so stepping needs neither the code pane nor the
//  toolbar. Each step and Run goes the way its key-scheme key does.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteTraceKey (const DxuiKeyEvent & ev)
{
    using Action = DebuggerKeySchemes::Action;

    TracePane::KeyAction  action = TracePane::GetKeyAction (ev.vk, ev.ctrl, ev.alt, ev.shift);



    switch (action)
    {
    case TracePane::KeyAction::StepInto:    return OnMappedCommand ((int) Action::StepInto);
    case TracePane::KeyAction::StepOver:    return OnMappedCommand ((int) Action::StepOver);
    case TracePane::KeyAction::StepOut:     return OnMappedCommand ((int) Action::StepOut);
    case TracePane::KeyAction::Run:         return OnMappedCommand ((int) Action::Run);
    case TracePane::KeyAction::ToggleTrace: RunCommandBarEntry (DebuggerCommands::kTrace); return true;
    case TracePane::KeyAction::ToggleBytes: m_tracePane->ToggleBytes(); Invalidate();      return true;
    case TracePane::KeyAction::Save:        SaveTrace();                                   return true;
    default:                                                                               return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RefreshCommandGhost
//
//  The gray text after the caret: the last reply's suggestion while the box
//  is as it was when offered, otherwise the newest earlier line that starts
//  with what is typed.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RefreshCommandGhost()
{
    const std::wstring &  text  = m_commandBox->GetText();
    bool                  atEnd = m_commandBox->GetSelectionStart() == text.size() && m_commandBox->GetSelectionEnd() == text.size();



    m_commandBox->SetGhostText (m_completion.GetGhost (text, atEnd, m_consoleHistory.GetLines()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowHistoryList
//
//  The earlier lines, newest first, in a menu over the command line; picking
//  one puts it in the box to edit or run.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowHistoryList()
{
    static constexpr size_t           s_kMaxRows = 30;
    const std::vector<std::wstring> & lines      = m_consoleHistory.GetLines();
    std::vector<DxuiPopupMenuItem>    menu;
    RECT                              box        = m_commandBox->GetBounds();



    if (lines.empty())
    {
        return;
    }

    m_menuCommands.clear();
    SetWindowMenus();

    for (auto it = lines.rbegin(); it != lines.rend() && menu.size() < s_kMaxRows; ++it)
    {
        std::wstring  line = *it;



        m_menuCommands.push_back (MakeMenuCommand (line, false, [this, line]
        {
            m_commandBox->SetText (line);
            m_commandBox->SetSelection (line.size(), line.size());
            SetFocusedControl (m_commandBox);
            RefreshCommandGhost();
            Invalidate();
        }));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    DxuiContextMenu::Show (*GetMenuHost(), box.left, box.top, std::move (menu));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveTrace
//
//  HISTORY SAVE to a file the user picks; backing out of the picker saves
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveTrace()
{
    HRESULT                hr     = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters          = { { L"Text files", L"*.txt" }, { L"All files", L"*.*" } };
    spec.defaultExtension = L"txt";
    spec.defaultFileName  = L"trace.txt";

    hr = m_host->GetHostDialogs().PickFileToSave (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    RunAction (DebuggerActions::GetSaveHistory (TextEncoding::WideToNarrow (chosen.wstring()), GetMode()));

Error:
    return;
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



    //  The menu bar takes the keys while one of its menus is open, and Alt
    //  with a letter opens the menu it marks.
    if (RouteMenuBarKey (ev, handled))
    {
        return handled;
    }

    //  A watch being edited takes every key: Enter keeps what was typed,
    //  Escape leaves the watch as it was.
    if (m_watchEdit.row >= 0)
    {
        if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE))
        {
            EndWatchEdit (ev.vk == VK_RETURN);
        }
        else
        {
            m_watchEditor->OnKey (ev);
        }

        Invalidate();
        return true;
    }

    //  Shift+Esc closes the pane that has the focus, as in Visual Studio.
    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && ev.shift && !ev.ctrl && !ev.alt && ClosePaneOfFocus())
    {
        return true;
    }

    //  Delete removes a manual watch; F2 edits the selected one's value, as
    //  it does in Visual Studio's watch window.
    if (ev.kind == DxuiKeyEventKind::Down && focused == m_watchList && !ev.ctrl && !ev.alt)
    {
        if (ev.vk == VK_DELETE)
        {
            RemoveSelectedWatch();
            return true;
        }

        if (ev.vk == VK_F2)
        {
            BeginWatchEdit (m_watchList->GetSelectedRow(), 1);
            return true;
        }
    }

    //  Ctrl+Z in the watch pane undoes its own last edit, and nothing of any
    //  memory window's (FR-097).
    if (ev.kind == DxuiKeyEventKind::Down && focused == m_watchList && ev.ctrl && !ev.alt && (ev.vk == 'Z' || ev.vk == 'Y'))
    {
        UndoWatchEdit (ev.vk == 'Y');
        return true;
    }

    //  The stack pane's the same way; F2 edits the selected byte.
    if (ev.kind == DxuiKeyEventKind::Down && focused == m_stackList && ev.ctrl && !ev.alt && (ev.vk == 'Z' || ev.vk == 'Y'))
    {
        UndoStackEdit (ev.vk == 'Y');
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && focused == m_stackList && !ev.ctrl && !ev.alt && ev.vk == VK_F2)
    {
        EditStackByte (m_stackList->GetSelectedRow());
        return true;
    }

    //  Ctrl+Z and Ctrl+Y in the registers pane undo and redo its own edits.
    if (ev.kind == DxuiKeyEventKind::Down && focused == m_registerList && ev.ctrl && !ev.alt && (ev.vk == 'Z' || ev.vk == 'Y'))
    {
        UndoRegisterEdit (ev.vk == 'Y');
        return true;
    }

    //  Ctrl+C with nothing selected in the command line copies what is
    //  selected in the output above it, where a click leaves the focus.
    if (ev.kind == DxuiKeyEventKind::Down && focused == m_commandBox && ev.ctrl && !ev.alt && ev.vk == 'C' &&
        m_commandBox->GetSelectionStart() == m_commandBox->GetSelectionEnd() && m_consoleView->HasSelection())
    {
        m_consoleView->CopySelection();
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && focused == m_commandBox && !ev.ctrl && !ev.alt && RouteCompletionKey (ev))
    {
        return true;
    }

    //  Up and Down in the command line walk the lines run before.
    if (ev.kind == DxuiKeyEventKind::Down && focused == m_commandBox && !ev.ctrl && !ev.alt && (ev.vk == VK_UP || ev.vk == VK_DOWN))
    {
        std::optional<std::wstring>  recalled = (ev.vk == VK_UP) ? m_consoleHistory.GetOlder (m_commandBox->GetText()) : m_consoleHistory.GetNewer (m_commandBox->GetText());



        if (recalled.has_value())
        {
            m_commandBox->SetText (*recalled);
            m_commandBox->SetSelection (recalled->size(), recalled->size());
            Invalidate();
        }

        return true;
    }

    if (RouteFindKey (ev, handled))
    {
        return handled;
    }

    if (RouteBoxKey (ev, handled))
    {
        return handled;
    }

    if (ev.kind == DxuiKeyEventKind::Down && focused == m_traceList && RouteTraceKey (ev))
    {
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && RouteDockKey (ev))
    {
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN)
    {
        if      (focused == m_commandBox) { SubmitCommandBox(); return true; }
        else if (focused == m_memoryBox)  { SubmitMemoryBox();  return true; }
    }

    if (ev.kind == DxuiKeyEventKind::Char && GetFocusedMemoryPane() != nullptr)
    {
        return focused->OnKey (ev);
    }

    //  A floating window types into its own focused control.
    if (ev.kind == DxuiKeyEventKind::Char && !m_routingPane.empty())
    {
        return focused != nullptr && focused->OnKey (ev);
    }

    //  Only the box this window's focus is on types. A box focused in a
    //  floating window keeps its own focused flag after a click back here.
    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return (focused == m_commandBox || focused == m_memoryBox) && focused->OnKey (ev);
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

    //  Ctrl+Z and Ctrl+Y in a memory window undo and redo that window's edits.
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && (ev.vk == 'Z' || ev.vk == 'Y') && GetFocusedMemoryPane() != nullptr)
    {
        UndoMemoryEdit (GetFocusedMemoryPane(), ev.vk == 'Y');
        return true;
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

        if (!handled && ev.vk == VK_TAB && m_routingPane.empty())
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
