#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/HeatMapRangeBarCommands.h"
#include "Ui/Debugger/HeatMapBarCommands.h"





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
    HeatMapRangeBarCommands::Handlers                  handlers;
    std::vector<DxuiEditableGrid::GridColumn>          columns =
    {
        { { L"Name",     kHeatRangeNameDip } },
        { { L"Start",    kHeatRangeSpanDip }, true, true },
        { { L"End",      kHeatRangeSpanDip } },
        { { L"Size",     kHeatRangeSizeDip } },
        { { L"Built-in", kHeatRangeSizeDip }, false },
    };



    m_isHeatRangesOpened = m_host != nullptr && !m_host->GetDebuggerHeatMapRanges().empty();
    m_heatRanges         = HeatMapRangeSets::FromText ((m_host != nullptr) ? m_host->GetDebuggerHeatMapRanges() : std::string());
    (void) m_heatRanges.TryAddDefaultSet();
    m_heatRangeSet = !m_heatRanges.shown.empty() ? m_heatRanges.shown : (m_heatRanges.sets.empty() ? std::string() : m_heatRanges.sets.front().name);

    handlers.dispatch  = [this] (int id) { RunHeatRangeBarEntry (id); };
    handlers.isEnabled = [this] (int id) { return IsHeatRangeBarEnabled (id); };
    handlers.getLabel  = [this] (int id) { return GetHeatRangeBarLabel (id); };

    m_heatRangeCommands = std::make_unique<HeatMapRangeBarCommands> (std::move (handlers));

    m_heatRangeBar->SetTextRenderer  (GetTextRenderer());
    m_heatRangeBar->SetPopupHost     (GetPopupHost());
    m_heatRangeBar->SetIconFace      (DxuiToolbar::kMdl2IconFace);
    m_heatRangeBar->SetCompact       (true);
    m_heatRangeBar->SetPaneTextInset (true);
    m_heatRangeBar->EnableSeeMore    (s_kpszMdl2More, L"See more");
    m_heatRangeBar->SetEntries       (m_heatRangeCommands->BuildEntries());
    m_heatRangeBar->SetVisible       (false);

    MakeDense (m_heatRangeList);
    m_heatRangeList->SetMonospace           (false);
    m_heatRangeList->SetGridColumns         (std::move (columns));
    m_heatRangeList->SetAddRowText          (L"Add new range");
    m_heatRangeList->SetAlwaysShowSelection (true);
    m_heatRangeList->SetEditorHwnd          (GetHwnd());
    m_heatRangeList->SetEditorMaxLength     (kHeatRangeEditMaxChars);
    m_heatRangeList->SetOnCheckToggled ([this] (int row, size_t, bool checked) { ToggleHeatRange (row, checked); });
    m_heatRangeList->SetOnCommitCell   ([this] (int row, int column, const std::wstring & text, std::wstring & outError) { return CommitHeatRangeCell (row, column, text, outError); });
    m_heatRangeList->SetOnAddRow       ([this] (const std::vector<std::wstring> & texts, std::wstring & outError) { return AddHeatRange (texts, outError); });
    m_heatRangeList->SetOnMoveRow      ([this] (int from, int to) { MoveHeatRangeRow (from, to); });
    m_heatRangeList->SetOnDeleteRow    ([this] (int row) { RemoveHeatRange (row); });
    m_heatRangeList->SetOnError        ([this] (const std::wstring & text) { SetHeatRangeError (TextEncoding::WideToUtf8 (text)); });

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
//  checked, and last Edit ranges, which opens the pane that makes them; and
//  the pane's, every set, the one edited checked. Built again
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

    m_rangeSetCommands.push_back (MakeMenuCommand (L"Edit ranges...", false, [this] { OpenHeatRanges(); }));
    shown.push_back (DxuiPopupMenuItem::ForSeparator());
    shown.push_back (DxuiPopupMenuItem::ForCommand (m_rangeSetCommands.back()));

    for (const std::shared_ptr<DxuiCommand> & command : m_editedSetCommands)
    {
        edited.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_heatViewBar->SetDropDownItems  (HeatMapBarCommands::kRangeSet,  std::move (shown));
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
    bool                     isOwn    = set != nullptr && selected.has_value() && *selected < set->ranges.size() &&
                                        !HeatMapRangeSets::IsBuiltIn (set->ranges[*selected]);



    switch (id)
    {
    case HeatMapRangeBarCommands::kSet:        return !m_heatRanges.sets.empty();
    case HeatMapRangeBarCommands::kRenameSet:  return set != nullptr;
    case HeatMapRangeBarCommands::kDeleteSet:  return set != nullptr;
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
    std::string  error;
    std::string  name;



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
        if (!ConfirmDeleteRangeSet())
        {
            break;
        }

        (void) m_heatRanges.TryDeleteSet (m_heatRangeSet);
        m_heatRangeSet = m_heatRanges.sets.empty() ? std::string() : m_heatRanges.sets.front().name;
        SaveHeatRanges();
        ApplyHeatRanges();
        SetHeatRangeMenus();
        break;

    case HeatMapRangeBarCommands::kEdit:
        BeginHeatRangeEdit (m_heatRangeList->GetSelectedRow(), HeatMapRangeSets::Field::Start);
        break;

    case HeatMapRangeBarCommands::kRemove:
        RemoveHeatRange (m_heatRangeList->GetSelectedRow());
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
//  The grid's new row, added at the set's end once it reads as a range.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::AddHeatRange (const std::vector<std::wstring> & texts, std::wstring & outError)
{
    HeatMapRangeSet  * set   = GetEditedSet();
    HeatMapRange       range;
    std::string        error;
    auto               field = [&texts] (HeatMapRangeSets::Field which) { return TextEncoding::WideToUtf8 (texts[(size_t) which]); };



    if (set == nullptr)
    {
        outError = L"There is no set to add the range to.";
        return false;
    }

    if (!HeatMapRangeSets::TryMakeRange (field (HeatMapRangeSets::Field::Name), field (HeatMapRangeSets::Field::Start), field (HeatMapRangeSets::Field::End),
                                         field (HeatMapRangeSets::Field::Size), GetHeatRangeSymbols(), range, error))
    {
        outError = TextEncoding::Utf8ToWide (error);
        return false;
    }

    set->ranges.push_back (range);
    SaveHeatRanges();
    ApplyHeatRanges();
    Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CommitHeatRangeCell
//
//  A cell edited in the grid, stored once the range reads with it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::CommitHeatRangeCell (int row, int column, const std::wstring & text, std::wstring & outError)
{
    HeatMapRangeSet  * set = GetEditedSet();
    std::string        error;



    if (set == nullptr || row < 0 || row >= (int) m_heatRangeRows.size() ||
        !HeatMapRangeSets::TryEditRange (*set, m_heatRangeRows[(size_t) row], (HeatMapRangeSets::Field) column, TextEncoding::WideToUtf8 (text),
                                        GetHeatRangeSymbols(), error))
    {
        outError = TextEncoding::Utf8ToWide (error);
        return false;
    }

    SaveHeatRanges();
    ApplyHeatRanges();
    Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RemoveHeatRange
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RemoveHeatRange (int row)
{
    HeatMapRangeSet  * set = GetEditedSet();
    std::string        error;



    if (set == nullptr || row < 0 || row >= (int) m_heatRangeRows.size())
    {
        return;
    }

    if (!HeatMapRangeSets::TryRemoveRange (*set, m_heatRangeRows[(size_t) row], error))
    {
        SetHeatRangeError (error);
        return;
    }

    //  The rows are rebuilt before the save, since saving can paint the
    //  window, and a paint reads the rows against the set.
    SetHeatRangeError ({});
    ApplyHeatRanges();
    SaveHeatRanges();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MoveHeatRange
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MoveHeatRange (int delta)
{
    int  row = m_heatRangeList->GetSelectedRow();



    MoveHeatRangeRow (row, row + delta);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MoveHeatRangeRow
//
//  A row moves to another row's place, past every range between them in the
//  set, a hidden one too, as "My program" is with no symbols loaded.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MoveHeatRangeRow (int row, int to)
{
    HeatMapRangeSet  * set    = GetEditedSet();
    int                delta  = (to < row) ? -1 : 1;
    size_t             index  = 0;
    size_t             target = 0;



    if (set == nullptr || row < 0 || to < 0 || to == row || row >= (int) m_heatRangeRows.size() || to >= (int) m_heatRangeRows.size())
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
//  The grid's box over the cell, with what was typed for it: a range's
//  name, start or end, or its length for its size. A built-in range has
//  nothing of its own to edit.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::BeginHeatRangeEdit (int row, HeatMapRangeSets::Field field)
{
    if (m_heatRangeList->BeginEdit (row, (int) field))
    {
        SetFocusedControl (m_heatRangeList);
    }

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

    m_heatRangeEdit = HeatRangeEdit { isNew, true };

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
//  A range's cell is the grid's to end. For the set's name, Enter stores
//  what was typed once it is a name no other set has, and the box stays
//  open with the reason under the bar when it is not; Escape cancels it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::EndHeatRangeEdit (bool commit)
{
    std::string        typed = TextEncoding::WideToUtf8 (m_heatRangeEditor->GetText());
    HeatMapRangeSet  * set   = GetEditedSet();
    std::string        error;



    if (!m_heatRangeEdit.isSetName)
    {
        m_heatRangeList->EndEdit (commit);
        Invalidate();
        return;
    }

    if (commit && !m_heatRanges.TryRenameSet (m_heatRangeSet, typed, error))
    {
        SetHeatRangeError (error);
        return;
    }

    if (commit)
    {
        m_heatRangeSet = (set != nullptr) ? set->name : m_heatRangeSet;
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
    std::vector<DxuiEditableGrid::GridRow>          rows;



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
        rows.push_back ({ std::move (cells),
                          { TextEncoding::Utf8ToWide (range.name), TextEncoding::Utf8ToWide (range.start), TextEncoding::Utf8ToWide (range.end),
                            TextEncoding::Utf8ToWide (range.end.starts_with ('+') ? range.end.substr (1) : std::string()) },
                          HeatMapRangeSets::IsBuiltIn (range) });
    }

    m_heatRangeList->SetGridRows (std::move (rows));
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
//  As a watch's edit: the set name's box takes the pointer while it is over
//  it, and a press or a wheel anywhere else commits what was typed and goes
//  on to whatever is there. The grid routes its own box.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatRangeEditMouse (const DxuiMouseEvent & ev)
{
    RECT  box = m_heatRangeEditor->GetBounds();



    if (!m_heatRangeEdit.isSetName)
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
//  The set name's edit takes every key: Enter commits what was typed and
//  Escape cancels it. The grid takes every key while a cell is edited, and
//  otherwise Delete, Insert, F2 and Enter, as in Visual Studio's tool
//  windows.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatRangeKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    bool  isPlain = ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt;



    if (m_heatRangeEdit.isSetName)
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

    if (focused != m_heatRangeList)
    {
        return false;
    }

    if (m_heatRangeList->IsEditing() ||
        (isPlain && (ev.vk == VK_DELETE || ev.vk == VK_INSERT || ev.vk == VK_F2 || ev.vk == VK_RETURN)))
    {
        (void) m_heatRangeList->OnKey (ev);
        Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfirmDeleteRangeSet
//
//  A set and its ranges go only once the user says yes, in a box centered
//  on the window that holds the pane, floating or not; No is the default.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ConfirmDeleteRangeSet()
{
    DxuiWindow    * host     = GetPaneHost (DebuggerLayout::kHeatRanges);
    std::wstring    question = std::format (L"Delete the set \"{}\" and all of its ranges?", TextEncoding::Utf8ToWide (m_heatRangeSet));
    int             answer   = IDNO;



    answer = ShowMessageBox ((host != nullptr) ? host->GetHwnd() : GetHwnd(), question, L"Delete set", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING);

    return answer == IDYES;
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




