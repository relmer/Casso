#include "Pch.h"

#include "CassoExplorer/Model/FolderOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderOptions::IsShown
//
////////////////////////////////////////////////////////////////////////////////

bool FolderOptions::IsShown (const FileSystemEntry & entry) const
{
    if (!entry.isHidden)
    {
        return true;
    }

    if (!showHidden)
    {
        return false;
    }

    return !entry.isSystem || showProtected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderOptions::ReadFromShell
//
////////////////////////////////////////////////////////////////////////////////

FolderOptions FolderOptions::ReadFromShell()
{
    static constexpr const wchar_t *  s_kAdvanced = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
    SHELLSTATEW    state       = {};
    FolderOptions  options;
    DWORD          allFolders  = 0;
    DWORD          expand      = 0;
    DWORD          compact     = 0;
    DWORD          size        = sizeof (DWORD);
    LSTATUS        status      = ERROR_SUCCESS;



    SHGetSetSettings (&state, SSF_SHOWALLOBJECTS | SSF_SHOWSUPERHIDDEN | SSF_SHOWCOMPCOLOR | SSF_SHOWEXTENSIONS | SSF_AUTOCHECKSELECT, FALSE);

    options.showHidden      = state.fShowAllObjects != 0;
    options.showProtected   = state.fShowSuperHidden != 0;
    options.colorCompressed = state.fShowCompColor != 0;
    options.showExtensions  = state.fShowExtensions != 0;
    options.checkBoxes      = state.fAutoCheckSelect != 0;

    //  Absent until the user first changes them; off is Explorer's default.
    status = RegGetValueW (HKEY_CURRENT_USER, s_kAdvanced, L"NavPaneShowAllFolders", RRF_RT_REG_DWORD, nullptr, &allFolders, &size);
    options.paneShowsAllFolders = status == ERROR_SUCCESS && allFolders != 0;

    size   = sizeof (DWORD);
    status = RegGetValueW (HKEY_CURRENT_USER, s_kAdvanced, L"NavPaneExpandToCurrentFolder", RRF_RT_REG_DWORD, nullptr, &expand, &size);
    options.paneExpandsToCurrent = status == ERROR_SUCCESS && expand != 0;

    size   = sizeof (DWORD);
    status = RegGetValueW (HKEY_CURRENT_USER, s_kAdvanced, L"UseCompactMode", RRF_RT_REG_DWORD, nullptr, &compact, &size);
    options.compactView = status == ERROR_SUCCESS && compact != 0;

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderOptions::SetInShell
//
//  Through SHGetSetSettings where the shell has the setting, and Compact view
//  in Explorer's key; a WM_SETTINGCHANGE broadcast then updates open File
//  Explorer windows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FolderOptions::SetInShell (Option option, bool on)
{
    static constexpr const wchar_t *  s_kAdvanced = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
    HRESULT                           hr          = S_OK;
    SHELLSTATEW                       state       = {};
    DWORD                             compact     = on ? 1 : 0;
    LSTATUS                           status      = ERROR_SUCCESS;
    DWORD_PTR                         result      = 0;



    switch (option)
    {
        case Option::HiddenItems: state.fShowAllObjects  = on; SHGetSetSettings (&state, SSF_SHOWALLOBJECTS,  TRUE); break;
        case Option::Extensions:  state.fShowExtensions  = on; SHGetSetSettings (&state, SSF_SHOWEXTENSIONS,  TRUE); break;
        case Option::CheckBoxes:  state.fAutoCheckSelect = on; SHGetSetSettings (&state, SSF_AUTOCHECKSELECT, TRUE); break;

        case Option::CompactView:
            status = RegSetKeyValueW (HKEY_CURRENT_USER, s_kAdvanced, L"UseCompactMode", REG_DWORD, &compact, sizeof (compact));
            CBREx (status == ERROR_SUCCESS, HRESULT_FROM_WIN32 (status));
            break;
    }

    SendMessageTimeoutW (HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) L"ShellState", SMTO_ABORTIFHUNG, 1000, &result);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderOptions::HideKnownExtension
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FolderOptions::HideKnownExtension (const std::wstring & name)
{
    size_t   dot   = name.find_last_of (L'.');
    HKEY     key   = nullptr;
    LSTATUS  found = ERROR_FILE_NOT_FOUND;



    if (dot == std::wstring::npos || dot == 0 || dot + 1 == name.size())
    {
        return name;
    }

    found = RegOpenKeyExW (HKEY_CLASSES_ROOT, name.substr (dot).c_str(), 0, KEY_READ, &key);

    if (found != ERROR_SUCCESS)
    {
        return name;
    }

    RegCloseKey (key);

    return name.substr (0, dot);
}
