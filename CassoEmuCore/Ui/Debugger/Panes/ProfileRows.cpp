#include "Pch.h"

#include "Ui/Debugger/Panes/ProfileRows.h"

#include "Core/TextEncoding.h"
#include "Debugger/SymbolTable.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "Ui/Debugger/Panes/ToolPanes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetViewLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ProfileRows::GetViewLabel (View view)
{
    switch (view)
    {
    case View::Addresses: return L"Addresses";
    case View::Routines:  return L"Routines";
    case View::Tree:      return L"Call tree";
    case View::Flame:     return L"Flame graph";
    default:              return L"Opcodes";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetRoutineTip
//
//  The heading tips the routines and the call tree share.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ProfileRows::GetRoutineTip()
{
    return L"The routine, by its symbol or address. Top level is the cycles in code outside any call seen during the period";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetSelfTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ProfileRows::GetSelfTip()
{
    return L"The cycles in the routine's instructions, without the routines it calls";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetTotalTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ProfileRows::GetTotalTip()
{
    return L"The cycles in the routine and every routine it calls";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetCallsTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ProfileRows::GetCallsTip()
{
    return L"The times the routine was called during the period";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetDefaultSort
//
//  Most first: the routines by self cycles, which the timeline's colors
//  follow, and the call tree by total, which follows the tree.
//
////////////////////////////////////////////////////////////////////////////////

ProfileRows::Sort ProfileRows::GetDefaultSort (View view)
{
    switch (view)
    {
    case View::Routines:  return { 1, true };
    case View::Tree:      return { 2, true };
    case View::Addresses: return { 2, true };
    case View::Opcodes:   return { 3, true };
    default:              return {};
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetDefaultSorts
//
//  Each view's GetDefaultSort, by the view's number.
//
////////////////////////////////////////////////////////////////////////////////

ProfileRows::Sorts ProfileRows::GetDefaultSorts()
{
    Sorts  sorts;



    for (int view = 0; view < kViewCount; view++)
    {
        sorts[(size_t) view] = GetDefaultSort ((View) view);
    }

    return sorts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::IsTextColumn
//
//  A column of text sorts A first; a column of numbers, most first.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfileRows::IsTextColumn (View view, int column)
{
    switch (view)
    {
    case View::Addresses: return column == 1;
    case View::Opcodes:   return column <= 1;
    default:              return column == 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetSortKey
//
//  A share sorts as the count it is a share of. A routine never called,
//  Top level, has no cycles per call and sorts as zero.
//
////////////////////////////////////////////////////////////////////////////////

ProfileRows::SortKey ProfileRows::GetSortKey (
    const ProfileData  & profile,
    View                 view,
    size_t               entry,
    int                  column)
{
    SortKey  key;



    if (view == View::Routines)
    {
        const ProfileRoutineEntry  & routine = profile.routines[entry];

        switch (column)
        {
        case 0:  key.text   = SymbolTable::ToUpper (routine.GetName());                 break;
        case 1:
        case 2:  key.number = routine.self;                                             break;
        case 3:
        case 4:  key.number = routine.total;                                            break;
        case 5:  key.number = routine.calls;                                            break;
        default: key.number = (routine.calls != 0) ? routine.total / routine.calls : 0; break;
        }
    }
    else if (view == View::Tree)
    {
        const ProfileTreeEntry  & node = profile.tree[entry];

        switch (column)
        {
        case 0:  key.text   = SymbolTable::ToUpper (profile.routines[node.routine].GetName()); break;
        case 1:  key.number = node.self;                                                      break;
        case 4:  key.number = node.calls;                                                     break;
        default: key.number = node.total;                                                     break;
        }
    }
    else if (view == View::Addresses)
    {
        const ProfileAddressEntry  & address = profile.addresses[entry];

        key.number = (column == 0) ? address.address : address.cycles;
        key.text   = (column == 1) ? SymbolTable::ToUpper (address.symbol) : std::string();
    }
    else
    {
        const ProfileEntry  & opcode = profile.opcodes[entry];

        key.number = (column == 2) ? opcode.count : opcode.cycles;
        key.text   = (column == 0) ? opcode.mnemonic : (column == 1) ? opcode.mode : std::string();
    }

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetOrder
//
//  The call tree lists each node's callees after it, a level deeper, so a
//  node's caller is the last node before it a level up.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<size_t> ProfileRows::GetOrder (
    const ProfileData  & profile,
    View                 view,
    int                  column,
    bool                 descending)
{
    std::vector<size_t>               order;
    std::vector<std::vector<size_t>>  children;
    std::vector<size_t>               roots;
    std::vector<size_t>               callers;
    size_t                            count    = 0;
    bool                              isText   = IsTextColumn (view, column);
    std::function<void (size_t)>      emit;
    auto                              isBefore = [&] (size_t a, size_t b)
    {
        SortKey  keyA = GetSortKey (profile, view, a, column);
        SortKey  keyB = GetSortKey (profile, view, b, column);



        if (isText)
        {
            return descending ? keyB.text < keyA.text : keyA.text < keyB.text;
        }

        return descending ? keyB.number < keyA.number : keyA.number < keyB.number;
    };



    switch (view)
    {
    case View::Routines:  count = profile.routines.size();  break;
    case View::Tree:      count = profile.tree.size();      break;
    case View::Addresses: count = profile.addresses.size(); break;
    case View::Opcodes:   count = profile.opcodes.size();   break;
    default:              return order;
    }

    if (view != View::Tree)
    {
        for (size_t i = 0; i < count; i++)
        {
            order.push_back (i);
        }

        std::stable_sort (order.begin(), order.end(), isBefore);
        return order;
    }

    children.resize (count);

    for (size_t i = 0; i < count; i++)
    {
        callers.resize (std::min (callers.size(), (size_t) std::max (profile.tree[i].depth, 0)));

        if (callers.empty())
        {
            roots.push_back (i);
        }
        else
        {
            children[callers.back()].push_back (i);
        }

        callers.push_back (i);
    }

    emit = [&] (size_t node)
    {
        order.push_back (node);
        std::stable_sort (children[node].begin(), children[node].end(), isBefore);

        for (size_t child : children[node])
        {
            emit (child);
        }
    };

    std::stable_sort (roots.begin(), roots.end(), isBefore);

    for (size_t root : roots)
    {
        emit (root);
    }

    return order;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetRowAddress
//
//  Each row is one entry.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> ProfileRows::GetRowAddress (const std::optional<ProfileData> & profile, View view, int row)
{
    size_t                       entry   = (size_t) row;
    const ProfileRoutineEntry  * routine = nullptr;



    if (!profile.has_value() || row < 0)
    {
        return std::nullopt;
    }

    if (view == View::Addresses)
    {
        return (entry < profile->addresses.size()) ? std::optional<Word> (profile->addresses[entry].address) : std::nullopt;
    }

    if (view == View::Routines && entry < profile->routines.size())
    {
        routine = &profile->routines[entry];
    }
    else if (view == View::Tree && entry < profile->tree.size())
    {
        routine = &profile->routines[profile->tree[entry].routine];
    }

    if (routine == nullptr || routine->kind == RoutineKind::Top)
    {
        return std::nullopt;
    }

    return routine->address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetCells
//
//  A row an opcode, an address, a routine or a node of the call tree; an
//  opcode with no operand has a blank mode. With no profile read yet, a
//  note: how to build one while profiling is off, and that the machine must
//  be paused while it is on.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> ProfileRows::GetCells (const std::optional<ProfileData> & profile, View view, bool isOn, const TipHeadFn & tipHead)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;
    auto                                          share = [&profile] (uint64_t cycles)
    {
        return std::format (L"{:.1f}%", 100.0 * (double) cycles / (double) std::max<uint64_t> (profile->cycles, 1));
    };
    auto                                          count = [] (uint64_t value) { return DebuggerStatusText::FormatCount (value); };
    auto                                          head  = [&tipHead] (const ProfileRoutineEntry & routine) { return tipHead ? tipHead (routine) : routine.GetTipHead(); };



    if (!profile.has_value())
    {
        cells.push_back (ToolListPane::MakeNote (isOn ? L"Pause the machine to read the profile."
                                        : L"Choose a period in Show to build a profile from history."));
        return cells;
    }

    if (view == View::Addresses)
    {
        for (const ProfileAddressEntry & entry : profile->addresses)
        {
            cells.push_back ({ { std::format (L"${:04X}", entry.address) }, { TextEncoding::NarrowToWide (entry.symbol) }, { count (entry.cycles) } });
        }
    }
    else if (view == View::Routines)
    {
        //  A routine's swatch is its color in the timeline; the routines it
        //  draws together in gray have none.
        for (size_t i = 0; i < profile->routines.size(); i++)
        {
            const ProfileRoutineEntry  & entry = profile->routines[i];

            cells.push_back ({ { TextEncoding::NarrowToWide (entry.GetName()) }, { count (entry.self) }, { share (entry.self) },
                               { count (entry.total) }, { share (entry.total) }, { count (entry.calls) },
                               { (entry.calls != 0) ? count (entry.total / entry.calls) : std::wstring (L"-") } });
            cells.back()[0].swatch = ProfileTimeline::GetShareColor (i, 0);
            cells.back()[0].tip    = TextEncoding::NarrowToWide ((entry.kind == RoutineKind::Top) ? profile->GetTopLevelTip() : head (entry));
        }
    }
    else if (view == View::Tree)
    {
        for (const ProfileTreeEntry & entry : profile->tree)
        {
            cells.push_back ({ { std::wstring ((size_t) entry.depth * 2, L' ') + TextEncoding::NarrowToWide (profile->routines[entry.routine].GetName()) },
                               { count (entry.self) }, { count (entry.total) }, { share (entry.total) }, { count (entry.calls) } });
            cells.back()[0].swatch = ProfileTimeline::GetShareColor (entry.routine, 0);
            cells.back()[0].tip    = TextEncoding::NarrowToWide ((profile->routines[entry.routine].kind == RoutineKind::Top) ? profile->GetTopLevelTip() : head (profile->routines[entry.routine]));
        }
    }
    else if (view == View::Opcodes)
    {
        for (const ProfileEntry & entry : profile->opcodes)
        {
            cells.push_back ({ { TextEncoding::NarrowToWide (entry.mnemonic) }, { (entry.mode == "[No Operand]") ? std::wstring() : TextEncoding::NarrowToWide (entry.mode) },
                               { count (entry.count) }, { count (entry.cycles) } });
        }
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::GetTotalsText
//
//  The span is the cycles at the machine's clock, about 1.02 MHz, so a
//  profile of a longer period than history covers reads as the shorter one
//  it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProfileRows::GetTotalsText (const std::optional<ProfileData> & profile)
{
    constexpr double  kCyclesPerSecond = 1020484.0;



    if (!profile.has_value())
    {
        return std::wstring();
    }

    return std::format (L"{} s of running: {} instructions, {} cycles, {} page crossings, {} branches taken",
                        FormatSeconds ((double) profile->cycles / kCyclesPerSecond), DebuggerStatusText::FormatCount (profile->instructions),
                        DebuggerStatusText::FormatCount (profile->cycles), DebuggerStatusText::FormatCount (profile->pageCross),
                        DebuggerStatusText::FormatCount (profile->branchTaken));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows::FormatSeconds
//
//  Tenths of a second, the whole seconds grouped as a count is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProfileRows::FormatSeconds (double seconds)
{
    uint64_t  tenths = (uint64_t) std::llround (std::max (seconds, 0.0) * 10.0);



    return std::format (L"{}.{}", DebuggerStatusText::FormatCount (tenths / 10), tenths % 10);
}
