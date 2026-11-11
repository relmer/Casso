#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Ui/Debugger/SymbolComments.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/Source/SourcePathList.h"
#include "Config/WindowPlacementProfile.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/FlagsDialog.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/SymbolDescriptions.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
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



    //  A right-click on a tab brings its pane forward first, as a left click
    //  does, so the menu is over the pane it acts on.
    if (fromTab)
    {
        ShowPane  (pane);
        FocusPane (pane);
    }

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
            m_menuCommands.push_back (DebuggerMenuCommand::Make (L"Follow PC", [this, view] { if (m_host != nullptr) { m_host->SetDebuggerFollowView (view); } }));
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

        m_menuCommands.push_back (DebuggerMenuCommand::Make (L"Syntax", [] {}));
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

        m_menuCommands.push_back (DebuggerMenuCommand::Make (item.label, [action = item.action] { if (action) { (void) action(); } }));
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
    int                                                          held   = -1;
    RECT                                                         bounds = {};



    if (m_snapshot == nullptr)
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
        ShowEditMenu (m_consolePane->GetOutput().GetView(), clientPx, GetConsoleMenuCommands());
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

        for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
        {
            held = (m_code->GetCodeList (view) == list) ? view : held;
        }

        //  As in Explorer: a right-click on a selected row does not change
        //  the selection, so Copy copies every selected row; on any other
        //  row it selects that row alone.
        if (row >= 0 && !list->IsRowSelected (row))
        {
            list->SetSelectedRow (row);
        }

        //  And the list takes the focus, as Explorer's does, so the row the
        //  menu acts on is drawn selected rather than in the faint color of a
        //  selection in a list without the focus.
        if (row >= 0)
        {
            SetFocusedControl (list);
        }

        AddListMenuItems (list, row, GetColumnAt (list, clientPx.x - bounds.left), items);
    }

    if (FindPane (pane) != nullptr)
    {
        FindPane (pane)->AddContentMenuItems (clientPx, items);
    }

    AddGoToDefinition (clientPx, items);

    //  The row and the focus are taken above whether or not a menu can open.
    if (items.empty() || GetMenuHost() == nullptr)
    {
        return !items.empty();
    }

    m_menuCommands.clear();
    SetWindowMenus();

    menu = MakePaneMenuRows (items);

    DxuiContextMenu::Show (*GetMenuHost(), clientPx.x, clientPx.y, std::move (menu), [this] (bool) { HoldCodeView (-1); });
    HoldCodeView (held);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HoldCodeView
