#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
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

    m_theme     = theme;
    m_host      = host;
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

    SetTheme (m_theme);
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
    m_commandBar        = CreateChild<DxuiToolbar>   ();
    m_codeList          = CreateChild<DxuiListView>  ();
    m_registerList      = CreateChild<DxuiListView>  ();
    m_breakpointList    = CreateChild<DxuiListView>  ();
    m_watchList         = CreateChild<DxuiListView>  ();
    m_stackList         = CreateChild<DxuiListView>  ();
    m_callStackList     = CreateChild<DxuiListView>  ();
    m_callStackButton   = CreateChild<DxuiButton>    (L"Hybrid");
    m_consoleList       = CreateChild<DxuiListView>  ();
    m_traceList         = CreateChild<DxuiListView>  ();
    m_commandBox        = CreateChild<DxuiTextInput> ();
    m_memoryBox         = CreateChild<DxuiTextInput> ();
    m_pokeBox           = CreateChild<DxuiTextInput> ();
    m_pokeButton        = CreateChild<DxuiButton>    (L"Poke");
    m_groupButton       = CreateChild<DxuiButton>    (L"Bytes");
    m_addMemoryButton   = CreateChild<DxuiButton>    (L"+ Memory");
    m_removeMemoryButton = CreateChild<DxuiButton>   (L"- Memory");

    //  All four windows exist from the start; the ones not open are hidden.
    for (int id = 1; id <= DebuggerViewState::kMaxMemoryWindows; id++)
    {
        DxuiHexView  * view = CreateChild<DxuiHexView>();

        m_memoryPanes[(size_t) (id - 1)] = std::make_unique<MemoryPane> (
            id, view,
            [this] (int window, Word first)          { if (m_host != nullptr) { m_host->SetDebuggerMemoryWindow (window, first); } },
            [this] (const std::string & line)        { RunCommand (line); },
            [this] (const std::string & line)        { AppendConsole ({ line }); });

        view->SetVisible (id == 1);
        m_memoryOpen[(size_t) (id - 1)] = (id == 1);
    }

    m_tracePane = std::make_unique<TracePane> (
        m_traceList,
        [this] (std::optional<uint64_t> first) { if (m_host != nullptr) { m_host->SetDebuggerTraceTop (first); } });

    //  Shown only while a debug file is loaded.
    m_sourceView   = CreateChild<DxuiTextView>();
    m_sourceBanner = CreateChild<DxuiActionBanner>();
    m_sourcePane   = std::make_unique<SourcePane> (
        m_sourceView, m_sourceBanner,
        [this] (const DebugSourceFile & record, const std::wstring & path, const std::string & key)
        {
            return (m_host != nullptr) ? m_host->FindDebuggerSource (record, path, key) : SourceLookup();
        },
        [this] (const std::string & line) { RunCommand (line); },
        [this] (Word address)             { ShowCode (address); });

    m_sourceView->SetVisible   (false);
    m_sourceBanner->SetVisible (false);

    m_callStackPane = std::make_unique<CallStackPane> (
        m_callStackList, m_callStackButton,
        [this] (const std::string & line) { RunCommand (line); },
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
    auto  run = [this] (const std::string & line) { RunCommand (line); };



    ConfigureCommandBar();

    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        pane->Configure();
    }

    m_pokeButton->SetOnClick ([this] { SubmitPokeBox(); });

    m_groupButton->SetOnClick ([this]
    {
        static const wchar_t * const  kNames[] = { L"", L"Bytes", L"Words", L"", L"Longs" };

        m_groupButton->SetLabel (kNames[GetActiveMemoryPane()->CycleGrouping()]);
    });

    m_addMemoryButton->SetOnClick    ([this] { AddMemoryWindow();    });

    //  The + after the memory tabs adds a window now; the button is kept for
    //  the keyboard's sake but not shown.
    m_addMemoryButton->SetVisible (false);
    m_removeMemoryButton->SetOnClick ([this] { RemoveMemoryWindow(); });

    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        pane->Configure (GetHwnd());
    }

    m_sourcePane->Configure (GetHwnd());
    m_callStackPane->Configure();
    SetAcceptsDroppedFiles  (true);

    //  A code row selected shows its line in the source pane (FR-054).
    m_codeList->SetOnSelectionChanged ([this] (int row)
    {
        if (m_snapshot != nullptr && row >= 0 && row < (int) m_snapshot->code.size())
        {
            m_sourcePane->ShowLine (m_snapshot->code[(size_t) row].sourceFileId, m_snapshot->code[(size_t) row].sourceLine);
        }
    });

    //  Every column fits its contents and none stretches, so a pane is as wide
    //  as what it shows and no wider (FR-026a). The marker column alone has a
    //  set width, since its glyphs are not text a fit could measure.
    m_codeList->SetColumns ({ { L"",            kGutterColumnDip, false, DxuiTextHAlign::Left   },
                              { L"",            kMarkerColumnDip, false, DxuiTextHAlign::Center },
                              { L"Address",     0, false, DxuiTextHAlign::Left },
                              { L"Bytes",       0, false, DxuiTextHAlign::Left },
                              { L"Label",       0, false, DxuiTextHAlign::Left },
                              { L"Instruction", 0, false, DxuiTextHAlign::Left },
                              { L"",            0, false, DxuiTextHAlign::Left } });

    //  A breakpoint is set from the gutter (see ClickGutter), as in an editor;
    //  a double-click on a line is a click on text and changes nothing.
    m_codeList->SetActivateOnDoubleClick       (true);
    m_breakpointList->SetActivateOnDoubleClick (true);
    m_registerList->SetActivateOnDoubleClick   (true);
    m_stackList->SetActivateOnDoubleClick      (true);

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
    m_breakpointList->SetColumns ({ { L"",            kGutterColumnDip, false, DxuiTextHAlign::Left   },
                                    { L"Breakpoints", 0, false, DxuiTextHAlign::Left } });
    m_watchList->SetColumns      ({ { L"Watch",       0, false, DxuiTextHAlign::Left },
                                    { L"Value",       0, false, DxuiTextHAlign::Left } });
    m_stackList->SetColumns      ({ { L"Stack",       0, false, DxuiTextHAlign::Left },
                                    { L"Value",       0, false, DxuiTextHAlign::Left } });
    m_consoleList->SetColumns    ({ { L"Console",     0, false, DxuiTextHAlign::Left } });
    m_consoleList->EnableStickyTail (true);

    //  THE CONSOLE IS A LOG, NOT A LIST. Its lines select as text does -- a
    //  drag or Shift+click takes a run of lines, in the color a text
    //  control selects with, and Ctrl+C copies them -- rather than lighting
    //  one row as a pick from a list.
    m_consoleList->SetMultiSelect         (true);
    m_consoleList->SetTextSelectionColors (true);
    m_consoleList->SetOwnerWindow         (GetHwnd());

    //  Activating a breakpoint shows its address; its circle, in the gutter,
    //  turns it on and off.
    m_breakpointList->SetOnActivateRow ([this] (int row)
    {
        if (m_snapshot != nullptr && row >= 0 && row < (int) m_snapshot->breakpoints.size())
        {
            ShowCode (m_snapshot->breakpoints[(size_t) row].address);
        }
    });

    for (DxuiListView * list : GetLists())
    {
        MakeDense (list);
    }

    //  The console can hold thousands of lines, so it fits its column from a
    //  character count rather than measuring every line it has ever held. The
    //  trace pane shows a window of a far longer list, so it does the same.
    m_consoleList->SetPreciseAutoFit (false);
    m_tracePane->Configure();

    for (DxuiTextInput * box : { m_commandBox, m_memoryBox, m_pokeBox })
    {
        box->SetHwnd      (GetHwnd());
        box->SetMaxLength (256);
    }

    m_commandBox->SetPrompt      (GetPromptText (CommandMode::AppleWin));
    m_commandBox->SetPlaceholder (L"Command (Enter to run, HELP for help)");
    m_memoryBox->SetPlaceholder  (L"Go to: 0300, PC, (3E),Y");
    m_pokeBox->SetPlaceholder    (L"Poke: 0300 A9");

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
    m_commandBar->EnableSeeMore   (L"\uE712", L"See more");
    m_commandBar->SetEntries      (m_commands->BuildEntries());

    SetCommandBarMenus();
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
//  DebuggerWindow::SetCommandBarMenus
//
//  The drop-downs: the machine's device panels, the command dialects and the
//  key schemes, each checked where it is the one in force. Rebuilt whenever
//  what they list changes, since the rows carry the state they were built
//  with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetCommandBarMenus()
{
    std::vector<DxuiPopupMenuItem>  panels;
    std::vector<DxuiPopupMenuItem>  modes;
    std::vector<DxuiPopupMenuItem>  schemes;



    m_menuCommands.clear();

    if (m_snapshot != nullptr)
    {
        for (const DebuggerViewSnapshot::PanelInfo & panel : m_snapshot->panels)
        {
            std::string  id   = panel.id;
            bool         open = panel.open;
            CommandMode  mode = m_snapshot->mode;

            m_menuCommands.push_back (MakeMenuCommand (Widen (panel.title), open, [this, id, open, mode]
            {
                RunCommand (DebuggerViewState::GetPanelLine (id, !open, mode));
            }));

            panels.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
        }
    }

    for (const auto & [mode, label] : { std::pair { CommandMode::AppleWin,  L"AppleWin"  },
                                        std::pair { CommandMode::Monitor,   L"Monitor"   },
                                        std::pair { CommandMode::GSSquared, L"GSSquared" },
                                        std::pair { CommandMode::WinDbg,    L"WinDbg"    } })
    {
        bool  current = (m_snapshot != nullptr) && m_snapshot->mode == mode;

        m_menuCommands.push_back (MakeMenuCommand (label, current, [this, mode]
        {
            RunCommand ("MODE " + CommandModeNames::GetUpperName (mode));
        }));

        modes.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    for (DebuggerKeyScheme scheme : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
    {
        m_menuCommands.push_back (MakeMenuCommand (DebuggerKeySchemes::GetMap (scheme).GetName(), scheme == m_keyScheme, [this, scheme]
        {
            ApplyKeyScheme (scheme);

            if (m_host != nullptr)
            {
                m_host->SetDebuggerKeyScheme (DebuggerKeySchemes::GetName (scheme));
            }

            SetCommandBarMenus();
        }));

        schemes.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    m_commandBar->SetDropDownItems (DebuggerCommands::kPanels,    std::move (panels));
    m_commandBar->SetDropDownItems (DebuggerCommands::kMode,      std::move (modes));
    m_commandBar->SetDropDownItems (DebuggerCommands::kKeyScheme, std::move (schemes));
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
        RunCommand (DebuggerViewState::GetTraceToggleLine (m_snapshot != nullptr && m_snapshot->trace.isOn));
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
        return paused && m_codeList != nullptr && m_codeList->GetSelectedRow() >= 0;
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
    std::vector<DxuiListView *>  lists = { m_codeList, m_registerList, m_breakpointList, m_watchList, m_stackList, m_callStackList,
                                           m_consoleList, m_traceList };



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
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MakeDense (DxuiListView * list)
{
    list->SetShowHeader      (true);
    list->SetMonospace       (true);
    list->SetFontSizeDip     (kPaneFontDip);
    list->SetRowHeightDip    (kPaneRowDip);
    list->SetHeaderHeightDip (kPaneHeaderDip);
    list->SetCellPaddingDip  (kPanePadDip, kPanePadDip);
    list->SetPreciseAutoFit  (true);
    list->SetRefitOnSetRows  (true);
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
    m_textZoom = std::clamp (zoom, 0.6f, 2.5f);

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

    m_sourceView->SetZoom (m_textZoom);

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



    for (DxuiButton * button : GetMemoryButtons())
    {
        targets.push_back (button);
    }

    targets.insert (targets.end(), { m_callStackButton, m_commandBox, m_memoryBox, m_pokeBox });

    return targets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMemoryButtons
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiButton *> DebuggerWindow::GetMemoryButtons() const
{
    return { m_pokeButton, m_groupButton, m_removeMemoryButton };
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
            AppendConsole ({ std::format ("Go to: \"{}\" is not an address, a register or a 6502 operand this can resolve.", goTo.text) });
        }
    }

    if (m_snapshot->machine != m_machine)
    {
        m_machine = m_snapshot->machine;

        for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
        {
            pane->ClearHistory();
        }
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
//  DebuggerWindow::AddMemoryWindow
//
//  The lowest-numbered closed window opens where the active one is.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddMemoryWindow()
{
    Word  at = GetActiveMemoryPane()->GetTopAddress();



    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        if (!m_memoryOpen[(size_t) (pane->GetId() - 1)] && m_host != nullptr)
        {
            m_host->SetDebuggerMemoryWindow (pane->GetId(), at);
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RemoveMemoryWindow
//
//  The highest-numbered open window closes; the first never does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RemoveMemoryWindow()
{
    for (size_t i = m_memoryPanes.size(); i > 1; i--)
    {
        MemoryPane  * pane = m_memoryPanes[i - 1].get();

        if (m_memoryOpen[(size_t) (pane->GetId() - 1)] && m_host != nullptr)
        {
            m_host->SetDebuggerMemoryWindow (pane->GetId(), std::nullopt);
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySource
//
//  The source pane takes the snapshot, and the layout changes when the pane
//  or its banner comes or goes, or the banner's text or actions change: a new
//  action has no place until it is laid out.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySource()
{
    bool          shown       = false;
    bool          bannerShown = false;
    std::wstring  bannerKey;



    m_sourcePane->Apply (*m_snapshot);
    bannerKey = m_sourceBanner->GetText() + (m_sourceBanner->GetAction (0) != nullptr ? m_sourceBanner->GetAction (0)->GetAccessibleName() : L"");

    shown       = m_sourcePane->IsActive();
    bannerShown = shown && m_sourcePane->HasBanner();

    if (shown != m_sourceShown)
    {
        m_sourceShown       = shown;
        m_sourceBannerShown = bannerShown;
        m_sourceBannerKey   = bannerKey;
        m_dockSite->Relayout();
    }
    else if (bannerShown != m_sourceBannerShown || bannerKey != m_sourceBannerKey)
    {
        m_sourceBannerShown = bannerShown;
        m_sourceBannerKey   = bannerKey;
        m_sourceFrame->Relayout();
    }
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
    POINT     at       = ev.positionDip;
    RECT      bounds   = m_sourceView->GetBounds();
    RECT      banner   = m_sourceBanner->GetBounds();
    bool      inside   = at.x >= bounds.left && at.x < bounds.right && at.y >= bounds.top && at.y < bounds.bottom;
    bool      onBanner = at.x >= banner.left && at.x < banner.right && at.y >= banner.top && at.y < banner.bottom;
    uint64_t  now      = GetTickCount64();
    bool      isDouble = false;



    //  A source pane behind another tab takes no input.
    if (!m_sourceShown || !m_sourceView->IsVisible() || !IsRoutable (m_sourceView))
    {
        return false;
    }

    if (m_sourceBannerShown && (onBanner || ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Move))
    {
        (void) m_sourceBanner->OnMouse (ev);

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

    if (m_sourceView->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
    {
        (void) m_sourceView->OnMouse (ev);
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
            m_sourcePane->OnDoubleClick (at);
        }
        else
        {
            m_sourcePane->OnClick (at);
        }

        SetFocusedControl (m_sourceView);
        NoteViewFocus (true);
    }

    (void) m_sourceView->OnMouse (ev);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NoteViewFocus
//
//  Steps follow the view: the source pane steps by source line, the
//  disassembly by instruction (FR-056). Sent only when it changes, so a click
//  does not fill the console.
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
        m_host->RunDebuggerCommand (isSource ? "SRC ON" : "SRC OFF");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFilesDropped
//
//  The first file goes to the source pane, matched against the loaded debug
//  file's records by the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnFilesDropped (const std::vector<std::wstring> & paths)
{
    SourceLookup  lookup;
    int           index = -1;



    if (paths.empty() || m_host == nullptr || m_snapshot == nullptr || !m_snapshot->source.has_value())
    {
        return false;
    }

    lookup = m_host->MatchDroppedDebuggerSource (m_snapshot->source->files, paths.front(), m_snapshot->source->programKey, index);
    m_sourcePane->ShowDropped (lookup, index);
    ApplySource();

    return true;
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



    for (DxuiTextInput * box : { m_commandBox, m_memoryBox, m_pokeBox })
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
//  DebuggerWindow::RunCommand
//
//  A line a control or a key sends, in the words of the session's mode.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunCommand (const std::string & line)
{
    CommandMode  mode = (m_snapshot != nullptr) ? m_snapshot->mode : CommandMode::AppleWin;



    if (m_host != nullptr)
    {
        m_host->RunDebuggerCommand (DebuggerViewState::GetModeLine (line, mode));
    }
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CycleKeyScheme
//
//  Visual Studio, AppleWin, GSSquared, and round again; the choice is saved at
//  once, so it holds whether or not the window is closed cleanly.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CycleKeyScheme()
{
    static constexpr DebuggerKeyScheme  kOrder[] = { DebuggerKeyScheme::VisualStudio,
                                                     DebuggerKeyScheme::AppleWin,
                                                     DebuggerKeyScheme::GSSquared };
    size_t                              next     = 0;



    for (size_t i = 0; i < std::size (kOrder); ++i)
    {
        if (kOrder[i] == m_keyScheme)
        {
            next = (i + 1) % std::size (kOrder);
        }
    }

    ApplyKeyScheme (kOrder[next]);

    if (m_host != nullptr)
    {
        m_host->SetDebuggerKeyScheme (DebuggerKeySchemes::GetName (kOrder[next]));
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
    DebuggerKeySchemes::Action  action = (DebuggerKeySchemes::Action) commandId;
    std::optional<std::string>  line;
    int                         row    = (m_codeList != nullptr) ? m_codeList->GetSelectedRow() : -1;



    if (action == DebuggerKeySchemes::Action::Pause)
    {
        if (m_host != nullptr)
        {
            m_host->PauseDebugger();
        }

        return true;
    }

    line = DebuggerViewState::GetActionLine (action, m_snapshot.get(), row);

    if (line.has_value())
    {
        RunCommand (*line);
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
//  Space and F10 step and Return resumes, whatever the scheme.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteBoxKey (const DxuiKeyEvent & ev, bool & handled)
{
    DxuiTextInput                               * box           = GetFocusedBox();
    bool                                          inMemory      = GetFocusedMemoryPane() != nullptr;
    bool                                          decided       = false;
    CommandMode                                   mode          = (m_snapshot != nullptr) ? m_snapshot->mode : CommandMode::AppleWin;
    std::optional<DebuggerKeySchemes::Action>     consoleAction;



    consoleAction = (ev.kind == DxuiKeyEventKind::Down && box != nullptr && box == m_commandBox)
                  ? DebuggerViewState::GetConsoleKeyAction (mode, ev.vk, ev.ctrl, ev.alt, ev.shift, box->GetText().empty())
                  : std::nullopt;

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
    else if (ev.kind == DxuiKeyEventKind::Down && (box != nullptr || inMemory) &&
             !DebuggerKeySchemes::DoesBoxKeepKey (ev.vk, ev.ctrl, ev.alt, true, box != nullptr && box->GetText().empty()) &&
             RouteMappedKey (ev))
    {
        m_swallowSpace = ev.vk == VK_SPACE;
        handled        = true;
        decided        = true;
    }

    return decided;
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
    m_commandBar->SetTextRenderer   (GetTextRenderer());
    m_commandBar->SetHostClientRect (RECT { 0, 0, width, height });
    m_commandBar->Layout (RECT { pad, rowY, width - pad, rowY + buttonH }, m_scaler);
    m_tooltip.SetDpi          (m_scaler.GetDpi());
    m_tooltip.SetViewportSize (width, height);

    top  = rowY + buttonH;
    barY = height - pad;

    m_dockSite->Layout (RECT { pad, top, width - pad, barY }, m_scaler);

    //  The code pane holds as many lines as it has room for, so the pane is
    //  full whatever height the user drags it to. Only a change is sent: the
    //  count crosses to the CPU thread, which rebuilds the snapshot.
    if (m_codeList != nullptr)
    {
        int  fits = m_codeList->GetVisibleRowCapacity();

        if (fits > 0 && fits != m_codeLinesSent && m_host != nullptr)
        {
            m_codeLinesSent = fits;
            m_host->SetDebuggerCodeLines (fits);
        }
    }

    PlaceMemoryBar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceMemoryBar
//
//  The Go to box, the poke box and the memory buttons sit in the bar of the
//  memory pane they act on: the active one when it is shown here, or else
//  the first memory pane shown. With none shown in this window, they hide.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceMemoryBar()
{
    auto                 px    = [this] (int dip) { return m_scaler.ToPx (dip); };
    int                  pad   = px (6);
    DebuggerPaneFrame  * bar   = nullptr;
    MemoryPane         * owner = GetActiveMemoryPane();
    RECT                 slot  = {};
    int                  x     = 0;
    auto                 isHere = [this] (size_t i)
    {
        return m_memoryBars[i] != nullptr && m_memoryBars[i]->IsVisible() &&
               !m_floats.contains (DebuggerLayout::GetMemoryPaneId ((int) i + 1));
    };



    if (owner != nullptr && isHere ((size_t) (owner->GetId() - 1)))
    {
        bar = m_memoryBars[(size_t) (owner->GetId() - 1)].get();
    }

    for (size_t i = 0; bar == nullptr && i < m_memoryBars.size(); i++)
    {
        if (isHere (i))
        {
            bar = m_memoryBars[i].get();
        }
    }

    for (IDxuiControl * control : { (IDxuiControl *) m_memoryBox, (IDxuiControl *) m_pokeBox })
    {
        control->SetVisible (bar != nullptr);
    }

    for (DxuiButton * button : GetMemoryButtons())
    {
        button->SetVisible (bar != nullptr);
    }

    if (bar == nullptr)
    {
        return;
    }

    slot = bar->GetBounds();
    x    = slot.left;

    m_memoryBox->Layout (RECT { x, slot.top, x + px (150), slot.bottom }, m_scaler);  x += px (150) + pad;
    m_pokeBox->Layout   (RECT { x, slot.top, x + px (190), slot.bottom }, m_scaler);  x += px (190) + pad;

    for (DxuiButton * button : GetMemoryButtons())
    {
        button->Layout (RECT { x, slot.top, x + px (80), slot.bottom }, m_scaler);
        x += px (80) + pad;
    }
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
    auto          boxHeight = [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (30); };
    std::wstring    savedText;
    DxuiPaneLayout  restored;



    m_sourceFrame = std::make_unique<DebuggerPaneFrame> (L"Source");
    m_sourceFrame->AddPart (m_sourceBanner,
                            [this] (int width, const DxuiDpiScaler & scaler) { return (int) m_sourceBanner->GetPreferredHeightPx ((float) width, scaler); },
                            [this] { return m_sourceBannerShown; });
    m_sourceFrame->AddPart (m_sourceView);

    m_consoleFrame = std::make_unique<DebuggerPaneFrame> (L"Console");
    m_consoleFrame->AddPart (m_consoleList);
    m_consoleFrame->AddPart (m_commandBox, boxHeight);

    m_callStackFrame = std::make_unique<DebuggerPaneFrame> (L"Call Stack");
    //  The pane always shows hybrid; CALLS MODE picks another (FR-068), so
    //  the button that cycled them is not shown.
    m_callStackButton->SetVisible (false);
    m_callStackFrame->AddPart (m_callStackList);

    m_dockSite->AddPane (DebuggerLayout::kCode,        L"Disassembly", m_codeList);
    m_dockSite->AddPane (DebuggerLayout::kSource,      L"Source",      m_sourceFrame.get());
    m_dockSite->AddPane (DebuggerLayout::kConsole,     L"Console",     m_consoleFrame.get());
    m_dockSite->AddPane (DebuggerLayout::kRegisters,   L"Registers",   m_registerList);
    m_dockSite->AddPane (DebuggerLayout::kBreakpoints, L"Breakpoints", m_breakpointList);
    m_dockSite->AddPane (DebuggerLayout::kWatches,     L"Watches",     m_watchList);
    m_dockSite->AddPane (DebuggerLayout::kStack,       L"Stack",       m_stackList);
    m_dockSite->AddPane (DebuggerLayout::kCallStack,   L"Call Stack",  m_callStackFrame.get());
    m_dockSite->AddPane (DebuggerLayout::kTrace,       L"Trace",       m_traceList);

    //  A memory pane is its command bar over its bytes (FR-089). The bar is a
    //  place held at the pane's top; the controls, shared by every memory
    //  pane, are put in the bar of the one they act on (PlaceMemoryBar).
    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        size_t  i = (size_t) (pane->GetId() - 1);

        m_memoryBars[i]   = std::make_unique<DebuggerPaneFrame> (L"Memory commands");
        m_memoryFrames[i] = std::make_unique<DebuggerPaneFrame> (std::format (L"Memory {}", pane->GetId()));
        m_memoryFrames[i]->AddPart (m_memoryBars[i].get(), boxHeight);
        m_memoryFrames[i]->AddPart (pane->GetView());

        m_dockSite->AddPane (DebuggerLayout::GetMemoryPaneId (pane->GetId()),
                             std::format (L"Memory {}", pane->GetId()), m_memoryFrames[i].get());
    }

    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        m_dockSite->AddPane (DebuggerLayout::GetDiagnosticsPaneId (pane->GetId()), pane->GetTitle(), pane->GetFrame());
    }

    m_dockSite->SetShownFn    ([this] (const std::wstring & pane) { return IsPaneShown (pane); });

    //  A + after the memory tabs opens the next memory window, as a browser
    //  opens a tab, until all four are open.
    m_dockSite->SetNewTab ([this] (const DxuiTabGroup & group)
    {
        bool  hasMemory = false;

        for (size_t i = 0; i < group.GetTabCount(); i++)
        {
            for (const std::unique_ptr<DebuggerPaneFrame> & frame : m_memoryFrames)
            {
                hasMemory = hasMemory || group.GetContent ((int) i) == frame.get();
            }
        }

        return hasMemory && std::count (m_memoryOpen.begin(), m_memoryOpen.end(), true) < DebuggerViewState::kMaxMemoryWindows;
    },
    [this] (const DxuiTabGroup &) { AddMemoryWindow(); });
    savedText = (m_host != nullptr) ? SourcePathList::Utf8ToWide (m_host->GetDebuggerLayout()) : std::wstring();
    restored = DebuggerLayout::Restore (savedText);
    restored.PlaceOnMonitors (GetMonitors());
    m_dockSite->SetPaneLayout (restored);
    m_syncFloats = true;

    m_dockSite->SetOnFloatRequested ([this] (const std::wstring & pane, POINT clientPx) { RequestFloat (pane, clientPx); });

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
//  open (FR-044); the rest always show.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsPaneShown (const std::wstring & pane) const
{
    std::string  diagnosticsId;



    if (pane == DebuggerLayout::kSource)
    {
        return m_sourceShown;
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
//  DebuggerWindow::GetPaneOfFocus
//
//  The pane holding the focused control, including the parts of a frame.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneOfFocus() const
{
    IDxuiControl  * focused = GetFocused();



    if (!m_routingPane.empty())
    {
        return m_routingPane;
    }

    if (focused == nullptr)
    {
        return L"";
    }

    if (focused == m_consoleList || focused == m_commandBox)
    {
        return DebuggerLayout::kConsole;
    }

    if (focused == m_sourceView)
    {
        return DebuggerLayout::kSource;
    }

    for (const std::unique_ptr<DiagnosticsPane> & pane : m_diagPanes)
    {
        if (focused == pane->GetList())
        {
            return DebuggerLayout::GetDiagnosticsPaneId (pane->GetId());
        }
    }

    return m_dockSite->GetPaneOf (focused);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowDockToMenu
//
//  The Dock To choices for a pane as a context menu (FR-042); the one chosen
//  runs through the site like a drop would.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowDockToMenu (const std::wstring & pane, POINT clientPx)
{
    std::vector<DxuiDockSite::MenuItem>  items = m_dockSite->GetDockToMenu (pane);
    std::vector<DxuiPopupMenuItem>       menu;
    DxuiHwndSource                     * host  = GetPopupHost();
    auto                                 found = m_floats.find (m_routingPane);



    //  A floating pane's menu opens in its own window, where the click was.
    if (!m_routingPane.empty() && found != m_floats.end())
    {
        host = found->second->GetPopupHost();
    }

    if (items.empty() || host == nullptr)
    {
        return;
    }

    m_menuCommands.clear();
    SetCommandBarMenus();

    for (const DxuiDockSite::MenuItem & item : items)
    {
        m_menuCommands.push_back (MakeMenuCommand (item.label, false, [action = item.action] { (void) action(); }));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
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



    if (m_snapshot == nullptr || GetPopupHost() == nullptr || !m_routingPane.empty())
    {
        return false;
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
        if (DxuiDockSite::Contains (each->GetView()->GetBounds(), clientPx) && each->GetView()->IsVisible())
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
    }

    if (items.empty())
    {
        return false;
    }

    m_menuCommands.clear();
    SetCommandBarMenus();

    for (auto & [label, action] : items)
    {
        m_menuCommands.push_back (MakeMenuCommand (label, false, action));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    DxuiContextMenu::Show (*GetPopupHost(), clientPx.x, clientPx.y, std::move (menu));
    return true;
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



    if (list == m_codeList && row >= 0 && row < (int) s.code.size())
    {
        const DebuggerViewSnapshot::CodeLine & line = s.code[(size_t) row];
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
                           [this, at] { RunCommand (DebuggerViewState::GetToggleBreakpointLine (*m_snapshot, at)); } });
        items.push_back ({ L"Run to cursor",       [this, at] { RunCommand (DebuggerViewState::GetRunToCursorLine (at)); } });
        items.push_back ({ L"Show next statement", [this]     { ShowCode (std::nullopt); } });
        items.push_back ({ L"Copy",                copy });
    }
    else if (list == m_breakpointList && row >= 0 && row < (int) s.breakpoints.size())
    {
        const DebuggerViewSnapshot::BreakpointLine  bp = s.breakpoints[(size_t) row];

        items.push_back ({ L"Show code",                        [this, bp] { ShowCode (bp.address); } });
        items.push_back ({ bp.enabled ? L"Disable" : L"Enable", [this, bp] { RunCommand (std::format ("{} {}", bp.enabled ? "BPD" : "BPE", bp.id)); } });
        items.push_back ({ L"Remove",                           [this, bp] { RunCommand (std::format ("BPC {}", bp.id)); } });

        //  Its type and the fields that type needs, in a dialog; the result is
        //  the BPEDIT line a person could have typed (FR-094).
        items.push_back ({ L"Edit...", [this, bp]
        {
            std::optional<std::string>  definition = BreakpointDialog::Ask (GetHwnd(), m_theme, bp.info);

            if (definition.has_value())
            {
                RunCommand (std::format ("BPEDIT {} {}", bp.id, *definition));
            }
        } });
    }
    else if (list == m_watchList && row >= 0 && row < (int) s.watches.size())
    {
        const DebuggerViewSnapshot::WatchLine  watch = s.watches[(size_t) row];

        AddShowInMemory (std::format (L"${:04X}", watch.address), std::format ("{:04X}", watch.address), items);
        items.push_back ({ L"Remove",         [this, watch] { RunCommand (std::format ("WC {}", watch.id)); } });
        items.push_back ({ L"Copy",           copy });
    }
    else if (list == m_stackList && row >= 0 && row < (int) s.stack.size())
    {
        Word  at = s.stack[s.stack.size() - 1 - (size_t) row].address;

        AddShowInMemory (std::format (L"${:04X}", at), std::format ("{:04X}", at), items);
        items.push_back ({ L"Copy", copy });
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
    }
    else if (list == m_consoleList)
    {
        items.push_back ({ L"Copy",  copy });
        items.push_back ({ L"Clear", [this] { m_console.clear(); m_consoleList->SetRows ({}); } });
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
            m_snapshot = std::move (snapshot);
            ApplySnapshot();

            //  The drop-downs carry the mode and the panels they were built
            //  with, so they are rebuilt when either changes.
            if (GetMenuState() != m_menuState)
            {
                m_menuState = GetMenuState();
                SetCommandBarMenus();
            }
        }

        AppendConsole (console);
    }

    for (DxuiListView * list : GetLists())
    {
        list->Tick (now);
    }

    //  A drop-down slides open on ticks its host supplies. Without them the
    //  menu stayed at the first frame of its reveal, a sliver under the
    //  entry, and Panels, Dialect and Keys looked as if they did nothing.
    //  The content menus are the same.
    if (m_commandBar->WantsTick())
    {
        m_commandBar->TickMenus (now);
    }

    if (GetPopupHost() != nullptr && GetPopupHost()->GetContextMenu().WantsTick())
    {
        GetPopupHost()->GetContextMenu().Tick (now);
    }

    if (m_tooltip.WantsTick())
    {
        m_tooltip.Tick (now);
    }

    for (MemoryPane * pane : GetOpenMemoryPanes())
    {
        pane->FollowScroll();
        (void) pane->GetView()->TickScrollbars (now);
    }

    m_tracePane->FollowScroll();

    SyncFloats();
    PlaceMemoryBar();
    m_sourcePane->FollowMarkedLine();

    for (const auto & entry : m_floats)
    {
        entry.second->PollCaptionDrag();
        entry.second->Invalidate();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySnapshot
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySnapshot()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    int                                           current = -1;
    std::optional<Word>                           target;



    for (const DebuggerViewSnapshot::CodeLine & line : m_snapshot->code)
    {
        if (line.isCurrent && line.target.has_value())
        {
            target = line.target;
        }
    }

    for (size_t i = 0; i < m_snapshot->code.size(); i++)
    {
        const DebuggerViewSnapshot::CodeLine & line   = m_snapshot->code[i];
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
        else if (m_navigatedTo.has_value() && *m_navigatedTo == line.address)
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
                  { Widen (line.annotation) } };

        cells[6].argb = GetAnnotationArgb();

        for (DxuiListView::Cell & cell : cells)
        {
            cell.background = fill;
        }

        rows.push_back (std::move (cells));
    }

    m_codeList->SetRows (std::move (rows));

    if (current >= 0 && m_codeList->GetSelectedRow() < 0)
    {
        m_codeList->EnsureVisible (current);
    }

    rows.clear();

    //  THE FLAGS ARE THE P REGISTER written so a person can read it, so they
    //  sit on P's row beside the byte they come from, in the same monospace
    //  face -- where a bit changing moves nothing else.
    for (const DebuggerViewSnapshot::RegisterRow & reg : m_snapshot->registers)
    {
        rows.push_back ({ { Widen (reg.name) }, { Widen (reg.value) }, { reg.name == "P" ? L"Flags: " + Widen (m_snapshot->flags) : L"" } });
    }

    m_registerList->SetRows (std::move (rows));

    //  The console reads as a command prompt in the dialect in force, with
    //  that dialect's own help command in its hint.
    m_commandBox->SetPrompt      (GetPromptText (m_snapshot->mode));
    m_commandBox->SetPlaceholder (std::format (L"Command (Enter to run, {} for help)", GetHelpCommand (m_snapshot->mode)));
    m_tracePane->Apply      (m_snapshot->trace);

    rows.clear();

    for (const DebuggerViewSnapshot::BreakpointLine & bp : m_snapshot->breakpoints)
    {
        DxuiListView::Cell  circle;

        circle.icon = GetBreakpointIcon (bp.enabled);

        rows.push_back ({ circle, { Widen (bp.text), !bp.enabled } });
    }

    m_breakpointList->SetRows (std::move (rows));

    rows.clear();

    //  A value that differs from the one the previous snapshot showed for the
    //  same watch is drawn in the changed color (FR-098).
    for (const DebuggerViewSnapshot::WatchLine & watch : m_snapshot->watches)
    {
        DxuiListView::Cell  value = { Widen (watch.value) };
        auto                was   = m_watchValues.find (watch.id);

        if (was != m_watchValues.end() && was->second != watch.value)
        {
            value.argb = GetChangedArgb();
        }

        rows.push_back ({ { std::format (L"#{} ${:04X}", watch.id, watch.address) }, value });
    }

    m_watchValues.clear();

    for (const DebuggerViewSnapshot::WatchLine & watch : m_snapshot->watches)
    {
        m_watchValues[watch.id] = watch.value;
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
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    if (lines.empty())
    {
        return;
    }

    m_console.insert (m_console.end(), lines.begin(), lines.end());

    if ((int) m_console.size() > kConsoleLineLimit)
    {
        m_console.erase (m_console.begin(), m_console.end() - kConsoleLineLimit);
    }

    for (const std::string & line : m_console)
    {
        rows.push_back ({ { Widen (line) } });
    }

    m_consoleList->SetRows (std::move (rows));
    m_consoleList->UpdateAutoFitFromRows();
    m_consoleList->EnsureVisible ((int) m_console.size() - 1);

    //  Output to a console out of sight marks its tab (FR-041).
    if (m_dockSite != nullptr && !m_consoleList->IsVisible())
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
    std::string               line    = TextEncoding::WideToNarrow (m_commandBox->GetText());
    std::optional<DebugVerb>  fileVerb;
    FileDialogSpec            spec;
    std::filesystem::path     chosen;
    bool                      picked  = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    //  The mode comes from the last snapshot; one built before a mode change
    //  can only miss a prompt, and the handler then reports the missing name.
    fileVerb = DebuggerViewState::GetMissingFileVerb (line, m_snapshot != nullptr ? m_snapshot->mode : CommandMode::AppleWin);

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
    Word  address = 0;



    if (TryParseHexWord (m_memoryBox->GetText(), address))
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
//  DebuggerWindow::SubmitPokeBox
//
//  "address byte". A box that does not hold both is left as typed so the
//  mistake can be corrected rather than retyped.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SubmitPokeBox()
{
    std::wstring  text  = m_pokeBox->GetText();
    size_t        space = text.find_first_of (L" \t,=");
    Word          address = 0;
    Word          value   = 0;



    if (space == std::wstring::npos)
    {
        return;
    }

    if (!TryParseHexWord (text.substr (0, space), address) || !TryParseHexWord (text.substr (space + 1), value) || value > 0xFF)
    {
        return;
    }

    if (m_host != nullptr)
    {
        RunCommand (DebuggerViewState::GetPokeLine (address, (Byte) value));
    }

    m_pokeBox->SetText (L"");
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
        RunCommand (std::format ("R P {:02X}", p));
    }
    else if (name == "S" && value.has_value() &&
             CassquePromptDialog::Ask (GetHwnd(), m_theme, L"Stack pointer", L"S, in hex ($00-$FF):", std::format (L"{:02X}", *value), 4, text) &&
             TryParseHexWord (text, typed) && typed <= 0xFF)
    {
        RunCommand (std::format ("R S {:02X}", typed));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UpdateTooltip
//
//  Over the flags on P's row, what each letter is, one to a line.
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
        m_tooltip.RequestShow (cell, FlagsDialog::Describe (*p), now);
        return;
    }

    if (TryGetSymbolTip (clientPx, cell, text))
    {
        m_tooltip.RequestShow (cell, text, now);
        return;
    }

    m_tooltip.RequestHide (now);
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
    RECT           bounds  = m_codeList->GetBounds();
    int            row     = -1;
    int            column  = -1;
    std::string    name;
    Word           address = 0;
    const char   * about   = nullptr;



    if (m_snapshot == nullptr || !IsRoutable (m_codeList) || !m_codeList->IsVisible() || !DxuiDockSite::Contains (bounds, clientPx))
    {
        return false;
    }

    row    = m_codeList->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    column = GetColumnAt (m_codeList, clientPx.x - bounds.left);

    if (row < 0 || row >= (int) m_snapshot->code.size())
    {
        return false;
    }

    const DebuggerViewSnapshot::CodeLine & line = m_snapshot->code[(size_t) row];

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

    if (name.empty() || !m_codeList->GetCellTextRectPx (row, (size_t) column, anchor))
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
//  DebuggerWindow::ShowCode
//
//  Moves the code pane to an address another pane chose, and remembers it so
//  the row is marked as the one brought into view. No address follows the PC
//  again, which marks nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowCode (std::optional<Word> address)
{
    m_navigatedTo = address;

    if (m_host != nullptr)
    {
        m_host->SetDebuggerCodeAddress (address);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ClickGutter
//
//  A press on the first column of the code pane sets or clears the
//  breakpoint on that row; on the breakpoints pane it turns the breakpoint
//  on or off without removing it.
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

    for (DxuiListView * list : { m_codeList, m_breakpointList })
    {
        bounds = list->GetBounds();
        lx     = ev.positionDip.x - bounds.left;
        ly     = ev.positionDip.y - bounds.top;

        if (!IsRoutable (list) || !list->IsVisible() || lx < 0 || ly < 0 || ev.positionDip.x >= bounds.right || ev.positionDip.y >= bounds.bottom)
        {
            continue;
        }

        if (lx + list->GetLeftPx() >= list->GetColumnEffectiveWidthPx (0) + (list == m_codeList ? list->GetColumnEffectiveWidthPx (1) : 0))
        {
            return false;
        }

        row = list->HitTestRow (lx, ly);

        if (list == m_codeList && row >= 0 && row < (int) m_snapshot->code.size())
        {
            RunCommand (DebuggerViewState::GetToggleBreakpointLine (*m_snapshot, m_snapshot->code[(size_t) row].address));
            return true;
        }

        if (list == m_breakpointList && row >= 0 && row < (int) m_snapshot->breakpoints.size())
        {
            const DebuggerViewSnapshot::BreakpointLine & bp = m_snapshot->breakpoints[(size_t) row];

            RunCommand (std::format ("{} {}", bp.enabled ? "BPD" : "BPE", bp.id));
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
    if (!handled && control != nullptr && IsRoutable (control) && control->OnMouse (ev))
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

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move: return m_commandBar->OnToolbarMouseMove   (x, y);
    case DxuiMouseEventKind::Down: return m_commandBar->OnToolbarLButtonDown (x, y);
    case DxuiMouseEventKind::Up:   return m_commandBar->OnToolbarLButtonUp   (x, y);
    default:                       return m_commandBar->IsMenuOpen();
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



    //  Ctrl+wheel sizes the panes' text as Ctrl+Plus and Ctrl+Minus do, over
    //  any pane, before a view that would take the wheel for itself.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal && ev.wheelDelta != 0.0f)
    {
        ApplyTextZoom (ev.wheelDelta > 0.0f ? m_textZoom * 1.1f : m_textZoom / 1.1f);
        return true;
    }

    //  The command bar first: it owns its strip and whatever menu it has
    //  open.
    if (m_routingPane.empty() && RouteCommandBarMouse (ev))
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

    if (RouteMemoryMouse (ev) || RouteSourceMouse (ev))
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
        //  The command bar and the memory bar never leave this window.
        if (m_routingPane.empty())
        {
            UpdateTooltip (ev.positionDip);

            for (DxuiButton * button : GetMemoryButtons())
            {
                button->SetMouse (x, y, button->HitTest (x, y) && lbDown);
            }
        }

        for (DxuiTextInput * box : { m_commandBox, m_memoryBox, m_pokeBox })
        {
            if (IsRoutable (box))
            {
                box->SetMouseHover (x, y);
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

                if (list == m_codeList)
                {
                    NoteViewFocus (false);
                }
            }
        }

        return true;

    case DxuiMouseEventKind::Up:
        for (IDxuiControl * control : GetPressTargets())
        {
            if (IsRoutable (control))
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
        if (IsRoutable (m_codeList) && m_codeList->IsVisible() && DxuiDockSite::Contains (m_codeList->GetBounds(), ev.positionDip) && m_host != nullptr)
        {
            m_host->ScrollDebuggerCode ((int) std::lround (-ev.wheelDelta * (float) m_codeList->GetWheelLinesPerNotch()));
            return true;
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
    if (pane == DebuggerLayout::kCode)        { return { m_codeList };                   }
    if (pane == DebuggerLayout::kSource)      { return { m_sourceBanner, m_sourceView }; }
    if (pane == DebuggerLayout::kConsole)     { return { m_consoleList, m_commandBox };  }
    if (pane == DebuggerLayout::kRegisters)   { return { m_registerList };               }
    if (pane == DebuggerLayout::kBreakpoints) { return { m_breakpointList };             }
    if (pane == DebuggerLayout::kWatches)     { return { m_watchList };                  }
    if (pane == DebuggerLayout::kStack)       { return { m_stackList };                  }
    if (pane == DebuggerLayout::kCallStack)   { return { m_callStackButton, m_callStackList }; }
    if (pane == DebuggerLayout::kTrace)       { return { m_traceList };                  }

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



    if (pane == DebuggerLayout::kSource)
    {
        return m_sourceFrame.get();
    }

    if (pane == DebuggerLayout::kConsole)
    {
        return m_consoleFrame.get();
    }

    if (pane == DebuggerLayout::kCallStack)
    {
        return m_callStackFrame.get();
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
    if (pane == DebuggerLayout::kSource)      { return L"Source";      }
    if (pane == DebuggerLayout::kConsole)     { return L"Console";     }
    if (pane == DebuggerLayout::kRegisters)   { return L"Registers";   }
    if (pane == DebuggerLayout::kBreakpoints) { return L"Breakpoints"; }
    if (pane == DebuggerLayout::kWatches)     { return L"Watches";     }
    if (pane == DebuggerLayout::kStack)       { return L"Stack";       }
    if (pane == DebuggerLayout::kCallStack)   { return L"Call Stack";  }
    if (pane == DebuggerLayout::kTrace)       { return L"Trace";       }

    if (DiagnosticsPane * diagnostics = GetDiagnosticsPane (pane))
    {
        return diagnostics->GetTitle();
    }

    return pane.starts_with (L"memory") ? L"Memory " + pane.substr (6) : pane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneOfControl
//
//  Empty for a control that belongs to no pane: the toolbar and the memory
//  bar, which never leave this window.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneOfControl (const IDxuiControl * control) const
{
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
    params.captionStyle     = DxuiCaptionStyle::CloseOnly;
    params.createNoActivate = true;

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

    window->SetOnContentMouse   ([this, pane] (const DxuiMouseEvent & ev) { return RouteFloatMouse (pane, ev); });
    window->SetOnContentKey     ([this, pane] (const DxuiKeyEvent & ev)   { return RouteFloatKey   (pane, ev); });
    window->SetOnMappedCommand  ([this]       (int commandId)             { return OnMappedCommand (commandId); });
    window->SetOnCaptionDrag    ([this, pane] (POINT screen)              { OnFloatDrag (pane, screen, false); });
    window->SetOnCaptionDragEnd ([this, pane] (POINT screen)              { OnFloatDrag (pane, screen, true);  });

    //  Closing a floating pane docks it where it came from; the debugger has
    //  no way yet to reopen a pane that was closed.
    window->SetOnClosed ([this, pane]
    {
        if (m_dockSite->EditPaneLayout().DockBack (pane))
        {
            m_syncFloats = true;
            SaveLayout();
        }
    });

    if (rect.right > rect.left && rect.bottom > rect.top)
    {
        window->SetScreenRect (rect);
    }

    m_floats[pane] = std::move (window);
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

    for (IDxuiControl * control : GetPaneControls (pane))
    {
        std::unique_ptr<IDxuiControl>  owned = window->DetachChild (control);

        if (owned != nullptr)
        {
            (void) AttachChild (std::move (owned));
        }
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
        m_host->SetDebuggerLayout (SourcePathList::WideToUtf8 (layout.ToText()));
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
        Invalidate();
        return;
    }

    if (!m_dockSite->IsDragging())
    {
        return;
    }

    //  Released outside the site, the pane stays floating where it was put;
    //  released over a zone it docks there, which the site reports.
    (void) m_dockSite->EndDrag (inside ? client : POINT { area.right + 1, area.bottom + 1 });

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
    std::wstring  pane   = GetPaneOfFocus();
    RECT          bounds = {};
    DxuiDockSide  side   = DxuiDockSide::Left;



    if (pane.empty())
    {
        return false;
    }

    if (ev.vk == VK_APPS || (ev.vk == VK_F10 && ev.shift && !ev.ctrl && !ev.alt))
    {
        bounds = m_focusMgr.GetFocusedControl()->GetBounds();
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
        if      (focused == m_commandBox) { SubmitCommandBox(); return true; }
        else if (focused == m_memoryBox)  { SubmitMemoryBox();  return true; }
        else if (focused == m_pokeBox)    { SubmitPokeBox();    return true; }
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

    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return m_commandBox->OnKey (ev) || m_memoryBox->OnKey (ev) || m_pokeBox->OnKey (ev);
    }

    //  Ctrl+Plus, Ctrl+Minus and Ctrl+0 size the panes' text (FR-083).
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt)
    {
        switch (ev.vk)
        {
        case VK_OEM_PLUS:  case VK_ADD:       ApplyTextZoom (m_textZoom * 1.1f); return true;
        case VK_OEM_MINUS: case VK_SUBTRACT:  ApplyTextZoom (m_textZoom / 1.1f); return true;
        case '0':          case VK_NUMPAD0:   ApplyTextZoom (1.0f);              return true;
        default:                                                                 break;
        }
    }

    //  Ctrl+Z in a memory window undoes that window's last edit.
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && ev.vk == 'Z' && GetFocusedMemoryPane() != nullptr)
    {
        (void) GetFocusedMemoryPane()->Undo();
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = (focused != nullptr) && focused->OnKey (ev);

        if (!handled && ev.vk == VK_TAB && m_routingPane.empty())
        {
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
