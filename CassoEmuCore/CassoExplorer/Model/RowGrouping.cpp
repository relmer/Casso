#include "Pch.h"

#include "CassoExplorer/Model/RowGrouping.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::ToDayNumber
//
//  Days since 1970-01-01 in the proleptic Gregorian calendar, so that two
//  dates a month or a year boundary apart still subtract to their distance.
//
////////////////////////////////////////////////////////////////////////////////

int64_t RowGrouping::ToDayNumber (int year, int month, int day)
{
    int64_t  y   = (int64_t) year - (month <= 2 ? 1 : 0);
    int64_t  era = (y >= 0 ? y : y - 399) / 400;
    int64_t  yoe = y - era * 400;
    int64_t  mp  = (int64_t) (month + 9) % 12;
    int64_t  doy = (153 * mp + 2) / 5 + day - 1;
    int64_t  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;



    return era * 146097 + doe - 719468;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetDateGroup
//
//  In Explorer's order of precedence: a day that is both yesterday and in
//  last week is Yesterday, one in last week and last month is Last week.
//  Weeks start on the locale's first day. There is no "Next month": a date
//  past next week and in this month is Later this month, and one past this
//  month but in this year is Later this year.
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetDateGroup (bool hasDate, int year, int month, int day, const Today & today)
{
    int64_t  d         = 0;
    int64_t  t         = 0;
    int64_t  dow       = 0;
    int64_t  weekStart = 0;
    int      lastYear  = today.month == 1 ? today.year - 1 : today.year;
    int      lastMonth = today.month == 1 ? 12 : today.month - 1;



    if (!hasDate)
    {
        return { L"Unknown", -1 };
    }

    d         = ToDayNumber (year, month, day);
    t         = ToDayNumber (today.year, today.month, today.day);
    dow       = ((t % 7) + 7 + 4) % 7;       // 1970-01-01 was a Thursday
    weekStart = t - ((dow - today.firstDayOfWeek + 7) % 7);

    if (d == t)                                            { return { L"Today",                  7 }; }
    if (d == t - 1)                                        { return { L"Yesterday",              6 }; }
    if (d == t + 1)                                        { return { L"Tomorrow",               8 }; }

    if (d < t)
    {
        if (d >= weekStart)                                { return { L"Earlier this week",      5 }; }
        if (d >= weekStart - 7)                            { return { L"Last week",              4 }; }
        if (year == today.year && month == today.month)    { return { L"Earlier this month",     3 }; }
        if (year == lastYear && month == lastMonth)        { return { L"Last month",             2 }; }
        if (year == today.year)                            { return { L"Earlier this year",      1 }; }

        return { L"A long time ago", 0 };
    }

    if (d < weekStart + 7)                                 { return { L"Later this week",        9 }; }
    if (d < weekStart + 14)                                { return { L"Next week",             10 }; }
    if (year == today.year && month == today.month)        { return { L"Later this month",      11 }; }
    if (year == today.year)                                { return { L"Later this year",       12 }; }

    return { L"Sometime in the future", 13 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetSizeGroup
//
//  Each label's upper bound is in it: 16 KB exactly is Tiny. A folder has
//  no size of its own, and Explorer groups it as Unspecified.
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetSizeGroup (bool isDirectory, uint64_t bytes)
{
    if (isDirectory)                   { return { L"Unspecified",           -1 }; }
    if (bytes == 0)                    { return { L"Empty (0 KB)",           0 }; }
    if (bytes <= 16 * s_kKiB)          { return { std::wstring (L"Tiny (0 ") + s_kpszEnDash + L" 16 KB)",       1 }; }
    if (bytes <= s_kMiB)               { return { std::wstring (L"Small (16 KB ") + s_kpszEnDash + L" 1 MB)",   2 }; }
    if (bytes <= 128 * s_kMiB)         { return { std::wstring (L"Medium (1 ") + s_kpszEnDash + L" 128 MB)",    3 }; }
    if (bytes <= s_kGiB)               { return { std::wstring (L"Large (128 MB ") + s_kpszEnDash + L" 1 GB)",  4 }; }
    if (bytes <= 4 * s_kGiB)           { return { std::wstring (L"Huge (1 ") + s_kpszEnDash + L" 4 GB)",        5 }; }

    return { L"Gigantic (>4 GB)", 6 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetNameGroup
//
//  Explorer's ranges for English names, by the first character: digits,
//  then three runs of letters, then everything else.
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetNameGroup (const std::wstring & name)
{
    wchar_t  c = name.empty() ? L'\0' : (wchar_t) towupper (name[0]);



    if (c >= L'0' && c <= L'9')        { return { std::wstring (L"0 ") + s_kpszEnDash + L" 9", 0 }; }
    if (c >= L'A' && c <= L'H')        { return { std::wstring (L"A ") + s_kpszEnDash + L" H", 1 }; }
    if (c >= L'I' && c <= L'P')        { return { std::wstring (L"I ") + s_kpszEnDash + L" P", 2 }; }
    if (c >= L'Q' && c <= L'Z')        { return { std::wstring (L"Q ") + s_kpszEnDash + L" Z", 3 }; }

    return { L"Other", 4 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetTypeGroup
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetTypeGroup (const std::wstring & typeText)
{
    return { typeText.empty() ? std::wstring (L"Unspecified") : typeText, 0 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetGroup
//
//  A catalog's date is the wall-clock time it was written, so it is read
//  back without a zone; a host file's is an instant, shown in local time.
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetGroup (const CatalogRow & row, Field field, const Today & today)
{
    switch (field)
    {
        case Field::Name:
            return GetNameGroup (row.name);

        case Field::Type:
            return GetTypeGroup (row.typeText);

        case Field::Size:
            return GetSizeGroup (row.isDirectory, row.sizeBytes);

        case Field::DateModified:
            return row.hasModified ? GetUnixDateGroup (row.modifiedUnix, row.modifiedIsWallClock, today) : GetDateGroup (false, 0, 0, 0, today);

        case Field::Folder:
            return GetTypeGroup (CatalogModel::GetFolderColumnText (row.folderShown.empty() ? row.folderPath : row.folderShown));

        case Field::OriginalLocation:
            return GetTypeGroup (row.originalFolder);

        case Field::Relevance:
            return { std::to_wstring (row.searchRank), row.searchRank };

        case Field::DateDeleted:
            return row.hasDeleted ? GetUnixDateGroup (row.deletedUnix, false, today) : GetDateGroup (false, 0, 0, 0, today);

        default:
            return {};
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetUnixDateGroup
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetUnixDateGroup (int64_t unixSeconds, bool wallClock, const Today & today)
{
    __time64_t  when  = (__time64_t) unixSeconds;
    tm          parts = {};
    errno_t     err   = wallClock ? _gmtime64_s (&parts, &when) : _localtime64_s (&parts, &when);



    return GetDateGroup (err == 0, parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, today);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetPropertyGroup
//
//  A date property's number is its FILETIME.
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetPropertyGroup (const CatalogModel::PropertyKey & key, const Today & today)
{
    static constexpr int64_t  s_kFileTimeTicksPerSecond = 10000000;
    static constexpr int64_t  s_kUnixEpochInSeconds     = 11644473600;



    if (!key.has)
    {
        return { L"Unspecified", -1 };
    }

    if (key.isDate)
    {
        return GetUnixDateGroup ((int64_t) key.number / s_kFileTimeTicksPerSecond - s_kUnixEpochInSeconds, false, today);
    }

    if (!key.groupLabel.empty())
    {
        return { key.groupLabel, key.groupRank };
    }

    return { key.text, 0 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetCapacityGroup
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetCapacityGroup (uint64_t bytes)
{
    if (bytes <= 16 * s_kGiB)   { return { L"0 - 16 GB",       0 }; }
    if (bytes <= 80 * s_kGiB)   { return { L"16 GB - 80 GB",   1 }; }
    if (bytes <= 250 * s_kGiB)  { return { L"80 GB - 250 GB",  2 }; }
    if (bytes <= 500 * s_kGiB)  { return { L"250 GB - 500 GB", 3 }; }
    if (bytes <= 1024 * s_kGiB) { return { L"500 GB - 1 TB",   4 }; }

    return { L">1 TB", 5 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetPercentGroup
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Group RowGrouping::GetPercentGroup (double percent, const wchar_t * suffix)
{
    int  tens = (int) percent / 10;



    if (tens >= 10)
    {
        return { std::format (L"100%{}", suffix), 10 };
    }

    return { std::format (L"{}% - {}%{}", tens * 10, tens * 10 + 9, suffix), tens };
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetPropertyGroups
//
////////////////////////////////////////////////////////////////////////////////

std::vector<RowGrouping::Group> RowGrouping::GetPropertyGroups (const CatalogModel::PropertyKey & key, const Today & today)
{
    std::vector<Group>  groups;



    for (const std::wstring & part : key.parts)
    {
        groups.push_back ({ part, 0 });
    }

    if (groups.empty())
    {
        groups.push_back (GetPropertyGroup (key, today));
    }

    return groups;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::IsBefore
//
////////////////////////////////////////////////////////////////////////////////

bool RowGrouping::IsBefore (const Group & a, const Group & b, bool descending)
{
    if (a.rank != b.rank)
    {
        return descending ? a.rank > b.rank : a.rank < b.rank;
    }

    return descending ? _wcsicmp (a.label.c_str(), b.label.c_str()) > 0
                      : _wcsicmp (a.label.c_str(), b.label.c_str()) < 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RowGrouping::GetToday
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Today RowGrouping::GetToday()
{
    SYSTEMTIME  now          = {};
    Today       today;
    DWORD       firstDay     = 0;
    int         got          = 0;



    GetLocalTime (&now);

    today.year  = now.wYear;
    today.month = now.wMonth;
    today.day   = now.wDay;

    //  The locale counts Monday as 0; tm_wday, and this, count Sunday as 0.
    got = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK | LOCALE_RETURN_NUMBER, (LPWSTR) &firstDay, sizeof (firstDay) / sizeof (wchar_t));

    today.firstDayOfWeek = (got != 0) ? (int) ((firstDay + 1) % 7) : 0;

    return today;
}
