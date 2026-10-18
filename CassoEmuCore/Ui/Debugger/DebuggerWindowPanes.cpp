#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/EventLogControllers.h"
#include "Ui/Debugger/Panes/HeatMapPane.h"
#include "Ui/Debugger/Panes/HeatRangesPane.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/BasicProgramController.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/ConsolePanes.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/FindWidgets.h"
#include "Ui/Debugger/Panes/DevicePanelController.h"
#include "Ui/Debugger/Panes/CallStackController.h"
#include "Ui/Debugger/Panes/MemoryWindows.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/TracePaneController.h"
#include "Ui/Debugger/Panes/UndoListPanes.h"
#include "Ui/Debugger/Panes/WatchesPane.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/StepFiltersDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreatePanes
//
//  The panes on the pane interface, each created and called in this order.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreatePanes()
{
    IDebuggerPaneHost                      & host     = *this;
    std::unique_ptr<ProfilerPane>            profiler = std::make_unique<ProfilerPane>           (host);
    std::unique_ptr<HeatMapPane>             heatMap  = std::make_unique<HeatMapPane>            (host, HeatMapPane::Ranges
    {
        [this] () -> const HeatMapRangeSets & { return m_heatRangesPane->GetSets(); },
        [this] () -> const HeatMapSymbols &   { return m_heatRangesPane->GetSymbols(); },
        [this] { m_heatRangesPane->SetMenus(); },
    });
    std::unique_ptr<HeatRangesPane>          ranges   = std::make_unique<HeatRangesPane>         (host, *heatMap);
    std::unique_ptr<MemoryWindowGroup>       memory   = std::make_unique<MemoryWindowGroup>      (host);
    std::unique_ptr<RegistersPane>           regs     = std::make_unique<RegistersPane>          (host);
    std::unique_ptr<StackPane>               stack    = std::make_unique<StackPane>              (host);
    std::unique_ptr<WatchesPane>             watches  = std::make_unique<WatchesPane>            (host);
    std::unique_ptr<BreakpointsPane>         bps      = std::make_unique<BreakpointsPane>        (host);
    std::unique_ptr<TracePaneController>     trace    = std::make_unique<TracePaneController>    (host);
    std::unique_ptr<BasicProgramController>  basic    = std::make_unique<BasicProgramController> (host);



    m_codeEdits = std::make_unique<CodeEditCommands> (host, static_cast<const ICodeViews &> (*this));
    m_finds     = std::make_unique<FindWidgetSet> (host, static_cast<IFindWidgetHost &> (*this));
    m_sources   = std::make_unique<SourceDocumentGroup> (host);
    m_code      = std::make_unique<DisassemblyGroup> (host, *m_gutter, *m_codeEdits, *m_sources);

    for (std::unique_ptr<IDebuggerPane> & view : m_code->CreateViews())
    {
        m_panes.push_back (std::move (view));
    }

    for (std::unique_ptr<IDebuggerPane> & document : m_sources->CreateDocuments())
    {
        m_panes.push_back (std::move (document));
    }

    {
        std::unique_ptr<ConsolePane>  console = std::make_unique<ConsolePane> (host);

        m_consolePane = console.get();
        m_panes.push_back (std::move (console));
    }

    m_profilerPane    = profiler.get();
    m_heatMapPane     = heatMap.get();
    m_heatRangesPane  = ranges.get();
    m_memoryGroup     = memory.get();
    m_registersPane   = regs.get();
    m_stackPane       = stack.get();
    m_watchesPane     = watches.get();
    m_breakpointsPane = bps.get();
    m_tracePane       = trace.get();
    m_basicProgram    = basic.get();

    m_panes.push_back (std::move (regs));
    m_panes.push_back (std::move (watches));
    m_panes.push_back (std::move (stack));
    m_panes.push_back (std::move (bps));
    m_panes.push_back (std::make_unique<CallStackController> (host));
    m_panes.push_back (std::move (trace));
    m_panes.push_back (std::move (basic));

    m_panes.push_back (std::move (heatMap));
    m_panes.push_back (std::move (ranges));
    m_panes.push_back (std::make_unique<DiskLogController>  (host));
    m_panes.push_back (std::make_unique<InputLogController> (host));
    m_panes.push_back (std::move (memory));

    for (std::unique_ptr<IDebuggerPane> & window : m_memoryGroup->CreateWindows())
    {
        m_panes.push_back (std::move (window));
    }

    m_panes.push_back (std::make_unique<SymbolsPane>    (host));
    m_panes.push_back (std::make_unique<BookmarksPane>  (host));
    m_panes.push_back (std::move (profiler));
    m_panes.push_back (std::make_unique<DataBlocksPane> (host));

    //  Every device panel the window can place; each shows while its device
    //  is present and its panel open.
    for (const DebuggerLayout::DiagnosticsPanel & panel : DebuggerLayout::GetDiagnosticsPanels())
    {
        m_panes.push_back (std::make_unique<DevicePanelController> (host, panel.id, panel.title));
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->CreateControls();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FindPane
//
//  The pane on the pane interface with a layout id, or null.
//
////////////////////////////////////////////////////////////////////////////////

IDebuggerPane * DebuggerWindow::FindPane (const std::wstring & pane) const
{
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->GetId() == pane)
        {
            return each.get();
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenEventLog
//
//  The pane in front, opened for good: it comes back open with the window.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenEventLog (DebugLogKind kind)
{
    ShowPane ((kind == DebugLogKind::Disk) ? DebuggerLayout::kDiskLog : DebuggerLayout::kInputLog);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyLogDeltas
//
//  Each frame, the log events the machine's thread sent, to every pane.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyLogDeltas()
{
    DebugLogDeltas  deltas;



    if (m_host == nullptr)
    {
        return;
    }

    m_host->TakeDebuggerEventLogs (deltas);

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->ApplyLogDeltas (deltas, IsVisible());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::HasOwnColumnWidths
//
//  Whether a list is a pane's whose column widths are saved in its own
//  settings, as an event log's are, not with the debug project.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::HasOwnColumnWidths (const DxuiListView * list) const
{
    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        if (pane->HasOwnColumnWidths() && std::ranges::count (pane->GetLists(), list) > 0)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsTextBox
//
//  Whether a control is a box that types: the command line, the Address box
//  or a pane's own box.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsTextBox (const IDxuiControl * control) const
{
    if (control == nullptr)
    {
        return false;
    }

    if (control == m_consolePane->GetCommandLine().GetBox())
    {
        return true;
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        if (std::ranges::count (pane->GetTextBoxes(), control) > 0)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ActivatePaneTab
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ActivatePaneTab (const std::wstring & pane)
{
    (void) m_dockSite->EditPaneLayout().Activate (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RelayoutDock
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RelayoutDock()
{
    m_dockSite->Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MoveSharedControls
//
//  Moves controls several panes share, such as the memory bar and its
//  Address box, into another window. Focus on one of them is left behind in
//  the window it leaves, so it goes to the command line there, as it does
//  when the control hides.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MoveSharedControls (const std::vector<IDxuiControl *> & controls, DxuiWindow * from, DxuiWindow * to)
{
    if (to == from)
    {
        return;
    }

    if (std::ranges::count (controls, m_focusMgr.GetFocusedControl()) > 0)
    {
        m_focusMgr.SetFocused (m_consolePane->GetCommandLine().GetBox());
    }

    m_floating->ForgetFocus (controls);

    for (IDxuiControl * control : controls)
    {
        std::unique_ptr<IDxuiControl>  owned = from->DetachChild (control);

        if (owned != nullptr)
        {
            (void) to->AttachChild (std::move (owned));
        }
    }

    //  The site's strips and drop zones paint over the panes, so it is put
    //  back as this window's last child.
    if (to == this)
    {
        std::unique_ptr<IDxuiControl>  site = DetachChild (m_dockSite);

        (void) AttachChild (std::move (site));
    }

    m_focusMgr.Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddHeatAccessItems
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddHeatAccessItems (Word address, HeatMapOptions::Bank bank, PaneMenuItems & items)
{
    m_heatMapPane->AddAccessItems (address, bank, items);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GoToMemory
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::GoToMemory (Word address)
{
    m_memoryGroup->GetActive()->GetModel().GoTo (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NoteListReleased
//
//  A press on a list ended, the list having had it, for the pane whose list
//  it is, as a click on a register's value edits it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::NoteListReleased (DxuiListView * list, POINT clientPx)
{
    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        pane->OnListReleased (list, clientPx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetLastChildren
//
//  The window's children that paint over the rest, in order: the site's
//  strips and drop zones over the panes, and a box being edited in place over
//  the site.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetLastChildren() const
{
    std::vector<IDxuiControl *>  last = { m_dockSite };



    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        std::ranges::copy (pane->GetOverlays(), std::back_inserter (last));
    }

    return last;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetUndoBar
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar * DebuggerWindow::GetUndoBar (const std::wstring & pane) const
{
    for (UndoListPane * each : std::initializer_list<UndoListPane *> { m_registersPane, m_stackPane, m_watchesPane })
    {
        if (each->GetId() == pane)
        {
            return each->GetBar();
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddZeroPagePointer
//
//  ZP of a zero-page address typed in a dialog, run directly.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddZeroPagePointer()
{
    std::optional<Byte>  address = RunByteDialog (L"Add zero-page pointer", L"Zero-page address");



    if (address.has_value())
    {
        RunAction (DebuggerActions::GetAddZeroPagePointer (*address, GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyBreakpoints()
{
    if (m_snapshot != nullptr)
    {
        m_breakpointsPane->Apply (*m_snapshot);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowBasicLine
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowBasicLine (Word line)
{
    m_basicProgram->ShowLine (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenSource
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenSource (int fileId, int line)
{
    m_sources->Open (fileId, line, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetPaneTabMark
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetPaneTabMark (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark, const std::wstring & tip)
{
    m_dockSite->SetLeadingMark (pane, mark);
    m_dockSite->SetTabTip      (pane, tip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsTipBlocked
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsTipBlocked() const
{
    return m_gutter->IsDragging() || IsAnyMenuOpen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCodeList
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView * DebuggerWindow::GetCodeList (int view) const
{
    return m_code->GetCodeList (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCodeLines
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DebuggerViewSnapshot::CodeLine> & DebuggerWindow::GetCodeLines (int view) const
{
    return m_code->GetCodeLines (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCodeLineOfRow
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::GetCodeLineOfRow (int view, int row) const
{
    return m_code->GetCodeLineOfRow (view, row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyCodeView
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyCodeView (int view)
{
    m_code->ApplyView (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsCodeHeatOn
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsCodeHeatOn() const
{
    return m_code->GetOptions().IsOn (DisassemblyOptions::Option::Heat);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowCode
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowCode (std::optional<Word> address)
{
    m_code->ShowCode (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetBranchTip
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetBranchTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    return m_code->TryGetBranchTip (clientPx, anchor, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetSymbolTip
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetSymbolTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    return m_code->TryGetSymbolTip (clientPx, anchor, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindView
//
//  The text a pane's find widget searches: the console's, or a source
//  document's.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView * DebuggerWindow::GetFindView (const std::wstring & pane) const
{
    return (FindPane (pane) != nullptr) ? FindPane (pane)->GetFindView() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFindFrame
//
////////////////////////////////////////////////////////////////////////////////

DebuggerPaneFrame * DebuggerWindow::GetFindFrame (const std::wstring & pane) const
{
    return (FindPane (pane) != nullptr) ? FindPane (pane)->GetFindFrame() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ReleaseFocusOf
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ReleaseFocusOf (IDxuiControl * control)
{
    if (m_focusMgr.GetFocusedControl() == control)
    {
        m_focusMgr.SetFocused (m_consolePane->GetCommandLine().GetBox());
    }

    m_floating->ForgetFocus ({ control });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::KeepDockLast
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::KeepDockLast()
{
    std::unique_ptr<IDxuiControl>  site = DetachChild (m_dockSite);



    (void) AttachChild (std::move (site));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::KeepOverlaysLast
//
//  The site's strips and drop zones paint over the panes, and a box being
//  edited in place over the site, so both are last.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::KeepOverlaysLast()
{
    for (IDxuiControl * last : GetLastChildren())
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
//  DebuggerWindow::FocusInPane
//
//  A floating pane has its own focus.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::FocusInPane (const std::wstring & pane, IDxuiControl * control)
{
    FloatingPanes::RouteScope  scope (*m_floating, m_floating->Contains (pane) ? pane : std::wstring());



    SetFocusedControl (control);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::BringPaneForward
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::BringPaneForward (const std::wstring & pane)
{
    if (m_dockSite != nullptr)
    {
        m_dockSite->ActivatePane (pane);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetModeChoices
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DebuggerWindow::ModeChoice> & DebuggerWindow::GetModeChoices()
{
    return ConsoleModeBar::GetModeChoices();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetModeLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetModeLabel (const wchar_t * mode)
{
    return ConsoleModeBar::GetModeLabel (mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowPopupMenu
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowPopupMenu (const PaneMenuItems & items, POINT clientPx)
{
    std::vector<DxuiPopupMenuItem>  menu;



    m_menuCommands.clear();
    SetWindowMenus();

    for (const auto & [label, action] : items)
    {
        m_menuCommands.push_back (DebuggerMenuCommand::Make (label, false, action));
        menu.push_back (DxuiPopupMenuItem::ForCommand (m_menuCommands.back()));
    }

    DxuiContextMenu::Show (*GetMenuHost(), clientPx.x, clientPx.y, std::move (menu));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MarkPaneTab
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MarkPaneTab (const std::wstring & pane)
{
    if (m_dockSite != nullptr)
    {
        m_dockSite->SetIndicator (pane, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyHeatMap
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyHeatMap()
{
    m_heatMapPane->Apply    (*m_snapshot);
    m_heatRangesPane->Apply (*m_snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunStepFiltersDialog
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<std::wstring>> DebuggerWindow::RunStepFiltersDialog (const std::vector<std::wstring> & filters,
                                                                               std::function<bool (const std::wstring &, std::wstring &)> check)
{
    return StepFiltersDialog::RunModal (GetHwnd(), m_theme, filters, std::move (check));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditStepFilters
//
//  Debug > Step filters... (FR-184): the dialog's list against the one it
//  opened with, each filter taken out run as SKIP - and each new one as
//  SKIP. A name must be a symbol, and the filter is added by that name.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EditStepFilters()
{
    std::vector<std::wstring>                  before;
    std::optional<std::vector<std::wstring>>   after;
    std::vector<DebuggerAction>                actions;
    auto                                       findSymbol = [this] (const std::wstring & name) -> std::optional<Word>
    {
        std::string  narrow = TextEncoding::WideToNarrow (name);



        if (m_snapshot == nullptr || m_snapshot->symbols == nullptr)
        {
            return std::nullopt;
        }

        for (const SymbolInfo & symbol : m_snapshot->symbols->symbols)
        {
            if (_stricmp (symbol.name.c_str(), narrow.c_str()) == 0)
            {
                return symbol.address;
            }
        }

        return std::nullopt;
    };



    if (m_snapshot == nullptr)
    {
        return;
    }

    for (const StepFilterEntry & entry : m_snapshot->stepFilters)
    {
        before.push_back (TextEncoding::NarrowToWide (DebuggerActions::GetRemoveSkip (entry, GetMode()).command.text));
    }

    after = RunStepFiltersDialog (before, [&findSymbol] (const std::wstring & text, std::wstring & error)
    {
        bool  isSymbol = findSymbol (text).has_value();



        error = isSymbol ? std::wstring() : std::format (L"{} is not a symbol, an address or a range.", text);
        return isSymbol;
    });

    if (!after.has_value())
    {
        return;
    }

    for (size_t i = 0; i < m_snapshot->stepFilters.size(); i++)
    {
        if (std::ranges::find (*after, before[i]) == after->end())
        {
            actions.push_back (DebuggerActions::GetRemoveSkip (m_snapshot->stepFilters[i], GetMode()));
        }
    }

    for (const std::wstring & text : *after)
    {
        Word                 first   = 0;
        Word                 last    = 0;
        std::optional<Word>  address;



        if (std::ranges::find (before, text) != before.end())
        {
            continue;
        }

        if (StepFiltersDialog::TryParseRange (text, first, last))
        {
            actions.push_back (DebuggerActions::GetSkipRange (first, last, GetMode()));
        }
        else if (address = findSymbol (text); address.has_value())
        {
            actions.push_back (DebuggerActions::GetSkipRoutine (TextEncoding::WideToNarrow (text), *address, GetMode()));
        }
    }

    for (const DebuggerAction & action : actions)
    {
        RunAction (action);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCursorAddress
//
//  The selected disassembly line's address, or the PC's.
//
////////////////////////////////////////////////////////////////////////////////

Word DebuggerWindow::GetCursorAddress() const
{
    int  line = GetCodeLineOfRow (m_code->GetActive(), m_code->GetCodeList (m_code->GetActive())->GetSelectedRow());



    if (line >= 0)
    {
        return GetCodeLines (m_code->GetActive())[(size_t) line].address;
    }

    return (m_snapshot != nullptr) ? m_snapshot->pc : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleBookmarkAtCursor
//
//  Ctrl+K, K (FR-189): a bookmark on the selected line is removed, and one
//  is added where there is none.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleBookmarkAtCursor()
{
    Word  at       = GetCursorAddress();
    auto  bookmark = std::ranges::find_if (m_snapshot->bookmarks, [at] (const DebuggerViewSnapshot::WatchLine & each) { return each.address == at; });



    RunAction (bookmark != m_snapshot->bookmarks.end() ? DebuggerActions::GetClearBookmark (bookmark->id, GetMode())
                                                       : DebuggerActions::GetAddBookmark (at, GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GoToBookmark
//
//  Ctrl+K, N and Ctrl+K, P: the next bookmark after the selected line, or
//  the one before it, round from the last to the first.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::GoToBookmark (bool isBackward)
{
    std::optional<Word>  next = BookmarksPane::GetNext (m_snapshot->bookmarks, GetCursorAddress(), isBackward);



    if (next.has_value())
    {
        ShowCode (*next);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowMessageBox
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::ShowMessageBox (
    HWND                   owner,
    const std::wstring   & text,
    const wchar_t        * caption,
    UINT                   type)
{
    return DxuiMessageBox (owner, m_theme, text.c_str(), caption, type);
}
