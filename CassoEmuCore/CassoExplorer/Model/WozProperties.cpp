#include "Pch.h"

#include "CassoExplorer/Model/WozProperties.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/WozCompatibility.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetMetaKeys
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<WozProperties::MetaKey> & WozProperties::GetMetaKeys()
{
    static const std::vector<MetaKey>  s_keys =
    {
        { kTitle,             L"Title" },
        { "subtitle",         L"Subtitle" },
        { "publisher",        L"Publisher" },
        { kDeveloper,         L"Developer" },
        { "copyright",        L"Copyright" },
        { "version",          L"Version" },
        { kLanguage,          L"Language" },
        { kRequiresRam,       L"Requires RAM" },
        { kRequiresMachine,   L"Requires" },
        { "notes",            L"Notes" },
        { "side",             L"Side" },
        { "side_name",        L"Side name" },
        { "contributor",      L"Contributor" },
        { "image_date",       L"Image date" },
    };



    return s_keys;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetMachines
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<WozProperties::Machine> & WozProperties::GetMachines()
{
    static const std::vector<Machine>  s_machines =
    {
        { WozCompatibility::kApple2,          "2",   L"][" },
        { WozCompatibility::kApple2Plus,      "2+",  L"][+" },
        { WozCompatibility::kApple2e,         "2e",  L"//e" },
        { WozCompatibility::kApple2c,         "2c",  L"//c" },
        { WozCompatibility::kApple2eEnhanced, "2e+", L"//e Enhanced" },
        { WozCompatibility::kApple2gs,        "2gs", L"IIgs" },
        { WozCompatibility::kApple2cPlus,     "2c+", L"//c+" },
        { WozCompatibility::kApple3,          "3",   L"///" },
        { WozCompatibility::kApple3Plus,      "3+",  L"///+" },
    };



    return s_machines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetRamSizes
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<WozProperties::RamSize> & WozProperties::GetRamSizes()
{
    static const std::vector<RamSize>  s_sizes =
    {
        { 0,    L"Unknown" },
        { 16,   L"16 KB" },
        { 24,   L"24 KB" },
        { 32,   L"32 KB" },
        { 48,   L"48 KB" },
        { 64,   L"64 KB" },
        { 128,  L"128 KB" },
        { 256,  L"256 KB" },
        { 512,  L"512 KB" },
        { 768,  L"768 KB" },
        { 1024, L"1 MB" },
        { 1280, L"1.25 MB" },
        { 1536, L"1.5 MB" },
    };



    return s_sizes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetBootSectorLabels
//
//  By INFO's value, 0 to 3.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::wstring> & WozProperties::GetBootSectorLabels()
{
    static const std::vector<std::wstring>  s_labels = { L"Unknown", L"16-sector", L"13-sector", L"Both 16-sector and 13-sector" };



    return s_labels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetMetaRamValues
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::wstring> & WozProperties::GetMetaRamValues()
{
    static const std::vector<std::wstring>  s_values = { L"16K", L"24K", L"32K", L"48K", L"64K", L"128K", L"256K", L"512K", L"768K",
                                                         L"1M", L"1.25M", L"1.5M+", L"Unknown" };



    return s_values;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetLanguages
//
//  The reference's languages, English first, then the rest alphabetically,
//  with Other last.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::wstring> & WozProperties::GetLanguages()
{
    static const std::vector<std::wstring>  s_languages =
    {
        L"English",
        L"Arabic", L"Catalan", L"Chinese", L"Croatian", L"Czech", L"Danish", L"Dutch", L"Finnish", L"French", L"German",
        L"Greek", L"Hebrew", L"Hungarian", L"Indonesian", L"Italian", L"Japanese", L"Malay", L"Norwegian", L"Polish", L"Portuguese",
        L"Romanian", L"Russian", L"Slovak", L"Spanish", L"Swedish", L"Thai", L"Turkish", L"Ukrainian", L"Vietnamese",
        L"Other",
    };



    return s_languages;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::IsStandardKey
//
////////////////////////////////////////////////////////////////////////////////

bool WozProperties::IsStandardKey (const std::string & key)
{
    return WozLoader::IsStandardMetaKey (key);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::FormatMachineCodes
//
//  The codes of the machines whose bits are set, in the reference's order,
//  separated by |.
//
////////////////////////////////////////////////////////////////////////////////

std::string WozProperties::FormatMachineCodes (uint16_t machines)
{
    std::string  codes;



    for (const Machine & machine : GetMachines())
    {
        if ((machines & machine.bit) != 0)
        {
            codes += (codes.empty() ? "" : "|") + std::string (machine.code);
        }
    }

    return codes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::ParseMachineCodes
//
//  A code that isn't the reference's doesn't set a bit.
//
////////////////////////////////////////////////////////////////////////////////

uint16_t WozProperties::ParseMachineCodes (const std::string & value)
{
    uint16_t  machines = 0;
    size_t    at       = 0;



    while (at <= value.size())
    {
        size_t       bar  = value.find ('|', at);
        size_t       stop = (bar == std::string::npos) ? value.size() : bar;
        std::string  code = value.substr (at, stop - at);

        for (const Machine & machine : GetMachines())
        {
            machines |= (code == machine.code) ? machine.bit : 0;
        }

        at = stop + 1;
    }

    return machines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::ToMetaValue
//
////////////////////////////////////////////////////////////////////////////////

std::string WozProperties::ToMetaValue (const std::wstring & typed)
{
    std::wstring  plain;
    size_t        first = 0;
    size_t        last  = 0;



    //  A CR LF pair is one line break, and one space. A | separates values,
    //  so a value can't have one.
    for (size_t at = 0; at < typed.size(); at++)
    {
        bool  pair = typed[at] == L'\r' && at + 1 < typed.size() && typed[at + 1] == L'\n';

        if (!pair && typed[at] != kValueSeparator)
        {
            plain += (typed[at] == L'\t' || typed[at] == L'\r' || typed[at] == L'\n') ? L' ' : typed[at];
        }
    }

    first = plain.find_first_not_of (L' ');

    if (first == std::wstring::npos)
    {
        return std::string();
    }

    last = plain.find_last_not_of (L' ');

    return TextEncoding::WideToUtf8 (plain.substr (first, last - first + 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::ToMetaList
//
//  A list typed a value a line, each as ToMetaValue makes it, the empty ones
//  left out, joined with |.
//
////////////////////////////////////////////////////////////////////////////////

std::string WozProperties::ToMetaList (const std::wstring & typed)
{
    std::string  list;
    size_t       at   = 0;



    while (at <= typed.size())
    {
        size_t       end   = typed.find (L'\n', at);
        size_t       stop  = (end == std::wstring::npos) ? typed.size() : end;
        std::string  value = ToMetaValue (typed.substr (at, stop - at));

        if (!value.empty())
        {
            list += (list.empty() ? "" : "|") + value;
        }

        at = stop + 1;
    }

    return list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::SplitList
//
//  A list key's value, a value an item, for a field each.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> WozProperties::SplitList (const std::wstring & value)
{
    std::vector<std::wstring>  items;
    size_t                     at    = 0;



    while (!value.empty() && at <= value.size())
    {
        size_t  bar  = value.find (kValueSeparator, at);
        size_t  stop = (bar == std::wstring::npos) ? value.size() : bar;

        items.push_back (value.substr (at, stop - at));
        at = stop + 1;
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::IsListKey
//
////////////////////////////////////////////////////////////////////////////////

bool WozProperties::IsListKey (const std::string & key)
{
    return key == kDeveloper || key == kRequiresMachine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::MakeMeta
//
//  Every standard key in the reference's order, with an empty value where
//  none was typed, as the reference writes a standard key with no value.
//  A list's values are typed a line each.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<WozLoader::MetaField> WozProperties::MakeMeta (const std::vector<std::pair<std::string, std::wstring>> & typed)
{
    std::vector<WozLoader::MetaField>  meta;



    for (const MetaKey & standard : GetMetaKeys())
    {
        std::string  value;

        for (const std::pair<std::string, std::wstring> & field : typed)
        {
            if (field.first == standard.key)
            {
                value = IsListKey (field.first) ? ToMetaList (field.second) : ToMetaValue (field.second);
            }
        }

        meta.push_back ({ standard.key, value });
    }

    //  Any other key's value may be a list, as the reference allows.
    for (const std::pair<std::string, std::wstring> & field : typed)
    {
        std::string  value = ToMetaList (field.second);

        if (!IsStandardKey (field.first) && !field.first.empty() && !value.empty())
        {
            meta.push_back ({ field.first, value });
        }
    }

    return meta;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::FormatEditError
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozProperties::FormatEditError (HRESULT hr)
{
    if (hr == HRESULT_FROM_WIN32 (ERROR_CRC))
    {
        return L"The image is damaged: its CRC doesn't match its contents. It wasn't changed, since a new CRC would hide the damage.";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED))
    {
        return L"The image's chunks are in an order that can't be rewritten in place, so it wasn't changed.";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_DATA))
    {
        return L"The image isn't a WOZ image that can be changed in place, so it wasn't changed.";
    }

    return L"The changes weren't written: " + TextEncoding::NarrowToWide (std::system_category().message (HRESULT_CODE (hr)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::TryParseImageDate
//
//  An RFC 3339 time in UTC, as image_date has it. Its fraction of a second
//  is left out.
//
////////////////////////////////////////////////////////////////////////////////

bool WozProperties::TryParseImageDate (const std::string & value, SYSTEMTIME & outUtc)
{
    int  year   = 0;
    int  month  = 0;
    int  day    = 0;
    int  hour   = 0;
    int  minute = 0;
    int  second = 0;
    int  fields = sscanf_s (value.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);



    outUtc = SYSTEMTIME {};

    if (fields != kImageDateFields || value.empty() || value.back() != 'Z')
    {
        return false;
    }

    outUtc.wYear   = (WORD) year;
    outUtc.wMonth  = (WORD) month;
    outUtc.wDay    = (WORD) day;
    outUtc.wHour   = (WORD) hour;
    outUtc.wMinute = (WORD) minute;
    outUtc.wSecond = (WORD) second;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::MakeImageDate
//
////////////////////////////////////////////////////////////////////////////////

std::string WozProperties::MakeImageDate (const SYSTEMTIME & utc)
{
    return std::format ("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.000Z", utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetValueError
//
//  Why a typed value can't be saved, or nothing when it can.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozProperties::GetValueError (const std::wstring & value)
{
    bool  illegal = value.find_first_of (kIllegalChars) != std::wstring::npos;



    return illegal ? std::wstring (L"Metadata can't have a |, a tab or a line break.") : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetListError
//
//  Each of a list's values checked separately, since the line breaks that
//  separate typed values aren't part of any value.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozProperties::GetListError (const std::vector<std::wstring> & values)
{
    std::wstring  error;



    for (size_t at = 0; error.empty() && at < values.size(); at++)
    {
        error = GetValueError (values[at]);
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetKeyError
//
//  Why a custom field's key can't be saved, or nothing when it can: a key
//  for a value, without the characters the format reserves, that neither
//  the reference nor another field already has.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozProperties::GetKeyError (const std::wstring & key, bool hasValue, const std::vector<std::wstring> & otherKeys)
{
    bool  illegal   = key.find_first_of (kIllegalChars) != std::wstring::npos;
    bool  standard  = IsStandardKey (TextEncoding::WideToUtf8 (key));
    bool  duplicate = std::find (otherKeys.begin(), otherKeys.end(), key) != otherKeys.end();



    return (key.empty() && hasValue) ? std::wstring (L"A field with a value must have a key.")
         : illegal                   ? std::wstring (L"A key can't have a |, a tab or a line break.")
         : standard                  ? std::wstring (L"This key is one of the reference's, and its field is above.")
         : (duplicate && !key.empty()) ? std::wstring (L"Another field has this key.")
                                     : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::TryGetLocalTime
//
//  image_date in local time.
//
////////////////////////////////////////////////////////////////////////////////

bool WozProperties::TryGetLocalTime (const std::string & value, SYSTEMTIME & outLocal)
{
    SYSTEMTIME  utc    = {};
    bool        parsed = TryParseImageDate (value, utc);



    outLocal = SYSTEMTIME {};

    return parsed && SystemTimeToTzSpecificLocalTime (nullptr, &utc, &outLocal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::FormatShortDate
//
//  A day in the Windows short date format.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozProperties::FormatShortDate (const SYSTEMTIME & local)
{
    wchar_t  day[64] = {};
    int      length  = GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr, day, (int) std::size (day), nullptr);



    return (length > 0) ? std::wstring (day) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::TryParseShortDate
//
//  A day typed in any form the Windows date parser reads. Its time of day
//  is set apart.
//
////////////////////////////////////////////////////////////////////////////////

bool WozProperties::TryParseShortDate (const std::wstring & typed, SYSTEMTIME & outLocal)
{
    DATE     parsed = 0;
    HRESULT  hr     = VarDateFromStr (typed.c_str(), LOCALE_USER_DEFAULT, VAR_DATEVALUEONLY, &parsed);



    outLocal = SYSTEMTIME {};

    return SUCCEEDED (hr) && VariantTimeToSystemTime (parsed, &outLocal) && outLocal.wYear > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::MakeLocalImageDate
//
//  A local date and time as image_date has it, or nothing when it isn't a
//  time Windows can convert.
//
////////////////////////////////////////////////////////////////////////////////

std::string WozProperties::MakeLocalImageDate (const SYSTEMTIME & local)
{
    SYSTEMTIME  utc       = {};
    bool        converted = TzSpecificLocalTimeToSystemTime (nullptr, &local, &utc) != FALSE;



    return converted ? MakeImageDate (utc) : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::IsClock24Hour
//
//  Whether the user's time format counts hours to 23, rather than to 12
//  with AM and PM.
//
////////////////////////////////////////////////////////////////////////////////

bool WozProperties::IsClock24Hour()
{
    wchar_t  format[80] = {};
    int      length     = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, format, (int) std::size (format));



    return length > 0 && wcschr (format, L'H') != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozProperties::GetDayHalfLabel
//
//  The user's AM or PM.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozProperties::GetDayHalfLabel (bool afternoon)
{
    wchar_t  label[32] = {};
    int      length    = GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, afternoon ? LOCALE_S2359 : LOCALE_S1159, label, (int) std::size (label));



    return (length > 1) ? std::wstring (label) : std::wstring (afternoon ? L"PM" : L"AM");
}
