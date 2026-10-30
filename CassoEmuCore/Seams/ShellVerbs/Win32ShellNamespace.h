#pragma once

#include "Pch.h"

#include "Seams/IShellItemVerbs.h"

class Win32ShellItemVerbs;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace
//
//  The shell's namespace: a shell folder's items, the navigation pane's
//  roots, the desktop's folders, Quick access and its pins, and an item's
//  name, parent and junction.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShellNamespace
{
public:
    explicit Win32ShellNamespace (Win32ShellItemVerbs & verbs);

    HRESULT         ListShellFolder        (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);
    HRESULT         GetShellItemName       (const std::wstring & id, std::wstring & outName);
    HRESULT         OpenShellItem          (HWND owner, const std::wstring & id);
    HRESULT         GetShellParent         (const std::wstring & id, IShellItemVerbs::ShellFolderItem & outParent);
    HRESULT         ListNavigationRoots    (std::vector<IShellItemVerbs::ShellFolderItem> & outRoots);
    HRESULT         ListPinnedFolders      (std::vector<IShellItemVerbs::ShellFolderItem> & outFolders);
    HRESULT         ListDesktopFolders     (std::vector<IShellItemVerbs::ShellFolderItem> & outFolders);
    HRESULT         ListDesktopNamespace   (std::vector<IShellItemVerbs::ShellFolderItem> & outFolders);
    HRESULT         SetPinnedToQuickAccess (HWND owner, const std::wstring & folder, bool pinned);
    bool            IsArchive              (const std::wstring & path);
    static HRESULT  ReadShellFolder        (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems);

    //  An item's id, name, path, type, size, date and what kind of folder it
    //  is, as a listing shows it.
    static void           ReadHomeProperties (IShellItem2 * item, IShellItemVerbs::ShellFolderItem & outEntry);
    static HRESULT        DescribeShellItem  (IShellItem * item, IShellItemVerbs::ShellFolderItem & outEntry);
    static PITEMID_CHILD  GetChildId         (IShellItem * item);
    static std::wstring   GetJunctionId      (IShellItem * item);

    //  A shell item by the shell's name for it, the desktop namespace's own
    //  root included.
    static HRESULT       CreateItem        (const std::wstring & id, IShellItem ** outItem);

    //  The shell's class ids for the roots Explorer treats apart.
    static constexpr const wchar_t *  s_kHomeId       = L"::{F874310E-B6B7-47DC-BC84-B9E6B38F5903}";
    static constexpr const wchar_t *  s_kGalleryId    = L"::{E88865EA-0E1C-4E20-9AA6-EDCD0212C87C}";
    static constexpr const wchar_t *  s_kThisPcId     = L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}";
    static constexpr const wchar_t *  s_kRecycleBinId = L"::{645FF040-5081-101B-9F08-00AA002F954E}";
    static constexpr const wchar_t *  s_kQuickAccessId = L"shell:::{679F85CB-0220-4080-B29B-5540CC05AAB6}";
    static constexpr const wchar_t *  s_kLibrariesId  = L"::{031E4825-7B94-4DC3-B131-E946B44C8DD5}";
    static constexpr const wchar_t *  s_kNetworkId    = L"::{F02C1A0D-BE21-4350-88B0-7367FC96EF3C}";
    static constexpr const wchar_t *  s_kControlPanelId = L"::{26EE0668-A00A-44D7-9371-BEB064C98683}";
    static constexpr const wchar_t *  s_kLinuxId      =L"::{B2B4A4D1-2754-4140-A2EB-9A76D9D7CDC6}";

    //  Where a navigation root goes in Explorer's order.
    static int   GetNavigationRank (const IShellItemVerbs::ShellFolderItem & root);
    static int   GetDesktopRank    (const IShellItemVerbs::ShellFolderItem & root);
    static bool  IsDesktopRoot     (const IShellItemVerbs::ShellFolderItem & item);
    static bool  IsOnDesktop       (const std::wstring & path);

private:
    friend class Win32ShellItemVerbs;

    Win32ShellItemVerbs  & m_verbs;
};
