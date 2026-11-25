#pragma once

#include "Pch.h"

#include "Seams/Win32ExplorerCommandEntries.h"

class IRegistryReader;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32WindowsCommands
//
//  The commands Windows lists on File Explorer's first level:
//  IExplorerCommand handlers from the shell's command store and the static
//  verbs registered for the item's type, in File Explorer's order.
//
//  Of the static verbs, only a few of Windows' verbs appear on File
//  Explorer's first level: Run with PowerShell, Set as desktop background
//  and Add to Favorites.
//
////////////////////////////////////////////////////////////////////////////////

class Win32WindowsCommands
{
public:
    explicit Win32WindowsCommands (IRegistryReader & registry) : m_registry (registry) {}

    //  Returns Windows' commands for the items, in File Explorer's order.
    std::vector<Win32ExplorerCommandEntries::Entry>  GetWindowsEntries (const std::vector<std::wstring> & paths, Win32ExplorerCommandEntries::ItemKind kind);

    //  The shell items passed to the commands in place of the paths: Home's rows.
    void                                             SetItemIds        (std::vector<std::wstring> ids) { m_itemIds = std::move (ids); }

    //  Adds the file's first-level static verbs, then the command store's
    //  Rotate verbs ahead of Add to Favorites.
    void                                             AddStaticVerbs    (const std::wstring & path, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);

private:
    void     AddStoreEntry (const wchar_t * name, const wchar_t * explorerIcon, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);
    HRESULT  AddStaticVerb (const std::wstring & classKey, const wchar_t * verb, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);
    HRESULT  AddStoreVerb  (const wchar_t * name, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries);

    IRegistryReader            & m_registry;
    std::vector<std::wstring>    m_itemIds;
};
