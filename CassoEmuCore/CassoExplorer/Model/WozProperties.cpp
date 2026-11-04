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
        { "developer",        L"Developer" },
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
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::wstring> & WozProperties::GetLanguages()
{
    static const std::vector<std::wstring>  s_languages =
    {
        L"English", L"Spanish", L"French", L"German", L"Chinese", L"Japanese", L"Italian", L"Dutch", L"Portuguese", L"Danish",
        L"Finnish", L"Norwegian", L"Swedish", L"Russian", L"Polish", L"Turkish", L"Arabic", L"Thai", L"Czech", L"Hungarian",
        L"Catalan", L"Croatian", L"Greek", L"Hebrew", L"Romanian", L"Slovak", L"Ukrainian", L"Indonesian", L"Malay", L"Vietnamese",
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
    const std::vector<MetaKey>  & keys = GetMetaKeys();



    return std::any_of (keys.begin(), keys.end(), [&key] (const MetaKey & standard) { return key == standard.key; });
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



    //  A CR LF pair is one line break, and one space.
    for (size_t at = 0; at < typed.size(); at++)
    {
        bool  pair = typed[at] == L'\r' && at + 1 < typed.size() && typed[at + 1] == L'\n';

        if (!pair)
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
//  WozProperties::MakeMeta
//
////////////////////////////////////////////////////////////////////////////////

std::vector<WozLoader::MetaField> WozProperties::MakeMeta (const std::vector<std::pair<std::string, std::wstring>> & typed)
{
    std::vector<WozLoader::MetaField>  meta;



    for (const MetaKey & standard : GetMetaKeys())
    {
        for (const std::pair<std::string, std::wstring> & field : typed)
        {
            std::string  value = ToMetaValue (field.second);

            if (field.first == standard.key && !value.empty())
            {
                meta.push_back ({ field.first, value });
            }
        }
    }

    for (const std::pair<std::string, std::wstring> & field : typed)
    {
        std::string  value = ToMetaValue (field.second);

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
