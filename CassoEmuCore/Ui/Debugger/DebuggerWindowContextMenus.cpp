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

    //  A row with no action is disabled, such as Delete on a row that cannot
    //  be deleted.
    for (auto & [label, action] : items)
    {
        bool  isEnabled = (bool) action;

        m_menuCommands.push_back (DebuggerMenuCommand::Make (label, false, action));
        m_menuCommands.back()->isEnabled = [isEnabled] { return isEnabled; };
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
