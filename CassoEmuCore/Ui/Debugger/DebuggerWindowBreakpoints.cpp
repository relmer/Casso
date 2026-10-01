#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureBreakpointBar
//
//  The breakpoints pane's toolbar (FR-119). Delete acts on every selected
//  row, so the list takes more than one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureBreakpointBar()
{
    BreakpointBarCommands::Handlers  handlers;



    handlers.dispatch  = [this] (int id) { RunBreakpointBarEntry (id); };
    handlers.isEnabled = [this] (int id) { return IsBreakpointBarEnabled (id); };

    m_breakpointCommands = std::make_unique<BreakpointBarCommands> (std::move (handlers));

    m_breakpointList->SetMultiSelect (true);

    m_breakpointBar->SetTextRenderer (GetTextRenderer());
    m_breakpointBar->SetPopupHost    (GetPopupHost());
    m_breakpointBar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
    m_breakpointBar->SetCompact      (true);
    m_breakpointBar->EnableSeeMore   (s_kpszMdl2More, L"See more");
    m_breakpointBar->SetEntries      (m_breakpointCommands->BuildEntries());
    m_breakpointBar->SetVisible      (false);

    SetBreakpointBarMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetBreakpointBarMenus
//
//  New's kinds, each opening the breakpoint dialog set to that kind, and the
//  optional columns, each checked while it shows (FR-118). Name always
//  shows, so it is not offered. Rebuilt before each drop-down opens, since
//  the rows carry the checks they were built with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetBreakpointBarMenus()
{
    struct Kind
    {
        const wchar_t   * label;
        BreakpointKind    kind;
        WatchAccess       access;
    };

    static constexpr Kind           kKinds[] =
    {
        { L"Breakpoint at address...", BreakpointKind::Address,  WatchAccess::ReadWrite },
        { L"Function breakpoint...",   BreakpointKind::Address,  WatchAccess::ReadWrite },
        { L"Data breakpoint...",       BreakpointKind::Memory,   WatchAccess::Write     },
        { L"Register condition...",    BreakpointKind::Register, WatchAccess::ReadWrite },
        { L"Opcode...",                BreakpointKind::Opcode,   WatchAccess::ReadWrite },
        { L"I/O range...",             BreakpointKind::Io,       WatchAccess::ReadWrite },
    };
    std::vector<DxuiPopupMenuItem>  kinds;
    std::vector<DxuiPopupMenuItem>  columns;



    for (const Kind & each : kKinds)
    {
        BreakpointKind  kind   = each.kind;
        WatchAccess     access = each.access;

        kinds.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (each.label, false, [this, kind, access] { NewBreakpoint (kind, access); })));
    }

    for (size_t i = 1; i < BreakpointColumns::kCount; i++)
    {
        columns.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (BreakpointColumns::GetHeading ((BreakpointColumns::Column) i), m_breakpointShown[i],
                                                                           [this, i] { ToggleBreakpointColumn ((BreakpointColumns::Column) i); })));
    }

    m_breakpointBar->SetDropDownItems (BreakpointBarCommands::kNew,     std::move (kinds));
    m_breakpointBar->SetDropDownItems (BreakpointBarCommands::kColumns, std::move (columns));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceBreakpointBar
//
//  The bar sits in the place held at the top of the breakpoints pane and is
//  one of the pane's controls, so it goes with the pane into a floating
//  window and draws, measures and opens its menus there.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceBreakpointBar()
{
    bool          shown = m_breakpointSlot != nullptr && m_breakpointSlot->IsVisible();
    DxuiWindow  * host  = GetPaneHost (DebuggerLayout::kBreakpoints);
    RECT          slot  = {};



    m_breakpointBar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    slot = m_breakpointSlot->GetBounds();

    m_breakpointBar->SetTextRenderer   (host->GetTextRenderer());
    m_breakpointBar->SetPopupHost      (host->GetPopupHost());
    m_breakpointBar->SetHostClientRect (host->GetBounds());
    m_breakpointBar->Layout            (slot, m_scaler);

    host->SetChildClip (m_breakpointBar, slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteBreakpointBarMouse
//
//  As the command bar's: the strip and whatever menu it has open take the
//  left button; the right button over the strip opens the pane's own menu.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteBreakpointBarMouse (const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    RECT  strip = m_breakpointBar->GetBounds();
    bool  open  = m_breakpointBar->IsMenuOpen();
    bool  over  = m_breakpointBar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y });



    if (!over && !open)
    {
        m_breakpointBar->OnToolbarMouseLeave();
        return false;
    }

    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        UpdateTooltip (ev.positionDip);

        return m_breakpointBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open)
        {
            SetBreakpointBarMenus();
        }

        return m_breakpointBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_breakpointBar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsBreakpointBarEnabled
