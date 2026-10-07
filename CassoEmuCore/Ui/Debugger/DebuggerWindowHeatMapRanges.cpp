#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureHeatRanges
//
//  The sets the user last left, read against whatever symbols the first
//  snapshot brings; the list of the set being edited, in the dense face the
//  other panes use, a check on each range to include it; and the pane's bar,
//  in the breakpoints pane's style.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureHeatRanges()
{
    HeatMapRangeBarCommands::Handlers  handlers;
    std::vector<DxuiListView::Column>  columns =
    {
        { L"Name",     kHeatRangeNameDip },
        { L"Start",    kHeatRangeSpanDip },
        { L"End",      kHeatRangeSpanDip },
        { L"Size",     kHeatRangeSizeDip },
        { L"Built-in", kHeatRangeSizeDip },
    };



    m_isHeatRangesOpened = m_host != nullptr && !m_host->GetDebuggerHeatMapRanges().empty();
    m_heatRanges         = HeatMapRangeSets::FromText ((m_host != nullptr) ? m_host->GetDebuggerHeatMapRanges() : std::string());
    m_heatRangeSet = !m_heatRanges.shown.empty() ? m_heatRanges.shown : (m_heatRanges.sets.empty() ? std::string() : m_heatRanges.sets.front().name);

    handlers.dispatch  = [this] (int id) { RunHeatRangeBarEntry (id); };
    handlers.isEnabled = [this] (int id) { return IsHeatRangeBarEnabled (id); };
    handlers.getLabel  = [this] (int id) { return GetHeatRangeBarLabel (id); };

    m_heatRangeCommands = std::make_unique<HeatMapRangeBarCommands> (std::move (handlers));

    m_heatRangeBar->SetTextRenderer (GetTextRenderer());
    m_heatRangeBar->SetPopupHost    (GetPopupHost());
    m_heatRangeBar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
    m_heatRangeBar->SetCompact      (true);
    m_heatRangeBar->EnableSeeMore   (s_kpszMdl2More, L"See more");
    m_heatRangeBar->SetEntries      (m_heatRangeCommands->BuildEntries());
    m_heatRangeBar->SetVisible      (false);

    MakeDense (m_heatRangeList);
    m_heatRangeList->SetColumns               (std::move (columns));
    m_heatRangeList->SetActivateOnDoubleClick (true);
    m_heatRangeList->SetAlwaysShowSelection   (true);
    m_heatRangeList->SetOnCheckToggled ([this] (int row, size_t, bool checked) { ToggleHeatRange (row, checked); });
    m_heatRangeList->SetOnActivateRow  ([this] (int row)
    {
        RECT  bounds = m_heatRangeList->GetBounds();
        int   column = GetColumnAt (m_heatRangeList, m_lastPressPx.x - bounds.left);



        BeginHeatRangeEdit (row, (column >= 1 && column <= (int) HeatMapRangeSets::Field::Size) ? (HeatMapRangeSets::Field) column : HeatMapRangeSets::Field::Name);
    });

    m_heatRangeError->SetTextRole  (DxuiTextRole::Error);
    m_heatRangeError->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_heatRangeEditor->SetVisible   (false);
    m_heatRangeEditor->SetOverText  (true);
    m_heatRangeEditor->SetHwnd      (GetHwnd());
    m_heatRangeEditor->SetMaxLength (kHeatRangeEditMaxChars);

    SetHeatRangeMenus();
    ApplyHeatRanges();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenHeatRanges
//
//  Edit ranges on the heat map's bar: the pane comes forward on the set the
//  map shows, or the one it last edited when the map shows all memory.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenHeatRanges()
{
    if (!m_heatRanges.shown.empty())
    {
        EditRangeSet (m_heatRanges.shown);
    }

    ShowPane (DebuggerLayout::kHeatRanges);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetHeatRangeMenus
//
//  The heat map's drop-down, All memory and then every set, the one shown
//  checked; and the pane's, every set, the one edited checked. Built again
//  before either opens, since the rows carry the checks they were built
//  with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatRangeMenus()
{
    std::vector<DxuiPopupMenuItem>  shown;
    std::vector<DxuiPopupMenuItem>  edited;



    m_rangeSetCommands.clear();
    m_editedSetCommands.clear();

    m_rangeSetCommands.push_back (MakeMenuCommand (TextEncoding::Utf8ToWide (HeatMapRangeSets::kpszAllMemory), m_heatRanges.shown.empty(),
                                                   [this] { ShowRangeSet ({}); }));

    for (const HeatMapRangeSet & set : m_heatRanges.sets)
    {
        std::string   name  = set.name;
        std::wstring  label = TextEncoding::Utf8ToWide (name);



        m_rangeSetCommands.push_back  (MakeMenuCommand (label, m_heatRanges.shown == name, [this, name] { ShowRangeSet (name); }));
        m_editedSetCommands.push_back (MakeMenuCommand (label, m_heatRangeSet     == name, [this, name] { EditRangeSet (name); }));
    }

    for (const std::shared_ptr<DxuiCommand> & command : m_rangeSetCommands)
    {
        shown.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    for (const std::shared_ptr<DxuiCommand> & command : m_editedSetCommands)
    {
        edited.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_heatMapBar->SetDropDownItems   (HeatMapBarCommands::kRangeSet,  std::move (shown));
    m_heatRangeBar->SetDropDownItems (HeatMapRangeBarCommands::kSet,  std::move (edited));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceHeatRangeBar
//
//  As the heat map's: the bar sits in the place held at the top of its pane
//  and goes with the pane into a floating window.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceHeatRangeBar()
{
    bool          shown = m_heatRangeSlot != nullptr && m_heatRangeSlot->IsVisible();
    DxuiWindow  * host  = GetPaneHost (DebuggerLayout::kHeatRanges);
    RECT          slot  = {};



    m_heatRangeBar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    slot = m_heatRangeSlot->GetBounds();

    m_heatRangeBar->SetTextRenderer   (host->GetTextRenderer());
    m_heatRangeBar->SetPopupHost      (host->GetPopupHost());
    m_heatRangeBar->SetHostClientRect (host->GetBounds());
    m_heatRangeBar->Layout            (slot, m_scaler);

    host->SetChildClip (m_heatRangeBar, slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteHeatRangeBarMouse
//
//  As the heat map's: the strip and whatever menu it has open take the left
//  button.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatRangeBarMouse (const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    RECT  strip = m_heatRangeBar->GetBounds();
    bool  open  = m_heatRangeBar->IsMenuOpen();
    bool  over  = m_heatRangeBar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y });



    if (!over && !open)
    {
        m_heatRangeBar->OnToolbarMouseLeave();
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

        return m_heatRangeBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open)
        {
            SetHeatRangeMenus();
        }

        return m_heatRangeBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_heatRangeBar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatRangeBarLabel
//
//  The set drop-down reads the set edited; the rest keep their labels.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetHeatRangeBarLabel (int id) const
{
    if (id == HeatMapRangeBarCommands::kSet)
    {
        return HeatMapRangeBarCommands::GetSetLabel (TextEncoding::Utf8ToWide (m_heatRangeSet));
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsHeatRangeBarEnabled
//
//  Only a set's own ranges can be edited or removed; a built-in one is
//  included or left out with its check. Moving does not go past either end.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsHeatRangeBarEnabled (int id) const
{
    const HeatMapRangeSet  * set      = GetEditedSet();
    std::optional<size_t>    selected = GetSelectedHeatRange();
    int                      row      = m_heatRangeList->GetSelectedRow();
    bool                     isOwn    = set != nullptr && selected.has_value() && !HeatMapRangeSets::IsBuiltIn (set->ranges[*selected]);



    switch (id)
    {
    case HeatMapRangeBarCommands::kSet:        return !m_heatRanges.sets.empty();
    case HeatMapRangeBarCommands::kRenameSet:  return set != nullptr;
    case HeatMapRangeBarCommands::kDeleteSet:  return set != nullptr;
    case HeatMapRangeBarCommands::kNew:        return set != nullptr;
    case HeatMapRangeBarCommands::kEdit:       return isOwn;
    case HeatMapRangeBarCommands::kRemove:     return isOwn;
    case HeatMapRangeBarCommands::kUp:         return selected.has_value() && row > 0;
    case HeatMapRangeBarCommands::kDown:       return selected.has_value() && row + 1 < (int) m_heatRangeRows.size();
    default:                                   return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunHeatRangeBarEntry
//
//  New set makes a set under a name no other has, then opens that name for
//  editing, as a new folder in Explorer does. The set is the drop-down's;
//  its rows act.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunHeatRangeBarEntry (int id)
{
    std::string              error;
    std::string              name;
    HeatMapRangeSet        * set      = GetEditedSet();
    std::optional<size_t>    selected = GetSelectedHeatRange();



    if (!IsHeatRangeBarEnabled (id))
    {
        return;
    }

    switch (id)
    {
    case HeatMapRangeBarCommands::kNewSet:
        name = m_heatRanges.GetNewSetName();

        if (m_heatRanges.TryAddSet (name, error))
        {
            m_heatRangeSet = name;
            SaveHeatRanges();
            ApplyHeatRanges();
            SetHeatRangeMenus();
            BeginSetNameEdit (true);
        }

        break;

    case HeatMapRangeBarCommands::kRenameSet:
        BeginSetNameEdit (false);
        break;

    case HeatMapRangeBarCommands::kDeleteSet:
        (void) m_heatRanges.TryDeleteSet (m_heatRangeSet);
        m_heatRangeSet = m_heatRanges.sets.empty() ? std::string() : m_heatRanges.sets.front().name;
        SaveHeatRanges();
        ApplyHeatRanges();
        SetHeatRangeMenus();
        break;

    case HeatMapRangeBarCommands::kNew:
        AddHeatRange();
        break;

    case HeatMapRangeBarCommands::kEdit:
        BeginHeatRangeEdit (m_heatRangeList->GetSelectedRow(), HeatMapRangeSets::Field::Start);
        break;

    case HeatMapRangeBarCommands::kRemove:
        if (!HeatMapRangeSets::TryRemoveRange (*set, *selected, error))
        {
            SetHeatRangeError (error);
            break;
        }

        SetHeatRangeError ({});
        SaveHeatRanges();
        ApplyHeatRanges();
        break;

    case HeatMapRangeBarCommands::kUp:    MoveHeatRange (-1); break;
    case HeatMapRangeBarCommands::kDown:  MoveHeatRange (1);  break;

    default:
        break;
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddHeatRange
//
//  A range is added at the set's end and its start opened for typing, as
//  Visual Studio's New opens a row; one left empty goes again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddHeatRange()
{
    HeatMapRangeSet  * set = GetEditedSet();



    if (set == nullptr)
    {
        return;
    }

    set->ranges.push_back (HeatMapRange {});
    ApplyHeatRangeRows();

    m_heatRangeList->SetSelectedRow (m_heatRangeList->GetRowCount() - 1);
    BeginHeatRangeEdit (m_heatRangeList->GetRowCount() - 1, HeatMapRangeSets::Field::Start);

    //  With no room to type in, as in a pane too short for a row, the range
    //  goes again rather than stay empty.
    if (!IsEditingHeatRange())
    {
        set->ranges.pop_back();
        ApplyHeatRangeRows();
        return;
    }

    m_heatRangeEdit.isNew = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MoveHeatRange
//
//  A row moves past the row beside it, even with a range hidden between
//  them in the set, as "My program" is with no symbols loaded.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MoveHeatRange (int delta)
{
    HeatMapRangeSet  * set    = GetEditedSet();
    int                row    = m_heatRangeList->GetSelectedRow();
    int                to     = row + delta;
    size_t             index  = 0;
    size_t             target = 0;



    if (set == nullptr || row < 0 || to < 0 || to >= (int) m_heatRangeRows.size())
    {
        return;
    }

    index  = m_heatRangeRows[(size_t) row];
    target = m_heatRangeRows[(size_t) to];

    while (index != target && HeatMapRangeSets::TryMoveRange (*set, index, delta))
    {
        index = (size_t) ((ptrdiff_t) index + delta);
    }

    SaveHeatRanges();
    ApplyHeatRanges();
    m_heatRangeList->SetSelectedRow (to);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::BeginHeatRangeEdit
//
//  A box over the cell, holding what was typed for it: a range's name,
//  start or end, or its length for its size. A built-in range has nothing
//  of its own to edit.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::BeginHeatRangeEdit (int row, HeatMapRangeSets::Field field)
{
    const HeatMapRangeSet  * set    = GetEditedSet();
    const HeatMapRange     * range  = nullptr;
    RECT                     list   = m_heatRangeList->GetBounds();
    RECT                     cell   = {};
    std::string              text;



    if (set == nullptr || row < 0 || row >= (int) m_heatRangeRows.size())
    {
        return;
    }

    range = &set->ranges[m_heatRangeRows[(size_t) row]];

    if (HeatMapRangeSets::IsBuiltIn (*range))
    {
        return;
    }

    switch (field)
    {
    case HeatMapRangeSets::Field::Name:  text = range->name;  break;
    case HeatMapRangeSets::Field::Start: text = range->start; break;
    case HeatMapRangeSets::Field::End:   text = range->end;   break;
    case HeatMapRangeSets::Field::Size:  text = range->end.starts_with ('+') ? range->end.substr (1) : std::string(); break;
    }

    m_heatRangeList->EnsureVisible (row);

    if (!m_heatRangeList->GetCellTextRectPx (row, (size_t) field, cell))
    {
        return;
    }

    OffsetRect (&cell, list.left, list.top);

    m_heatRangeEdit = HeatRangeEdit { row, field, false, false };

    //  In the list's own face and size, so the text does not jump when the
    //  box opens over it.
    m_heatRangeEditor->SetTextRenderer (GetTextRenderer());
    m_heatRangeEditor->SetFont         (DxuiTheme::kMonoFace, m_heatRangeList->GetFontSizeDip());
    m_heatRangeEditor->SetText         (TextEncoding::Utf8ToWide (text));
    m_heatRangeEditor->Layout          (cell, m_scaler);
    m_heatRangeEditor->SetVisible      (true);
    SetFocusedControl                  (m_heatRangeEditor);
    m_heatRangeEditor->SelectAll();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::BeginSetNameEdit
//
//  The same box, over the set's drop-down on the bar.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::BeginSetNameEdit (bool isNew)
{
    RECT  entry = {};



    if (GetEditedSet() == nullptr)
    {
        return;
    }

    if (!m_heatRangeBar->TryGetEntryRect (HeatMapRangeBarCommands::kSet, entry))
    {
        entry = m_heatRangeBar->GetBounds();
    }

    m_heatRangeEdit = HeatRangeEdit { -1, HeatMapRangeSets::Field::Name, isNew, true };

    m_heatRangeEditor->SetTextRenderer (GetTextRenderer());
    m_heatRangeEditor->SetFont         (DxuiTheme::kBodyFace, m_heatRangeList->GetFontSizeDip());
    m_heatRangeEditor->SetText         (TextEncoding::Utf8ToWide (m_heatRangeSet));
    m_heatRangeEditor->Layout          (entry, m_scaler);
    m_heatRangeEditor->SetVisible      (true);
    SetFocusedControl                  (m_heatRangeEditor);
    m_heatRangeEditor->SelectAll();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EndHeatRangeEdit
//
//  Enter keeps what was typed once it reads, and the box stays open with
//  the reason under the bar when it does not; Escape leaves the range or
//  the set as it was, and a new range left empty, or abandoned, goes again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EndHeatRangeEdit (bool commit)
{
    std::string        typed    = TextEncoding::WideToUtf8 (m_heatRangeEditor->GetText());
    HeatRangeEdit      edit     = m_heatRangeEdit;
    HeatMapRangeSet  * set      = GetEditedSet();
    size_t             index    = (edit.row >= 0 && edit.row < (int) m_heatRangeRows.size()) ? m_heatRangeRows[(size_t) edit.row] : 0;
    bool               isBlank  = typed.find_first_not_of (" \t") == std::string::npos;
    std::string        error;



    if (!IsEditingHeatRange())
    {
        return;
    }

    if (commit && edit.isSetName && !m_heatRanges.TryRenameSet (m_heatRangeSet, typed, error))
    {
        SetHeatRangeError (error);
        return;
    }

    if (commit && edit.isSetName)
    {
        m_heatRangeSet = (set != nullptr) ? set->name : m_heatRangeSet;
    }

    if (!edit.isSetName && set != nullptr && edit.isNew && (!commit || isBlank))
    {
        set->ranges.erase (set->ranges.begin() + (ptrdiff_t) index);
    }
    else if (!edit.isSetName && set != nullptr && commit && !HeatMapRangeSets::TryEditRange (*set, index, edit.field, typed, GetHeatRangeSymbols(), error))
    {
        SetHeatRangeError (error);
        return;
    }

    m_heatRangeEdit = HeatRangeEdit {};
    m_heatRangeEditor->SetVisible (false);
    SetFocusedControl             (m_heatRangeList);
    SetHeatRangeError             ({});
    SaveHeatRanges();
    ApplyHeatRanges();
    SetHeatRangeMenus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleHeatRange
//
//  A check in the Name column: a range in the set's map, or left out of it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleHeatRange (int row, bool included)
{
    HeatMapRangeSet  * set = GetEditedSet();



    if (set == nullptr || row < 0 || row >= (int) m_heatRangeRows.size())
    {
        return;
    }

    set->ranges[m_heatRangeRows[(size_t) row]].included = included;

    SaveHeatRanges();
    ApplyHeatRanges();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowRangeSet
//
//  A choice on the heat map's drop-down: a set's ranges, or all memory for
//  an empty name.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowRangeSet (const std::string & name)
{
    m_heatRanges.shown = (m_heatRanges.FindSet (name) != nullptr) ? name : std::string();

    SaveHeatRanges();
    ApplyHeatRanges();
    SetHeatRangeMenus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::EditRangeSet
//
//  A choice on the pane's drop-down: which set its list shows and edits.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EditRangeSet (const std::string & name)
{
    if (IsEditingHeatRange())
    {
        EndHeatRangeEdit (false);
    }

    m_heatRangeSet = (m_heatRanges.FindSet (name) != nullptr) ? name : m_heatRangeSet;

    m_heatRangeList->SetSelectedRow (-1);
    ApplyHeatRangeRows();
    SetHeatRangeMenus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyHeatRanges
//
//  The map shows the shown set's ranges as they read against the symbols
//  now, or all memory; the pane lists the set it edits. The set whose reads
//  before written are left out reads again too.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyHeatRanges()
{
    std::vector<HeatMapView::Band>  bands;



    SendHeatMapIgnore();

    if (m_heatRanges.shown.empty())
    {
        m_heatMapView->ClearRanges();
        ApplyHeatRangeRows();
        return;
    }

    for (const HeatMapRangeSets::Span & span : m_heatRanges.GetShownSpans (GetHeatRangeSymbols()))
    {
        bands.push_back ({ TextEncoding::Utf8ToWide (span.name), span.first, (int) span.last - (int) span.first + 1 });
    }

    m_heatMapView->SetRanges (std::move (bands));
    ApplyHeatRangeRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyHeatRangeRows
//
//  A row for each range of the set edited, "My program" only while there
//  is a program: its check and name, then where it reads as starting and
//  ending and its size. A start or end typed as other than its own number
//  follows it, dimmed. A range that does not read gives why across the rest
//  of its row, in the error color.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyHeatRangeRows()
{
    constexpr size_t                                kColumns  = 5;
    const HeatMapRangeSet                         * set       = GetEditedSet();
    const HeatMapSymbols                          & symbols   = GetHeatRangeSymbols();
    uint32_t                                        error     = (m_theme != nullptr) ? m_theme->ErrorForeground() : 0;
    std::wstring                                    separator = L"  ";
    std::vector<std::vector<DxuiListView::Cell>>    rows;



    m_heatRangeRows.clear();

    for (size_t i = 0; set != nullptr && i < set->ranges.size(); i++)
    {
        const HeatMapRange               & range   = set->ranges[i];
        std::vector<DxuiListView::Cell>    cells (kColumns);
        Word                               first   = 0;
        Word                               last    = 0;
        std::string                        reason;
        bool                               isRead  = false;



        if (HeatMapRangeSets::IsHidden (range, symbols))
        {
            continue;
        }

        isRead = HeatMapRangeSets::TryResolve (range, symbols, first, last, reason);

        cells[0].check = range.included;
        cells[0].text  = TextEncoding::Utf8ToWide (HeatMapRangeSets::GetName (range));
        cells[0].dim   = !HeatMapRangeSets::IsBuiltIn (range) && range.name.empty();
        cells[4].text  = HeatMapRangeSets::IsBuiltIn (range) ? L"Yes" : L"";

        if (!isRead)
        {
            cells[1].text     = TextEncoding::Utf8ToWide (range.start);
            cells[2].text     = TextEncoding::Utf8ToWide (reason);
            cells[2].argb     = error;
            cells[2].spansRow = true;
            cells[2].tip      = cells[2].text;
        }
        else
        {
            cells[1].text = std::format (L"${:04X}", first);
            cells[2].text = std::format (L"${:04X}", last);
            cells[3].text = std::format (L"${:04X}", (uint32_t) last - first + 1);

            for (const auto & [cell, typed] : { std::pair { &cells[1], range.start }, std::pair { &cells[2], range.end } })
            {
                if (!typed.empty() && !HeatMapRangeSets::IsHexNumber (typed))
                {
                    std::wstring  shown = TextEncoding::Utf8ToWide (typed);



                    cell->dimRanges.push_back ({ (int) (cell->text.size() + separator.size()), (int) (cell->text.size() + separator.size() + shown.size()) });
                    cell->text += separator + shown;
                }
            }
        }

        m_heatRangeRows.push_back (i);
        rows.push_back (std::move (cells));
    }

    m_heatRangeList->SetRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveHeatRanges
//
//  Every change is kept as it is made.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveHeatRanges()
{
    if (m_host != nullptr)
    {
        m_host->SetDebuggerHeatMapRanges (m_heatRanges.ToText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetHeatRangeError
//
//  The line under the bar, which shows only while it has something to say.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatRangeError (const std::string & error)
{
    std::wstring  what = m_heatRangeEdit.isSetName ? L"Invalid set name: " : L"Invalid range: ";



    m_heatRangeError->SetText (error.empty() ? std::wstring() : what + TextEncoding::Utf8ToWide (error));

    if (m_heatRangeFrame != nullptr)
    {
        m_heatRangeFrame->Relayout();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatRangeError
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetHeatRangeError() const
{
    return m_heatRangeError->GetText();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetEditedSet
//
////////////////////////////////////////////////////////////////////////////////

HeatMapRangeSet * DebuggerWindow::GetEditedSet()
{
    return m_heatRanges.FindSet (m_heatRangeSet);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetEditedSet
//
////////////////////////////////////////////////////////////////////////////////

const HeatMapRangeSet * DebuggerWindow::GetEditedSet() const
{
    return m_heatRanges.FindSet (m_heatRangeSet);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatRangeSymbols
//
//  The copy the last snapshot carried, or none before the first, which
//  reads numbers alone.
//
////////////////////////////////////////////////////////////////////////////////

const HeatMapSymbols & DebuggerWindow::GetHeatRangeSymbols() const
{
    static const HeatMapSymbols  none;



    return (m_heatRangeSymbols != nullptr) ? *m_heatRangeSymbols : none;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSelectedHeatRange
//
//  The selected row's range, as its place in the set.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> DebuggerWindow::GetSelectedHeatRange() const
{
    int  row = m_heatRangeList->GetSelectedRow();



    if (row < 0 || row >= (int) m_heatRangeRows.size())
    {
        return std::nullopt;
    }

    return m_heatRangeRows[(size_t) row];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteHeatRangeEditMouse
//
//  As a watch's edit: the box takes the pointer while it is over it, and a
//  press or a wheel anywhere else keeps what was typed and lets it through.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatRangeEditMouse (const DxuiMouseEvent & ev)
{
    RECT  box = m_heatRangeEditor->GetBounds();



    if (!IsEditingHeatRange())
    {
        return false;
    }

    if (DxuiDockSite::Contains (box, ev.positionDip))
    {
        m_heatRangeEditor->OnMouse (ev);
        Invalidate();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel)
    {
        EndHeatRangeEdit (true);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteHeatRangeKey
//
//  An edit takes every key: Enter keeps what was typed, Escape leaves it as
//  it was. In the list, Delete removes a range, F2 renames one and Insert
//  adds one, as in Visual Studio's tool windows.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatRangeKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    bool  isPlain = ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt;



    if (IsEditingHeatRange())
    {
        if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE))
        {
            EndHeatRangeEdit (ev.vk == VK_RETURN);
        }
        else
        {
            m_heatRangeEditor->OnKey (ev);
        }

        Invalidate();
        return true;
    }

    if (!isPlain || focused != m_heatRangeList)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_DELETE:
        RunHeatRangeBarEntry (HeatMapRangeBarCommands::kRemove);
        return true;

    case VK_INSERT:
        RunHeatRangeBarEntry (HeatMapRangeBarCommands::kNew);
        return true;

    case VK_F2:
        BeginHeatRangeEdit (m_heatRangeList->GetSelectedRow(), HeatMapRangeSets::Field::Name);
        return true;

    default:
        return false;
    }
}





