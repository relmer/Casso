#include "Pch.h"

#include "Core/DxuiStandardCommand.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureMenuBar
//
//  The window's menu bar, under the caption as Visual Studio's is. Its rows
//  are built by SetWindowMenus, which the command bar's setup calls.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureMenuBar()
{
    m_menuBar->SetPopupHost              (GetPopupHost());
    m_menuBar->SetTextRendererForMeasure (GetTextRenderer());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MakeKeyedMenuCommand
//
//  A menu row for a command the key schemes and the command bar share: it
//  shows the key the scheme in force gives it, and runs and dims as the
//  command bar's entry does.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> DebuggerWindow::MakeKeyedMenuCommand (int id, const std::wstring & label)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->id          = id;
    command->label       = label;
    command->accelerator = DebuggerKeySchemes::GetMap (m_keyScheme).GetChordText (id);
    command->dispatch    = [this, id] { RunCommandBarEntry (id); };
    command->isEnabled   = [this, id] { return IsCommandBarEntryEnabled (id); };

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MakeEditMenuCommand
//
//  A row of the Edit menu, acting on the control with the focus as its own
//  context menu does, and dimmed when that control has nothing to act on.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> DebuggerWindow::MakeEditMenuCommand (DxuiStandardCommand command, const std::wstring & label, const std::wstring & accelerator)
{
    std::shared_ptr<DxuiCommand>  row = std::make_shared<DxuiCommand>();



    row->label       = label;
    row->accelerator = accelerator;

    row->dispatch = [this, command]
    {
        IDxuiControl  * focused = GetFocused();



        if (focused != nullptr)
        {
            (void) DxuiCommandRouter::Invoke (focused, command);
            Invalidate();
        }
    };

    row->isEnabled = [this, command]
    {
        IDxuiControl  * focused = GetFocused();
        bool            enabled = false;



        return focused != nullptr && DxuiCommandRouter::Query (focused, command, enabled) && enabled;
    };

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetViewMenuGroup
//
//  The cascade of the View menu a window's row goes in: the Disassembly
//  views, the memory windows and the device panels each share one. Empty
//  for a window that is a row of View itself.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetViewMenuGroup (const std::wstring & pane)
{
    std::string  diagnosticsId;



    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            return L"Disassembly";
        }
    }

    for (int window = 1; window <= DebuggerViewState::kMaxMemoryWindows; window++)
    {
        if (pane == DebuggerLayout::GetMemoryPaneId (window))
        {
            return L"Memory";
        }
    }

    if (DebuggerLayout::TryGetDiagnosticsId (pane, diagnosticsId))
    {
        return L"Device panels";
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetWindowMenus
//
//  File, Edit, View, Debug, Window and Tools. View lists every debug window,
//  Window the machine's device panels and the layout reset, and Tools the
//  key schemes. Rebuilt whenever what they list changes, since the rows
//  carry the state they were built with; a menu that is open keeps its rows
//  until it closes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetWindowMenus()
{
    std::vector<DxuiPopupMenuItem>               file;
    std::vector<DxuiPopupMenuItem>               edit;
    std::vector<DxuiPopupMenuItem>               view;
    std::vector<DxuiPopupMenuItem>               debug;
    std::vector<DxuiPopupMenuItem>               window;
    std::vector<DxuiPopupMenuItem>               tools;
    std::vector<DxuiPopupMenuItem>               panels;
    std::vector<DxuiPopupMenuItem>               schemes;
    std::vector<DxuiPopupMenuItem>               themes;
    std::shared_ptr<DxuiCommand>                 row;
    std::vector<std::wstring>                    cascades;
    std::vector<std::vector<DxuiPopupMenuItem>>  cascadeRows;
    std::vector<size_t>                          cascadeAt;
    auto  add = [this] (std::vector<DxuiPopupMenuItem> & menu, std::shared_ptr<DxuiCommand> command)
    {
        m_menuCommands.push_back (command);
        menu.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
    };
    auto  emulator = [this] (const std::wstring & label, int commandId)
    {
        return MakeMenuCommand (label, false, [this, commandId]
        {
            if (m_host != nullptr)
            {
                m_host->RunEmulatorCommand (commandId);
            }
        });
    };



    m_menuCommands.clear();

    //  File: what the window reads and writes.
    row            = MakeMenuCommand (L"Open source file...", false, [this] { OpenSourceFile(); });
    row->isEnabled = [this] { return m_snapshot != nullptr && m_snapshot->source.has_value(); };
    add (file, row);
    add (file, MakeMenuCommand (L"Open symbol or debug file...", false, [this] { OpenSymbolFile(); }));
    file.push_back (DxuiPopupMenuItem::ForSeparator());
    add (file, MakeMenuCommand (L"Load breakpoints...", false, [this] { ImportBreakpoints(); }));
    row            = MakeMenuCommand (L"Save breakpoints...", false, [this] { ExportBreakpoints(); });
    row->isEnabled = [this] { return IsBreakpointBarEnabled (BreakpointBarCommands::kExport); };
    add (file, row);
    add (file, MakeMenuCommand (L"Save trace...", false, [this] { SaveTrace(); }));
    file.push_back (DxuiPopupMenuItem::ForSeparator());
    add (file, MakeMenuCommand (L"Close", false, [this] { PostMessageW (GetHwnd(), WM_CLOSE, 0, 0); }));

    //  Edit: the focused control's own commands, then find in the console.
    add (edit, MakeEditMenuCommand (DxuiStandardCommand::Copy,      L"Copy",       L"Ctrl+C"));
    add (edit, MakeEditMenuCommand (DxuiStandardCommand::SelectAll, L"Select all", L"Ctrl+A"));
    edit.push_back (DxuiPopupMenuItem::ForSeparator());
    add (edit, MakeKeyedMenuCommand ((int) DebuggerKeySchemes::Action::Find,         L"Find"));
    add (edit, MakeKeyedMenuCommand ((int) DebuggerKeySchemes::Action::FindNext,     L"Find next"));
    add (edit, MakeKeyedMenuCommand ((int) DebuggerKeySchemes::Action::FindPrevious, L"Find previous"));

    //  View: every debug window, the shown ones checked. The windows that
    //  come in several instances, and the device panels, fold into a cascade
    //  each, placed where the first of them would have been.
    for (const std::wstring & pane : GetViewMenuPanes())
    {
        std::wstring  group = GetViewMenuGroup (pane);
        std::wstring  title = GetPaneTitle (pane);
        size_t        index = 0;

        //  Among its siblings the first view carries its number too.
        if (pane == DebuggerLayout::kCode)
        {
            title += L" 1";
        }

        row            = MakeMenuCommand (title, false, [this, pane] { ShowPane (pane); });
        row->isChecked = [this, pane] { return IsPaneShown (pane); };

        if (group.empty())
        {
            add (view, row);
            continue;
        }

        index = (size_t) (std::find (cascades.begin(), cascades.end(), group) - cascades.begin());

        if (index == cascades.size())
        {
            cascades.push_back    (group);
            cascadeRows.push_back ({});
            cascadeAt.push_back   (view.size());
            view.push_back        (DxuiPopupMenuItem::ForSeparator());
        }

        add (cascadeRows[index], row);
    }

    for (size_t i = 0; i < cascades.size(); i++)
    {
        row = MakeMenuCommand (cascades[i], false, [] {});
        m_menuCommands.push_back (row);
        view[cascadeAt[i]] = DxuiPopupMenuItem::ForSubmenu (row, std::move (cascadeRows[i]));
    }

    //  Debug: running and stopping, the machine's own restarts, then the
    //  steps and how they step.
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kRun,   L"Run"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kPause, L"Break"));
    add (debug, MakeMenuCommand (L"Stop debugging", false, [this] { PostMessageW (GetHwnd(), WM_CLOSE, 0, 0); }));
    debug.push_back (DxuiPopupMenuItem::ForSeparator());
    add (debug, emulator (L"Reset",                  IDM_MACHINE_RESET));
    add (debug, emulator (L"Power cycle",            IDM_MACHINE_POWERCYCLE));
    add (debug, emulator (L"Restart under debugger", IDM_DEBUG_RESTART));
    debug.push_back (DxuiPopupMenuItem::ForSeparator());
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepInto,    L"Step into"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepOver,    L"Step over"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepOut,     L"Step out"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kRunToCursor, L"Run to cursor"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kShowNext,    L"Show next statement"));
    debug.push_back (DxuiPopupMenuItem::ForSeparator());

    row = MakeMenuCommand (L"Step by source line", false, [this]
    {
        if (m_host != nullptr && m_snapshot != nullptr && m_snapshot->source.has_value())
        {
            m_host->RunDebuggerCommand (DebuggerViewState::GetSourceStepLine (!m_snapshot->source->stepBySource, m_snapshot->mode));
        }
    });
    row->isEnabled = [this] { return m_snapshot != nullptr && m_snapshot->source.has_value(); };
    row->isChecked = [this] { return m_snapshot != nullptr && m_snapshot->source.has_value() && m_snapshot->source->stepBySource; };
    add (debug, row);

    row            = MakeKeyedMenuCommand (DebuggerCommands::kTrace, L"Trace");
    row->isChecked = [this] { return m_snapshot != nullptr && m_snapshot->trace.isOn; };
    add (debug, row);

    //  Window: the machine's device panels, a cascade of their own, and the
    //  arrangement put back as it first was.
    if (m_snapshot != nullptr)
    {
        for (const DebuggerViewSnapshot::PanelInfo & panel : m_snapshot->panels)
        {
            std::string  id   = panel.id;
            bool         open = panel.open;

            add (panels, MakeMenuCommand (TextEncoding::NarrowToWide (panel.title), open, [this, id, open]
            {
                RunAction (DebuggerActions::GetPanel (id, !open, GetMode()));
            }));
        }
    }

    row            = MakeMenuCommand (L"Device panels", false, [] {});
    row->isEnabled = [hasPanels = !panels.empty()] { return hasPanels; };
    m_menuCommands.push_back (row);
    window.push_back (DxuiPopupMenuItem::ForSubmenu (row, std::move (panels)));
    window.push_back (DxuiPopupMenuItem::ForSeparator());
    add (window, MakeMenuCommand (L"Reset window layout", false, [this] { ResetPaneLayout(); }));

    //  Tools: which editor's keys the window takes.
    for (DebuggerKeyScheme scheme : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
    {
        //  Applying the scheme rebuilds these rows, this one among them, so
        //  it is the last thing the row does.
        add (schemes, MakeMenuCommand (DebuggerKeySchemes::GetMap (scheme).GetName(), scheme == m_keyScheme, [this, scheme]
        {
            if (m_host != nullptr)
            {
                m_host->SetDebuggerKeyScheme (DebuggerKeySchemes::GetName (scheme));
            }

            ApplyKeyScheme (scheme);
        }));
    }

    row = MakeMenuCommand (L"Keyboard scheme", false, [] {});
    m_menuCommands.push_back (row);
    tools.push_back (DxuiPopupMenuItem::ForSubmenu (row, std::move (schemes)));

    //  Tools: the window's own colors, or Casso's.
    for (const DebuggerThemes::Choice & choice : DebuggerThemes::GetChoices())
    {
        std::string  name    = choice.name;
        bool         current = DebuggerThemes::IsKnown (m_themeName) ? name == m_themeName : name.empty();

        //  Applying the theme rebuilds these rows, so it is the last thing
        //  the row does. A choice ends the preview without undoing it.
        row = MakeMenuCommand (choice.label, current, [this, name]
        {
            m_themeBeforePreview.reset();

            if (m_host != nullptr)
            {
                m_host->SetDebuggerTheme (name);
            }

            ApplyTheme (name);
        });

        row->preview = [this, name] { PreviewTheme (name); };
        add (themes, row);
    }

    row = MakeMenuCommand (L"Theme", false, [] {});
    m_menuCommands.push_back (row);
    tools.push_back (DxuiPopupMenuItem::ForSubmenu (row, std::move (themes)));

    m_menuBarItems =
    {
        { L"&File",   0, std::move (file)   },
        { L"&Edit",   0, std::move (edit)   },
        { L"&View",   0, std::move (view)   },
        { L"&Debug",  0, std::move (debug)  },
        { L"&Window", 0, std::move (window) },
        { L"&Tools",  0, std::move (tools)  },
    };

    if (m_menuBar != nullptr && !m_menuBar->IsOpen())
    {
        m_menuBar->SetItems (m_menuBarItems);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PreviewTheme
//
//  The theme under the Theme menu's highlight, in force until the menu
//  closes. The first preview of an opening keeps the theme to go back to.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PreviewTheme (const std::string & name)
{
    if (!m_themeBeforePreview.has_value())
    {
        m_themeBeforePreview = m_themeName;
    }

    if (name != m_themeName)
    {
        ApplyTheme (name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EndThemePreview
//
//  Once the menu bar has closed, a preview no row was chosen for gives way
//  to the theme that was in force when the menu opened.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EndThemePreview()
{
    std::string  restore;



    if (!m_themeBeforePreview.has_value() || (m_menuBar != nullptr && m_menuBar->IsOpen()))
    {
        return;
    }

    restore = *m_themeBeforePreview;
    m_themeBeforePreview.reset();

    if (restore != m_themeName)
    {
        ApplyTheme (restore);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteMenuBarMouse
//
//  The strip takes the pointer over itself, and the bar takes it everywhere
//  while one of its menus is open. A press builds the rows afresh before a
//  menu opens, so they show what is true now.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteMenuBarMouse (const DxuiMouseEvent & ev)
{
    RECT  strip   = m_menuBar->GetBounds();
    bool  over    = m_menuBar->IsVisible() && DxuiDockSite::Contains (strip, ev.positionDip);
    bool  open    = m_menuBar->IsOpen();
    bool  handled = false;



    if (!over && !open)
    {
        m_menuBar->ClearHover();
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && !open)
    {
        GetRoutedTooltip().HideImmediate();
        SetWindowMenus();
    }

    handled = m_menuBar->OnMouse (ev);
    EndThemePreview();
    Invalidate();

    return handled || over;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteMenuBarKey
//
//  While a menu is open the bar takes every key; otherwise Alt with a letter
//  opens the menu that letter marks, with its rows built afresh.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteMenuBarKey (const DxuiKeyEvent & ev, bool & handled)
{
    bool  owns = m_menuBar->IsOpen() || m_menuBar->HasFocus();



    if (owns)
    {
        handled = (ev.kind != DxuiKeyEventKind::Down) || m_menuBar->OnKey (ev);
        EndThemePreview();
        Invalidate();
        return true;
    }

    if (ev.kind != DxuiKeyEventKind::Down || !ev.alt || ev.ctrl || ev.vk < 'A' || ev.vk > 'Z')
    {
        return false;
    }

    SetWindowMenus();

    handled = m_menuBar->HandleAltKey ((wchar_t) ev.vk);

    if (handled)
    {
        Invalidate();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ResetPaneLayout
//
//  Puts every pane back where the window first had it, and saves that.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ResetPaneLayout()
{
    DxuiPaneLayout  layout = DebuggerLayout::MakeDefault();



    layout.PlaceOnMonitors (GetMonitors());
    m_dockSite->SetPaneLayout (layout);
    m_syncFloats = true;
    m_dockSite->Relayout();
    SaveLayout();
    SetWindowMenus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenSourceFile
//
//  A source file the user picks, matched against the loaded debug file's
//  records as a dropped file is.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenSourceFile()
{
    HRESULT                hr     = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters = { { L"Source files", L"*.s;*.asm;*.a65;*.inc" }, { L"All files", L"*.*" } };

    hr = m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    (void) OnFilesDropped ({ chosen.wstring() });

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenSymbolFile
//
//  SYM LOAD of a debug or symbol file the user picks, in AppleWin's words
//  whatever the console's dialect.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenSymbolFile()
{
    HRESULT                hr     = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters = { { L"Debug and symbol files", L"*.dbg;*.sym" }, { L"All files", L"*.*" } };

    hr = m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    m_host->RunDebuggerCommandInMode (std::format ("SYM LOAD \"{}\"", TextEncoding::WideToNarrow (chosen.wstring())), CommandMode::AppleWin);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureConsoleBar
//
//  The console pane's toolbar: the dialect the console reads, which the
//  memory Address box follows too.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureConsoleBar()
{
    constexpr const wchar_t *  kGlyphDialect = s_kpszMdl2Code;   // braces
    DxuiToolbar::Entry         dialect;



    m_dialectCommand        = std::make_shared<DxuiCommand>();
    m_dialectCommand->id    = kDialectEntry;
    m_dialectCommand->label = L"Dialect";
    m_dialectCommand->glyph = kGlyphDialect;
    m_dialectCommand->tip   = L"Dialect\nChoose the command dialect for the console and the memory Address box";

    dialect.command = m_dialectCommand;
    dialect.kind    = DxuiToolbar::Kind::DropDown;

    m_consoleBar->SetTextRenderer (GetTextRenderer());
    m_consoleBar->SetPopupHost    (GetPopupHost());
    m_consoleBar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
    m_consoleBar->SetCompact      (true);
    m_consoleBar->SetEntries      ({ dialect, MakeFindEntry (DebuggerLayout::kConsole) });
    m_consoleBar->SetVisible      (false);

    SetConsoleBarMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetConsoleBarMenus
//
//  The dialects, the one in force checked. Rebuilt before the drop-down
//  opens and when the mode changes, since the rows carry the check they
//  were built with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetConsoleBarMenus()
{
    std::vector<DxuiPopupMenuItem>  modes;



    for (const auto & [mode, label] : { std::pair { CommandMode::AppleWin,  L"AppleWin"  },
                                        std::pair { CommandMode::Monitor,   L"Monitor"   },
                                        std::pair { CommandMode::GSSquared, L"GSSquared" },
                                        std::pair { CommandMode::WinDbg,    L"WinDbg"    },
                                        std::pair { CommandMode::Casso,     L"Casso"     } })
    {
        CommandMode  target  = mode;
        bool         current = (m_snapshot != nullptr) && m_snapshot->mode == mode;

        modes.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (label, current, [this, target]
        {
            RunAction (DebuggerActions::GetSetMode (target, GetMode()));
        })));
    }

    m_consoleBar->SetDropDownItems (kDialectEntry, std::move (modes));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceConsoleBar
//
//  The bar sits in the place held at the top of the console pane and is one
//  of the pane's controls, so it goes with the pane into a floating window
//  and draws, measures and opens its menu there.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceConsoleBar()
{
    bool          shown = m_consoleBarSlot != nullptr && m_consoleBarSlot->IsVisible();
    DxuiWindow  * host  = GetPaneHost (DebuggerLayout::kConsole);
    RECT          slot  = {};



    m_consoleBar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    slot = m_consoleBarSlot->GetBounds();

    m_consoleBar->SetTextRenderer   (host->GetTextRenderer());
    m_consoleBar->SetPopupHost      (host->GetPopupHost());
    m_consoleBar->SetHostClientRect (host->GetBounds());
    m_consoleBar->Layout            (slot, m_scaler);

    host->SetChildClip (m_consoleBar, slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteConsoleBarMouse
//
//  As the breakpoints pane's: the strip and whatever menu it has open take
//  the left button; the right button over the strip opens the pane's own
//  menu.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteConsoleBarMouse (const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    RECT  strip = m_consoleBar->GetBounds();
    bool  open  = m_consoleBar->IsMenuOpen();
    bool  over  = m_consoleBar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y });



    if (!over && !open)
    {
        m_consoleBar->OnToolbarMouseLeave();
        return false;
    }

    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        if (m_routingPane.empty())
        {
            UpdateTooltip (ev.positionDip);
        }

        return m_consoleBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open)
        {
            SetConsoleBarMenus();
        }

        return m_consoleBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_consoleBar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MakeFindEntry
//
//  The search button on a pane's toolbar, which opens that pane's find bar
//  as Ctrl+F does.
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar::Entry DebuggerWindow::MakeFindEntry (const std::wstring & pane)
{
    auto                command = std::make_shared<DxuiCommand>();
    DxuiToolbar::Entry  entry;



    command->id       = kFindEntry;
    command->label    = L"Find";
    command->glyph    = s_kpszMdl2Search;
    command->tip      = L"Find (Ctrl+F)\nFind text in this pane";
    command->dispatch = [this, pane] { OpenFindIn (pane); };

    entry.command = command;
    entry.kind    = DxuiToolbar::Kind::Command;

    return entry;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureSourceBars
//
//  Each source document's toolbar, which holds its search button.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureSourceBars()
{
    for (int slot = 0; slot < (int) m_sourceDocs.size(); slot++)
    {
        DxuiToolbar  * bar = m_sourceDocs[(size_t) slot].bar;

        bar->SetTextRenderer (GetTextRenderer());
        bar->SetPopupHost    (GetPopupHost());
        bar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
        bar->SetCompact      (true);
        bar->SetEntries      ({ MakeFindEntry (DebuggerLayout::GetSourcePaneId (slot)) });
        bar->SetVisible      (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceSourceBars
//
//  As the console's: each bar sits in the place held at the top of its
//  document and goes with the document into a floating window.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceSourceBars()
{
    for (int slot = 0; slot < (int) m_sourceDocs.size(); slot++)
    {
        SourceDocument  & document = m_sourceDocs[(size_t) slot];
        bool              shown    = document.barSlot != nullptr && document.barSlot->IsVisible();
        DxuiWindow      * host     = nullptr;
        RECT              place    = {};

        document.bar->SetVisible (shown);

        if (!shown)
        {
            continue;
        }

        host  = GetPaneHost (DebuggerLayout::GetSourcePaneId (slot));
        place = document.barSlot->GetBounds();

        document.bar->SetTextRenderer   (host->GetTextRenderer());
        document.bar->SetPopupHost      (host->GetPopupHost());
        document.bar->SetHostClientRect (host->GetBounds());
        document.bar->Layout            (place, m_scaler);

        host->SetChildClip (document.bar, place);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteSourceBarMouse
//
//  As the console's: the strip takes the left button, and the right button
//  over it opens the pane's own menu.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteSourceBarMouse (DxuiToolbar * bar, const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    RECT  strip = bar->GetBounds();
    bool  open  = bar->IsMenuOpen();
    bool  over  = bar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y });



    if (!over && !open)
    {
        bar->OnToolbarMouseLeave();
        return false;
    }

    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        if (m_routingPane.empty())
        {
            UpdateTooltip (ev.positionDip);
        }

        return bar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        return bar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return bar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





