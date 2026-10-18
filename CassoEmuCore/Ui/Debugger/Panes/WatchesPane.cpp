#include "Pch.h"

#include "Ui/Debugger/Panes/WatchesPane.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::WatchesPane
//
////////////////////////////////////////////////////////////////////////////////

WatchesPane::WatchesPane (IDebuggerPaneHost & host) :
    UndoListPane (host, DebuggerLayout::kWatches, L"Watches", ColorLegend::Pane::Watch)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::~WatchesPane
//
////////////////////////////////////////////////////////////////////////////////

WatchesPane::~WatchesPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::Configure
//
//  A click on a watch's cell edits it in place: the expression on the left,
//  the value on the right. A watch's or a zero-page pointer's check enables
//  or disables it. The value runs to the pane's edge, as Visual Studio's
//  does, so the Automatic and Watches headings span the pane rather than the
//  columns. A double-click on the BASIC variables heading expands or
//  collapses it.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::Configure()
{
    UndoListPane::Configure();

    m_list->SetOnActivateRow ([this] (int row)
    {
        const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();

        if (snapshot != nullptr && row >= 0 && row < (int) m_rows.size() && m_rows[(size_t) row].kind == RowKind::BasicHeading)
        {
            m_isBasicVariablesExpanded = !m_isBasicVariablesExpanded;
            ApplyRows (*snapshot);
        }
    });

    m_list->SetOnCheckToggled ([this] (int row, size_t, bool checked)
    {
        if (row < 0 || row >= (int) m_rows.size())
        {
            return;
        }

        if (m_rows[(size_t) row].kind == RowKind::Manual)
        {
            m_host.RunAction (DebuggerActions::GetEnableWatch (m_rows[(size_t) row].index, checked, m_host.GetMode()));
        }
        else if (m_rows[(size_t) row].kind == RowKind::ZeroPage)
        {
            m_host.RunAction (DebuggerActions::GetEnableZeroPagePointer (m_rows[(size_t) row].index, checked, m_host.GetMode()));
        }
    });

    m_grid->SetGridColumns ({ { { L"Watch", 0, false, DxuiTextHAlign::Left }, true, true },
                              { { L"Value", 0, true,  DxuiTextHAlign::Left }, true, false } });

    m_grid->SetAddRowText      (L"Add new watch");
    m_grid->SetEditorHwnd      (m_host.GetOwnerHwnd());
    m_grid->SetEditorMaxLength (kEditMaxChars);
    m_grid->SetOnCommitCell    ([this] (int row, int column, const std::wstring & text, std::wstring &) { return CommitCell (row, column, text); });
    m_grid->SetOnAddRow        ([this] (const std::vector<std::wstring> & texts, std::wstring &) { return AddWatch (texts[0]); });
    m_grid->SetOnDeleteRow     ([this] (int) { RemoveSelected(); });
    m_grid->SetOnBeginEdit     ([this] (int row, int) { NoteEditBegun (row); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::CreateList
//
//  An editable grid, its rows fixed in place, since the watches are in the
//  engine's order.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView * WatchesPane::CreateList()
{
    m_grid = m_host.GetControlParent().CreateChild<DxuiEditableGrid>();
    m_grid->SetRowsMovable (false);

    return m_grid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> WatchesPane::GetControls() const
{
    return { m_bar.GetSlot(), m_list, m_bar.GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::Apply
//
//  The rows, then the undo history's view of the snapshot, which finds a
//  moved watch again by its new id.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    ApplyRows (snapshot);
    m_history.OnSnapshot (snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::ApplyRows
//
//  A value that changed since the last stop is drawn in the changed color;
//  one the instruction touched and left as it was is shown plainly, since it
//  was an input or an output all the same.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::ApplyRows (const DebuggerViewSnapshot & snapshot)
{
    const StopChanges                             & changes     = m_host.GetStopChanges();
    uint32_t                                        changed     = m_host.GetTextColors().changed;
    std::vector<DxuiEditableGrid::GridRow>          rows;
    bool                                            isInProgram = BasicWatches::IsStoppedInProgram (snapshot.basic);
    std::vector<BasicWatches::Item>                 basicAutos  = isInProgram ? BasicWatches::GetStatementItems (*snapshot.basic) : std::vector<BasicWatches::Item>();



    m_rows.clear();

    if (!snapshot.autoWatches.empty() || !basicAutos.empty())
    {
        rows.push_back ({ MakeHeading (L"Automatic"), {}, true });
        m_rows.push_back ({ RowKind::Heading, 0 });

        for (size_t i = 0; i < snapshot.autoWatches.size(); i++)
        {
            const DebuggerViewSnapshot::AutoWatchLine & line  = snapshot.autoWatches[i];
            DxuiListView::Cell                          name  = { TextEncoding::NarrowToWide (line.label) };
            DxuiListView::Cell                          value = { TextEncoding::NarrowToWide (line.value) };

            //  What the previous instruction touched is in italics, apart
            //  from a disabled watch's muted text.
            name.weight  = line.isPrevious ? DxuiFontWeight::Italic : DxuiFontWeight::Normal;
            value.weight = name.weight;
            name.tip     = line.isPrevious ? ColorLegend::GetText (ColorLegend::Meaning::PreviousAutoWatch) : L"";

            //  A memory watch's symbol and address in the colors the code
            //  pane draws them in.
            if (line.key.starts_with ("M:"))
            {
                name.colorRanges = DebuggerTextColors::GetOperandRanges (name.text, m_host.GetTextColors());
            }

            if (changes.IsChanged ("A:" + line.key))
            {
                value.argb = changed;
                value.tip  = ColorLegend::GetText (ColorLegend::Meaning::Changed);
            }

            //  What the instruction touches is not the user's to change, so
            //  only the value edits.
            rows.push_back ({ { name, value }, {}, false, { true, false } });
            m_rows.push_back ({ RowKind::Automatic, (int) i });
        }

        //  The BASIC variables the statement being run uses.
        for (const BasicWatches::Item & item : basicAutos)
        {
            rows.push_back ({ { BasicWatches::MakeNameCell (item.name), { TextEncoding::NarrowToWide (item.value) } }, {}, true });
            m_rows.push_back ({ RowKind::BasicItem, 0 });
        }

        rows.push_back ({ MakeHeading (L"Watches"), {}, true });
        m_rows.push_back ({ RowKind::Heading, 0 });
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
    {
        DxuiListView::Cell  name  = watch.basicName.empty() ? DxuiListView::Cell { std::format (L"#{} ${:04X}", watch.id, watch.address) }
                                                            : BasicWatches::MakeNameCell (watch.basicName);
        DxuiListView::Cell  value = { TextEncoding::NarrowToWide (watch.value) };

        //  A disabled watch is dimmed, as a disabled breakpoint is; an enabled
        //  one's address is in the color the code pane draws addresses in.
        name.dim  = !watch.enabled;
        value.dim = !watch.enabled;
        name.tip  = watch.enabled ? L"" : ColorLegend::GetText (ColorLegend::Meaning::DisabledWatch);

        if (watch.enabled && watch.basicName.empty())
        {
            name.colorRanges = DebuggerTextColors::GetOperandRanges (name.text, m_host.GetTextColors());

            //  The #n before it is the watch's number, not an immediate.
            std::erase_if (name.colorRanges, [&name] (const auto & range) { return std::get<0> (range) < (int) name.text.find (L' '); });
        }

        if (changes.IsChanged (std::format ("W:{}", watch.id)))
        {
            value.argb = changed;
        }

        value.tip = ColorLegend::JoinLines ({ watch.enabled ? L"" : ColorLegend::GetText (ColorLegend::Meaning::DisabledWatch),
                                              value.argb == 0 ? L"" : ColorLegend::GetText (ColorLegend::Meaning::Changed) });

        //  The expression edits as its address or BASIC name; a BASIC watch's
        //  value is the program's to change.
        name.check = watch.enabled;
        rows.push_back ({ { name, value },
                          { watch.basicName.empty() ? std::format (L"{:04X}", watch.address) : TextEncoding::NarrowToWide (watch.basicName),
                            TextEncoding::NarrowToWide (watch.value) },
                          false, { false, !watch.basicName.empty() } });
        m_rows.push_back ({ RowKind::Manual, watch.id });
    }

    //  The zero-page pointers under their own heading: each the zero-page
    //  address, then the address its two bytes form and the bytes there.
    if (!snapshot.zeroPagePointers.empty())
    {
        rows.push_back ({ MakeHeading (L"Zero-page pointers"), {}, true });
        m_rows.push_back ({ RowKind::Heading, 0 });

        for (const DebuggerViewSnapshot::WatchLine & pointer : snapshot.zeroPagePointers)
        {
            DxuiListView::Cell  name  = { std::format (L"ZP{} ${:02X}", pointer.id, pointer.address) };
            DxuiListView::Cell  value = { TextEncoding::NarrowToWide (pointer.value) };

            name.check = pointer.enabled;
            name.dim   = !pointer.enabled;
            value.dim  = !pointer.enabled;
            rows.push_back ({ { name, value }, {}, true });
            m_rows.push_back ({ RowKind::ZeroPage, pointer.id });
        }
    }

    //  Every BASIC variable and array element, under a heading a click
    //  expands or collapses.
    if (isInProgram)
    {
        rows.push_back ({ MakeHeading (std::format (L"{} BASIC variables", m_isBasicVariablesExpanded ? s_kpszTriangleDown : s_kpszTriangleRight)), {}, true });
        m_rows.push_back ({ RowKind::BasicHeading, 0 });

        for (const BasicWatches::Item & item : m_isBasicVariablesExpanded ? BasicWatches::GetAllItems (*snapshot.basic) : std::vector<BasicWatches::Item>())
        {
            rows.push_back ({ { BasicWatches::MakeNameCell (item.name), { TextEncoding::NarrowToWide (item.value) } }, {}, true });
            m_rows.push_back ({ RowKind::BasicItem, 0 });
        }
    }

    //  As in Visual Studio, the grid's last row adds a watch: an expression
    //  typed into it in place becomes one.
    m_grid->SetGridRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::MakeHeading
//
//  A row that divides the pane rather than listing a watch: its title in the
//  heading color and the proportional face of a list's column headings, on a
//  fill a little way from the pane's background toward the hover color,
//  enough to mark the row without a strong band.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> WatchesPane::MakeHeading (const std::wstring & title) const
{
    constexpr float     kTint = 0.35f;
    const DxuiTheme   * theme = m_host.GetTheme();
    DxuiListView::Cell  label = { title };
    DxuiListView::Cell  blank = { L"" };
    uint32_t            fill  = DxuiColor::Mix (theme->ContentBackground(), theme->HoverBackground(), kTint);



    label.argb       = theme->HeadingForeground();
    label.face       = DxuiTheme::kBodyFace;
    label.background = fill;
    blank.background = fill;

    return { label, blank };
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::BeginEdit
//
//  The add row has only an expression to type, wherever it was opened.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::BeginEdit (int row, int column)
{
    if (m_grid->IsAddRow (row))
    {
        column = 0;
    }

    if (m_grid->BeginEdit (row, column))
    {
        m_host.FocusControl (m_grid);
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::EndEdit
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::EndEdit (bool commit)
{
    (void) m_grid->EndEdit (commit);
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::CommitCell
//
//  The undo record is taken from the snapshot as it is, before the edit
//  runs.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchesPane::CommitCell (int row, int column, const std::wstring & typed)
{
    const DebuggerViewSnapshot                  * snapshot = m_host.GetLatestSnapshot();
    Edit                                          edit     = m_edit;
    std::optional<int>                            watchId;
    std::optional<int>                            autoAt;
    std::optional<DebuggerViewState::WatchUndo>   undo;
    std::vector<DebuggerAction>                   actions;



    (void) row;
    m_edit = Edit {};

    if (snapshot == nullptr || !edit.isOpen)
    {
        return true;
    }

    if (edit.what.kind == RowKind::Manual)
    {
        watchId = edit.what.index;
    }

    for (size_t i = 0; edit.what.kind == RowKind::Automatic && i < snapshot->autoWatches.size(); i++)
    {
        if (snapshot->autoWatches[i].key == edit.autoKey)
        {
            autoAt = (int) i;
            break;
        }
    }

    //  An automatic watch no longer listed takes no edit.
    if (!watchId.has_value() && !autoAt.has_value())
    {
        return true;
    }

    actions = DebuggerViewState::GetWatchEditActions (*snapshot, watchId, autoAt, column, TextEncoding::WideToNarrow (typed), m_host.GetMode());
    undo    = actions.empty() ? std::nullopt : DebuggerViewState::GetWatchUndo (*snapshot, watchId, autoAt, column, m_host.GetMode());

    if (undo.has_value())
    {
        m_history.Record (std::move (*undo), actions, WatchHistory::GetEditText (*snapshot, watchId, autoAt, column));
    }

    for (const DebuggerAction & action : actions)
    {
        m_host.RunAction (action);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::NoteEditBegun
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::NoteEditBegun (int row)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();



    m_edit = Edit {};

    if (snapshot == nullptr || row < 0 || row >= (int) m_rows.size())
    {
        return;
    }

    m_edit.what   = m_rows[(size_t) row];
    m_edit.isOpen = true;

    if (m_edit.what.kind == RowKind::Automatic && m_edit.what.index < (int) snapshot->autoWatches.size())
    {
        m_edit.autoKey = snapshot->autoWatches[(size_t) m_edit.what.index].key;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::AddWatch
//
//  The expression typed into the add row, less the spaces around it.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchesPane::AddWatch (const std::wstring & typed)
{
    std::string  expression = TextEncoding::WideToNarrow (typed);



    expression.erase (0, std::min (expression.size(), expression.find_first_not_of (" \t")));
    expression.erase (expression.find_last_not_of (" \t") + 1);

    if (!expression.empty())
    {
        m_host.RunAction (DebuggerActions::GetAddWatch (expression, m_host.GetMode()));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::RemoveSelected
//
//  Delete on a manual watch removes it. Automatic watches come and go with
//  the instruction, so there is nothing of theirs to remove.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::RemoveSelected()
{
    int  row = m_list->GetSelectedRow();



    if (row >= 0 && row < (int) m_rows.size() && m_rows[(size_t) row].kind == RowKind::Manual)
    {
        m_host.RunAction (DebuggerActions::GetClearWatch (m_rows[(size_t) row].index, m_host.GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::RouteEditKey
//
//  The grid has every key while a watch is edited. In the list, Delete
//  removes a manual watch and F2 edits the selected one's value, as in
//  Visual Studio's watch window; Ctrl+Z undoes the pane's own last edit, and
//  nothing of any memory window's.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchesPane::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    if (focused != m_list)
    {
        return false;
    }

    if (m_grid->IsEditing())
    {
        (void) m_grid->OnKey (ev);
        m_host.Repaint();
        return true;
    }

    if (ev.kind != DxuiKeyEventKind::Down || ev.alt)
    {
        return false;
    }

    if (!ev.ctrl && ev.vk == VK_DELETE)
    {
        RemoveSelected();
        return true;
    }

    if (!ev.ctrl && ev.vk == VK_F2)
    {
        BeginEdit (m_list->GetSelectedRow(), 1);
        return true;
    }

    if (ev.ctrl && (ev.vk == 'Z' || ev.vk == 'Y'))
    {
        Undo (ev.vk == 'Y');
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::AddMenuItems
//
//  The row map indicates what the row is: the pane mixes headings, automatic
//  watches and the user's own, so a row number is not an index into any one
//  of them.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchesPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    Row                           what;



    if (list != m_list)
    {
        return false;
    }

    if (snapshot == nullptr || row < 0 || row >= (int) m_rows.size())
    {
        return true;
    }

    what = m_rows[(size_t) row];

    if (what.kind == RowKind::Manual)
    {
        auto  found = std::find_if (snapshot->watches.begin(), snapshot->watches.end(),
                                    [&what] (const DebuggerViewSnapshot::WatchLine & w) { return w.id == what.index; });

        if (found != snapshot->watches.end())
        {
            const DebuggerViewSnapshot::WatchLine  watch = *found;

            m_host.AddShowInMemoryItems (std::format (L"${:04X}", watch.address), std::format ("{:04X}", watch.address), items);
            items.push_back ({ L"Remove", [this, watch] { m_host.RunAction (DebuggerActions::GetClearWatch (watch.id, m_host.GetMode())); } });
        }
    }
    else if (what.kind == RowKind::ZeroPage)
    {
        items.push_back ({ L"Remove", [this, slot = what.index] { m_host.RunAction (DebuggerActions::GetClearZeroPagePointer (slot, m_host.GetMode())); } });
    }
    else if (what.kind == RowKind::Automatic && what.index < (int) snapshot->autoWatches.size() &&
             snapshot->autoWatches[(size_t) what.index].key.starts_with ("M:"))
    {
        std::string  hex = snapshot->autoWatches[(size_t) what.index].key.substr (2);

        m_host.AddShowInMemoryItems (L"$" + TextEncoding::NarrowToWide (hex), hex, items);
    }

    if (what.kind != RowKind::Heading && what.kind != RowKind::BasicHeading)
    {
        items.push_back ({ L"Copy", [this] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });
    }

    items.push_back ({ L"Add zero-page pointer...", [this] { m_host.AddZeroPagePointer(); } });
    AddUndoItems (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::CanUndo
//
////////////////////////////////////////////////////////////////////////////////

bool WatchesPane::CanUndo (bool redo) const
{
    return redo ? m_history.CanRedo() : m_history.CanUndo();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::GetUndoText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WatchesPane::GetUndoText (bool redo) const
{
    return redo ? m_history.GetRedoText() : m_history.GetUndoText();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::Undo
//
//  The last watch edit, put back, or with `redo` made again. A moved watch
//  is found by being the one whose id did not exist before the move, since
//  the engine numbered it only once the move ran.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::Undo (bool redo)
{
    const DebuggerViewSnapshot                  * snapshot = m_host.GetLatestSnapshot();
    std::optional<std::vector<DebuggerAction>>    actions;



    if (snapshot == nullptr)
    {
        return;
    }

    actions = redo ? m_history.TryRedo() : m_history.TryUndo (*snapshot, m_host.GetMode());

    if (!actions.has_value())
    {
        return;
    }

    for (const DebuggerAction & action : *actions)
    {
        m_host.RunAction (action);
    }
}
