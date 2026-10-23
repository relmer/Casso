#include "Pch.h"

#include "Ui/Debugger/Panes/HeatRangesPane.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Ui/Debugger/HeatMapRangeBarCommands.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/HeatMapPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::HeatRangesPane
//
////////////////////////////////////////////////////////////////////////////////

HeatRangesPane::HeatRangesPane (IDebuggerPaneHost & host, HeatMapPane & heatMap) :
    IDebuggerPane (host),
    m_heatMap     (heatMap),
    m_bar         (host, DebuggerLayout::kHeatRanges, L"Heat map range commands")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::~HeatRangesPane
//
////////////////////////////////////////////////////////////////////////////////

HeatRangesPane::~HeatRangesPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatRangesPane::GetId() const
{
    return DebuggerLayout::kHeatRanges;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::CreateControls
//
//  The line with the reason an edit was refused and the list, then the bar,
//  which paints over them.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::CreateControls()
{
    m_error = m_host.GetControlParent().CreateChild<DxuiLabel>();
    m_list  = m_host.GetControlParent().CreateChild<DxuiEditableGrid>();
    m_bar.Create();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::CreateOverlays
//
//  The box over a set's name, which paints over the pane.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::CreateOverlays()
{
    m_editor = m_host.GetControlParent().CreateChild<DxuiTextInput>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::Configure
//
//  The sets the user last left, read against whatever symbols the first
//  snapshot brings; the list of the set being edited, a check on each range
//  to include it; and the pane's bar. The window makes the list dense in the
//  face the other panes use; the map shows the ranges once it is docked.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::Configure()
{
    HeatMapRangeBarCommands::Handlers                  handlers;
    std::vector<DxuiEditableGrid::GridColumn>          columns =
    {
        { { L"Include",  0 }, false },
        { { L"Name",     0 } },
        { { L"Start",    0 }, true, true },
        { { L"End",      0 } },
        { { L"Size",     0 } },
        { { L"Built-in", 0 }, false },
    };



    LoadSets();

    m_editedSet = !m_sets.shown.empty() ? m_sets.shown : (m_sets.sets.empty() ? std::string() : m_sets.sets.front().name);

    handlers.dispatch  = [this] (int id) { RunBarEntry (id); };
    handlers.isEnabled = [this] (int id) { return IsBarEnabled (id); };
    handlers.getLabel  = [this] (int id) { return GetBarLabel (id); };

    m_commands = std::make_unique<HeatMapRangeBarCommands> (std::move (handlers));

    m_bar.Configure();
    m_bar.SetOnPress ([this] { SetMenus(); });
    m_bar.GetBar()->SetEntries (m_commands->BuildEntries());

    m_list->SetGridColumns         (std::move (columns));
    m_list->SetAddRowText          (L"Add new range");
    m_list->SetAlwaysShowSelection (true);
    m_list->SetEditorHwnd          (m_host.GetOwnerHwnd());
    m_list->SetEditorMaxLength     (kEditMaxChars);
    m_list->SetOnCheckToggled ([this] (int row, size_t, bool checked) { ToggleRange (row, checked); });
    m_list->SetOnCommitCell   ([this] (int row, int column, const std::wstring & text, std::wstring & outError) { return CommitCell (row, column, text, outError); });
    m_list->SetOnAddRow       ([this] (const std::vector<std::wstring> & texts, std::wstring & outError) { return AddRange (texts, outError); });
    m_list->SetOnMoveRow      ([this] (int from, int to) { MoveRow (from, to); });
    m_list->SetOnDeleteRow    ([this] (int row) { RemoveRange (row); });
    m_list->SetOnError        ([this] (const std::wstring & text) { SetError (TextEncoding::WideToUtf8 (text)); });

    m_error->SetTextRole  (DxuiTextRole::Error);
    m_error->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_editor->SetVisible   (false);
    m_editor->SetOverText  (true);
    m_editor->SetHwnd      (m_host.GetOwnerHwnd());
    m_editor->SetMaxLength (kEditMaxChars);

    SetMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::LoadSets
//
//  The saved sets, with the default set added where it is missing. A pane
//  with sets saved has been opened.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::LoadSets()
{
    IDebuggerWindowHost  * windowHost = m_host.GetWindowHost();
    std::string            text       = (windowHost != nullptr) ? windowHost->GetDebuggerHeatMapRanges() : std::string();



    m_isOpened = !text.empty();
    m_sets     = HeatMapRangeSets::FromText (text);
    (void) m_sets.TryAddDefaultSet();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::OnProjectSwitched
//
//  The other project's sets, on the set edited where it has one by that
//  name.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::OnProjectSwitched()
{
    LoadSets();

    m_editedSet = (m_sets.FindSet (m_editedSet) != nullptr || m_sets.sets.empty()) ? m_editedSet : m_sets.sets.front().name;

    ApplyRanges();
    SetMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::AddToDock
//
//  The bar, a place PlaceBar fills, over the reason an edit failed,
//  which is shown only then, over the list.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (GetTitle());
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_error, [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (kErrorDip); },
                      [this] { return !m_error->GetText().empty(); });
    m_frame->AddPart (m_list);
    m_frame->SetPartTextAligned (m_error);

    dock.AddPane (GetId(), GetTitle(), m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::Open
//
//  The setting, written now, opens the pane from then on.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::Open()
{
    m_isOpened = true;
    Save();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * HeatRangesPane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> HeatRangesPane::GetControls() const
{
    return { m_bar.GetSlot(), m_error, m_list, m_editor, m_bar.GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetLists
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> HeatRangesPane::GetLists() const
{
    return { m_list };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetEditor
//
//  The box open over a cell, or over the set's name on the bar.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextInput * HeatRangesPane::GetEditor() const
{
    return m_edit.isSetName ? m_editor : &m_list->GetEditor();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::IsEditing
//
////////////////////////////////////////////////////////////////////////////////

bool HeatRangesPane::IsEditing() const
{
    return m_edit.isSetName || m_list->IsEditing();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::Apply
//
//  The ranges read again only when the symbols they read against change.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    if (snapshot.heatMapSymbols != m_symbols)
    {
        m_symbols = snapshot.heatMapSymbols;
        ApplyRanges();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::OpenForShownSet
//
//  Edit ranges on the heat map's strip: the pane comes forward on the set
//  the map shows, or the one it last edited when the map shows all memory.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::OpenForShownSet()
{
    if (!m_sets.shown.empty())
    {
        EditRangeSet (m_sets.shown);
    }

    m_host.OpenPane (GetId());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::AddMenuItems
//
//  A built-in range cannot be deleted, so its Delete is disabled.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatRangesPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    if (list != m_list)
    {
        return false;
    }

    if (m_list->CanDeleteRow (row))
    {
        items.push_back ({ L"Delete", [this, row] { m_list->DeleteRow (row); m_host.Repaint(); } });
    }
    else if (row >= 0 && GetEditedSet() != nullptr && row < (int) GetEditedSet()->ranges.size())
    {
        items.push_back ({ L"Delete", nullptr });
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::SetMenus
//
//  The heat map's drop-down, All memory and then every set, the one shown
//  checked, and last Edit ranges, which opens the pane that makes them; and
//  the pane's, every set, the one edited checked. Built again
//  before either opens, since the rows carry the checks they were built
//  with.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::SetMenus()
{
    std::vector<DxuiPopupMenuItem>  shown;
    std::vector<DxuiPopupMenuItem>  edited;



    m_rangeSetCommands.clear();
    m_editedSetCommands.clear();

    m_rangeSetCommands.push_back (DebuggerMenuCommand::Make (TextEncoding::Utf8ToWide (HeatMapRangeSets::kpszAllMemory), m_sets.shown.empty(),
                                                   [this] { ShowRangeSet ({}); }));

    for (const HeatMapRangeSet & set : m_sets.sets)
    {
        std::string   name  = set.name;
        std::wstring  label = TextEncoding::Utf8ToWide (name);



        m_rangeSetCommands.push_back  (DebuggerMenuCommand::Make (label, m_sets.shown == name, [this, name] { ShowRangeSet (name); }));
        m_editedSetCommands.push_back (DebuggerMenuCommand::Make (label, m_editedSet     == name, [this, name] { EditRangeSet (name); }));
    }

    for (const std::shared_ptr<DxuiCommand> & command : m_rangeSetCommands)
    {
        shown.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_rangeSetCommands.push_back (DebuggerMenuCommand::Make (L"Edit ranges...", false, [this] { OpenForShownSet(); }));
    shown.push_back (DxuiPopupMenuItem::ForSeparator());
    shown.push_back (DxuiPopupMenuItem::ForCommand (m_rangeSetCommands.back()));

    for (const std::shared_ptr<DxuiCommand> & command : m_editedSetCommands)
    {
        edited.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_editedSetCommands.push_back (DebuggerMenuCommand::Make (L"New set...", false, [this] { RunBarEntry (HeatMapRangeBarCommands::kNewSet); }));

    if (!edited.empty())
    {
        edited.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    edited.push_back (DxuiPopupMenuItem::ForCommand (m_editedSetCommands.back()));

    m_heatMap.GetViewBar()->SetDropDownItems (HeatMapBarCommands::kRangeSet,  std::move (shown));
    m_bar.GetBar()->SetDropDownItems (HeatMapRangeBarCommands::kSet,  std::move (edited));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetBarLabel
//
//  The set drop-down reads the set edited; the rest keep their labels.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatRangesPane::GetBarLabel (int id) const
{
    if (id == HeatMapRangeBarCommands::kSet)
    {
        return HeatMapRangeBarCommands::GetSetLabel (TextEncoding::Utf8ToWide (m_editedSet));
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::IsBarEnabled
//
//  Only a set's own ranges can be edited or removed; a built-in one is
//  included or left out with its check. Moving does not go past either end.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatRangesPane::IsBarEnabled (int id) const
{
    const HeatMapRangeSet  * set      = GetEditedSet();
    std::optional<size_t>    selected = GetSelectedRange();
    int                      row      = m_list->GetSelectedRow();
    bool                     isOwn    = set != nullptr && selected.has_value() && *selected < set->ranges.size() &&
                                        !HeatMapRangeSets::IsBuiltIn (set->ranges[*selected]);



    switch (id)
    {
    case HeatMapRangeBarCommands::kSet:        return true;
    case HeatMapRangeBarCommands::kRenameSet:  return set != nullptr;
    case HeatMapRangeBarCommands::kDeleteSet:  return set != nullptr;
    case HeatMapRangeBarCommands::kEdit:       return isOwn;
    case HeatMapRangeBarCommands::kRemove:     return isOwn;
    case HeatMapRangeBarCommands::kUp:         return selected.has_value() && row > 0;
    case HeatMapRangeBarCommands::kDown:       return selected.has_value() && row + 1 < (int) m_rows.size();
    default:                                   return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::RunBarEntry
//
//  New set makes a set under a name no other has, then opens that name for
//  editing, as a new folder in Explorer does. The set is the drop-down's;
//  its rows act.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::RunBarEntry (int id)
{
    std::string  error;
    std::string  name;



    if (!IsBarEnabled (id))
    {
        return;
    }

    switch (id)
    {
    case HeatMapRangeBarCommands::kNewSet:
        name = m_sets.GetNewSetName();

        if (m_sets.TryAddSet (name, error))
        {
            m_editedSet = name;
            Save();
            ApplyRanges();
            SetMenus();
            BeginSetNameEdit (true);
        }

        break;

    case HeatMapRangeBarCommands::kRenameSet:
        BeginSetNameEdit (false);
        break;

    case HeatMapRangeBarCommands::kDeleteSet:
        if (!ConfirmDeleteSet())
        {
            break;
        }

        (void) m_sets.TryDeleteSet (m_editedSet);
        m_editedSet = m_sets.sets.empty() ? std::string() : m_sets.sets.front().name;
        Save();
        ApplyRanges();
        SetMenus();
        break;

    case HeatMapRangeBarCommands::kEdit:
        BeginEdit (m_list->GetSelectedRow(), HeatMapRangeSets::Field::Start);
        break;

    case HeatMapRangeBarCommands::kRemove:
        RemoveRange (m_list->GetSelectedRow());
        break;

    case HeatMapRangeBarCommands::kUp:    MoveRange (-1); break;
    case HeatMapRangeBarCommands::kDown:  MoveRange (1);  break;

    default:
        break;
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::AddRange
//
//  The grid's new row, added at the set's end once it reads as a range.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatRangesPane::AddRange (const std::vector<std::wstring> & texts, std::wstring & outError)
{
    HeatMapRangeSet  * set   = GetEditedSet();
    HeatMapRange       range;
    std::string        error;
    auto               field = [&texts] (HeatMapRangeSets::Field which) { return TextEncoding::WideToUtf8 (texts[(size_t) which + kFirstFieldColumn]); };



    if (set == nullptr)
    {
        outError = L"There is no set to add the range to.";
        return false;
    }

    if (!HeatMapRangeSets::TryMakeRange (field (HeatMapRangeSets::Field::Name), field (HeatMapRangeSets::Field::Start), field (HeatMapRangeSets::Field::End),
                                         field (HeatMapRangeSets::Field::Size), GetSymbols(), range, error))
    {
        outError = TextEncoding::Utf8ToWide (error);
        return false;
    }

    set->ranges.push_back (range);
    Save();
    ApplyRanges();
    m_host.Repaint();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::CommitCell
//
//  A cell edited in the grid, stored once the range reads with it.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatRangesPane::CommitCell (int row, int column, const std::wstring & text, std::wstring & outError)
{
    HeatMapRangeSet  * set = GetEditedSet();
    std::string        error;



    if (set == nullptr || row < 0 || row >= (int) m_rows.size() ||
        !HeatMapRangeSets::TryEditRange (*set, m_rows[(size_t) row], (HeatMapRangeSets::Field) (column - kFirstFieldColumn), TextEncoding::WideToUtf8 (text),
                                        GetSymbols(), error))
    {
        outError = TextEncoding::Utf8ToWide (error);
        return false;
    }

    Save();
    ApplyRanges();
    m_host.Repaint();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::RemoveRange
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::RemoveRange (int row)
{
    HeatMapRangeSet  * set = GetEditedSet();
    std::string        error;



    if (set == nullptr || row < 0 || row >= (int) m_rows.size())
    {
        return;
    }

    if (!HeatMapRangeSets::TryRemoveRange (*set, m_rows[(size_t) row], error))
    {
        SetError (error);
        return;
    }

    //  The rows are rebuilt before the save, since saving can paint the
    //  window, and a paint reads the rows against the set.
    SetError ({});
    ApplyRanges();
    Save();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::MoveRange
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::MoveRange (int delta)
{
    int  row = m_list->GetSelectedRow();



    MoveRow (row, row + delta);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::MoveRow
//
//  A row moves to another row's place, past every range between them in the
//  set, a hidden one too, as "My program" is with no symbols loaded.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::MoveRow (int row, int to)
{
    HeatMapRangeSet  * set    = GetEditedSet();
    int                delta  = (to < row) ? -1 : 1;
    size_t             index  = 0;
    size_t             target = 0;



    if (set == nullptr || row < 0 || to < 0 || to == row || row >= (int) m_rows.size() || to >= (int) m_rows.size())
    {
        return;
    }

    index  = m_rows[(size_t) row];
    target = m_rows[(size_t) to];

    while (index != target && HeatMapRangeSets::TryMoveRange (*set, index, delta))
    {
        index = (size_t) ((ptrdiff_t) index + delta);
    }

    Save();
    ApplyRanges();
    m_list->SetSelectedRow (to);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::BeginEdit
//
//  The grid's box over the cell, with what was typed for it: a range's
//  name, start or end, or its length for its size. A built-in range has
//  nothing of its own to edit.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::BeginEdit (int row, HeatMapRangeSets::Field field)
{
    if (m_list->BeginEdit (row, (int) field + kFirstFieldColumn))
    {
        m_host.FocusControl (m_list);
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::BeginSetNameEdit
//
//  The same box, over the set's drop-down on the bar.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::BeginSetNameEdit (bool isNew)
{
    RECT  entry = {};



    if (GetEditedSet() == nullptr)
    {
        return;
    }

    if (!m_bar.GetBar()->TryGetEntryRect (HeatMapRangeBarCommands::kSet, entry))
    {
        entry = m_bar.GetBar()->GetBounds();
    }

    m_edit = Edit { isNew, true };

    m_editor->SetTextRenderer (m_host.GetPaneTextRenderer());
    m_editor->SetFont         (DxuiTheme::kBodyFace, m_list->GetFontSizeDip());
    m_editor->SetText         (TextEncoding::Utf8ToWide (m_editedSet));
    m_editor->Layout          (entry, m_host.GetPaneScaler());
    m_editor->SetVisible      (true);
    m_host.FocusControl                  (m_editor);
    m_editor->SelectAll();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::EndEdit
//
//  A range's cell is the grid's to end. For the set's name, Enter stores
//  what was typed once it is a name no other set has, and the box stays
//  open with the reason under the bar when it is not; Escape cancels it.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::EndEdit (bool commit)
{
    std::string        typed   = TextEncoding::WideToUtf8 (m_editor->GetText());
    HeatMapRangeSet  * set     = GetEditedSet();
    std::string        renamed = m_editedSet;
    std::string        error;



    if (!m_edit.isSetName)
    {
        m_list->EndEdit (commit);
        m_host.Repaint();
        return;
    }

    if (commit && !m_sets.TryRenameSet (m_editedSet, typed, error))
    {
        SetError (error);
        return;
    }

    if (commit)
    {
        m_editedSet = (set != nullptr) ? set->name : m_editedSet;
        m_heatMap.RenameIgnoreSet (renamed, m_editedSet);
    }

    m_edit = Edit {};
    m_editor->SetVisible (false);
    m_host.FocusControl             (m_list);
    SetError             ({});
    Save();
    ApplyRanges();
    SetMenus();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::ToggleRange
//
//  A check in the Name column: a range in the set's map, or left out of it.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::ToggleRange (int row, bool included)
{
    HeatMapRangeSet  * set = GetEditedSet();



    if (set == nullptr || row < 0 || row >= (int) m_rows.size())
    {
        return;
    }

    set->ranges[m_rows[(size_t) row]].included = included;

    Save();
    ApplyRanges();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::ShowRangeSet
//
//  A choice on the heat map's drop-down: a set's ranges, or all memory for
//  an empty name.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::ShowRangeSet (const std::string & name)
{
    m_sets.shown = (m_sets.FindSet (name) != nullptr) ? name : std::string();

    Save();
    ApplyRanges();
    SetMenus();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::EditRangeSet
//
//  A choice on the pane's drop-down: which set its list shows and edits.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::EditRangeSet (const std::string & name)
{
    if (IsEditing())
    {
        EndEdit (false);
    }

    m_editedSet = (m_sets.FindSet (name) != nullptr) ? name : m_editedSet;

    m_list->SetSelectedRow (-1);
    ApplyRows();
    SetMenus();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::ApplyRanges
//
//  The map shows the shown set's ranges as they read against the symbols
//  now, or all memory; the pane lists the set it edits. The set whose reads
//  before written are left out reads again too.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::ApplyRanges()
{
    std::vector<HeatMapView::Band>  bands;



    m_heatMap.SendIgnore();

    if (m_sets.shown.empty())
    {
        m_heatMap.GetView()->ClearRanges();
        ApplyRows();
        return;
    }

    for (const HeatMapRangeSets::Span & span : m_sets.GetShownSpans (GetSymbols()))
    {
        bands.push_back ({ TextEncoding::Utf8ToWide (span.name), span.first, (int) span.last - (int) span.first + 1 });
    }

    m_heatMap.GetView()->SetRanges (std::move (bands));
    ApplyRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::ApplyRows
//
//  A row for each range of the set edited, "My program" only while there
//  is a program: its check and name, then where it reads as starting and
//  ending and its size. A start or end typed as other than its own number
//  follows it, dimmed. A range that does not read gives why across the rest
//  of its row, in the error color.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::ApplyRows()
{
    constexpr size_t                                kColumns  = 6;
    const HeatMapRangeSet                         * set       = GetEditedSet();
    const HeatMapSymbols                          & symbols   = GetSymbols();
    uint32_t                                        error     = (m_host.GetTheme() != nullptr) ? m_host.GetTheme()->ErrorForeground() : 0;
    std::wstring                                    separator = L"  ";
    std::vector<DxuiEditableGrid::GridRow>          rows;



    m_rows.clear();

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
        cells[1].text  = TextEncoding::Utf8ToWide (HeatMapRangeSets::GetName (range));
        cells[1].dim   = !HeatMapRangeSets::IsBuiltIn (range) && range.name.empty();
        cells[5].text  = HeatMapRangeSets::IsBuiltIn (range) ? L"Yes" : L"";

        if (!isRead)
        {
            cells[2].text     = TextEncoding::Utf8ToWide (range.start);
            cells[3].text     = TextEncoding::Utf8ToWide (reason);
            cells[3].argb     = error;
            cells[3].spansRow = true;
            cells[3].tip      = cells[3].text;
        }
        else
        {
            cells[2].text = std::format (L"${:04X}", first);
            cells[3].text = std::format (L"${:04X}", last);
            cells[4].text = std::format (L"${:04X}", (uint32_t) last - first + 1);

            for (const auto & [cell, typed] : { std::pair { &cells[2], range.start }, std::pair { &cells[3], range.end } })
            {
                if (!typed.empty() && !HeatMapRangeSets::IsHexNumber (typed))
                {
                    std::wstring  shown = TextEncoding::Utf8ToWide (typed);



                    cell->dimRanges.push_back ({ (int) (cell->text.size() + separator.size()), (int) (cell->text.size() + separator.size() + shown.size()) });
                    cell->text += separator + shown;
                }
            }
        }

        m_rows.push_back (i);
        rows.push_back ({ std::move (cells),
                          { std::wstring(), TextEncoding::Utf8ToWide (range.name), TextEncoding::Utf8ToWide (range.start), TextEncoding::Utf8ToWide (range.end),
                            TextEncoding::Utf8ToWide (range.end.starts_with ('+') ? range.end.substr (1) : std::string()) },
                          HeatMapRangeSets::IsBuiltIn (range) });
    }

    m_list->SetGridRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::Save
//
//  Every change is kept as it is made.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::Save()
{
    if (m_host.GetWindowHost() != nullptr)
    {
        m_host.GetWindowHost()->SetDebuggerHeatMapRanges (m_sets.ToText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::SetError
//
//  The line under the bar, which shows only while it has something to say.
//
////////////////////////////////////////////////////////////////////////////////

void HeatRangesPane::SetError (const std::string & error)
{
    std::wstring  what = m_edit.isSetName ? L"Invalid set name: " : L"Invalid range: ";



    m_error->SetText (error.empty() ? std::wstring() : what + TextEncoding::Utf8ToWide (error));

    if (m_frame != nullptr)
    {
        m_frame->Relayout();
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetError
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatRangesPane::GetError() const
{
    return m_error->GetText();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetEditedSet
//
////////////////////////////////////////////////////////////////////////////////

HeatMapRangeSet * HeatRangesPane::GetEditedSet()
{
    return m_sets.FindSet (m_editedSet);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetEditedSet
//
////////////////////////////////////////////////////////////////////////////////

const HeatMapRangeSet * HeatRangesPane::GetEditedSet() const
{
    return m_sets.FindSet (m_editedSet);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetSymbols
//
//  The copy the last snapshot carried, or none before the first, which
//  reads numbers alone.
//
////////////////////////////////////////////////////////////////////////////////

const HeatMapSymbols & HeatRangesPane::GetSymbols() const
{
    static const HeatMapSymbols  none;



    return (m_symbols != nullptr) ? *m_symbols : none;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::GetSelectedRange
//
//  The selected row's range, as its place in the set.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> HeatRangesPane::GetSelectedRange() const
{
    int  row = m_list->GetSelectedRow();



    if (row < 0 || row >= (int) m_rows.size())
    {
        return std::nullopt;
    }

    return m_rows[(size_t) row];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::RouteEditMouse
//
//  As a watch's edit: the set name's box takes the pointer while it is over
//  it, and a press or a wheel anywhere else commits what was typed and goes
//  on to whatever is there. The grid routes its own box.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatRangesPane::RouteEditMouse (const DxuiMouseEvent & ev)
{
    RECT  box = m_editor->GetBounds();



    if (!m_edit.isSetName)
    {
        return false;
    }

    if (DxuiDockSite::Contains (box, ev.positionDip))
    {
        m_editor->OnMouse (ev);
        m_host.Repaint();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel)
    {
        EndEdit (true);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::RouteEditKey
//
//  The set name's edit takes every key: Enter commits what was typed and
//  Escape cancels it. The grid takes every key while a cell is edited, and
//  otherwise Delete, Insert, F2 and Enter, as in Visual Studio's tool
//  windows.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatRangesPane::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    bool  isPlain = ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt;



    if (m_edit.isSetName)
    {
        if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE))
        {
            EndEdit (ev.vk == VK_RETURN);
        }
        else
        {
            m_editor->OnKey (ev);
        }

        m_host.Repaint();
        return true;
    }

    if (focused != m_list)
    {
        return false;
    }

    if (m_list->IsEditing() ||
        (isPlain && (ev.vk == VK_DELETE || ev.vk == VK_INSERT || ev.vk == VK_F2 || ev.vk == VK_RETURN)))
    {
        (void) m_list->OnKey (ev);
        m_host.Repaint();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane::ConfirmDeleteSet
//
//  A set and its ranges go only once the user presses Delete, in a box
//  centered on the pane's window, floating or not; Cancel is the default.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatRangesPane::ConfirmDeleteSet()
{
    DxuiWindow    * host     = m_host.GetPaneWindow (DebuggerLayout::kHeatRanges);
    std::wstring    question = std::format (L"Delete the range set \"{}\"?", TextEncoding::Utf8ToWide (m_editedSet));
    int             answer   = IDCANCEL;



    answer = m_host.ShowMessageBox ((host != nullptr) ? host->GetHwnd() : m_host.GetOwnerHwnd(), question, L"Delete range set", MB_ICONWARNING,
                                    { { L"&Delete", IDYES }, { L"&Cancel", IDCANCEL } }, IDCANCEL);

    return answer == IDYES;
}