//
//  A button that cannot act is disabled: nothing selected, nothing to undo,
//  no source line for the selection (FR-119).
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsBreakpointBarEnabled (int id) const
{
    std::vector<DebuggerViewSnapshot::BreakpointLine>  selected = GetSelectedBreakpoints();
    bool                                               any      = m_snapshot != nullptr && !m_snapshot->breakpoints.empty();
    bool                                               one      = selected.size() == 1;
    int                                                fileId   = -1;
    int                                                line     = 0;
    auto                                               hasState = [this] (bool enabled)
    {
        return std::ranges::any_of (m_snapshot->breakpoints, [enabled] (const DebuggerViewSnapshot::BreakpointLine & bp) { return bp.enabled == enabled; });
    };



    switch (id)
    {
    case BreakpointBarCommands::kDelete:     return !selected.empty();
    case BreakpointBarCommands::kDeleteAll:  return any;
    case BreakpointBarCommands::kExport:     return any;
    case BreakpointBarCommands::kEnableAll:  return any && hasState (false);
    case BreakpointBarCommands::kDisableAll: return any && hasState (true);
    case BreakpointBarCommands::kUndo:       return m_snapshot != nullptr && m_snapshot->canUndoBreakpoints;
    case BreakpointBarCommands::kRedo:       return m_snapshot != nullptr && m_snapshot->canRedoBreakpoints;
    case BreakpointBarCommands::kGoToSource: return one && BreakpointColumns::TryGetSourcePlace (*m_snapshot, selected[0], fileId, line);
    case BreakpointBarCommands::kGoToCode:   return one && BreakpointColumns::HasAddress (selected[0].info);
    default:                                 return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunBreakpointBarEntry
//
//  Each change is one step on the pane's undo list; Delete all and Disable
//  all are one action each, and Delete one for each selected row.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunBreakpointBarEntry (int id)
{
    std::vector<DebuggerViewSnapshot::BreakpointLine>  selected = GetSelectedBreakpoints();
    BreakpointStep                                     step;
    int                                                fileId   = -1;
    int                                                line     = 0;



    if (!IsBreakpointBarEnabled (id))
    {
        return;
    }

    switch (id)
    {
    case BreakpointBarCommands::kDelete:
        for (const DebuggerViewSnapshot::BreakpointLine & bp : selected)
        {
            step.actions.push_back (DebuggerActions::GetClearBreakpoint (bp.id, GetMode()));
        }

        RunBreakpointStep (std::move (step));
        break;

    case BreakpointBarCommands::kDeleteAll:  RunBreakpointStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetClearAllBreakpoints  (GetMode()) },        {} }); break;
    case BreakpointBarCommands::kEnableAll:  RunBreakpointStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEnableAllBreakpoints (true,  GetMode()) }, {} }); break;
    case BreakpointBarCommands::kDisableAll: RunBreakpointStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEnableAllBreakpoints (false, GetMode()) }, {} }); break;
    case BreakpointBarCommands::kUndo:       RunBreakpointStep ({ BreakpointStep::Kind::Undo,    {}, {} });                                                     break;
    case BreakpointBarCommands::kRedo:       RunBreakpointStep ({ BreakpointStep::Kind::Redo,    {}, {} });                                                     break;
    case BreakpointBarCommands::kExport:     ExportBreakpoints();                                                                                                break;
    case BreakpointBarCommands::kImport:     ImportBreakpoints();                                                                                                break;

    case BreakpointBarCommands::kGoToSource:
        if (BreakpointColumns::TryGetSourcePlace (*m_snapshot, selected[0], fileId, line))
        {
            OpenSourceDocument (fileId, line, true);
        }

        break;

    case BreakpointBarCommands::kGoToCode:
        ShowCode (selected[0].address);
        break;

    default:
        //  New and Show columns are drop-downs; their rows act.
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NewBreakpoint
//
//  The breakpoint dialog, set to the kind chosen and at the PC, asks for what
//  that kind needs; its definition is the line a person could have typed.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::NewBreakpoint (BreakpointKind kind, WatchAccess access)
{
    BreakpointInfo              info;
    std::optional<std::string>  definition;



    info.kind    = kind;
    info.access  = access;
    info.address = (m_snapshot != nullptr) ? m_snapshot->pc : (Word) 0;
    info.last    = info.address;

    definition = BreakpointDialog::Ask (GetHwnd(), m_theme, info);

    if (definition.has_value())
    {
        RunBreakpointStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetDefineBreakpoint (*definition, GetMode()) }, {} });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunBreakpointStep
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunBreakpointStep (BreakpointStep step)
{
    RunAction (DebuggerActions::GetBreakpointStep (std::move (step)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSelectedBreakpoints
//
//  Every selected row's breakpoint, or the one row the keyboard is on.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerViewSnapshot::BreakpointLine> DebuggerWindow::GetSelectedBreakpoints() const
{
    std::vector<DebuggerViewSnapshot::BreakpointLine>  selected;
    std::vector<int>                                   rows;



    if (m_breakpointList == nullptr)
    {
        return selected;
    }

    rows = m_breakpointList->GetSelectedRows();

    if (rows.empty() && m_breakpointList->GetSelectedRow() >= 0)
    {
        rows.push_back (m_breakpointList->GetSelectedRow());
    }

    for (int row : rows)
    {
        const DebuggerViewSnapshot::BreakpointLine  * bp = GetBreakpointOfRow (row);

        if (bp != nullptr)
        {
            selected.push_back (*bp);
        }
    }

    return selected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ExportBreakpoints
//
//  BPSAVE to a file the user chooses (FR-121), in AppleWin's words whatever
//  the console's dialect.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ExportBreakpoints()
{
    HRESULT                hr     = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters          = { { L"Breakpoint files", L"*.txt" }, { L"All files", L"*.*" } };
    spec.defaultExtension = L"txt";
    spec.defaultFileName  = L"Breakpoints.txt";

    hr = m_host->GetHostDialogs().PickFileToSave (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    m_host->RunDebuggerCommandInMode ("BPSAVE " + TextEncoding::WideToNarrow (chosen.wstring()), CommandMode::AppleWin);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ImportBreakpoints
//
//  The breakpoints of a file the user chooses, added to those already set as
//  one undo step (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ImportBreakpoints()
{
    HRESULT                hr     = S_OK;
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    spec.filters = { { L"Breakpoint files", L"*.txt" }, { L"All files", L"*.*" } };

    hr = m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    RunBreakpointStep ({ BreakpointStep::Kind::Import, {}, TextEncoding::WideToNarrow (chosen.wstring()) });

Error:
    return;
}





