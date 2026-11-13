#include "Pch.h"

#include "Ui/Debugger/Panes/BreakpointsPane.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Ui/Debugger/BreakpointDefinitionCheck.h"
#include "Ui/Debugger/UndoText.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Debugger/Source/SourcePathList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::BreakpointsPane
//
////////////////////////////////////////////////////////////////////////////////

BreakpointsPane::BreakpointsPane (IDebuggerPaneHost & host) :
    IDebuggerPane (host),
    m_bar         (host, DebuggerLayout::kBreakpoints, L"Breakpoint commands"),
    m_filters     (host)
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

const std::wstring & BreakpointsPane::GetId() const
{
    static const std::wstring  s_kId = DebuggerLayout::kBreakpoints;



    return s_kId;
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
    handlers.getTip    = [this] (int id)
    {
        const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
        bool                          isRedo   = id == BreakpointBarCommands::kRedo;
        std::wstring                  text     = (snapshot == nullptr) ? std::wstring() : isRedo ? snapshot->redoBreakpointsText : snapshot->undoBreakpointsText;



        return ((id == BreakpointBarCommands::kUndo || isRedo) && !text.empty()) ? UndoText::GetLabel (isRedo, text) : std::wstring();
    };

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

    m_filters.Attach (m_list, { (size_t) BreakpointColumns::Column::Kind, (size_t) BreakpointColumns::Column::Trigger, (size_t) BreakpointColumns::Column::File },
                      [this] { ApplyRows(); });

    //  A double-click, anywhere on the row, goes to where the breakpoint is:
    //  its source line, or its address in the disassembly. Rename and F2
    //  edit the name, and Settings... the rest.
    m_list->SetOnActivateRow ([this] (int row)
    {
        const DebuggerViewSnapshot::BreakpointLine  * bp       = GetBreakpointOfRow (row);
        const DebuggerViewSnapshot                  * snapshot = m_host.GetLatestSnapshot();
        int                                           fileId   = -1;
        int                                           line     = 0;

        if (bp != nullptr && snapshot != nullptr && BreakpointColumns::TryGetSourcePlace (*snapshot, *bp, fileId, line))
        {
            m_host.OpenSource (fileId, line);
        }
        else if (bp != nullptr && (BreakpointColumns::HasAddress (bp->info) || BreakpointColumns::HasRoutine (bp->info)))
        {
            m_host.GoToCode (bp->address);
        }
    });

    m_list->SetSelectOnCheckPress (false);
    m_list->SetOnCheckToggled ([this] (int row, size_t, bool checked)
    {
        const DebuggerViewSnapshot::BreakpointLine  * bp = GetBreakpointOfRow (row);

        //  A session stop has no off state of its own; unchecked, it is cleared.
        if (bp != nullptr && bp->id < 0 && !checked)
        {
            RunStep ({ BreakpointStep::Kind::Actions, { GetClearAction (*bp) }, {} });
        }
        else if (bp != nullptr && bp->id >= 0)
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
//  BreakpointsPane::GroupOrder
//
//  A heading is the group's name and how many breakpoints are in it. With no
//  group, no headings. A group collapsed before is collapsed again, found by
//  its name, since the list finds a group by its heading, which has the count.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Group> BreakpointsPane::GroupOrder (const DebuggerViewSnapshot & snapshot)
{
    std::vector<DxuiListView::Group>  groups;
    std::vector<size_t>               order;
    std::vector<std::string>          collapsed;



    for (size_t i = 0; i < m_groupNames.size() && i < m_list->GetGroups().size(); i++)
    {
        if (m_list->IsGroupCollapsed ((int) i))
        {
            collapsed.push_back (m_groupNames[i]);
        }
    }

    m_groupNames.clear();

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (!bp.info.group.empty() && std::ranges::none_of (m_groupNames, [&bp] (const std::string & each) { return BreakpointTable::IsSameGroup (each, bp.info.group); }))
        {
            m_groupNames.push_back (bp.info.group);
        }
    }

    std::ranges::copy_if (m_order, std::back_inserter (order), [&snapshot] (size_t index) { return snapshot.breakpoints[index].info.group.empty(); });

    for (const std::string & group : m_groupNames)
    {
        size_t  first = order.size();

        std::ranges::copy_if (m_order, std::back_inserter (order), [&snapshot, &group] (size_t index) { return BreakpointTable::IsSameGroup (snapshot.breakpoints[index].info.group, group); });
        groups.push_back ({ std::format (L"{} ({})", TextEncoding::NarrowToWide (group), order.size() - first), (int) first,
                            std::ranges::any_of (collapsed, [&group] (const std::string & each) { return BreakpointTable::IsSameGroup (each, group); }) });
    }

    m_order = std::move (order);
    return groups;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::AddGroupItems
//
//  Add to breakpoint group, a submenu of the groups there are, New group...,
//  and Remove from group while a selected breakpoint is in one; each acts on
//  every selected breakpoint, as one undo step.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::AddGroupItems (const std::vector<DebuggerViewSnapshot::BreakpointLine> & selected, PaneMenuItems & items)
{
    static constexpr const wchar_t *  kSubmenu = L"Add to breakpoint group";
    std::string                       ids      = MakeIdList (selected);
    bool                              isIn     = std::ranges::any_of (selected, [] (const DebuggerViewSnapshot::BreakpointLine & each) { return !each.info.group.empty(); });



    if (ids.empty())
    {
        return;
    }

    for (const std::string & group : m_groupNames)
    {
        items.push_back ({ PaneMenu::MakeInSubmenu (kSubmenu, TextEncoding::NarrowToWide (group)), [this, group, ids] { RunGroupAction (std::format ("\"{}\" {}", group, ids)); } });
    }

    items.push_back ({ PaneMenu::MakeInSubmenu (kSubmenu, L"New group..."), [this, ids]
    {
        std::optional<std::string>  name = AskGroupName (L"New breakpoint group", L"A name for the group");

        if (name.has_value())
        {
            RunGroupAction (std::format ("\"{}\" {}", *name, ids));
        }
    } });

    if (isIn)
    {
        items.push_back ({ L"Remove from group", [this, ids] { RunGroupAction ("! " + ids); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::AddHeadingItems
//
//  A group heading's menu: Enable all, Disable all, Rename group...,
//  Ungroup, and Delete group and its breakpoints, after a confirmation.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::AddHeadingItems (const std::string & group, PaneMenuItems & items)
{
    items.push_back ({ L"Enable all",  [this, group] { RunGroupAction (std::format ("ENABLE \"{}\"", group)); } });
    items.push_back ({ L"Disable all", [this, group] { RunGroupAction (std::format ("DISABLE \"{}\"", group)); } });
    items.push_back ({ L"", {} });

    items.push_back ({ L"Rename group...", [this, group]
    {
        std::optional<std::string>  name = AskGroupName (L"Rename breakpoint group", TextEncoding::NarrowToWide (group));

        if (name.has_value())
        {
            RunGroupAction (std::format ("RENAME \"{}\" \"{}\"", group, *name));
        }
    } });

    items.push_back ({ L"Ungroup", [this, group] { RunGroupAction (std::format ("UNGROUP \"{}\"", group)); } });

    items.push_back ({ L"Delete group and its breakpoints", [this, group]
    {
        DxuiWindow  * window = m_host.GetPaneWindow (DebuggerLayout::kBreakpoints);
        int           answer = m_host.ShowMessageBox ((window != nullptr) ? window->GetHwnd() : m_host.GetOwnerHwnd(),
                                                      std::format (L"Delete group \"{}\" and its breakpoints?", TextEncoding::NarrowToWide (group)), L"Delete breakpoint group",
                                                      MB_ICONWARNING, { { L"&Delete", IDYES }, { L"&Cancel", IDCANCEL } }, IDCANCEL);

        if (answer == IDYES)
        {
            RunGroupAction (std::format ("DELETE \"{}\"", group));
        }
    } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::RunGroupAction
//
//  BPGROUP as one step on the pane's undo list.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::RunGroupAction (const std::string & arguments)
{
    RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetGroupBreakpoints (arguments, m_host.GetMode()) }, {} });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::AskGroupName
//
//  A name with no quote in it, since BPGROUP takes a quoted name.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> BreakpointsPane::AskGroupName (const std::wstring & title, const std::wstring & placeholder)
{
    std::optional<std::wstring>  typed = m_host.RunTextDialog (title, L"Name", placeholder, [] (const std::wstring & text, std::wstring & error)
    {
        error = L"Type a name with no quotes in it.";
        return text.find_first_not_of (L" \t") != std::wstring::npos && text.find (L'"') == std::wstring::npos;
    });



    if (!typed.has_value())
    {
        return std::nullopt;
    }

    return TextEncoding::WideToNarrow (*typed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::MakeIdList
//
//  The ids of the table entries among the lines, apart by commas; a
//  session stop has no id and isn't listed.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointsPane::MakeIdList (const std::vector<DebuggerViewSnapshot::BreakpointLine> & lines)
{
    std::string  ids;



    for (const DebuggerViewSnapshot::BreakpointLine & line : lines)
    {
        if (line.id >= 0)
        {
            ids += (ids.empty() ? "" : ",") + std::to_string (line.id);
        }
    }

    return ids;
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
    POINT                                         press    = m_host.GetLastPressPx();
    int                                           heading  = (row < 0) ? m_list->HitTestGroupHeader (press.x - m_list->GetBounds().left, press.y - m_list->GetBounds().top) : -1;



    if (list != m_list)
    {
        return false;
    }

    //  A right-click on a group's heading.
    if (found == nullptr && heading >= 0 && heading < (int) m_groupNames.size())
    {
        AddHeadingItems (m_groupNames[(size_t) heading], items);
        return true;
    }

    if (found == nullptr)
    {
        return true;
    }

    bp = *found;

    //  In Visual Studio's order: Delete, the two Go to rows, Settings and the
    //  name, then the rows Visual Studio doesn't have.
    items.push_back ({ L"Delete", [this, bp] { RunStep ({ BreakpointStep::Kind::Actions, { GetClearAction (bp) }, {} }); } });
    items.push_back ({ L"", {} });

    if (BreakpointColumns::TryGetSourcePlace (*snapshot, bp, fileId, line))
    {
        items.push_back ({ L"Go to source code", [this, fileId, line] { m_host.OpenSource (fileId, line); } });
    }

    if (BreakpointColumns::HasAddress (bp.info))
    {
        items.push_back ({ L"Go to disassembly", [this, bp] { m_host.GoToCode (bp.address); } });
    }

    items.push_back ({ L"", {} });

    //  Its type and the fields of that type, in a dialog; the result is
    //  the definition BPEDIT takes, as a person could have typed it
    //  (FR-094).
    if (bp.info.kind != BreakpointKind::UnwrittenRead)
    {
        items.push_back ({ L"Settings...", [this, bp] { EditInDialog (bp); } });
    }

    //  A session stop is no table entry: it has no off state, no name and no
    //  id, and a new definition replaces it.
    if (bp.id >= 0)
    {
        items.push_back ({ L"Rename", [this, bp] { BeginNameEdit (GetRowOfId (bp.id)); } });
        AddGroupItems (GetSelected(), items);
        items.push_back ({ L"", {} });
        items.push_back ({ bp.enabled ? L"Disable" : L"Enable", [this, bp] { RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetEnableBreakpoint (bp.id, !bp.enabled, m_host.GetMode()) }, {} }); } });
    }

    if (BreakpointColumns::HasAddress (bp.info))
    {
        m_host.AddBookmarkItem (bp.address, items);
    }

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
                                                                          [] (const DebuggerViewSnapshot::BreakpointLine & bp) { return bp.id >= 0 && bp.enabled; });



    if (!IsBarEntryEnabled (id))
    {
        return;
    }

    switch (id)
    {
    case BreakpointBarCommands::kDelete:
        for (const DebuggerViewSnapshot::BreakpointLine & bp : selected)
        {
            step.actions.push_back (GetClearAction (bp));
        }

        RunStep (std::move (step));
        break;

    case BreakpointBarCommands::kDeleteAll:
        if (ConfirmDeleteAll())
        {
            RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetClearAllBreakpoints (mode) }, {} });
        }

        break;

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
//  BreakpointsPane::ConfirmDeleteAll
//
//  Every breakpoint goes only once the user presses Delete; Cancel is the
//  default.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointsPane::ConfirmDeleteAll()
{
    DxuiWindow  * window = m_host.GetPaneWindow (DebuggerLayout::kBreakpoints);
    int           answer = IDCANCEL;



    answer = m_host.ShowMessageBox ((window != nullptr) ? window->GetHwnd() : m_host.GetOwnerHwnd(), L"Delete all breakpoints?", L"Delete all breakpoints",
                                    MB_ICONWARNING, { { L"&Delete", IDYES }, { L"&Cancel", IDCANCEL } }, IDCANCEL);

    return answer == IDYES;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::SetBarMenus
//
//  Execution's types at the top, then Memory, BASIC and Hardware as
//  submenus. A type with fields opens the breakpoint dialog on it; one with
//  none is set or cleared at once and checked while on. Built again before
//  the drop-down opens, so the checks are current.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::SetBarMenus()
{
    m_bar.GetBar()->SetDropDownItems (BreakpointBarCommands::kNew, GetNewMenuItems());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetNewMenuItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> BreakpointsPane::GetNewMenuItems()
{
    std::vector<DxuiPopupMenuItem>  items;
    std::vector<DxuiPopupMenuItem>  submenu;
    std::wstring                    heading;
    auto                            close = [&]
    {
        if (!heading.empty() && heading != L"Execution")
        {
            items.push_back (DxuiPopupMenuItem::ForSubmenu (DebuggerMenuCommand::Make (heading, [] {}), std::move (submenu)));
        }

        submenu.clear();
    };



    for (const BreakpointDialog::TypeRow & row : BreakpointDialog::GetTypeRows())
    {
        if (!row.type.has_value())
        {
            close();
            heading = row.label;
            continue;
        }

        (heading == L"Execution" ? items : submenu).push_back (DxuiPopupMenuItem::ForCommand (MakeTypeCommand (*row.type, row.label)));
    }

    close();
    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetSourceFiles
//
//  The loaded debug file's sources by name, for a breakpoint on a line;
//  empty without a debug file.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> BreakpointsPane::GetSourceFiles (const DebuggerViewSnapshot * snapshot)
{
    std::vector<std::wstring>  files;



    if (snapshot != nullptr && snapshot->source.has_value())
    {
        for (const DebugSourceFile & file : snapshot->source->files)
        {
            files.push_back (SourcePathList::Utf8ToWide (file.name));
        }
    }

    return files;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::MakeTypeCommand
//
//  BRK, BRK on interrupt, Reading uninitialized memory and BASIC errors take
//  only a condition, which Edit adds; they toggle with ON and OFF.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> BreakpointsPane::MakeTypeCommand (BreakpointDialog::Type type, const wchar_t * label)
{
    using Type = BreakpointDialog::Type;

    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    BreakpointKind                kind     = (type == Type::Brk)           ? BreakpointKind::Brk
                                           : (type == Type::Interrupt)     ? BreakpointKind::Interrupt
                                           : (type == Type::UnwrittenRead) ? BreakpointKind::UnwrittenRead
                                           :                                 BreakpointKind::BasicError;
    const char                  * verb     = (type == Type::Brk)           ? "BRK"
                                           : (type == Type::Interrupt)     ? "BRKINT"
                                           : (type == Type::UnwrittenRead) ? "BRKUNINIT"
                                           :                                 "BRKBASIC";
    bool                          isToggle = type == Type::Brk || type == Type::Interrupt || type == Type::UnwrittenRead || type == Type::BasicError;
    bool                          isOn     = snapshot != nullptr && std::ranges::any_of (snapshot->breakpoints,
                                                 [kind] (const DebuggerViewSnapshot::BreakpointLine & bp) { return bp.info.kind == kind; });



    //  A source line comes from the debug file's sources, so without one the
    //  row is disabled and its tip has why.
    if (type == Type::SourceLine)
    {
        auto  command = DebuggerMenuCommand::Make (std::wstring (label) + L"...", [this, type] { NewBreakpoint (type); });
        bool  hasFile = !GetSourceFiles (snapshot).empty();

        command->isEnabled = [hasFile] { return hasFile; };
        command->tip       = hasFile ? std::wstring() : std::wstring (BreakpointDialog::s_kNoSourceTip);
        return command;
    }

    if (!isToggle)
    {
        return DebuggerMenuCommand::Make (std::wstring (label) + L"...", [this, type] { NewBreakpoint (type); });
    }

    return DebuggerMenuCommand::Make (label, isOn, [this, verb, isOn]
    {
        RunStep ({ BreakpointStep::Kind::Actions, { DebuggerActions::GetDefineBreakpoint (std::format ("{} {}", verb, isOn ? "OFF" : "ON"), m_host.GetMode()) }, {} });
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetClearAction
//
//  BPC for a table entry; the OFF form for a session stop.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointsPane::GetClearAction (const DebuggerViewSnapshot::BreakpointLine & bp) const
{
    const char  * off = (bp.info.kind == BreakpointKind::UnwrittenRead) ? "BRKUNINIT OFF"
                      : (bp.info.kind == BreakpointKind::VideoLine)     ? "BPV OFF"
                      :                                                   "BPBEAM OFF";



    return (bp.id >= 0) ? DebuggerActions::GetClearBreakpoint (bp.id, m_host.GetMode()) : DebuggerActions::GetDefineBreakpoint (off, m_host.GetMode());
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
    const DebuggerViewSnapshot               * snapshot = m_host.GetLatestSnapshot();
    std::optional<BreakpointDialog::Result>    result;



    result = BreakpointDialog::RunNew (m_host.GetOwnerHwnd(), m_host.GetTheme(), type, (snapshot != nullptr) ? snapshot->pc : (Word) 0, MakeValidator (std::nullopt),
                                       GetSourceFiles (snapshot));

    //  The dialog is modal, so the latest snapshot is read again for the id
    //  the new breakpoint takes.
    snapshot = m_host.GetLatestSnapshot();

    if (result.has_value())
    {
        RunStep ({ BreakpointStep::Kind::Actions, GetDialogActions (*result, std::nullopt, BreakpointInfo {}, (snapshot != nullptr) ? snapshot->nextBreakpointId : 0, m_host.GetMode()), {} });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::EditInDialog
//
//  The breakpoint's dialog, opened as a breakpoint set on a source line
//  when it was one; OK runs the edit.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::EditInDialog (const DebuggerViewSnapshot::BreakpointLine & bp)
{
    std::optional<int>                            editedId = (bp.id >= 0) ? std::optional<int> (bp.id) : std::nullopt;
    const DebuggerViewSnapshot                  * snapshot = m_host.GetLatestSnapshot();
    std::optional<BreakpointDialog::SourcePlace>  place;
    std::optional<BreakpointDialog::Result>       result;
    std::string                                   file;
    int                                           line     = 0;



    if (snapshot != nullptr && BreakpointColumns::TryGetSourceLine (*snapshot, bp.id, file, line))
    {
        place = BreakpointDialog::SourcePlace { SourcePathList::Utf8ToWide (file), line };
    }

    result   = BreakpointDialog::RunModal (m_host.GetOwnerHwnd(), m_host.GetTheme(), bp.info, std::nullopt, MakeValidator (editedId), GetSourceFiles (snapshot), place);
    snapshot = m_host.GetLatestSnapshot();

    if (result.has_value())
    {
        RunStep ({ BreakpointStep::Kind::Actions, GetDialogActions (*result, editedId, bp.info, (snapshot != nullptr) ? snapshot->nextBreakpointId : 0, m_host.GetMode()), {} });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetDialogActions
//
//  The definition, as an edit of the breakpoint or a new one, then BPLOG
//  where the message changed, or whether a hit stops changed with a message
//  set. Without a message, Don't break is the breakpoint's flag, set with
//  BPCHANGE; a breakpoint stops after BPLOG CLEAR, so a cleared message is
//  followed by BPCHANGE too. A new breakpoint's message and flag
//  are set on the id it takes.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerAction> BreakpointsPane::GetDialogActions (const BreakpointDialog::Result & result, std::optional<int> editedId, const BreakpointInfo & original,
                                                               int nextId, CommandMode mode)
{
    std::vector<DebuggerAction>  actions;
    int                          id           = editedId.value_or (nextId);
    bool                         wasStopping  = !editedId.has_value() || original.stops;
    bool                         isStopping   = !result.isLogGo;
    bool                         isLogChanged = result.log != original.log || (!result.log.empty() && isStopping != wasStopping);
    bool                         isStopsAfter = isLogChanged ? true : wasStopping;



    actions.push_back (editedId.has_value() ? DebuggerActions::GetEditBreakpoint (*editedId, result.definition, mode)
                                            : DebuggerActions::GetDefineBreakpoint (result.definition, mode));

    if (isLogChanged)
    {
        actions.push_back (DebuggerActions::GetLogBreakpoint (id, result.log, result.isLogGo, mode));
    }

    if (result.log.empty() && isStopping != isStopsAfter)
    {
        actions.push_back (DebuggerActions::GetChangeBreakpoint (id, !editedId.has_value() || original.enabled, editedId.has_value() && original.temporary, isStopping, mode));
    }

    return actions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::MakeValidator
//
//  The dialog's check, against the latest snapshot when OK is pressed.
//
////////////////////////////////////////////////////////////////////////////////

BreakpointDialog::Validator BreakpointsPane::MakeValidator (std::optional<int> editedId)
{
    return [this, editedId] (const std::string & definition)
    {
        const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();



        return (snapshot != nullptr) ? BreakpointDefinitionCheck::GetError (definition, *snapshot, editedId) : std::string();
    };
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



    if (bp == nullptr || bp->id < 0 || !m_list->GetCellTextRectPx (row, 0, cell))
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
//  Visual Studio, and Delete, Ctrl+Z and Ctrl+Y on it delete, undo and redo.
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

        m_host.RepaintLater();
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && focused == m_list && !ev.ctrl && !ev.alt && ev.vk == VK_F2)
    {
        BeginNameEdit (m_list->GetSelectedRow());
        return true;
    }

    //  Delete, Ctrl+Z and Ctrl+Y run the bar's Delete, Undo and Redo, each
    //  only while it is enabled, as the bar's entries are.
    if (ev.kind == DxuiKeyEventKind::Down && focused == m_list && !ev.alt && !ev.shift)
    {
        int  id = (!ev.ctrl && ev.vk == VK_DELETE) ? BreakpointBarCommands::kDelete
                : ( ev.ctrl && ev.vk == 'Z')       ? BreakpointBarCommands::kUndo
                : ( ev.ctrl && ev.vk == 'Y')       ? BreakpointBarCommands::kRedo
                :                                    -1;

        if (id >= 0)
        {
            RunBarEntry (id);
            return true;
        }
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
    m_filters.ShowState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetRowKeys
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> BreakpointsPane::GetRowKeys() const
{
    std::vector<std::wstring>  keys;



    for (int id : m_rowIds)
    {
        keys.push_back (std::to_wstring (id));
    }

    return keys;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::FilterOrder
//
//  Each filtering column's values are those of every breakpoint, so a hidden
//  value stays in the heading's menu; then each breakpoint with a hidden
//  value is taken out of the order.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::FilterOrder (const std::vector<BreakpointColumns::Cells> & allCells)
{
    static constexpr BreakpointColumns::Column  s_kFiltered[] = { BreakpointColumns::Column::Kind, BreakpointColumns::Column::Trigger, BreakpointColumns::Column::File };



    for (BreakpointColumns::Column column : s_kFiltered)
    {
        std::vector<std::wstring>  values;

        for (const BreakpointColumns::Cells & cells : allCells)
        {
            values.push_back (TextEncoding::NarrowToWide (cells[(size_t) column]));
        }

        m_filters.SetValues ((size_t) column, values);
    }

    std::erase_if (m_order, [this, &allCells] (size_t index)
    {
        return index >= allCells.size() || std::ranges::any_of (s_kFiltered, [this, &allCells, index] (BreakpointColumns::Column column)
        {
            return !m_filters.IsShown ((size_t) column, TextEncoding::NarrowToWide (allCells[index][(size_t) column]));
        });
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::ApplyRows
//
//  A checkbox and the code views' gutter mark ahead of the name, then a
//  cell for each column, in the order the chosen heading sorts them, without
//  the breakpoints hidden by the headings' filters.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointsPane::ApplyRows()
{
    const DebuggerViewSnapshot                    * snapshot = m_host.GetLatestSnapshot();
    std::vector<std::vector<DxuiListView::Cell>>    rows;
    std::vector<BreakpointColumns::Cells>           allCells;
    std::vector<int>                                selectedIds;
    std::vector<int>                                selectedRows;
    std::vector<DxuiListView::Group>                groups;
    int                                             anchorId = GetIdOfRow (m_list->GetAnchorRow());
    int                                             rowId    = GetIdOfRow (m_list->GetSelectedRow());
    const DxuiTheme                               * theme    = m_host.GetTheme();



    if (snapshot == nullptr)
    {
        return;
    }

    //  The selection is taken by breakpoint before the rows move, and set
    //  again by breakpoint after, since a sort can put each in another row.
    for (int row : m_list->GetSelectedRows())
    {
        selectedIds.push_back (GetIdOfRow (row));
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

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot->breakpoints)
    {
        allCells.push_back (BreakpointColumns::GetCells (*snapshot, bp));
    }

    FilterOrder (allCells);
    groups = GroupOrder (*snapshot);

    for (size_t index : m_order)
    {
        const DebuggerViewSnapshot::BreakpointLine  & bp    = snapshot->breakpoints[index];
        const BreakpointColumns::Cells              & cells = allCells[index];
        std::vector<DxuiListView::Cell>               row;

        //  A breakpoint on a BASIC line the program does not have is drawn as
        //  one that can't be bound: in the disabled color and italic, with its
        //  checkbox disabled and the disabled breakpoint's icon.
        for (const std::string & text : cells)
        {
            DxuiListView::Cell  cell;

            cell.text   = TextEncoding::NarrowToWide (text);
            cell.dim    = !bp.enabled && !bp.isAbsent;
            cell.argb   = (bp.isAbsent && theme != nullptr) ? theme->ForegroundDisabled() : 0;
            cell.weight = bp.isAbsent ? DxuiFontWeight::Italic : DxuiFontWeight::Normal;
            row.push_back (cell);
        }

        row[(size_t) BreakpointColumns::Column::Name].check = DxuiListView::Cell::GetCheck (bp.enabled, bp.isAbsent);
        row[(size_t) BreakpointColumns::Column::Name].icon            = m_host.GetBreakpointIcon (bp.enabled && !bp.isAbsent);
        rows.push_back (std::move (row));
    }

    m_rowIds.clear();

    for (size_t index : m_order)
    {
        m_rowIds.push_back (snapshot->breakpoints[index].id);
    }

    m_list->SetRows   (std::move (rows));
    m_list->SetGroups (groups);
    m_list->ShowNewRow (GetRowKeys());

    for (int id : selectedIds)
    {
        if (GetRowOfId (id) >= 0)
        {
            selectedRows.push_back (GetRowOfId (id));
        }
    }

    if (!selectedRows.empty() || rowId != kNoId)
    {
        m_list->RestoreSelectedRows (selectedRows, GetRowOfId (anchorId), GetRowOfId (rowId));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetIdOfRow
//
//  The id of the breakpoint in the row when the rows were last set;
//  kNoId for no row.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointsPane::GetIdOfRow (int row) const
{
    return (row >= 0 && row < (int) m_rowIds.size()) ? m_rowIds[(size_t) row] : kNoId;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointsPane::GetRowOfId
//
//  The row of the breakpoint with that id, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointsPane::GetRowOfId (int id) const
{
    auto  found = std::ranges::find (m_rowIds, id);



    return (id != kNoId && found != m_rowIds.end()) ? (int) (found - m_rowIds.begin()) : -1;
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

    //  A hidden column has no heading to clear its filter from.
    if (!m_shown[(size_t) column])
    {
        m_filters.Clear ((size_t) column);
    }

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
