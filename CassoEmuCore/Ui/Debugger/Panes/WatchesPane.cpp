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
//  The ID column has "Auto" for what the instruction touches, a watch's
//  number, or ZPn for a zero-page pointer, with the check that enables or
//  disables a watch or pointer. A click on a watch's cell edits it in place:
//  the expression, then the value, which runs to the pane's edge, as Visual
//  Studio's does. A double-click on the BASIC variables heading expands or
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

    m_grid->SetGridColumns ({ { { L"ID",    0, false, DxuiTextHAlign::Left }, false, false },
                              { { L"Watch", 0, false, DxuiTextHAlign::Left }, true,  true  },
                              { { L"Value", 0, true,  DxuiTextHAlign::Left }, true,  false } });

    m_grid->SetAddRowText      (L"Add new watch");
    m_grid->SetAddsOnFirstField (true);
    m_grid->SetEditorHwnd      (m_host.GetOwnerHwnd());
    m_grid->SetEditorMaxLength (kEditMaxChars);
    m_grid->SetOnCommitCell    ([this] (int row, int column, const std::wstring & text, std::wstring & outError) { return CommitCell (row, column - kFirstFieldColumn, text, outError); });
    m_grid->SetOnAddRow        ([this] (const std::vector<std::wstring> & texts, std::wstring & outError) { return AddWatch (texts[kFirstFieldColumn], outError); });
    m_grid->SetOnError         ([this] (const std::wstring & text) { SetError (text); });
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
    EditAddedValue (snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::EditAddedValue
//
//  After a Tab added a watch, its value is edited once a snapshot lists it:
//  the watch whose id was not there before the add. An add that failed in
//  the engine lists no watch, so past kAddedWaitMs the edit is dropped; a
//  watch added later, from anywhere, is not edited for it.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::EditAddedValue (const DebuggerViewSnapshot & snapshot)
{
    if (m_isValueEditAfterAdd && m_clock() > m_addedBy)
    {
        m_isValueEditAfterAdd = false;
    }

    if (!m_isValueEditAfterAdd)
    {
        return;
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
    {
        if (std::ranges::find (m_idsBeforeAdd, watch.id) != m_idsBeforeAdd.end())
        {
            continue;
        }

        m_isValueEditAfterAdd = false;

        for (size_t row = 0; row < m_rows.size(); row++)
        {
            if (m_rows[row].kind == RowKind::Manual && m_rows[row].index == watch.id)
            {
                BeginEdit ((int) row, 1);
                break;
            }
        }

        return;
    }
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
    std::vector<BasicAutoWatches::Row>              basicAutos  = isInProgram ? m_basicAutos.Update (*snapshot.basic, snapshot.cycles) : std::vector<BasicAutoWatches::Row>();



    m_rows.clear();

    if (!snapshot.autoWatches.empty() || !basicAutos.empty())
    {
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
            rows.push_back ({ { MakeIdCell (L"Auto"), name, value }, {}, false, { true, true, false } });
            m_rows.push_back ({ RowKind::Automatic, (int) i });
        }

        //  The BASIC variables the statement being run uses, then those the
        //  statement just run used, in italics as what the previous
        //  instruction touched is.
        for (const BasicAutoWatches::Row & basicAuto : basicAutos)
        {
            DxuiListView::Cell  name  = BasicWatches::MakeNameCell (basicAuto.item.name);
            DxuiListView::Cell  value = { TextEncoding::NarrowToWide (basicAuto.item.value) };

            name.weight  = basicAuto.isPrevious ? DxuiFontWeight::Italic : DxuiFontWeight::Normal;
            value.weight = name.weight;
            name.tip     = basicAuto.isPrevious ? ColorLegend::GetText (ColorLegend::Meaning::PreviousStatement) : L"";

            if (basicAuto.isChanged)
            {
                value.argb = changed;
                value.tip  = ColorLegend::GetText (ColorLegend::Meaning::Changed);
            }

            rows.push_back ({ { MakeIdCell (L"Auto"), name, value }, {}, true });
            m_rows.push_back ({ RowKind::BasicItem, 0 });
        }
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
    {
        DxuiListView::Cell  id    = MakeIdCell (std::to_wstring (watch.id));
        DxuiListView::Cell  name  = !watch.basicName.empty()  ? BasicWatches::MakeNameCell (watch.basicName)
                                  : !watch.expression.empty() ? DxuiListView::Cell { TextEncoding::NarrowToWide (watch.expression) }
                                  :                             DxuiListView::Cell { std::format (L"${:04X}", watch.address) };
        DxuiListView::Cell  value = { TextEncoding::NarrowToWide (watch.value) };

        //  A watch the console disabled is dimmed, as a disabled breakpoint
        //  is; an enabled one's address is in the color the code pane draws
        //  addresses in. The grid has no check box: a watch costs nothing
        //  noticeable to update.
        id.dim    = !watch.enabled;
        name.dim  = !watch.enabled;
        value.dim = !watch.enabled;
        name.tip  = watch.enabled ? L"" : ColorLegend::GetText (ColorLegend::Meaning::DisabledWatch);

        if (watch.enabled && watch.basicName.empty())
        {
            name.colorRanges = DebuggerTextColors::GetOperandRanges (name.text, m_host.GetTextColors());
        }

        if (changes.IsChanged (std::format ("W:{}", watch.id)))
        {
            value.argb = changed;
        }

        //  A symbol not yet loaded, or an expression that does not evaluate,
        //  has why in place of a value.
        if (!watch.error.empty())
        {
            value.text = TextEncoding::NarrowToWide (watch.error);
            value.argb = (m_host.GetTheme() != nullptr) ? m_host.GetTheme()->ErrorForeground() : 0;
        }

        value.tip = ColorLegend::JoinLines ({ watch.enabled ? L"" : ColorLegend::GetText (ColorLegend::Meaning::DisabledWatch),
                                              value.argb == 0 ? L"" : ColorLegend::GetText (ColorLegend::Meaning::Changed) });

        //  The expression edits as its address or BASIC name; a BASIC watch's
        //  value is the program's to change.
        rows.push_back ({ { id, name, value },
                          { std::wstring(),
                            !watch.basicName.empty()  ? TextEncoding::NarrowToWide (watch.basicName)
                            : !watch.expression.empty() ? TextEncoding::NarrowToWide (watch.expression)
                            :                             std::format (L"{:04X}", watch.address),
                            TextEncoding::NarrowToWide (watch.value) },
                          false, { true, false, !watch.basicName.empty() || !watch.isValueEditable || !watch.error.empty() } });
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
            DxuiListView::Cell  id    = MakeIdCell (std::format (L"ZP{}", pointer.id));
            DxuiListView::Cell  name  = { std::format (L"${:02X}", pointer.address) };
            DxuiListView::Cell  value = { TextEncoding::NarrowToWide (pointer.value) };

            id.dim    = !pointer.enabled;
            name.dim  = !pointer.enabled;
            value.dim = !pointer.enabled;
            rows.push_back ({ { id, name, value }, {}, true });
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
            rows.push_back ({ { DxuiListView::Cell { L"" }, BasicWatches::MakeNameCell (item.name), { TextEncoding::NarrowToWide (item.value) } }, {}, true });
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
    label.spansRow   = true;
    blank.background = fill;

    return { label, blank, blank };
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::MakeIdCell
//
//  The ID column's text in the muted color, so the watch itself reads first.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell WatchesPane::MakeIdCell (const std::wstring & text) const
{
    DxuiListView::Cell  cell = { text };



    cell.argb = (m_host.GetTheme() != nullptr) ? m_host.GetTheme()->ForegroundMuted() : 0;
    return cell;
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

    if (m_grid->BeginEdit (row, column + kFirstFieldColumn))
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

bool WatchesPane::CommitCell (int row, int column, const std::wstring & typed, std::wstring & outError)
{
    const DebuggerViewSnapshot                  * snapshot = m_host.GetLatestSnapshot();
    Edit                                          edit     = m_edit;
    std::optional<int>                            watchId;
    std::optional<int>                            autoAt;
    std::optional<DebuggerViewState::WatchUndo>   undo;
    std::vector<DebuggerAction>                   actions;
    std::string                                   error;



    (void) row;

    //  For an expression that cannot be watched, the edit stays open and the
    //  watch it would replace remains.
    if (snapshot != nullptr && edit.isOpen && column == 0 && edit.what.kind == RowKind::Manual)
    {
        error = DebuggerViewState::GetWatchError (*snapshot, TextEncoding::WideToNarrow (typed));

        if (!error.empty())
        {
            outError = TextEncoding::NarrowToWide (error);
            return false;
        }
    }

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

bool WatchesPane::AddWatch (const std::wstring & typed, std::wstring & outError)
{
    const DebuggerViewSnapshot  * snapshot   = m_host.GetLatestSnapshot();
    std::string                   expression = TextEncoding::WideToNarrow (typed);
    std::string                   error;



    expression.erase (0, std::min (expression.size(), expression.find_first_not_of (" \t")));
    expression.erase (expression.find_last_not_of (" \t") + 1);

    if (expression.empty())
    {
        return true;
    }

    error = (snapshot != nullptr) ? DebuggerViewState::GetWatchError (*snapshot, expression) : std::string();

    if (!error.empty())
    {
        outError = TextEncoding::NarrowToWide (error);
        return false;
    }

    //  A Tab goes on to the new watch's value once a snapshot lists it.
    m_idsBeforeAdd.clear();

    for (const DebuggerViewSnapshot::WatchLine & watch : (snapshot != nullptr) ? snapshot->watches : std::vector<DebuggerViewSnapshot::WatchLine>())
    {
        m_idsBeforeAdd.push_back (watch.id);
    }

    m_isValueEditAfterAdd = m_grid->IsTabCommit();
    m_addedBy             = m_clock() + kAddedWaitMs;
    SetError ({});
    m_host.RunAction (DebuggerActions::GetAddWatch (expression, m_host.GetMode()));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::SetError
//
//  The reason an entry cannot be watched, in the add row's Value cell.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::SetError (const std::wstring & text)
{
    uint32_t  error = (m_host.GetTheme() != nullptr) ? m_host.GetTheme()->ErrorForeground() : 0;



    m_grid->SetAddRowNote (kFirstFieldColumn + 1, text, error);
    m_host.Repaint();
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
