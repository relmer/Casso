#include "Pch.h"

#include "Core/NumberFormat.h"





static constexpr int  s_kMaxLocale = 16;

static NumberSettings     s_fixedSettings;
static std::atomic<bool>  s_isFixed = false;





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::FormatInteger
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::FormatInteger (int64_t value)
{
    return FormatInteger (value, GetSettings());
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::FormatInteger
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::FormatInteger (int64_t value, const NumberSettings & settings)
{
    return FormatDigits (std::to_wstring (value), true, settings);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::FormatFixed
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::FormatFixed (double value, int decimals, bool isGrouped)
{
    return FormatFixed (value, decimals, isGrouped, GetSettings());
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::FormatFixed
//
//  A value that isn't finite is written as std::format writes it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::FormatFixed (double value, int decimals, bool isGrouped, const NumberSettings & settings)
{
    std::wstring  invariant = std::format (L"{:.{}f}", value, std::max (0, decimals));



    return std::isfinite (value) ? FormatDigits (invariant, isGrouped, settings) : invariant;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::GetThousandsSeparator
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::GetThousandsSeparator()
{
    return GetSettings().thousands;
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
//  NumberFormat::ReadUserSettings
//
////////////////////////////////////////////////////////////////////////////////

NumberSettings NumberFormat::ReadUserSettings()
{
    NumberSettings  settings;
    wchar_t         thousands[s_kMaxLocale] = {};
    wchar_t         decimal[s_kMaxLocale]   = {};
    wchar_t         grouping[s_kMaxLocale]  = {};
    DWORD           negative                = 1;
    DWORD           leading                 = 1;
    int             gotThousands            = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, thousands, s_kMaxLocale);
    int             gotDecimal              = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL,  decimal,   s_kMaxLocale);
    int             gotGrouping             = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_SGROUPING, grouping,  s_kMaxLocale);
    int             gotNegative             = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_INEGNUMBER | LOCALE_RETURN_NUMBER, (LPWSTR) &negative, sizeof (negative) / sizeof (wchar_t));
    int             gotLeading              = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_ILZERO | LOCALE_RETURN_NUMBER,     (LPWSTR) &leading,  sizeof (leading) / sizeof (wchar_t));



    settings.thousands     = gotThousands != 0 ? std::wstring (thousands) : settings.thousands;
    settings.decimal       = gotDecimal > 1 ? std::wstring (decimal) : settings.decimal;
    settings.grouping      = gotGrouping != 0 ? ParseGrouping (grouping) : settings.grouping;
    settings.negativeOrder = gotNegative != 0 ? (UINT) negative : settings.negativeOrder;
    settings.leadingZero   = gotLeading != 0 ? leading != 0 : settings.leadingZero;

    return settings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::SetSettings
//
////////////////////////////////////////////////////////////////////////////////

void NumberFormat::SetSettings (const NumberSettings * settings)
{
    if (settings != nullptr)
    {
        s_fixedSettings = *settings;
    }

    s_isFixed = settings != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::GetSettings
//
////////////////////////////////////////////////////////////////////////////////

NumberSettings NumberFormat::GetSettings()
{
    return s_isFixed ? s_fixedSettings : ReadUserSettings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::FormatDigits
//
//  `invariant` is in "-1234.5" form. The whole digits are grouped, the
//  decimal separator replaces the period, a zero before it is dropped where
//  the settings have no leading zero, and the sign is placed in the
//  settings' negative order. Zero is never negative.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::FormatDigits (const std::wstring & invariant, bool isGrouped, const NumberSettings & settings)
{
    bool          isNegative = !invariant.empty() && invariant[0] == L'-';
    std::wstring  digits     = isNegative ? invariant.substr (1) : invariant;
    size_t        point      = digits.find (L'.');
    std::wstring  whole      = digits.substr (0, point);
    std::wstring  fraction   = point == std::wstring::npos ? std::wstring() : digits.substr (point + 1);
    std::wstring  body;



    isNegative = isNegative && digits.find_first_not_of (L"0.") != std::wstring::npos;

    if (whole == L"0" && !fraction.empty() && !settings.leadingZero)
    {
        whole.clear();
    }

    body = (isGrouped ? GroupWhole (whole, settings.grouping, settings.thousands) : whole) + (fraction.empty() ? std::wstring() : settings.decimal + fraction);

    if (!isNegative)
    {
        return body;
    }

    switch (settings.negativeOrder)
    {
        case 0:  return L"(" + body + L")";
        case 2:  return L"- " + body;
        case 3:  return body + L"-";
        case 4:  return body + L" -";
        default: return L"-" + body;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormat::GroupWhole
//
//  `grouping`'s digits are group sizes from the right, the last repeated;
//  a last 0 ends the grouping, so the digits past it are one group.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NumberFormat::GroupWhole (const std::wstring & whole, UINT grouping, const std::wstring & separator)
{
    std::wstring  sizes = std::to_wstring (grouping);
    std::wstring  text;
    size_t        end   = whole.size();
    size_t        index = 0;
    size_t        size  = 0;



    while (grouping != 0 && end > 0)
    {
        size = (size_t) (sizes[std::min (index, sizes.size() - 1)] - L'0');

        if (size == 0 || size >= end)
        {
            break;
        }

        text  = separator + whole.substr (end - size, size) + text;
        end  -= size;
        index++;
    }

    return whole.substr (0, end) + text;
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