//
//  While a code view's menu is open, the running machine doesn't move the
//  view out from under it; -1 lets the view go. The menu is shared, so
//  opening one closes the last, and its close lets the last view go.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::HoldCodeView (int view)
{
    if (m_host == nullptr || view == m_heldCodeView)
    {
        return;
    }

    if (m_heldCodeView >= 0)
    {
        m_host->HoldDebuggerCode (m_heldCodeView, false);
    }

    m_heldCodeView = view;

    if (view >= 0)
    {
        m_host->HoldDebuggerCode (view, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddGoToDefinition
//
//  Insert Go to definition first on the menu when the point is over a
//  symbol in a code view or a source document.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddGoToDefinition (POINT clientPx, std::vector<std::pair<std::wstring, std::function<void()>>> & items)
{
    Word  address = 0;



    if (TryGetDefinitionAt (clientPx, address))
    {
        items.insert (items.begin(), { L"Go to definition", [this, address] { GoToDefinition (address); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetDefinitionAt
//
//  The address of the symbol at a point in a code view or a source
//  document. A constant has no defining line to go to.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetDefinitionAt (POINT clientPx, Word & address) const
{
    const SymbolInfo  * found = nullptr;
    std::string         name;



    if (m_snapshot == nullptr || m_snapshot->symbols == nullptr)
    {
        return false;
    }

    if (!m_code->TryGetSymbolAt (clientPx, name, address))
    {
        for (int slot = 0; slot < SourceDocuments::kMaxDocuments && found == nullptr; slot++)
        {
            const SourceDocumentPane  & document = m_sources->GetDocument (slot);

            if (document.IsShown() && document.GetView() != nullptr && DxuiDockSite::Contains (document.GetView()->GetBounds(), clientPx))
            {
                found = document.GetModel().FindSymbolAt (clientPx, m_snapshot->symbols->symbols);
            }
        }

        if (found == nullptr)
        {
            return false;
        }

        name    = found->name;
        address = found->address;
    }

    return std::ranges::none_of (m_snapshot->symbols->symbols, [&name] (const SymbolInfo & symbol) { return symbol.name == name && symbol.isConstant; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GoToDefinition
//
//  The source line that defines the label at an address, in its document,
//  when a debug file maps one there; otherwise the address in the
//  disassembly.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::GoToDefinition (Word address)
{
    std::optional<std::pair<int, int>>  place;



    if (m_snapshot != nullptr && m_snapshot->source.has_value() && m_snapshot->source->lineAddresses != nullptr)
    {
        place = SymbolComments::FindDefiningLine (*m_snapshot->source->lineAddresses, address);
    }

    if (place.has_value())
    {
        m_sources->Open (place->first, place->second, true);
        return;
    }

    ShowCode (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MakePaneMenuRows
//
//  A pane's rows as the menu draws them. A row with no action is disabled,
//  such as Delete on a row that cannot be deleted; one with no label either
//  is a separator, dropped at either end and where two are next to each
//  other. A row whose label starts with a submenu's goes into the submenu.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DebuggerWindow::MakePaneMenuRows (const PaneMenuItems & items)
{
    std::vector<DxuiPopupMenuItem>  menu;
    std::wstring                    submenu;
    std::vector<DxuiPopupMenuItem>  children;
    bool                            isSeparatorDue = false;



    auto  addRow = [&menu, &isSeparatorDue] (DxuiPopupMenuItem row)
    {
        if (isSeparatorDue && !menu.empty())
        {
            menu.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        isSeparatorDue = false;
        menu.push_back (std::move (row));
    };

    auto  endSubmenu = [this, &submenu, &children, &addRow]
    {
        if (!children.empty())
        {
            m_menuCommands.push_back (DebuggerMenuCommand::Make (submenu, nullptr));
            addRow (DxuiPopupMenuItem::ForSubmenu (m_menuCommands.back(), std::move (children)));
        }

        children.clear();
        submenu.clear();
    };

    for (const auto & [label, action] : items)
    {
        size_t        split     = label.find (PaneMenu::kSubmenu);
        std::wstring  parent    = (split == std::wstring::npos) ? std::wstring() : label.substr (0, split);
        std::wstring  rowLabel  = (split == std::wstring::npos) ? label : label.substr (split + 1);
        bool          isEnabled = (bool) action;

        if (parent != submenu)
        {
            endSubmenu();
            submenu = parent;
        }

        if (label.empty() && !action)
        {
            isSeparatorDue = true;
            continue;
        }

        m_menuCommands.push_back (PaneMenu::IsChecked (rowLabel) ? DebuggerMenuCommand::Make (PaneMenu::GetText (rowLabel), true, action) : DebuggerMenuCommand::Make (rowLabel, action));
        m_menuCommands.back()->isEnabled = [isEnabled] { return isEnabled; };

        if (parent.empty())
        {
            addRow (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
        }
        else
        {
            children.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
        }
    }

    endSubmenu();
    return menu;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddSkipItem
//
//  A routine is in the step filter by its name, or by an address its range
//  has when it has no name.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddSkipItem (const std::string & name, Word address, PaneMenuItems & items)
{
    constexpr const wchar_t  * kpszLabel = L"Never step into this routine";
    auto                       found     = m_snapshot == nullptr ? std::vector<StepFilterEntry>::const_iterator()
                                                                 : std::ranges::find_if (m_snapshot->stepFilters, [&name, address] (const StepFilterEntry & entry)
                                                                   {
                                                                       return !name.empty() ? _stricmp (entry.name.c_str(), name.c_str()) == 0
                                                                                            : entry.name.empty() && address >= entry.first && address <= entry.last;
                                                                   });



    if (m_snapshot != nullptr && found != m_snapshot->stepFilters.end())
    {
        items.push_back ({ PaneMenu::MakeChecked (kpszLabel), [this, entry = *found] { RunAction (DebuggerActions::GetRemoveSkip (entry, GetMode())); } });
        return;
    }

    items.push_back ({ kpszLabel, [this, name, address]
    {
        RunAction (DebuggerActions::GetSkipRoutine (name, address, GetMode()));
        ShowSkipNotice (name.empty() ? std::format ("{:04X}", address) : name);
    } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddBookmarkItem
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddBookmarkItem (Word address, PaneMenuItems & items)
{
    auto  found = (m_snapshot == nullptr) ? std::vector<DebuggerViewSnapshot::WatchLine>::const_iterator()
                                          : std::ranges::find_if (m_snapshot->bookmarks, [address] (const DebuggerViewSnapshot::WatchLine & each) { return each.address == address; });



    if (m_snapshot != nullptr && found != m_snapshot->bookmarks.end())
    {
        items.push_back ({ L"Remove bookmark", [this, id = found->id] { RunAction (DebuggerActions::GetClearBookmark (id, GetMode())); } });
    }
    else
    {
        items.push_back ({ L"Add bookmark", [this, address] { RunAction (DebuggerActions::GetAddBookmark (address, GetMode())); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowSkipNotice
//
//  How to take a routine out of the step filter again, until the notice's
//  check box turns it off.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowSkipNotice (const std::string & routine)
{
    bool          isOff = false;
    std::wstring  text;



    if (m_host == nullptr || !m_host->IsDebuggerSkipNoticeShown())
    {
        return;
    }

    text = std::format (L"A step into {0} now steps over it.\n\nTo step into it again, click the checked Never step into this routine on its menu, "
                        L"remove it in Debug > Step filters..., or type SKIP - {0}.", TextEncoding::NarrowToWide (routine));

    (void) ShowCheckMessageBox (GetRoutingHwnd(), text, L"Step filter", MB_OK | MB_ICONINFORMATION, L"Don't show this message again", isOff);

    if (isOff)
    {
        m_host->SetDebuggerSkipNoticeShown (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowCheckMessageBox
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::ShowCheckMessageBox (HWND owner, const std::wstring & text, const wchar_t * caption, UINT type, const std::wstring & checkLabel, bool & ioChecked)
{
    return DxuiMessageBox (owner, m_theme, text.c_str(), caption, type, checkLabel, ioChecked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowColumnMenu
//
//  On the headings of a list whose columns can be chosen, a check for each
//  column, then the Filter cascade of the column under the point when it has
//  one. False when the point is not on the headings or the list has no such
//  menu.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ShowColumnMenu (DxuiListView * list, POINT clientPx)
{
    std::vector<std::shared_ptr<DxuiCommand>>  commands;
    std::vector<std::shared_ptr<DxuiCommand>>  filterCommands;
    std::vector<DxuiPopupMenuItem>             menu;
    std::vector<DxuiPopupMenuItem>             filter;
    RECT                                       bounds   = list->GetBounds();
    int                                        column   = GetColumnAt (list, clientPx.x - bounds.left);



    if (!list->IsHeaderShown() || clientPx.y - bounds.top >= list->GetHeaderHeightPx())
    {
        return false;
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        (void) pane->AddColumnMenuItems (list, commands);

        if (filter.empty() && column >= 0)
        {
            filter = pane->GetColumnFilterItems (list, (size_t) column, filterCommands);
        }
    }

    if (commands.empty() && filter.empty())
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

    if (!filter.empty())
    {
        std::ranges::copy (filterCommands, std::back_inserter (m_menuCommands));
        m_menuCommands.push_back (DebuggerMenuCommand::Make (L"Filter", [] {}));

        if (!menu.empty())
        {
            menu.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        menu.push_back (DxuiPopupMenuItem::ForSubmenu (m_menuCommands.back(), std::move (filter)));
    }

    DxuiContextMenu::Show (*GetMenuHost(), clientPx.x, clientPx.y, std::move (menu));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetConsoleMenuCommands
//
//  The console output's rows after the edit rows: Clear, whether the console
//  lists the window's actions as commands, and whether its links open in File
//  Explorer rather than Casso Explorer.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<DxuiCommand>> DebuggerWindow::GetConsoleMenuCommands()
{
    bool  isEchoing  = m_host != nullptr && m_host->IsDebuggerEchoingActions();
    bool  isExplorer = m_host != nullptr && m_host->IsDebuggerLinkingToFileExplorer();



    return
    {
        DebuggerMenuCommand::Make (L"Clear", [this] { m_consolePane->GetOutput().Clear(); }),
        DebuggerMenuCommand::Make (L"Show UI actions as commands", isEchoing, [this, isEchoing]
        {
            if (m_host != nullptr)
            {
                m_host->SetDebuggerEchoingActions (!isEchoing);
            }
        }),
        DebuggerMenuCommand::Make (L"Open links in File Explorer", isExplorer, [this, isExplorer]
        {
            if (m_host != nullptr)
            {
                m_host->SetDebuggerLinkingToFileExplorer (!isExplorer);
            }
        }),
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowEditMenu
//
//  Cut, Copy, Paste and Select all, as far as the control does them, each
//  dimmed when it has nothing to act on; then the extra rows after a line.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowEditMenu (IDxuiControl * control, POINT clientPx, std::vector<std::shared_ptr<DxuiCommand>> extra)
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

        m_menuCommands.push_back (DebuggerMenuCommand::Make (label, [this, control, command] { (void) control->InvokeCommand (command); Invalidate(); }));
        m_menuCommands.back()->accelerator = accelerator;
        m_menuCommands.back()->isEnabled   = [control, command] { bool on = false; return control->QueryCommand (command, on) && on; };
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    if (!extra.empty())
    {
        menu.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    for (std::shared_ptr<DxuiCommand> & command : extra)
    {
        m_menuCommands.push_back (std::move (command));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    if (GetMenuHost() != nullptr)
    {
        DxuiContextMenu::Show (*GetMenuHost(), clientPx.x, clientPx.y, std::move (menu));
    }
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
