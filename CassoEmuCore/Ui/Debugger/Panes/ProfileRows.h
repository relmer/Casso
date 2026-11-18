#pragma once

#include "Pch.h"

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileRows
//
//  The Profiler's lists as rows: each view's cells and column tips, the
//  order a sort puts the rows in, the address a row goes to, and the totals
//  line above the list.
//
////////////////////////////////////////////////////////////////////////////////

class ProfileRows
{
public:
    //  The views, most used first, as the view drop-down lists them.
    enum class View
    {
        Routines,
        Tree,
        Flame,
        Addresses,
        Opcodes,
    };

    static constexpr int  kViewCount = 5;

    //  How a view's list is sorted: a column and its direction.
    struct Sort
    {
        int   column     = 0;
        bool  descending = true;
    };

    using Sorts = std::array<Sort, kViewCount>;

    //  A routine's tip head: GetTipHead, or the window's, which adds the
    //  source comment of the routine's label.
    using TipHeadFn = std::function<std::string (const ProfileRoutineEntry & routine)>;

    //  The drop-down's words for a view, and the heading tips the routines
    //  and the call tree share.
    static const wchar_t *  GetViewLabel  (View view);
    static const wchar_t *  GetRoutineTip ();
    static const wchar_t *  GetSelfTip    ();
    static const wchar_t *  GetTotalTip   ();
    static const wchar_t *  GetCallsTip   ();

    static std::vector<std::vector<DxuiListView::Cell>>  GetCells (const std::optional<ProfileData> & profile, View view, bool isOn, const TipHeadFn & tipHead = {});

    //  The line above the list: the profile's totals and the time its cycles
    //  span; empty with no profile.
    static std::wstring  GetTotalsText (const std::optional<ProfileData> & profile);
    static std::wstring  FormatSeconds (double seconds);

    //  The address a row's activation moves the disassembly to: an address
    //  row's, or a routine's start other than Top level's. The row is an
    //  index in the profile's entries, before any sort.
    static std::optional<Word>  GetRowAddress (const std::optional<ProfileData> & profile, View view, int row);

    //  The sort a view opens with: the opcodes and the addresses by cycles,
    //  the routines by self and the call tree by total, most first.
    static Sort                 GetDefaultSort  (View view);
    static Sorts                GetDefaultSorts ();

    //  The order of a view's rows as indexes in the profile's entries, by a
    //  column, ties in the profile's order. The call tree sorts each
    //  routine's callees among themselves, under their caller.
    static std::vector<size_t>  GetOrder        (const ProfileData & profile, View view, int column, bool descending);
    static bool                 IsTextColumn    (View view, int column);

private:
    //  A row's value in a column, to sort by: a number, or text for a name.
    struct SortKey
    {
        uint64_t     number = 0;
        std::string  text;
    };

    static SortKey  GetSortKey (const ProfileData & profile, View view, size_t entry, int column);
};
