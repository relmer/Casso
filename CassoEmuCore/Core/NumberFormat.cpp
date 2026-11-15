#include "Pch.h"

#include "Core/NumberFormat.h"





static constexpr int  s_kMaxLocale = 16;
static constexpr int  s_kMaxText   = 96;





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::FormatInteger
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::FormatInteger (int64_t value)
{
    return FormatDigits (std::to_wstring (value), 0, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::FormatFixed
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::FormatFixed (double value, int decimals, bool isGrouped)
{
    int  places = std::max (0, decimals);



    return FormatDigits (std::format (L"{:.{}f}", value, places), places, isGrouped);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::GetThousandsSeparator
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::GetThousandsSeparator()
{
    wchar_t  thousands[s_kMaxLocale] = {};
    int      got                     = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, thousands, s_kMaxLocale);



    return got > 1 ? std::wstring (thousands) : std::wstring (L",");
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::RemoveThousandsSeparators
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::RemoveThousandsSeparators (std::wstring_view text)
{
    std::wstring  separator = GetThousandsSeparator();
    std::wstring  result (text);



    std::erase_if (result, [&] (wchar_t c) { return c == L',' || separator.find (c) != std::wstring::npos; });

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::FormatDigits
//
//  `invariant` is "-1234.5" form. GetNumberFormatEx writes it with the
//  user's separators and negative sign, grouped by the user's grouping or
//  not at all.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::FormatDigits (const std::wstring & invariant, int decimals, bool isGrouped)
{
    wchar_t     thousands[s_kMaxLocale] = {};
    wchar_t     decimal[s_kMaxLocale]   = {};
    wchar_t     grouping[s_kMaxLocale]  = {};
    wchar_t     formatted[s_kMaxText]   = {};
    DWORD       negative                = 1;
    DWORD       leading                 = 1;
    NUMBERFMTW  format                  = {};
    int         gotThousands            = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, thousands, s_kMaxLocale);
    int         gotDecimal              = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL,  decimal,   s_kMaxLocale);
    int         gotGrouping             = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_SGROUPING, grouping,  s_kMaxLocale);
    int         gotNegative             = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_INEGNUMBER | LOCALE_RETURN_NUMBER, (LPWSTR) &negative, sizeof (negative) / sizeof (wchar_t));
    int         gotLeading              = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_ILZERO | LOCALE_RETURN_NUMBER,     (LPWSTR) &leading,  sizeof (leading) / sizeof (wchar_t));
    int         written                 = 0;



    if (gotThousands == 0 || gotDecimal == 0 || gotGrouping == 0 || gotNegative == 0 || gotLeading == 0)
    {
        return isGrouped ? GroupByThree (invariant) : invariant;
    }

    format.NumDigits     = (UINT) decimals;
    format.LeadingZero   = leading;
    format.Grouping      = isGrouped ? ParseGrouping (grouping) : 0;
    format.lpDecimalSep  = decimal;
    format.lpThousandSep = thousands;
    format.NegativeOrder = negative;

    written = GetNumberFormatEx (LOCALE_NAME_USER_DEFAULT, 0, invariant.c_str(), &format, formatted, s_kMaxText);

    return written != 0 ? std::wstring (formatted) : (isGrouped ? GroupByThree (invariant) : invariant);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::GroupByThree
//
//  The fallback: a comma before each group of three whole digits.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::GroupByThree (const std::wstring & invariant)
{
    static constexpr size_t  kGroup = 3;



    size_t        start = (!invariant.empty() && invariant[0] == L'-') ? 1 : 0;
    size_t        point = invariant.find (L'.');
    size_t        end   = point == std::wstring::npos ? invariant.size() : point;
    std::wstring  text  = invariant.substr (0, start);
    size_t        i     = 0;



    for (i = start; i < end; i++)
    {
        if (i > start && (end - i) % kGroup == 0)
        {
            text += L',';
        }

        text += invariant[i];
    }

    return text + invariant.substr (end);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::ParseGrouping
//
//  LOCALE_SGROUPING in NUMBERFMTW's form: "3;0" is groups of 3 (3),
//  "3;2;0" is 3 then 2s (32), and "3" without the closing 0 is one group of
//  3 (30).
//
////////////////////////////////////////////////////////////////////////////////

UINT NumberFormat::ParseGrouping (const std::wstring & pattern)
{
    static constexpr UINT  kScale = 10;



    bool          isRepeated = pattern.ends_with (L";0");
    std::wstring  sizes      = isRepeated ? pattern.substr (0, pattern.size() - 2) : pattern;
    UINT          grouping   = 0;



    for (wchar_t c : sizes)
    {
        if (c >= L'0' && c <= L'9')
        {
            grouping = grouping * kScale + (UINT) (c - L'0');
        }
    }

    return isRepeated ? grouping : grouping * kScale;
}
