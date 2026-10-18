#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "resource.h"
#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/TracePaneController.h"
#include "Ui/Debugger/ViewMenuRows.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::Detach
//
//  Closes the debugger and leaves the machine running untouched: the host
//  takes the debugger's CPU hook off and resumes a stopped machine, and
//  keeps the breakpoints for when the debugger is opened again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::Detach()
{
    HWND  hwnd = GetHwnd();



    if (m_host != nullptr)
    {
        m_host->DetachDebugger();
    }

    if (hwnd != nullptr)
    {
        PostMessageW (hwnd, WM_CLOSE, 0, 0);
    }
}





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
//  DebuggerWindow::SetWindowMenus
//
//  File, Edit, Machine, View, Debug and Tools. View has a row for every
//  debug window, the machine's device panels and the layout reset, and
//  Tools the key schemes. What a pane's colors mean is behind the info button on the
//  pane's toolbar, so there is no Help menu. Rebuilt whenever what they list
//  changes, since the rows carry the state they were built with; a menu that
//  is open keeps its rows until it closes.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetWindowMenus()
{
    std::vector<DxuiPopupMenuItem>               file;
    std::vector<DxuiPopupMenuItem>               edit;
    std::vector<DxuiPopupMenuItem>               machine;
    std::vector<DxuiPopupMenuItem>               view;
    std::vector<DxuiPopupMenuItem>               debug;
    std::vector<DxuiPopupMenuItem>               tools;
    std::vector<DxuiPopupMenuItem>               schemes;
    std::vector<DxuiPopupMenuItem>               themes;
    std::shared_ptr<DxuiCommand>                 row;
    auto  add = [this] (std::vector<DxuiPopupMenuItem> & menu, std::shared_ptr<DxuiCommand> command)
    {
        m_menuCommands.push_back (command);
        menu.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
    };
    auto  emulator = [this] (const std::wstring & label, int commandId)
    {
        return DebuggerMenuCommand::Make (label, false, [this, commandId]
        {
            if (m_host != nullptr)
            {
                m_host->RunEmulatorCommand (commandId);
            }
        });
    };



    m_menuCommands.clear();

    //  File: what the window reads and writes.
    add (file, DebuggerMenuCommand::Make (L"Open source file...", false, [this] { OpenSourceFile(); }));
    add (file, DebuggerMenuCommand::Make (L"Open symbol or debug file...", false, [this] { OpenSymbolFile(); }));
    file.push_back (DxuiPopupMenuItem::ForSeparator());
    add (file, DebuggerMenuCommand::Make (L"Load breakpoints...", false, [this] { m_breakpointsPane->Import(); }));
    row            = DebuggerMenuCommand::Make (L"Save breakpoints...", false, [this] { m_breakpointsPane->Export(); });
    row->isEnabled = [this] { return IsBreakpointBarEnabled (BreakpointBarCommands::kExport); };
    add (file, row);
    add (file, DebuggerMenuCommand::Make (L"Save trace...", false, [this] { m_tracePane->Save(); }));
    file.push_back (DxuiPopupMenuItem::ForSeparator());
    row            = DebuggerMenuCommand::Make (L"Load file into memory...", false, [this] { LoadFileIntoMemory(); });
    row->isEnabled = [this] { return IsCommandBarEntryEnabled (DebuggerCommands::kLoadFile); };
    add (file, row);
    add (file, DebuggerMenuCommand::Make (L"Save memory to file...", false, [this] { SaveMemoryToFile(); }));
    file.push_back (DxuiPopupMenuItem::ForSeparator());
    add (file, DebuggerMenuCommand::Make (L"Open project...", false, [this] { OpenProject(); }));
    add (file, DebuggerMenuCommand::Make (L"Save project as...", false, [this] { SaveProjectAs(); }));
    add (file, DebuggerMenuCommand::Make (L"Project settings...", false, [this] { EditProjectSettings(); }));
    file.push_back (DxuiPopupMenuItem::ForSeparator());
    add (file, DebuggerMenuCommand::Make (L"Close", false, [this] { PostMessageW (GetHwnd(), WM_CLOSE, 0, 0); }));

    //  Edit: the focused control's own commands, then find in the console.
    add (edit, MakeEditMenuCommand (DxuiStandardCommand::Copy,      L"Copy",       L"Ctrl+C"));
    add (edit, MakeEditMenuCommand (DxuiStandardCommand::SelectAll, L"Select all", L"Ctrl+A"));
    edit.push_back (DxuiPopupMenuItem::ForSeparator());
    add (edit, MakeKeyedMenuCommand ((int) DebuggerKeySchemes::Action::Find,         L"Find"));
    add (edit, MakeKeyedMenuCommand ((int) DebuggerKeySchemes::Action::FindNext,     L"Find next"));
    add (edit, MakeKeyedMenuCommand ((int) DebuggerKeySchemes::Action::FindPrevious, L"Find previous"));

    //  View: every debug window, the shown ones checked.
    view = ViewMenuRows::Build (m_panes, m_snapshot.get(),
                                [this] (const std::wstring & pane)
                                {
                                    std::optional<DebuggerKeySchemes::Action>  action = DebuggerKeySchemes::GetActionOfPane (pane);

                                    return action.has_value() ? DebuggerKeySchemes::GetMap (m_keyScheme).GetChordText ((int) *action) : std::wstring();
                                },
                                [this] (const std::wstring & pane) { ShowPane (pane); },
                                [this] (const std::wstring & pane) { return IsPaneShown (pane); },
                                m_menuCommands);

    //  View ends with the arrangement put back as it first was.
    view.push_back (DxuiPopupMenuItem::ForSeparator());
    add (view, DebuggerMenuCommand::Make (L"Reset window layout", false, [this] { ResetPaneLayout(); }));

    //  Debug: running and stopping, the steps and how they step. The
    //  machine's own restarts are in Machine.
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kRun,   L"Run"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kPause, L"Break"));
    add (debug, DebuggerMenuCommand::Make (L"Detach", false, [this] { Detach(); }));
    debug.push_back (DxuiPopupMenuItem::ForSeparator());
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepInto,    L"Step into"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepOver,    L"Step over"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepOut,     L"Step out"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kRunToCursor, L"Run to cursor"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kRunFrame,    L"Run one frame"));

    row = DebuggerMenuCommand::Make (L"Show beam on screen", false, [this]
    {
        if (m_host != nullptr)
        {
            m_host->SetBeamOverlayOn (!m_host->IsBeamOverlayOn());
        }
    });
    row->isChecked = [this] { return m_host != nullptr && m_host->IsBeamOverlayOn(); };
    add (debug, row);

    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kShowNext,    L"Show next statement"));
    debug.push_back (DxuiPopupMenuItem::ForSeparator());
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepBackInto,    L"Step back into"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepBackOver,    L"Step back over"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kStepBackOut,     L"Step back out"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kReverseContinue, L"Reverse continue"));
    add (debug, MakeKeyedMenuCommand (DebuggerCommands::kGoLive,          L"Go live"));
    debug.push_back (DxuiPopupMenuItem::ForSeparator());

    row = DebuggerMenuCommand::Make (L"Step by source line", false, [this]
    {
        if (m_host != nullptr && m_snapshot != nullptr && m_snapshot->source.has_value())
        {
            RunAction (DebuggerActions::GetSourceStepping (!m_snapshot->source->stepBySource, m_snapshot->mode));
        }
    });
    row->isEnabled = [this] { return m_snapshot != nullptr && m_snapshot->source.has_value(); };
    row->isChecked = [this] { return m_snapshot != nullptr && m_snapshot->source.has_value() && m_snapshot->source->stepBySource; };
    add (debug, row);

    row            = MakeKeyedMenuCommand (DebuggerCommands::kTrace, L"Trace");
    row->isChecked = [this] { return m_snapshot != nullptr && m_snapshot->trace.isOn; };
    add (debug, row);
    add (debug, DebuggerMenuCommand::Make (L"Step filters...", false, [this] { EditStepFilters(); }));

    //  Machine: the machine's own restarts. The main window's Restart under
    //  debugger is not here: with the debugger open, it is Power cycle.
    add (machine, emulator (L"Reset",       IDM_MACHINE_RESET));
    add (machine, emulator (L"Power cycle", IDM_MACHINE_POWERCYCLE));

    //  Tools: which editor's keys the window takes.
    for (DebuggerKeyScheme scheme : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
    {
        //  Applying the scheme rebuilds these rows, this one among them, so
        //  it is the last thing the row does.
        add (schemes, DebuggerMenuCommand::Make (DebuggerKeySchemes::GetMap (scheme).GetName(), scheme == m_keyScheme, [this, scheme]
        {
            if (m_host != nullptr)
            {
                m_host->SetDebuggerKeyScheme (DebuggerKeySchemes::GetName (scheme));
            }

            ApplyKeyScheme (scheme);
        }));
    }

    row = DebuggerMenuCommand::Make (L"Keyboard scheme", false, [] {});
    m_menuCommands.push_back (row);
    tools.push_back (DxuiPopupMenuItem::ForSubmenu (row, std::move (schemes)));

    //  Tools: the window's own colors, or Casso's.
    for (const DebuggerThemes::Choice & choice : DebuggerThemes::GetChoices())
    {
        std::string  name    = choice.name;
        bool         current = DebuggerThemes::IsKnown (m_themeName) ? name == m_themeName : name.empty();

        //  Applying the theme rebuilds these rows, so it is the last thing
        //  the row does. A choice ends the preview without undoing it.
        row = DebuggerMenuCommand::Make (choice.label, current, [this, name]
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

    row = DebuggerMenuCommand::Make (L"Theme", false, [] {});
    m_menuCommands.push_back (row);
    tools.push_back (DxuiPopupMenuItem::ForSubmenu (row, std::move (themes)));

    //  Tools: the settings that have no place in a menu of their own.
    tools.push_back (DxuiPopupMenuItem::ForSeparator());
    add (tools, DebuggerMenuCommand::Make (L"Options...", false, [this] { OpenOptions(); }));

    m_menuBarItems =
    {
        { L"&File",    0, std::move (file)    },
        { L"&Edit",    0, std::move (edit)    },
        { L"&Machine", 0, std::move (machine) },
        { L"&View",    0, std::move (view)    },
        { L"&Debug",   0, std::move (debug)   },
        { L"&Tools",   0, std::move (tools)   },
    };

    if (m_menuBar != nullptr && !m_menuBar->IsOpen())
    {
        m_menuBar->SetItems (m_menuBarItems);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenOptions
//
//  Tools > Options: reverse execution's settings and the disassembly views'
//  options, both saved by the host. The views are redrawn with the options
//  chosen, as a toolbar check box redraws them.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenOptions()
{
    DebuggerOptions                 current;
    std::optional<DebuggerOptions>  chosen;
    int                             view    = 0;



    if (m_host == nullptr)
    {
        return;
    }

    current.reverse     = m_host->GetReverseOptions();
    current.disassembly = m_code->GetOptions();

    chosen = DebuggerOptionsSheet::Ask (GetHwnd(), m_theme, current, (m_snapshot != nullptr) ? m_snapshot->history : HistoryStatus());

    if (!chosen.has_value())
    {
        return;
    }

    m_host->SetReverseOptions (chosen->reverse);

    m_code->SetOptions (chosen->disassembly);
    m_host->SetDebuggerDisassemblyOptions (m_code->GetOptions().ToText());

    for (view = 0; view < DebuggerViewState::kMaxCodeViews && m_snapshot != nullptr; view++)
    {
        ApplyCodeView (view);
    }

    Invalidate();
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



    //  Every press starts or ends a tap of Alt; OnKeyUp sees the release.
    (void) m_menuBar->TrackAltTap (ev.kind, ev.vk);

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
//  DebuggerWindow::OnKeyUp
//
//  The release that ends a tap of Alt toggles the menu bar's access-key
//  underlines. Claiming it keeps it from DefWindowProc, which would enter
//  the window menu's modal loop. Every other release goes on as before.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DebuggerWindow::OnKeyUp (WPARAM vk, LPARAM lParam)
{
    DxuiMessageResult  result = DxuiMessageResult::NotHandled;



    UNREFERENCED_PARAMETER (lParam);

    if (m_menuBar != nullptr && m_menuBar->TrackAltTap (DxuiKeyEventKind::Up, vk))
    {
        Invalidate();
        result = DxuiMessageResult::Handled;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ResetPaneLayout
//
//  Puts every pane back where the window first had it, reopening the ones
//  closed from their close buttons, and saves that.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ResetPaneLayout()
{
    DxuiPaneLayout  layout = DebuggerLayout::MakeDefault();



    layout.PlaceOnMonitors (GetMonitors());
    m_closedPanes.clear();
    m_dockSite->SetPaneLayout (layout);
    m_floating->MarkSyncNeeded();
    m_dockSite->Relayout();
    SaveLayout();

    //  The command bar goes back to its default place too: the top band,
    //  at the start of it, under the menu bar.
    m_barHost.ResetDock();
    m_timelineHost.ResetDock();
    LayoutWidgets();

    SetWindowMenus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenSourceFile
//
//  A source file the user picks, opened as a dropped source is: matched
//  against the loaded debug file's records, or shown on its own.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenSourceFile()
{
    HRESULT                hr     = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters = { { L"Source files", L"*.s;*.asm;*.a65;*.inc;*.a;*.src;*.mac;*.65s;*.s65" }, { L"All files", L"*.*" } };

    hr = m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    OpenSourcePath (chosen.wstring());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenSymbolFile
//
//  SYM LOAD of a debug or symbol file the user picks, in AppleWin's words
//  whatever the console's dialect. A source given is matched against it once
//  it loads.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenSymbolFile (const std::wstring & thenShow)
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

    RunAction (DebuggerActions::GetLoadSymbols (TextEncoding::WideToNarrow (chosen.wstring()), GetMode()));
    m_sources->SetPendingLoose (thenShow);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::LoadFileIntoMemory
//
//  BLOAD of a file the user picks, in the format, at the address and for the
//  length typed in the dialog after the picker, run directly. The dialog
//  opens with the fields typed last.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::LoadFileIntoMemory()
{
    HRESULT                                  hr     = S_OK;
    FileDialogSpec                           spec;
    std::filesystem::path                    chosen;
    bool                                     picked = false;
    std::optional<MemoryFileDialog::Fields>  fields;
    DebuggerAction                           action;
    std::wstring                             error;



    BAIL_OUT_IF (m_host == nullptr || !IsCommandBarEntryEnabled (DebuggerCommands::kLoadFile), S_OK);

    spec.filters = { { L"Binary and hex files", L"*.bin;*.b;*.obj;*.hex;*.ihx;*.s19;*.srec;*.as" }, { L"All files", L"*.*" } };

    hr = m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    fields = RunMemoryFileDialog (chosen.wstring(), false, m_loadFileFields);
    BAIL_OUT_IF (!fields.has_value(), S_OK);

    m_loadFileFields = *fields;

    if (MemoryFileDialog::TryMakeLoad (TextEncoding::WideToNarrow (chosen.wstring()), *fields, GetMode(), action, error))
    {
        RunAction (action);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveMemoryToFile
//
//  BSAVE of a range to a file the user picks, run directly.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveMemoryToFile()
{
    HRESULT                                  hr     = S_OK;
    FileDialogSpec                           spec;
    std::filesystem::path                    chosen;
    bool                                     picked = false;
    std::optional<MemoryFileDialog::Fields>  fields;
    DebuggerAction                           action;
    std::wstring                             error;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters          = { { L"Binary files", L"*.bin" }, { L"All files", L"*.*" } };
    spec.defaultExtension = L"bin";
    spec.defaultFileName  = L"memory.bin";

    hr = m_host->GetHostDialogs().PickFileToSave (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    fields = RunMemoryFileDialog (chosen.wstring(), true, m_saveFileFields);
    BAIL_OUT_IF (!fields.has_value(), S_OK);

    m_saveFileFields = *fields;

    if (MemoryFileDialog::TryMakeSave (TextEncoding::WideToNarrow (chosen.wstring()), *fields, GetMode(), action, error))
    {
        RunAction (action);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunTextDialog
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::wstring> DebuggerWindow::RunTextDialog (const std::wstring & title, const std::wstring & label, const std::wstring & placeholder,
                                                           TextDialog::CheckFn check)
{
    return TextDialog::RunModal (GetHwnd(), m_theme, title, label, placeholder, std::move (check));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunByteDialog
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Byte> DebuggerWindow::RunByteDialog (const std::wstring & title, const std::wstring & label)
{
    return ByteDialog::RunModal (GetHwnd(), m_theme, title, label);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunMemoryFileDialog
//
////////////////////////////////////////////////////////////////////////////////

std::optional<MemoryFileDialog::Fields> DebuggerWindow::RunMemoryFileDialog (const std::wstring & path, bool isSave, const MemoryFileDialog::Fields & initial)
{
    return MemoryFileDialog::RunModal (GetHwnd(), m_theme, path, isSave, initial);
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
        if (m_floating->GetRoutingPane().empty())
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





