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
    DWORD          size        = sizeof (DWORD);
    LSTATUS        status      = ERROR_SUCCESS;



    SHGetSetSettings (&state, SSF_SHOWALLOBJECTS | SSF_SHOWSUPERHIDDEN | SSF_SHOWCOMPCOLOR, FALSE);

    options.showHidden      = state.fShowAllObjects != 0;
    options.showProtected   = state.fShowSuperHidden != 0;
    options.colorCompressed = state.fShowCompColor != 0;

    //  Absent until the user first changes them; off is Explorer's default.
    status = RegGetValueW (HKEY_CURRENT_USER, s_kAdvanced, L"NavPaneShowAllFolders", RRF_RT_REG_DWORD, nullptr, &allFolders, &size);
    options.paneShowsAllFolders = status == ERROR_SUCCESS && allFolders != 0;

    size   = sizeof (DWORD);
    status = RegGetValueW (HKEY_CURRENT_USER, s_kAdvanced, L"NavPaneExpandToCurrentFolder", RRF_RT_REG_DWORD, nullptr, &expand, &size);
    options.paneExpandsToCurrent = status == ERROR_SUCCESS && expand != 0;

    return options;
}
