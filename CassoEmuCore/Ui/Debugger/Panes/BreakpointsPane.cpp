#include "Pch.h"

#include "Ui/Debugger/Panes/BreakpointsPane.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::BreakpointsPane
//
////////////////////////////////////////////////////////////////////////////////

BreakpointsPane::BreakpointsPane (IDebuggerPaneHost & host) :
    IDebuggerPane (host),
    m_bar         (host, DebuggerLayout::kBreakpoints, L"Breakpoint commands")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::~BreakpointsPane
//
////////////////////////////////////////////////////////////////////////////////

BreakpointsPane::~BreakpointsPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BreakpointsPane::GetId() const
{
    return DebuggerLayout::kBreakpoints;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::CreateControls()
{
    m_list = m_host.GetControlParent().CreateChild<DxuiListView>();
    m_bar.Create();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::CreateOverlays
//
//  The rename box, drawn over its row.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::CreateOverlays()
{
    m_nameEditor = m_host.GetControlParent().CreateChild<DxuiTextInput>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::Configure
//
//  The toolbar (FR-119), whose Delete acts on every selected row, so the
//  list takes more than one. Activating a breakpoint brings its address into
//  view, or on its Name opens the rename box; its checkbox turns it on and
//  off, as in Visual Studio's Breakpoints window.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::Configure()
{
    BreakpointBarCommands::Handlers  handlers;



    handlers.dispatch  = [this] (int id) { RunBarEntry (id); };
    handlers.isEnabled = [this] (int id) { return IsBarEntryEnabled (id); };
    handlers.crossArgb = [this]          { return m_host.GetBreakpointArgb(); };

    m_commands = std::make_unique<BreakpointBarCommands> (std::move (handlers));

    m_list->SetMultiSelect (true);

    m_bar.Configure();
    m_bar.GetBar()->SetEntries (m_commands->BuildEntries());
    m_bar.SetOnPress ([this] { SetBarMenus(); });

    SetBarMenus();

    m_nameEditor->SetVisible   (false);
    m_nameEditor->SetOverText  (true);
    m_nameEditor->SetHwnd      (m_host.GetOwnerHwnd());
    m_nameEditor->SetMaxLength (64);

    m_list->SetActivateOnDoubleClick (true);

    SetColumns();

    m_list->SetOnActivateRow ([this] (int row)
    {
        const DebuggerViewSnapshot::BreakpointLine  * bp    = GetBreakpointOfRow (row);
        RECT                                          name  = {};
        RECT                                          list  = m_list->GetBounds();
        POINT                                         press = m_host.GetLastPressPx();
        bool                                          isOn  = m_list->GetCellTextRectPx (row, 0, name) && press.x - list.left >= name.left &&
                                                              press.x - list.left < name.right;

        if (bp != nullptr && isOn)
        {
            BeginNameEdit (row);
        }
        else if (bp != nullptr)
        {
            m_host.GoToCode (bp->address);
        }
    });

    m_list->SetOnCheckToggled ([this] (int row, size_t, bool checked)
    {
        const DebuggerViewSnapshot::BreakpointLine  * bp = GetBreakpointOfRow (row);

        if (bp != nullptr)
        {
            RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEnableBreakpoint (bp->id, checked, m_host.GetMode()) }, {} });
        }
    });

    m_list->SetOnSortColumn ([this] (int column) { Sort ((BreakpointColumns::Column) column); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::AddToDock
//
//  The toolbar, a place PlaceBar fills, over the rows (FR-119).
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (L"Breakpoints");
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_list);

    dock.AddPane (DebuggerLayout::kBreakpoints, L"Breakpoints", m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * BreakpointsPane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> BreakpointsPane::GetControls() const
{
    return { m_bar.GetSlot(), m_list, m_bar.GetBar(), m_nameEditor };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetColorKey
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PaneColorKeySpec> BreakpointsPane::GetColorKey() const
{
    return PaneColorKeySpec { m_bar.GetSlot(), ColorLegend::Pane::Breakpoints, nullptr };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    (void) snapshot;

    ApplyRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::SetShown
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::SetShown (const Shown & shown)
{
    m_shown = shown;
    SetColumns();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::AddColumnMenuItems
//
//  The menu on the headings: the columns, each checked while visible
//  (FR-118). Name is always visible, so it is listed checked and disabled,
//  as in Visual Studio.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointsPane::AddColumnMenuItems (DxuiListView * list, std::vector<std::shared_ptr<DxuiCommand>> & commands)
{
    if (list != m_list)
    {
        return false;
    }

    commands.push_back (DebuggerMenuCommand::Make (BreakpointColumns::GetHeading (BreakpointColumns::Column::Name), true, [] {}));
    commands.back()->isEnabled = [] { return false; };

    for (size_t i = 1; i < BreakpointColumns::kCount; i++)
    {
        commands.push_back (DebuggerMenuCommand::Make (BreakpointColumns::GetHeading ((BreakpointColumns::Column) i), m_shown[i],
                                                       [this, i] { ToggleColumn ((BreakpointColumns::Column) i); }));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::AddMenuItems
//
//  Go to source code for one set from source or at an address with a source
//  line, and Go to disassembly for one at an address, as a double-click does
//  (FR-119).
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointsPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    const DebuggerViewSnapshot                  * snapshot = m_host.GetLatestSnapshot();
    const DebuggerViewSnapshot::BreakpointLine  * found    = GetBreakpointOfRow (row);
    DebuggerViewSnapshot::BreakpointLine          bp;
    int                                           fileId   = -1;
    int                                           line     = 0;



    if (list != m_list)
    {
        return false;
    }

    if (found == nullptr)
    {
        return true;
    }

    bp = *found;

    if (BreakpointColumns::TryGetSourcePlace (*snapshot, bp, fileId, line))
    {
        items.push_back ({ L"Go to source code", [this, fileId, line] { m_host.OpenSource (fileId, line); } });
    }

    if (BreakpointColumns::HasAddress (bp.info))
    {
        items.push_back ({ L"Go to disassembly", [this, bp] { m_host.GoToCode (bp.address); } });
    }

    items.push_back ({ bp.enabled ? L"Disable" : L"Enable", [this, bp] { RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEnableBreakpoint (bp.id, !bp.enabled, m_host.GetMode()) }, {} }); } });
    items.push_back ({ L"Rename",                           [this, row] { BeginNameEdit (row); } });
    items.push_back ({ L"Remove",                           [this, bp] { RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetClearBreakpoint (bp.id, m_host.GetMode()) }, {} }); } });

    //  Its type and the fields of that type, in a dialog; the result is
    //  the definition BPEDIT takes, as a person could have typed it
    //  (FR-094).
    items.push_back ({ L"Edit...", [this, bp]
    {
        std::optional<std::string>  definition = BreakpointDialog::RunModal (m_host.GetOwnerHwnd(), m_host.GetTheme(), bp.info);

        if (definition.has_value())
        {
            RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEditBreakpoint (bp.id, *definition, m_host.GetMode()) }, {} });
        }
    } });

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::IsBarEntryEnabled
//
//  A button that cannot act is disabled: nothing selected, nothing to undo,
//  no source line for the selection (FR-119).
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointsPane::IsBarEntryEnabled (int id) const
{
    const DebuggerViewSnapshot                         * snapshot = m_host.GetLatestSnapshot();
    std::vector<DebuggerViewSnapshot::BreakpointLine>    selected = GetSelected();
    bool                                                 any      = snapshot != nullptr && !snapshot->breakpoints.empty();
    bool                                                 one      = selected.size() == 1;
    int                                                  fileId   = -1;
    int                                                  line     = 0;



    switch (id)
    {
    case BreakpointBarCommands::kDelete:     return !selected.empty();
    case BreakpointBarCommands::kDeleteAll:  return any;
    case BreakpointBarCommands::kExport:     return any;
    case BreakpointBarCommands::kDisableAll: return any;
    case BreakpointBarCommands::kUndo:       return snapshot != nullptr && snapshot->canUndoBreakpoints;
    case BreakpointBarCommands::kRedo:       return snapshot != nullptr && snapshot->canRedoBreakpoints;
    case BreakpointBarCommands::kGoToSource: return one && snapshot != nullptr && BreakpointColumns::TryGetSourcePlace (*snapshot, selected[0], fileId, line);
    case BreakpointBarCommands::kGoToCode:   return one && (BreakpointColumns::HasAddress (selected[0].info) || BreakpointColumns::HasRoutine (selected[0].info));
    default:                                 return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::RunBarEntry
//
//  Delete all and Disable all are one action each, and Delete one for each
//  selected row. Disable all enables them all instead when none is enabled,
//  as Visual Studio's button does.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::RunBarEntry (int id)
{
    const DebuggerViewSnapshot                         * snapshot   = m_host.GetLatestSnapshot();
    std::vector<DebuggerViewSnapshot::BreakpointLine>    selected   = GetSelected();
    CommandMode                                          mode       = m_host.GetMode();
    BreakpointStep                                       step;
    int                                                  fileId     = -1;
    int                                                  line       = 0;
    bool                                                 anyEnabled = snapshot != nullptr && std::ranges::any_of (snapshot->breakpoints,
                                                                          [] (const DebuggerViewSnapshot::BreakpointLine & bp) { return bp.enabled; });



    if (!IsBarEntryEnabled (id))
    {
        return;
    }

    switch (id)
    {
    case BreakpointBarCommands::kDelete:
        for (const DebuggerViewSnapshot::BreakpointLine & bp : selected)
        {
            step.actions.push_back (DebuggerActions::GetClearBreakpoint (bp.id, mode));
        }

        RunStep (std::move (step));
        break;

    case BreakpointBarCommands::kDeleteAll:  RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetClearAllBreakpoints  (mode) },              {} }); break;
    case BreakpointBarCommands::kDisableAll: RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEnableAllBreakpoints (!anyEnabled, mode) }, {} }); break;
    case BreakpointBarCommands::kUndo:       RunStep ({ BreakpointStep::Kind::Undo,    {}, {} });                                                      break;
    case BreakpointBarCommands::kRedo:       RunStep ({ BreakpointStep::Kind::Redo,    {}, {} });                                                      break;
    case BreakpointBarCommands::kExport:     Export();                                                                                                 break;
    case BreakpointBarCommands::kImport:     Import();                                                                                                 break;

    case BreakpointBarCommands::kGoToSource:
        if (snapshot != nullptr && BreakpointColumns::TryGetSourcePlace (*snapshot, selected[0], fileId, line))
        {
            m_host.OpenSource (fileId, line);
        }

        break;

    case BreakpointBarCommands::kGoToCode:
        m_host.GoToCode (selected[0].address);
        break;

    default:
        //  New is a drop-down; its rows act.
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::SetBarMenus
//
//  New's types under their headings, each opening the breakpoint dialog on
//  that type. Built again before the drop-down opens.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::SetBarMenus()
{
    std::vector<DxuiPopupMenuItem>  items;



    for (const BreakpointDialog::TypeRow & row : BreakpointDialog::GetTypeRows())
    {
        BreakpointDialog::Type  type = row.type.value_or (BreakpointDialog::Type::Execute);

        if (!row.type.has_value())
        {
            items.push_back (DxuiPopupMenuItem::ForHeader (row.label));
            continue;
        }

        items.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (std::wstring (row.label) + L"...", false, [this, type] { NewBreakpoint (type); })));
    }

    m_bar.GetBar()->SetDropDownItems (BreakpointBarCommands::kNew, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::NewBreakpoint
//
//  The breakpoint dialog for the type chosen, an execution address at the
//  PC; its definition is the line a person could have typed.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::NewBreakpoint (BreakpointDialog::Type type)
{
    const DebuggerViewSnapshot   * snapshot   = m_host.GetLatestSnapshot();
    std::optional<std::string>     definition;



    definition = BreakpointDialog::RunNew (m_host.GetOwnerHwnd(), m_host.GetTheme(), type, (snapshot != nullptr) ? snapshot->pc : (Word) 0);

    if (definition.has_value())
    {
        RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetDefineBreakpoint (*definition, m_host.GetMode()) }, {} });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::RunStep
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::RunStep (BreakpointStep step)
{
    m_host.RunAction (DebuggerActions::GetBreakpointStep (std::move (step)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetSelected
//
//  Every selected row's breakpoint, or the one row the keyboard is on.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerViewSnapshot::BreakpointLine> BreakpointsPane::GetSelected() const
{
    std::vector<DebuggerViewSnapshot::BreakpointLine>  selected;
    std::vector<int>                                   rows;



    if (m_list == nullptr)
    {
        return selected;
    }

    rows = m_list->GetSelectedRows();

    if (rows.empty() && m_list->GetSelectedRow() >= 0)
    {
        rows.push_back (m_list->GetSelectedRow());
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
//  BreakpointsPane::Export
//
//  BPSAVE to a file the user chooses (FR-121), in AppleWin's words whatever
//  the console's dialect.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::Export()
{
    HRESULT                hr      = S_OK;
    IHostDialogs         * dialogs = m_host.GetHostDialogs();
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked  = false;



    BAIL_OUT_IF (dialogs == nullptr, S_OK);

    spec.filters          = { { L"Breakpoint files", L"*.txt" }, { L"All files", L"*.*" } };
    spec.defaultExtension = L"txt";
    spec.defaultFileName  = L"Breakpoints.txt";

    hr = dialogs->PickFileToSave (m_host.GetOwnerHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    m_host.RunAction (DebuggerActions::GetSaveBreakpoints (TextEncoding::WideToNarrow (chosen.wstring()), m_host.GetMode()));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::Import
//
//  The breakpoints of a file the user chooses, added to those already set as
//  one undo step (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::Import()
{
    HRESULT                hr      = S_OK;
    IHostDialogs         * dialogs = m_host.GetHostDialogs();
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked  = false;



    BAIL_OUT_IF (dialogs == nullptr, S_OK);

    spec.filters = { { L"Breakpoint files", L"*.txt" }, { L"All files", L"*.*" } };

    hr = dialogs->PickFileToOpen (m_host.GetOwnerHwnd(), spec, chosen, picked);
    CHR (hr);

    BAIL_OUT_IF (!picked, S_OK);

    RunStep ({ BreakpointStep::Kind::Import, {}, TextEncoding::WideToNarrow (chosen.wstring()) });

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::BeginNameEdit
//
//  A box over the Name cell with the name the row has, all of it selected
//  so typing replaces it, as Visual Studio's rename opens.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::BeginNameEdit (int row)
{
    const DebuggerViewSnapshot::BreakpointLine  * bp   = GetBreakpointOfRow (row);
    RECT                                          cell = {};
    RECT                                          list = m_list->GetBounds();
    std::wstring                                  text;



    if (bp == nullptr || !m_list->GetCellTextRectPx (row, 0, cell))
    {
        return;
    }

    text = TextEncoding::NarrowToWide (BreakpointColumns::GetCells (*m_host.GetLatestSnapshot(), *bp)[(size_t) BreakpointColumns::Column::Name]);

    OffsetRect (&cell, list.left, list.top);

    m_nameEdit = NameEdit { row, bp->id, text };

    m_nameEditor->SetTextRenderer (m_host.GetPaneTextRenderer());
    m_nameEditor->SetFont         (DxuiTheme::kMonoFace, m_list->GetFontSizeDip());
    m_nameEditor->SetText         (text);
    m_nameEditor->Layout          (cell, m_host.GetPaneScaler());
    m_nameEditor->SetVisible      (true);
    m_host.FocusControl           (m_nameEditor);
    m_nameEditor->SelectAll();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::EndNameEdit
//
//  Enter or a click away commits what was typed, as one step the pane can
//  undo; Escape, or the name as it opened, changes nothing. An emptied box
//  returns the breakpoint to its default name.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::EndNameEdit (bool commit)
{
    std::string  typed = TextEncoding::WideToNarrow (m_nameEditor->GetText());
    int          id    = m_nameEdit.id;
    bool         isNew = commit && m_nameEditor->GetText() != m_nameEdit.opened;



    if (m_nameEdit.row < 0)
    {
        return;
    }

    typed.erase (0, std::min (typed.size(), typed.find_first_not_of (" \t")));
    typed.erase (typed.find_last_not_of (" \t") + 1);

    m_nameEdit = NameEdit {};
    m_nameEditor->SetVisible (false);
    m_host.FocusControl      (m_list);
    m_host.Repaint();

    if (isNew)
    {
        RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetNameBreakpoint (id, typed, m_host.GetMode()) }, {} });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::RouteEditMouse
//
//  The rename box takes the pointer while it is over the box; a press or a
//  wheel anywhere else commits the edit and passes the event on.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointsPane::RouteEditMouse (const DxuiMouseEvent & ev)
{
    if (m_nameEdit.row < 0)
    {
        return false;
    }

    if (DxuiDockSite::Contains (m_nameEditor->GetBounds(), ev.positionDip))
    {
        m_nameEditor->OnMouse (ev);
        m_host.Repaint();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel)
    {
        EndNameEdit (true);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::RouteEditKey
//
//  The rename box takes every key: Enter commits what was typed, Escape
//  cancels the rename. F2 on the list renames the selected breakpoint, as in
//  Visual Studio.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointsPane::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    if (m_nameEdit.row >= 0)
    {
        if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE))
        {
            EndNameEdit (ev.vk == VK_RETURN);
        }
        else
        {
            m_nameEditor->OnKey (ev);
        }

        m_host.Repaint();
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && focused == m_list && !ev.ctrl && !ev.alt && ev.vk == VK_F2)
    {
        BeginNameEdit (m_list->GetSelectedRow());
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::SetColumns
//
//  Every column of FR-117, each visible or hidden as chosen; Name is always
//  visible.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::SetColumns()
{
    std::vector<DxuiListView::Column>  columns;



    for (size_t i = 0; i < BreakpointColumns::kCount; i++)
    {
        DxuiListView::Column  column;

        column.title   = BreakpointColumns::GetHeading ((BreakpointColumns::Column) i);
        column.visible = i == (size_t) BreakpointColumns::Column::Name || m_shown[i];
        columns.push_back (column);
    }

    m_list->SetColumns (columns);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::ApplyRows
//
//  A checkbox and the code views' gutter mark ahead of the name, then a
//  cell for each column, in the order the chosen heading sorts them.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::ApplyRows()
{
    const DebuggerViewSnapshot                    * snapshot = m_host.GetLatestSnapshot();
    std::vector<std::vector<DxuiListView::Cell>>    rows;



    if (snapshot == nullptr)
    {
        return;
    }

    if (m_sort >= 0 && m_sort < (int) BreakpointColumns::kCount)
    {
        m_order = BreakpointColumns::GetOrder (*snapshot, (BreakpointColumns::Column) m_sort, m_isReverse);
        m_list->SetSortIndicator (m_sort, m_isReverse);
    }
    else
    {
        m_order.resize (snapshot->breakpoints.size());

        for (size_t i = 0; i < m_order.size(); i++)
        {
            m_order[i] = i;
        }
    }

    for (size_t index : m_order)
    {
        const DebuggerViewSnapshot::BreakpointLine  & bp    = snapshot->breakpoints[index];
        BreakpointColumns::Cells                      cells = BreakpointColumns::GetCells (*snapshot, bp);
        std::vector<DxuiListView::Cell>               row;

        for (const std::string & text : cells)
        {
            DxuiListView::Cell  cell;

            cell.text = TextEncoding::NarrowToWide (text);
            cell.dim  = !bp.enabled;
            row.push_back (cell);
        }

        row[(size_t) BreakpointColumns::Column::Name].check = bp.enabled;
        row[(size_t) BreakpointColumns::Column::Name].icon  = m_host.GetBreakpointIcon (bp.enabled);
        rows.push_back (std::move (row));
    }

    m_list->SetRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetBreakpointOfRow
//
////////////////////////////////////////////////////////////////////////////////

const DebuggerViewSnapshot::BreakpointLine * BreakpointsPane::GetBreakpointOfRow (int row) const
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();



    if (snapshot == nullptr || row < 0 || row >= (int) m_order.size())
    {
        return nullptr;
    }

    if (m_order[(size_t) row] >= snapshot->breakpoints.size())
    {
        return nullptr;
    }

    return &snapshot->breakpoints[m_order[(size_t) row]];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::ToggleColumn
//
//  Turns a column on or off, and saves the choice with the open views.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::ToggleColumn (BreakpointColumns::Column column)
{
    if (column == BreakpointColumns::Column::Name || column >= BreakpointColumns::Column::Count)
    {
        return;
    }

    m_shown[(size_t) column] = !m_shown[(size_t) column];
    SetColumns();
    ApplyRows();

    if (m_host.GetLatestSnapshot() != nullptr)
    {
        m_host.SaveOpenViews();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::Sort
//
//  A click on a heading sorts by that column, and a second click on the
//  same one turns the order around.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::Sort (BreakpointColumns::Column column)
{
    m_isReverse = ((int) column == m_sort) && !m_isReverse;
    m_sort      = (int) column;
    ApplyRows();
}
