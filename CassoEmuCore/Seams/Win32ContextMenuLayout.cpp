#include "Pch.h"

#include "Seams/Win32ContextMenuLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContextMenuLayout::Read
//
////////////////////////////////////////////////////////////////////////////////

Win32ContextMenuLayout Win32ContextMenuLayout::Read()
{
    Win32ContextMenuLayout  layout;



    layout.showMore       = ReadSetting (L"ShowMore",          layout.showMore);
    layout.showExtensions = ReadSetting (L"ShowExtensions",    layout.showExtensions);
    layout.showCustomize  = ReadSetting (L"ShowCustomizeMenu", layout.showCustomize);

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContextMenuLayout::ReadSetting
//
//  A choice on Settings' Context menu page: an Enabled value under the
//  choice's key in Preferences2, or a value by the choice's name in
//  Preferences. Returns the default when neither is there.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ContextMenuLayout::ReadSetting (const wchar_t * setting, bool fallback)
{
    static constexpr const wchar_t *  s_kPreferences  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ContextMenu\\Preferences";
    static constexpr const wchar_t *  s_kPreferences2 = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ContextMenu\\Preferences2\\";
    DWORD                             value           = 0;
    DWORD                             size            = sizeof (value);
    std::wstring                      key             = std::wstring (s_kPreferences2) + setting;



    if (RegGetValueW (HKEY_CURRENT_USER, key.c_str(), L"Enabled", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS)
    {
        return value != 0;
    }

    size = sizeof (value);

    if (RegGetValueW (HKEY_CURRENT_USER, s_kPreferences, setting, RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS)
    {
        return value != 0;
    }

    return fallback;
}
