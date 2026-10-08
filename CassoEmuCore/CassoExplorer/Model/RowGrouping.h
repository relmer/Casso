#pragma once

#include "Pch.h"
#include "CassoExplorer/Model/CatalogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping
//
//  Which of Explorer's groups a row falls in when the list is grouped by a
//  column, and the order the groups come in.
//
//  EXPLORER'S OWN LABELS. The dates, sizes and name ranges are Windows' own
//  strings, as its property system's resources hold them (propsys.dll.mui),
//  and the boundaries are the ones those labels state. A group's rank orders
//  it oldest, smallest or first in the alphabet first; a descending list
//  reverses the ranks, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

class RowGrouping
{
public:
    enum class Field { None, Name, DateModified, Type, Size };

    struct Group
    {
        std::wstring  label;
        int           rank = 0;       // within one field; Type groups share a rank and order by label

        bool operator== (const Group & other) const { return label == other.label && rank == other.rank; }
    };

    //  The local date the date groups are counted from, and the day the
    //  locale's week starts on (0 is Sunday, as tm_wday counts).
    struct Today
    {
        int  year           = 1970;
        int  month          = 1;       // 1 to 12
        int  day            = 1;
        int  firstDayOfWeek = 0;
    };

    static Group  GetGroup      (const CatalogRow & row, Field field, const Today & today);

    //  The pieces, for rows and for tests. A date group takes the item's own
    //  local date; `hasDate` false is Explorer's "Unknown".
    static Group  GetDateGroup  (bool hasDate, int year, int month, int day, const Today & today);
    static Group  GetSizeGroup  (bool isDirectory, uint64_t bytes);
    static Group  GetNameGroup  (const std::wstring & name);
    static Group  GetTypeGroup  (const std::wstring & typeText);

    //  Whether group `a` comes before group `b` in a list sorted up (or down).
    static bool   IsBefore      (const Group & a, const Group & b, bool descending);

    //  Today in local time, with the user's first day of the week.
    static Today  GetToday      ();

private:
    static int64_t  ToDayNumber (int year, int month, int day);

    static constexpr uint64_t  s_kKiB = 1024;
    static constexpr uint64_t  s_kMiB = 1024 * s_kKiB;
    static constexpr uint64_t  s_kGiB = 1024 * s_kMiB;
};
