#include "Pch.h"

#include "Ui/Debugger/Panes/WatchesPane.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/WatchTable.h"
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
//  WatchesPane::RouteContentMouse
//
//  A press on the BASIC variables heading expands or collapses it at once.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchesPane::RouteContentMouse (const DxuiMouseEvent & ev)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    RECT                          bounds   = m_list->GetBounds();
    int                           row      = -1;



    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left || snapshot == nullptr ||
        !m_list->IsVisible() || !m_host.IsRoutable (m_list) || !DxuiDockSite::Contains (bounds, ev.positionDip))
    {
        return false;
    }

    row = m_list->HitTestRow (ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top);

    if (row < 0 || row >= (int) m_rows.size() || m_rows[(size_t) row].kind != RowKind::BasicHeading)
    {
        return false;
    }

    m_isBasicVariablesExpanded = !m_isBasicVariablesExpanded;
    ApplyRows (*snapshot);
    m_host.Repaint();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::Configure
//
//  The ID column has "Auto" for what the instruction touches, a watch's
//  number, or ZPn for a zero-page pointer, with the check that enables or
//  disables a watch or pointer. A click on a watch's cell edits it in place:
//  the expression, then the value, which runs to the pane's edge, as Visual
//  Studio's does. A click on the BASIC variables heading, or Enter on it,
//  expands or collapses it.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::Configure()
{
    UndoListPane::Configure();

    m_list->SetOnActivateRow ([this] (int row)
    {
        const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();

        //  A double-click's press has toggled the heading already.
        bool  isMouse = DxuiInputState::IsKeyDown (VK_LBUTTON);



        if (!isMouse && snapshot != nullptr && row >= 0 && row < (int) m_rows.size() && m_rows[(size_t) row].kind == RowKind::BasicHeading)
        {
            m_isBasicVariablesExpanded = !m_isBasicVariablesExpanded;
            ApplyRows (*snapshot);
        }
    });

    //  Every column fits its text and the last takes the room left, so Type
    //  is beside the values rather than at the pane's far edge.
    m_grid->SetGridColumns ({ { { L"ID",    0, false, DxuiTextHAlign::Left }, false, false },
                              { { L"Watch", 0, false, DxuiTextHAlign::Left }, true,  true  },
                              { { L"Value", 0, false, DxuiTextHAlign::Left }, true,  false },
                              { { L"Type",  0, true,  DxuiTextHAlign::Left }, false, false } });

    m_grid->SetAddRowText      (L"Add new watch");
    m_grid->SetAddsOnFirstField (true);
    m_grid->SetReopensAddRow    (true);
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
    RecordAdded (snapshot);
    EditAddedValue (snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::RecordAdded
//
//  An add from the add row is an undoable step once a snapshot lists its
//  watch: the one whose id was not there before the add. Its undo removes
//  that watch, and its redo adds it back under the same id. An add that
//  failed in the engine doesn't list a watch, so past kAddedWaitMs the step
//  is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::RecordAdded (const DebuggerViewSnapshot & snapshot)
{
    DebuggerViewState::WatchUndo  undo;



    if (m_addedText.empty())
    {
        return;
    }

    if (m_clock() > m_addedBy)
    {
        m_addedText.clear();
        return;
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
    {
        if (std::ranges::find (m_idsBeforeAdd, watch.id) != m_idsBeforeAdd.end())
        {
            continue;
        }

        undo.actions = { DebuggerActions::GetClearWatch (watch.id, m_host.GetMode()) };
        m_history.Record (std::move (undo), { DebuggerActions::GetAddWatchAt (watch.id, m_addedText, m_host.GetMode()) }, std::format (L"add watch {}", watch.id));
        m_addedText.clear();
        return;
    }
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
    bool                                            isInBasic   = isInProgram && IsInRom (snapshot.pc);
    std::vector<BasicAutoWatches::Row>              basicAutos  = isInBasic ? m_basicAutos.Update (*snapshot.basic, snapshot.cycles) : std::vector<BasicAutoWatches::Row>();
    std::set<std::string>                           otherLabels = { "A", "X", "Y", "S", "P", "PC", "C", "Z", "I", "D", "B", "V", "N" };



    m_rows.clear();

    //  The labels a BASIC variable's name can be mistaken for: the registers
    //  and flags, listed or not, and every other watch's.
    for (const DebuggerViewSnapshot::AutoWatchLine & line : snapshot.autoWatches)
    {
        otherLabels.insert (line.label);
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
    {
        if (watch.basicName.empty())
        {
            otherLabels.insert (watch.expression);
        }
    }

    //  The variables the BASIC statement being run uses, and those the one
    //  before it used, ahead of the instruction's: in a program, they are
    //  what the line is doing.
    for (const BasicAutoWatches::Row & used : basicAutos)
    {
        DxuiListView::Cell  name  = BasicWatches::MakeNameCell (used.item.name, otherLabels);
        DxuiListView::Cell  value = { TextEncoding::NarrowToWide (used.item.value) };

        if (used.isPrevious)
        {
            name.weight  = DxuiFontWeight::Italic;
            value.weight = name.weight;
            name.tip     = ColorLegend::GetText (ColorLegend::Meaning::PreviousStatement);
        }

        if (used.isChanged)
        {
            value.argb = changed;
            value.tip  = ColorLegend::GetText (ColorLegend::Meaning::Changed);
        }

        rows.push_back ({ { MakeIdCell (L"Auto"), name, value, MakeTypeCell (L"BASIC") }, {}, true });
        m_rows.push_back ({ RowKind::BasicItem, 0 });
    }

    if (!snapshot.autoWatches.empty())
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
            rows.push_back ({ { MakeIdCell (L"Auto"), name, value, MakeTypeCell (GetAutoType (line.key, line.label)) }, {}, false, { true, true, false, true } });
            m_rows.push_back ({ RowKind::Automatic, (int) i });
        }
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
    {
        DxuiListView::Cell  id    = MakeIdCell (std::to_wstring (watch.id));
        DxuiListView::Cell  name  = !watch.basicName.empty()  ? BasicWatches::MakeNameCell (watch.basicName, otherLabels)
                                  : !watch.expression.empty() ? DxuiListView::Cell { TextEncoding::NarrowToWide (watch.expression) }
                                  :                             DxuiListView::Cell { std::format (L"${:04X}", watch.address) };
        DxuiListView::Cell  value = { TextEncoding::NarrowToWide (watch.value) };

        //  A symbol's address follows its name, as an automatic watch's does.
        if (watch.basicName.empty() && watch.error.empty() && GetWatchType (watch) == L"Symbol")
        {
            name.text += std::format (L" ${:04X}", watch.address);
        }

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
        rows.push_back ({ { id, name, value, MakeTypeCell (GetWatchType (watch)) },
                          { std::wstring(),
                            !watch.basicName.empty()  ? TextEncoding::NarrowToWide (watch.basicName)
                            : !watch.expression.empty() ? TextEncoding::NarrowToWide (watch.expression)
                            :                             std::format (L"{:04X}", watch.address),
                            TextEncoding::NarrowToWide (watch.value) },
                          false, { true, false, !watch.basicName.empty() || !watch.isValueEditable || !watch.error.empty(), true } });
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
            rows.push_back ({ { id, name, value, MakeTypeCell (L"Pointer") }, {}, true });
            m_rows.push_back ({ RowKind::ZeroPage, pointer.id });
        }
    }

    //  Every BASIC variable and array element, under a heading a click
    //  expands or collapses.
    if (isInProgram)
    {
        rows.push_back ({ MakeHeading (std::format (L"{} BASIC variables", m_isBasicVariablesExpanded ? s_kpszSmallTriangleDown : s_kpszSmallTriangleRight)), {}, true });
        m_rows.push_back ({ RowKind::BasicHeading, 0 });

        for (const BasicWatches::Item & item : m_isBasicVariablesExpanded ? BasicWatches::GetAllItems (*snapshot.basic) : std::vector<BasicWatches::Item>())
        {
            DxuiListView::Cell  name  = BasicWatches::MakeNameCell (item.name, otherLabels);
            DxuiListView::Cell  value = { TextEncoding::NarrowToWide (item.value) };
            auto                used  = std::ranges::find_if (basicAutos, [&item] (const BasicAutoWatches::Row & each) { return each.item.name == item.name; });

            //  A variable the statement just run used is in italics, as the
            //  Auto rows mark what the previous instruction touched.
            if (used != basicAutos.end() && used->isPrevious)
            {
                name.weight  = DxuiFontWeight::Italic;
                value.weight = name.weight;
                name.tip     = ColorLegend::GetText (ColorLegend::Meaning::PreviousStatement);
            }

            if (used != basicAutos.end() && used->isChanged)
            {
                value.argb = changed;
                value.tip  = ColorLegend::GetText (ColorLegend::Meaning::Changed);
            }

            rows.push_back ({ { DxuiListView::Cell { L"" }, name, value, MakeTypeCell (L"BASIC") }, {}, true });
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

    return { label, blank, blank, blank };
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
//  WatchesPane::MakeTypeCell
//
//  The Type column's text, muted as the ID column's is.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell WatchesPane::MakeTypeCell (const std::wstring & text) const
{
    return MakeIdCell (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::GetAutoType
//
//  An automatic watch's kind, from its key in the snapshot: memory shown
//  by a symbol is a symbol, as a watch typed by the symbol's name is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WatchesPane::GetAutoType (const std::string & key, const std::string & label)
{
    return key.starts_with ("R:")                    ? L"Register"
         : key.starts_with ("F:")                    ? L"Flag"
         : !label.empty() && !label.starts_with ('$') ? L"Symbol"
         :                                             L"Memory";
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::GetWatchType
//
//  A watch's kind: a BASIC variable, a register, a symbol by name, an
//  address, or an expression evaluated at each read.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WatchesPane::GetWatchType (const DebuggerViewSnapshot::WatchLine & watch)
{
    std::string  base;
    bool         isName = false;
    bool         isHex  = false;



    if (!watch.basicName.empty())
    {
        return L"BASIC";
    }

    if (DebuggerViewState::IsRegisterWatch (watch))
    {
        return L"Register";
    }

    (void) WatchItem::SplitWidth (watch.expression, base);
    base   = base.starts_with ("$") ? base.substr (1) : base;
    isHex  = !base.empty() && base.find_first_not_of ("0123456789ABCDEFabcdef") == std::string::npos;
    isName = !base.empty() && !isHex && (isalpha ((unsigned char) base[0]) || base[0] == '_') &&
             base.find_first_not_of ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.") == std::string::npos;

    return (base.empty() || isHex)      ? std::wstring (L"Memory")
         : isName                        ? std::wstring (L"Symbol")
         :                                 std::wstring (L"Expression");
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::IsInRom
//
//  Whether the PC is in ROM or a slot's firmware, where Applesoft runs a
//  statement and the Monitor and firmware routines it calls are; a PC moved
//  to machine code in RAM is outside the program's statement.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchesPane::IsInRom (Word pc)
{
    //  From the slots' firmware up: the //c's and an enhanced //e's 80-column
    //  firmware at $C300 and $C800 run inside a PRINT.
    constexpr Word  kRomFirst = 0xC100;



    return pc >= kRomFirst;
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

    //  A watch's expression erased removes the watch, as an undoable step.
    if (edit.isOpen && column == 0 && edit.what.kind == RowKind::Manual && typed.find_first_not_of (L" \t") == std::wstring::npos)
    {
        m_edit = Edit {};
        RemoveWatch (edit.what.index);
        return true;
    }

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
    m_addedText           = expression;
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
        RemoveWatch (m_rows[(size_t) row].index);
    }
    else if (row >= 0 && row < (int) m_rows.size() && m_rows[(size_t) row].kind == RowKind::ZeroPage)
    {
        RemovePointer (m_rows[(size_t) row].index);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::RemoveWatch
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::RemoveWatch (int id)
{
    const DebuggerViewSnapshot     * snapshot = m_host.GetLatestSnapshot();
    DebuggerViewState::WatchUndo     undo;
    DebuggerAction                   clear    = DebuggerActions::GetClearWatch (id, m_host.GetMode());



    for (const DebuggerViewSnapshot::WatchLine & watch : (snapshot != nullptr) ? snapshot->watches : std::vector<DebuggerViewSnapshot::WatchLine>())
    {
        if (watch.id == id)
        {
            undo.actions = { DebuggerActions::GetAddWatchAt (id, GetRestoreText (watch), m_host.GetMode()) };
            m_history.Record (std::move (undo), { clear }, std::format (L"remove watch {}", id));
            break;
        }
    }

    m_host.RunAction (clear);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::RemovePointer
//
//  The undo puts the pointer back in the slot it had.
//
////////////////////////////////////////////////////////////////////////////////

void WatchesPane::RemovePointer (int slot)
{
    const DebuggerViewSnapshot     * snapshot = m_host.GetLatestSnapshot();
    DebuggerViewState::WatchUndo     undo;
    DebuggerAction                   clear    = DebuggerActions::GetClearZeroPagePointer (slot, m_host.GetMode());



    for (const DebuggerViewSnapshot::WatchLine & pointer : (snapshot != nullptr) ? snapshot->zeroPagePointers : std::vector<DebuggerViewSnapshot::WatchLine>())
    {
        if (pointer.id == slot)
        {
            undo.actions = { DebuggerActions::GetAddZeroPagePointerAt (slot, (Byte) pointer.address, m_host.GetMode()) };
            m_history.Record (std::move (undo), { clear }, std::format (L"remove zero-page pointer {}", slot));
            break;
        }
    }

    m_host.RunAction (clear);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchesPane::GetRestoreText
//
////////////////////////////////////////////////////////////////////////////////

std::string WatchesPane::GetRestoreText (const DebuggerViewSnapshot::WatchLine & watch)
{
    return !watch.expression.empty() ? watch.expression
         : watch.basicName.empty()   ? std::format ("{:04X}", watch.address)
         :                             "BASIC " + watch.basicName;
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
        m_host.RepaintLater();
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
            items.push_back ({ L"Remove", [this, id = watch.id] { RemoveWatch (id); } });
        }
    }
    else if (what.kind == RowKind::ZeroPage)
    {
        items.push_back ({ L"Remove", [this, slot = what.index] { RemovePointer (slot); } });
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

    items.push_back ({ L"Add pointer watch...", [this] { m_host.AddZeroPagePointer(); } });
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
