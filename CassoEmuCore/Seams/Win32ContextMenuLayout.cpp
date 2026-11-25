#include "Pch.h"

#include "Seams/Win32ContextMenuLayout.h"





static constexpr const wchar_t *  s_kpszMenuKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ContextMenu\\";





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContextMenuLayout::Read
//
////////////////////////////////////////////////////////////////////////////////

Win32ContextMenuLayout Win32ContextMenuLayout::Read()
{
    Win32ContextMenuLayout  layout;



    layout.showMore           = ReadSetting (L"ShowMore",            layout.showMore);
    layout.showExtensions     = ReadSetting (L"ShowExtensions",      layout.showExtensions);
    layout.showCustomize      = ReadSetting (L"ShowCustomizeMenu",   layout.showCustomize);
    layout.inlinePrimary      = ReadSetting (L"DemoteTopBar",        layout.inlinePrimary);
    layout.propertiesAtBottom = ReadSetting (L"MoveProperties",      layout.propertiesAtBottom);
    layout.cloudSection       = ReadSetting (L"CloudStorageActions", layout.cloudSection);
    layout.copyAsPath         = ReadSetting (L"verbs\\copyaspath",   layout.copyAsPath);
    layout.sendTo             = ReadSetting (L"verbs\\sendto",       layout.sendTo);
    layout.createShortcut     = ReadSetting (L"verbs\\link",         layout.createShortcut);
    layout.print              = ReadSetting (L"verbs\\print",        layout.print);
    layout.rotate             = ReadSetting (L"verbs\\rotate90",     layout.rotate);

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContextMenuLayout::IsExtensionInMainMenu
//
//  The app's switch on the page: the user's choice under Preferences, else
//  the default Windows sets for the app under DefaultState. An app with
//  neither is in the submenu.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ContextMenuLayout::IsExtensionInMainMenu (const std::wstring & packageFamily)
{
    bool  enabled = false;



    if (packageFamily.empty())
    {
        return false;
    }

    if (ReadEnabled (std::wstring (s_kpszMenuKey) + L"Preferences\\extensions\\" + packageFamily, enabled))
    {
        return enabled;
    }

    if (ReadEnabled (std::wstring (s_kpszMenuKey) + L"DefaultState\\extensions\\" + packageFamily, enabled))
    {
        return enabled;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContextMenuLayout::ReadSetting
//
//  A choice on Settings' Context menu page: an Enabled value under the
//  choice's key in Preferences, as Settings writes it, or in Preferences2.
//  Returns the default when neither is there.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ContextMenuLayout::ReadSetting (const wchar_t * setting, bool fallback)
{
    bool  enabled = fallback;



    if (ReadEnabled (std::wstring (s_kpszMenuKey) + L"Preferences\\" + setting, enabled))
    {
        return enabled;
    }

    if (ReadEnabled (std::wstring (s_kpszMenuKey) + L"Preferences2\\" + setting, enabled))
    {
        return enabled;
    }

    return fallback;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContextMenuLayout::ReadEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ContextMenuLayout::ReadEnabled (const std::wstring & key, bool & outEnabled)
{
    DWORD  value = 0;
    DWORD  size  = sizeof (value);



    if (RegGetValueW (HKEY_CURRENT_USER, key.c_str(), L"Enabled", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS)
    {
        return false;
    }

    outEnabled = value != 0;

    return true;
}
