#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NumberSettings
//
//  The regional settings a number is written with. `grouping` is in
//  NUMBERFMTW's form: 3 for groups of three, 32 for three then twos, 30 for
//  one group of three. `negativeOrder` is LOCALE_INEGNUMBER's: 0 "(1.1)",
//  1 "-1.1", 2 "- 1.1", 3 "1.1-", 4 "1.1 -". The defaults are English
//  (United States).
//
////////////////////////////////////////////////////////////////////////////////

struct NumberSettings
{
    std::wstring  thousands     = L",";
    std::wstring  decimal       = L".";
    UINT          grouping      = 3;
    UINT          negativeOrder = 1;
    bool          leadingZero   = true;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat
//
//  Every number on screen in Casso, written with the user's regional
//  settings in Windows: the thousands separator, the grouping, the decimal
//  separator and the negative sign. The settings are read from Windows for
//  each number, unless SetSettings has fixed them, as a test module does.
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

    //  The same, with the settings given.
    static std::wstring  FormatInteger (int64_t value, const NumberSettings & settings);
    static std::wstring  FormatFixed   (double value, int decimals, bool isGrouped, const NumberSettings & settings);

    //  The user's settings from Windows, or English (United States) where
    //  one can't be read.
    static NumberSettings  ReadUserSettings ();

    //  Fixes the settings every number is written with in place of the
    //  user's, or with nullptr, returns to the user's.
    static void  SetSettings (const NumberSettings * settings);

private:
    static NumberSettings  GetSettings    ();
    static std::wstring    FormatDigits   (const std::wstring & invariant, bool isGrouped, const NumberSettings & settings);
    static std::wstring    GroupWhole     (const std::wstring & whole, UINT grouping, const std::wstring & separator);
    static UINT            ParseGrouping  (const std::wstring & pattern);
};
