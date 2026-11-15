#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat
//
//  Every number on screen in Casso, written with the user's regional
//  settings in Windows: the thousands separator, the grouping, the decimal
//  separator and the negative sign. If the settings can't be read, the
//  digits are grouped by three with a comma and a period before the
//  decimals.
//
////////////////////////////////////////////////////////////////////////////////

class NumberFormat
{
public:
    //  51,007 in English (United States), 51.007 in German (Germany).
    static std::wstring  FormatInteger (int64_t value);

    //  The value with `decimals` digits after the decimal separator, grouped
    //  or not.
    static std::wstring  FormatFixed (double value, int decimals, bool isGrouped);

    static std::wstring  GetThousandsSeparator ();

    //  The text with each thousands separator removed, a comma or the user's
    //  separator, so a number typed with separators can be parsed.
    static std::wstring  RemoveThousandsSeparators (std::wstring_view text);

private:
    static std::wstring  FormatDigits   (const std::wstring & invariant, int decimals, bool isGrouped);
    static std::wstring  GroupByThree   (const std::wstring & invariant);
    static UINT          ParseGrouping  (const std::wstring & pattern);
};
