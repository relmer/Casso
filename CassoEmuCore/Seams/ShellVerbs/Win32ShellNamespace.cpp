#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShellNamespace.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Model/SearchQuery.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::Win32ShellNamespace
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellNamespace::Win32ShellNamespace (Win32ShellItemVerbs & verbs)
    : m_verbs (verbs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::ListShellFolder
//
//  What the shell lists in the folder, as Explorer lists it, with the
//  columns Explorer shows: type, size and date modified.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::ListShellFolder (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
{
    return ReadShellFolder (id, outItems);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::ReadShellFolder
//
//  Belongs to no object, so it runs on a reader thread that may outlive the
//  window.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::ReadShellFolder (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
{
    HRESULT                     hr     = S_OK;
    ComPtr<IShellItem>          folder;
    ComPtr<IEnumShellItems>     items;
    ComPtr<IShellFolder>        shellFolder;
    std::vector<PITEMID_CHILD>  children;
    std::vector<size_t>         order;
    HRESULT                     hrSort = S_OK;



    outItems.clear();

    //  A search's results are a folder of their own.
    if (SearchQuery::IsId (id))
    {
        return Win32ShellSearch::RunSearch (id, outItems);
    }

    hr = CreateItem (id, &folder);
    CHR (hr);

    hr = folder->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    for (;;)
    {
        ComPtr<IShellItem>                item;
        IShellItemVerbs::ShellFolderItem  entry;
        HRESULT                           hrNext = items->Next (1, &item, nullptr);
        HRESULT                           hrItem = S_OK;

        if (hrNext != S_OK)
        {
            break;
        }

        hrItem = DescribeShellItem (item.Get(), entry);

        //  A desktop entry such as OneDrive has the entry's icon, its cloud,
        //  rather than its folder's, as in ListDesktopNamespace.
        if (SUCCEEDED (hrItem) && id == IShellItemVerbs::kDesktopNamespaceId)
        {
            entry.iconId = GetJunctionId (item.Get());
        }

        if (SUCCEEDED (hrItem))
        {
            outItems.push_back (std::move (entry));
            children.push_back (GetChildId (item.Get()));
        }
    }

    //  Explorer's Name column follows the folder's own order, the result of
    //  its CompareIDs on the name column, and each item is given its place in
    //  that order. When an item's id cannot be read, none is.
    hrSort = folder->BindToHandler (nullptr, BHID_SFObject, IID_PPV_ARGS (&shellFolder));

    if (SUCCEEDED (hrSort) && std::none_of (children.begin(), children.end(), [] (PITEMID_CHILD child) { return child == nullptr; }))
    {
        for (size_t index = 0; index < children.size(); index++)
        {
            order.push_back (index);
        }

        std::stable_sort (order.begin(), order.end(), [&shellFolder, &children] (size_t a, size_t b)
        {
            return (short) HRESULT_CODE (shellFolder->CompareIDs (0, children[a], children[b])) < 0;
        });

        for (size_t rank = 0; rank < order.size(); rank++)
        {
            outItems[order[rank]].shellOrder = (int) rank + 1;
        }
    }

Error:
    for (PITEMID_CHILD child : children)
    {
        CoTaskMemFree (child);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::GetJunctionId
//
//  The item's name in its folder when that name is a CLSID, as "::{...}";
//  empty for any other item.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShellNamespace::GetJunctionId (IShellItem * item)
{
    PWSTR         name = nullptr;
    std::wstring  id;
    HRESULT       hr   = item->GetDisplayName (SIGDN_PARENTRELATIVEPARSING, &name);



    if (SUCCEEDED (hr) && name != nullptr && wcsncmp (name, L"::", 2) == 0)
    {
        id = name;
    }

    CoTaskMemFree (name);
    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::GetChildId
//
//  The item's id relative to its parent, for its parent's CompareIDs; null
//  when it cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

PITEMID_CHILD Win32ShellNamespace::GetChildId (IShellItem * item)
{
    ComPtr<IParentAndItem>  parentAndItem;
    PITEMID_CHILD           child = nullptr;
    HRESULT                 hr    = item->QueryInterface (IID_PPV_ARGS (&parentAndItem));



    if (SUCCEEDED (hr))
    {
        hr = parentAndItem->GetParentAndItem (nullptr, nullptr, &child);
    }

    return SUCCEEDED (hr) ? child : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::ReadHomeProperties
//
//  What the Home folder records of its items. An item of any other folder has
//  no grouping, and nothing else is read.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellNamespace::ReadHomeProperties (IShellItem2 * item, IShellItemVerbs::ShellFolderItem & outEntry)
{
    static constexpr int64_t  s_kTicksPerSecond   = 10000000;
    static constexpr int64_t  s_kTicksToUnixEpoch = 116444736000000000;

    ULONG     group    = 0;
    ULONG     order    = 0;
    BOOL      flag     = FALSE;
    FILETIME  when     = {};
    PWSTR     text     = nullptr;
    bool      hasGroup = false;
    bool      hasValue = false;

    auto  Read = [] (const wchar_t * name, auto && get) -> bool
    {
        PROPERTYKEY  key = {};
        HRESULT      hr  = PSGetPropertyKeyFromName (name, &key);

        if (SUCCEEDED (hr))
        {
            hr = get (key);
        }

        return SUCCEEDED (hr);
    };

    auto  ReadUnix = [&] (const wchar_t * name) -> int64_t
    {
        bool     read  = Read (name, [&] (const PROPERTYKEY & key) { return item->GetFileTime (key, &when); });
        int64_t  ticks = read ? (int64_t) (((uint64_t) when.dwHighDateTime << 32) | when.dwLowDateTime) : 0;

        return (ticks > s_kTicksToUnixEpoch) ? (ticks - s_kTicksToUnixEpoch) / s_kTicksPerSecond : 0;
    };



    hasGroup = Read (L"System.Home.Grouping", [&] (const PROPERTYKEY & key) { return item->GetUInt32 (key, &group); });

    if (!hasGroup)
    {
        return;
    }

    outEntry.homeGroup = (int) group;

    hasValue = Read (L"System.Home.SortOrder", [&] (const PROPERTYKEY & key) { return item->GetUInt32 (key, &order); });
    outEntry.homeOrder = hasValue ? order : 0;

    hasValue = Read (L"System.Home.IsPinned", [&] (const PROPERTYKEY & key) { return item->GetBool (key, &flag); });
    outEntry.homePinned = hasValue && flag != FALSE;

    hasValue = Read (L"System.IsShared", [&] (const PROPERTYKEY & key) { return item->GetBool (key, &flag); });
    outEntry.isShared = hasValue && flag != FALSE;

    outEntry.accessedUnix = ReadUnix (L"System.DateAccessed");
    outEntry.activityUnix = ReadUnix (L"System.ActivityDate");

    hasValue = Read (L"System.ItemFolderPathDisplay", [&] (const PROPERTYKEY & key) { return item->GetString (key, &text); });

    if (hasValue && text != nullptr)
    {
        outEntry.folderDisplay = text;
        CoTaskMemFree (text);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::DescribeShellItem
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::DescribeShellItem (IShellItem * item, IShellItemVerbs::ShellFolderItem & outEntry)
{
    HRESULT              hr         = S_OK;
    HRESULT              hrItem     = S_OK;
    ComPtr<IShellItem2>  item2;
    PWSTR                text       = nullptr;
    FILETIME             when       = {};
    ULONGLONG            size       = 0;
    SFGAOF               attributes = 0;



    outEntry = IShellItemVerbs::ShellFolderItem();

    hr = item->QueryInterface (IID_PPV_ARGS (&item2));
    CHR (hr);

    hr = item->GetDisplayName (SIGDN_DESKTOPABSOLUTEPARSING, &text);
    CHR (hr);

    outEntry.id = text;
    CoTaskMemFree (text);
    text = nullptr;

    hrItem = item->GetDisplayName (SIGDN_NORMALDISPLAY, &text);

    if (SUCCEEDED (hrItem) && text != nullptr)
    {
        outEntry.name = text;
        CoTaskMemFree (text);
        text = nullptr;
    }

    outEntry.path = Win32ShellTransfers::GetItemPath (item);

    ReadHomeProperties (item2.Get(), outEntry);

    hrItem = item2->GetString (Win32ShellItemVerbs::s_kTypeText, &text);

    if (SUCCEEDED (hrItem) && text != nullptr)
    {
        outEntry.typeText = text;
        CoTaskMemFree (text);
        text = nullptr;
    }

    hrItem = item2->GetUInt64 (Win32ShellItemVerbs::s_kSize, &size);

    if (SUCCEEDED (hrItem))
    {
        outEntry.sizeBytes = size;
    }

    hrItem = item2->GetFileTime (Win32ShellItemVerbs::s_kDateModified, &when);

    if (SUCCEEDED (hrItem))
    {
        outEntry.modifiedUnix = Win32RecycleBin::ToUnixSeconds (when);
        outEntry.hasModified  = true;
    }

    //  A zip file or a library is a folder to the shell and a file too; it
    //  opens as a folder, through the shell.
    hrItem = item->GetAttributes (SFGAO_FOLDER | SFGAO_STREAM | SFGAO_HASSUBFOLDER, &attributes);

    if (SUCCEEDED (hrItem))
    {
        outEntry.isFolder      = (attributes & SFGAO_FOLDER) != 0;
        outEntry.isFile        = (attributes & SFGAO_STREAM) != 0;
        outEntry.hasSubfolders = (attributes & SFGAO_HASSUBFOLDER) != 0;
    }

Error:
    CoTaskMemFree (text);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::ListNavigationRoots
//
//  The desktop's items that ask for the navigation pane, as Explorer's pane
//  lists them, in its order. Home, Gallery and the user's own OneDrive lead,
//  above the pinned folders.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::ListNavigationRoots (std::vector<IShellItemVerbs::ShellFolderItem> & outRoots)
{
    HRESULT               hr                 = S_OK;
    ComPtr<IShellFolder>  desktop;
    ComPtr<IEnumIDList>   children;
    PROPERTYKEY           inTree             = {};
    wchar_t               oneDrive[MAX_PATH] = {};
    DWORD                 length   = GetEnvironmentVariableW (L"OneDriveConsumer", oneDrive, MAX_PATH);
    bool                  sawNetwork   = false;
    bool                  sawLibraries = false;



    outRoots.clear();

    if (length == 0 || length >= MAX_PATH)
    {
        length = GetEnvironmentVariableW (L"OneDrive", oneDrive, MAX_PATH);
    }

    //  The items that ask for a place in the navigation pane, as Explorer
    //  reads them; the desktop's own files and shortcuts do not.
    hr = PSGetPropertyKeyFromName (L"System.IsPinnedToNameSpaceTree", &inTree);
    CHR (hr);

    hr = SHGetDesktopFolder (&desktop);
    CHR (hr);

    hr = desktop->EnumObjects (nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_NAVIGATION_ENUM, &children);
    CHR (hr);
    CBREx (hr == S_OK, S_OK);

    for (;;)
    {
        PITEMID_CHILD                     child  = nullptr;
        ComPtr<IShellItem>                item;
        ComPtr<IShellItem2>               item2;
        IShellItemVerbs::ShellFolderItem  entry;
        BOOL                              pinned = FALSE;
        HRESULT                           hrNext = children->Next (1, &child, nullptr);
        HRESULT                           hrItem = S_OK;
        HRESULT                           hrName = S_OK;
        STRRET                            strret = {};
        PWSTR                             name   = nullptr;
        std::wstring                      iconId;

        if (hrNext != S_OK)
        {
            break;
        }

        hrItem = SHCreateItemWithParent (nullptr, desktop.Get(), child, IID_PPV_ARGS (&item));

        //  The desktop's own name for the entry, as ::{CLSID} for a junction
        //  such as OneDrive, whose icon is the entry's rather than its folder's.
        hrName = desktop->GetDisplayNameOf (child, SHGDN_INFOLDER | SHGDN_FORPARSING, &strret);

        if (SUCCEEDED (hrName))
        {
            hrName = StrRetToStrW (&strret, child, &name);
        }

        if (SUCCEEDED (hrName) && name != nullptr && wcsncmp (name, L"::", 2) == 0)
        {
            iconId = name;
        }

        CoTaskMemFree (name);
        CoTaskMemFree (child);

        if (SUCCEEDED (hrItem))
        {
            hrItem = item.As (&item2);
        }

        if (SUCCEEDED (hrItem))
        {
            hrItem = item2->GetBool (inTree, &pinned);
        }

        if (SUCCEEDED (hrItem))
        {
            hrItem = DescribeShellItem (item.Get(), entry);
        }

        if (FAILED (hrItem))
        {
            continue;
        }

        entry.iconId       = iconId;
        entry.isThisPc     = _wcsicmp (entry.id.c_str(), s_kThisPcId) == 0;
        entry.isRecycleBin = _wcsicmp (entry.id.c_str(), s_kRecycleBinId) == 0;
        entry.isNetwork    = _wcsicmp (entry.id.c_str(), s_kNetworkId) == 0;
        entry.isLibraries  = _wcsicmp (entry.id.c_str(), s_kLibrariesId) == 0;

        //  Network and Libraries come whether Explorer shows them or not; the
        //  pane's own options choose.
        if (!pinned && !entry.isNetwork && !entry.isLibraries)
        {
            continue;
        }

        entry.shownByShell = pinned != FALSE;
        sawNetwork   = sawNetwork   || entry.isNetwork;
        sawLibraries = sawLibraries || entry.isLibraries;
        entry.leading      = _wcsicmp (entry.id.c_str(), s_kHomeId) == 0 || _wcsicmp (entry.id.c_str(), s_kGalleryId) == 0
                          || (length > 0 && length < MAX_PATH && _wcsicmp (entry.path.c_str(), oneDrive) == 0);

        //  Explorer's Home and Gallery open in the list alone, with no
        //  folders under them in the pane.
        if (_wcsicmp (entry.id.c_str(), s_kHomeId) == 0 || _wcsicmp (entry.id.c_str(), s_kGalleryId) == 0)
        {
            entry.hasSubfolders = false;
        }

        outRoots.push_back (std::move (entry));
    }

    //  The desktop leaves out a root Explorer hides; it is read by its name.
    for (const wchar_t * id : { sawNetwork ? nullptr : s_kNetworkId, sawLibraries ? nullptr : s_kLibrariesId })
    {
        ComPtr<IShellItem>                item;
        IShellItemVerbs::ShellFolderItem  entry;
        HRESULT                           hrItem = (id != nullptr) ? SHCreateItemFromParsingName (id, nullptr, IID_PPV_ARGS (&item)) : E_ABORT;

        if (SUCCEEDED (hrItem))
        {
            hrItem = DescribeShellItem (item.Get(), entry);
        }

        if (SUCCEEDED (hrItem))
        {
            entry.isNetwork   = id == s_kNetworkId;
            entry.isLibraries  = id == s_kLibrariesId;
            entry.shownByShell = false;
            outRoots.push_back (std::move (entry));
        }
    }

    std::stable_sort (outRoots.begin(), outRoots.end(), [] (const IShellItemVerbs::ShellFolderItem & a, const IShellItemVerbs::ShellFolderItem & b)
    {
        int  rankA = GetNavigationRank (a);
        int  rankB = GetNavigationRank (b);

        return (rankA != rankB) ? rankA < rankB : _wcsicmp (a.name.c_str(), b.name.c_str()) < 0;
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::SetPinnedToQuickAccess
//
//  The verbs Explorer's own Pin to Quick access and Unpin from Quick access
//  run. Pin is on the folder's own menu; Unpin is on its entry in Quick
//  access alone. A menu has to be built before a verb is asked of it by name.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::SetPinnedToQuickAccess (HWND owner, const std::wstring & folder, bool pinned)
{
    HRESULT                  hr     = S_OK;
    ComPtr<IContextMenu>     menu;
    ComPtr<IShellItem>       quick;
    ComPtr<IEnumShellItems>  items;
    HMENU                    popup  = CreatePopupMenu();
    CMINVOKECOMMANDINFO      invoke = { sizeof (invoke) };



    CWR (popup != nullptr);

    if (pinned)
    {
        hr = m_verbs.m_menus.GetItemsMenu (owner, { folder }, &menu);
        CHR (hr);
    }
    else
    {
        hr = SHCreateItemFromParsingName (s_kQuickAccessId, nullptr, IID_PPV_ARGS (&quick));
        CHR (hr);

        hr = quick->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
        CHR (hr);

        for (;;)
        {
            ComPtr<IShellItem>  item;
            HRESULT             hrNext = items->Next (1, &item, nullptr);

            if (hrNext != S_OK)
            {
                break;
            }

            if (_wcsicmp (Win32ShellTransfers::GetItemPath (item.Get()).c_str(), folder.c_str()) == 0)
            {
                hr = item->BindToHandler (nullptr, BHID_SFUIObject, IID_PPV_ARGS (&menu));
                CHR (hr);
                break;
            }
        }

        CBREx (menu != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));
    }

    hr = menu->QueryContextMenu (popup, 0, Win32ShellItemVerbs::s_kFirstCommandId, Win32ShellItemVerbs::s_kLastCommandId, CMF_NORMAL);
    CHR (hr);

    invoke.hwnd   = owner;
    invoke.lpVerb = pinned ? "pintohome" : "unpinfromhome";
    invoke.nShow  = SW_SHOWNORMAL;

    hr = menu->InvokeCommand (&invoke);
    CHR (hr);

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::GetNavigationRank
//
//  Explorer's order: Home, Gallery, the user's OneDrive; the cloud drives on
//  the disk; This PC, Libraries, Network, Linux; anything else after.
//
////////////////////////////////////////////////////////////////////////////////

int Win32ShellNamespace::GetNavigationRank (const IShellItemVerbs::ShellFolderItem & root)
{
    static constexpr std::pair<const wchar_t *, int>  kRanks[] =
    {
        { s_kHomeId,      0 },
        { s_kGalleryId,   1 },
        { s_kThisPcId,   20 },
        { s_kLibrariesId, 21 },
        { s_kNetworkId,  22 },
        { s_kLinuxId,    23 },
    };



    for (const auto & rank : kRanks)
    {
        if (_wcsicmp (root.id.c_str(), rank.first) == 0)
        {
            return rank.second;
        }
    }

    if (root.leading)
    {
        return 2;
    }

    return root.path.empty() ? 30 : 10;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::ListDesktopFolders
//
//  The desktop as the shell's namespace has it, folders only: its roots in
//  the shell's own order, then the folders on the user's desktop by name. A
//  zip file is a folder to the shell, but not one Explorer lists here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::ListDesktopFolders (std::vector<IShellItemVerbs::ShellFolderItem> & outFolders)
{
    HRESULT                  hr      = S_OK;
    PIDLIST_ABSOLUTE         root    = nullptr;
    ComPtr<IShellItem>       desktop;
    ComPtr<IEnumShellItems>  items;
    size_t                   roots   = 0;



    outFolders.clear();

    hr = SHGetSpecialFolderLocation (nullptr, CSIDL_DESKTOP, &root);
    CHR (hr);

    hr = SHCreateItemFromIDList (root, IID_PPV_ARGS (&desktop));
    CHR (hr);

    hr = desktop->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    for (;;)
    {
        ComPtr<IShellItem>                item;
        IShellItemVerbs::ShellFolderItem  entry;
        HRESULT                           hrNext = items->Next (1, &item, nullptr);
        HRESULT                           hrItem = S_OK;

        if (hrNext != S_OK)
        {
            break;
        }

        hrItem = DescribeShellItem (item.Get(), entry);

        if (FAILED (hrItem) || !entry.isFolder || entry.isFile)
        {
            continue;
        }

        //  A root is one with no path, or the user's own folder or OneDrive.
        //  Of the rest, only a folder that is on a desktop is listed; the
        //  shell also offers shortcuts and known folders that live elsewhere.
        entry.isThisPc     = _wcsicmp (entry.id.c_str(), s_kThisPcId) == 0;
        entry.isRecycleBin = _wcsicmp (entry.id.c_str(), s_kRecycleBinId) == 0;

        if (IsDesktopRoot (entry))
        {
            outFolders.insert (outFolders.begin() + (ptrdiff_t) roots, std::move (entry));
            roots++;
        }
        else if (IsOnDesktop (entry.path))
        {
            outFolders.push_back (std::move (entry));
        }
    }

    std::stable_sort (outFolders.begin(), outFolders.begin() + (ptrdiff_t) roots, [] (const IShellItemVerbs::ShellFolderItem & a, const IShellItemVerbs::ShellFolderItem & b)
    {
        return GetDesktopRank (a) < GetDesktopRank (b);
    });

    std::sort (outFolders.begin() + (ptrdiff_t) roots, outFolders.end(), [] (const IShellItemVerbs::ShellFolderItem & a, const IShellItemVerbs::ShellFolderItem & b)
    {
        return StrCmpLogicalW (a.name.c_str(), b.name.c_str()) < 0;
    });

Error:
    CoTaskMemFree (root);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::ListDesktopNamespace
//
//  Sorted by the desktop folder's own comparison, which puts Home and
//  Gallery first, then the folders the user's files live in, then This PC
//  and the other shell roots, as File Explorer's tree lists them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::ListDesktopNamespace (std::vector<IShellItemVerbs::ShellFolderItem> & outFolders)
{
    HRESULT                        hr      = S_OK;
    ComPtr<IShellFolder>           desktop;
    ComPtr<IEnumIDList>            items;
    std::vector<PITEMID_CHILD>     children;
    PITEMID_CHILD                  child   = nullptr;



    outFolders.clear();

    hr = SHGetDesktopFolder (&desktop);
    CHR (hr);

    hr = desktop->EnumObjects (nullptr, SHCONTF_FOLDERS, &items);
    CHR (hr);
    CBREx (hr == S_OK && items != nullptr, S_OK);

    while (items->Next (1, &child, nullptr) == S_OK)
    {
        children.push_back (child);
    }

    std::stable_sort (children.begin(), children.end(), [&desktop] (PITEMID_CHILD a, PITEMID_CHILD b)
    {
        return (short) HRESULT_CODE (desktop->CompareIDs (0, a, b)) < 0;
    });

    for (PITEMID_CHILD item : children)
    {
        ComPtr<IShellItem>                shellItem;
        IShellItemVerbs::ShellFolderItem  entry;
        STRRET                            strret    = {};
        PWSTR                             name      = nullptr;
        HRESULT                           hrItem    = SHCreateItemWithParent (nullptr, desktop.Get(), item, IID_PPV_ARGS (&shellItem));
        HRESULT                           hrName    = desktop->GetDisplayNameOf (item, SHGDN_INFOLDER | SHGDN_FORPARSING, &strret);

        if (SUCCEEDED (hrItem))
        {
            hrItem = DescribeShellItem (shellItem.Get(), entry);
        }

        //  A junction such as OneDrive has the desktop entry's icon, its
        //  cloud, rather than its folder's, as in ListNavigationRoots.
        if (SUCCEEDED (hrName))
        {
            hrName = StrRetToStrW (&strret, item, &name);
        }

        if (SUCCEEDED (hrName) && name != nullptr && wcsncmp (name, L"::", 2) == 0)
        {
            entry.iconId = name;
        }

        CoTaskMemFree (name);

        if (SUCCEEDED (hrItem) && entry.isFolder && !entry.isFile)
        {
            entry.isThisPc     = _wcsicmp (entry.id.c_str(), s_kThisPcId) == 0;
            entry.isRecycleBin = _wcsicmp (entry.id.c_str(), s_kRecycleBinId) == 0;
            entry.isNetwork    = _wcsicmp (entry.id.c_str(), s_kNetworkId) == 0;
            entry.isLibraries  = _wcsicmp (entry.id.c_str(), s_kLibrariesId) == 0;
            outFolders.push_back (std::move (entry));
        }
    }

Error:
    for (PITEMID_CHILD item : children)
    {
        CoTaskMemFree (item);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::CreateItem
//
//  The desktop namespace's root is the empty item list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::CreateItem (const std::wstring & id, IShellItem ** outItem)
{
    static const SHITEMID  s_kEmpty = {};



    if (_wcsicmp (id.c_str(), IShellItemVerbs::kDesktopNamespaceId) == 0)
    {
        return SHCreateItemFromIDList ((PCIDLIST_ABSOLUTE) &s_kEmpty, IID_PPV_ARGS (outItem));
    }

    return SHCreateItemFromParsingName (id.c_str(), nullptr, IID_PPV_ARGS (outItem));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::IsDesktopRoot
//
//  The desktop's own items: the shell's folders with no path, and the
//  user's folder and OneDrive, which the desktop holds as roots too.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellNamespace::IsDesktopRoot (const IShellItemVerbs::ShellFolderItem & item)
{
    wchar_t  profile[MAX_PATH]  = {};
    wchar_t  oneDrive[MAX_PATH] = {};
    DWORD    profileLength      = GetEnvironmentVariableW (L"USERPROFILE", profile, MAX_PATH);
    DWORD    oneDriveLength     = GetEnvironmentVariableW (L"OneDrive", oneDrive, MAX_PATH);



    if (item.path.empty())
    {
        return true;
    }

    return (profileLength  > 0 && profileLength  < MAX_PATH && _wcsicmp (item.path.c_str(), profile)  == 0)
        || (oneDriveLength > 0 && oneDriveLength < MAX_PATH && _wcsicmp (item.path.c_str(), oneDrive) == 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::IsOnDesktop
//
//  A real folder whose parent is the user's desktop or the public one; a
//  shortcut is not a folder there.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellNamespace::IsOnDesktop (const std::wstring & path)
{
    size_t  slash  = path.find_last_of (L"\\/");
    bool    onDesk = false;



    if (slash == std::wstring::npos || path.ends_with (L".lnk"))
    {
        return false;
    }

    for (REFKNOWNFOLDERID folderId : { FOLDERID_Desktop, FOLDERID_PublicDesktop })
    {
        PWSTR    desktop = nullptr;
        HRESULT  hr      = SHGetKnownFolderPath (folderId, KF_FLAG_DEFAULT, nullptr, &desktop);

        if (SUCCEEDED (hr) && _wcsicmp (path.substr (0, slash).c_str(), desktop) == 0)
        {
            onDesk = true;
        }

        CoTaskMemFree (desktop);
    }

    return onDesk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::GetDesktopRank
//
//  Explorer's order for the desktop's roots: Home, Gallery, OneDrive, the
//  user's folder, This PC, Libraries, Network, Control Panel, Linux and the
//  Recycle Bin; anything else among the shell's with Network.
//
////////////////////////////////////////////////////////////////////////////////

int Win32ShellNamespace::GetDesktopRank (const IShellItemVerbs::ShellFolderItem & root)
{
    static constexpr std::pair<const wchar_t *, int>  kRanks[] =
    {
        { s_kHomeId,         0 },
        { s_kGalleryId,      1 },
        { s_kThisPcId,       4 },
        { s_kLibrariesId,    5 },
        { s_kNetworkId,      6 },
        { s_kControlPanelId, 7 },
        { s_kLinuxId,        8 },
        { s_kRecycleBinId,   9 },
    };
    wchar_t  oneDrive[MAX_PATH] = {};
    DWORD    length             = GetEnvironmentVariableW (L"OneDrive", oneDrive, MAX_PATH);



    for (const auto & rank : kRanks)
    {
        if (_wcsicmp (root.id.c_str(), rank.first) == 0)
        {
            return rank.second;
        }
    }

    if (root.path.empty())
    {
        return 6;
    }

    return (length > 0 && length < MAX_PATH && _wcsicmp (root.path.c_str(), oneDrive) == 0) ? 2 : 3;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::ListPinnedFolders
//
//  Quick access holds the pinned folders, the frequent ones and recent files.
//  Explorer's pane shows the pinned folders, then the frequent ones; a zip or
//  a library is a file, however the shell lists it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::ListPinnedFolders (std::vector<IShellItemVerbs::ShellFolderItem> & outFolders)
{
    HRESULT                                        hr       = S_OK;
    ComPtr<IShellItem>                             quick;
    ComPtr<IEnumShellItems>                        items;
    PROPERTYKEY                                    pinned   = {};
    std::vector<IShellItemVerbs::ShellFolderItem>  frequent;



    outFolders.clear();

    hr = PSGetPropertyKeyFromName (L"System.Home.IsPinned", &pinned);
    CHR (hr);

    hr = SHCreateItemFromParsingName (s_kQuickAccessId, nullptr, IID_PPV_ARGS (&quick));
    CHR (hr);

    hr = quick->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    for (;;)
    {
        ComPtr<IShellItem>                item;
        ComPtr<IShellItem2>               item2;
        IShellItemVerbs::ShellFolderItem  entry;
        BOOL                              isPinned = FALSE;
        HRESULT                           hrNext   = items->Next (1, &item, nullptr);
        HRESULT                           hrItem   = S_OK;

        if (hrNext != S_OK)
        {
            break;
        }

        hrItem = item.As (&item2);

        if (SUCCEEDED (hrItem))
        {
            hrItem = item2->GetBool (pinned, &isPinned);
        }

        if (FAILED (hrItem))
        {
            continue;
        }

        hrItem = DescribeShellItem (item.Get(), entry);

        if (FAILED (hrItem) || !entry.isFolder || entry.isFile)
        {
            continue;
        }

        entry.isPinned = isPinned != FALSE;

        if (entry.isPinned)
        {
            outFolders.push_back (std::move (entry));
        }
        else
        {
            frequent.push_back (std::move (entry));
        }
    }

    for (IShellItemVerbs::ShellFolderItem & entry : frequent)
    {
        outFolders.push_back (std::move (entry));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::IsArchive
//
//  The shell's own answer, so an archive type Windows learns to open, as it
//  learned 7z and rar, is one here too, and one it hands to another program
//  is not.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellNamespace::IsArchive (const std::wstring & path)
{
    HRESULT             hr         = S_OK;
    ComPtr<IShellItem>  item;
    SFGAOF              attributes = 0;



    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->GetAttributes (SFGAO_FOLDER | SFGAO_STREAM, &attributes);
    CHR (hr);

Error:
    return SUCCEEDED (hr) && (attributes & SFGAO_FOLDER) != 0 && (attributes & SFGAO_STREAM) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::GetShellItemName
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::GetShellItemName (const std::wstring & id, std::wstring & outName)
{
    HRESULT             hr   = S_OK;
    ComPtr<IShellItem>  item;
    PWSTR               text = nullptr;



    hr = CreateItem (id, &item);
    CHR (hr);

    hr = item->GetDisplayName (SIGDN_NORMALDISPLAY, &text);
    CHR (hr);

    outName = text;

Error:
    CoTaskMemFree (text);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::GetShellParent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::GetShellParent (const std::wstring & id, IShellItemVerbs::ShellFolderItem & outParent)
{
    HRESULT             hr     = S_OK;
    ComPtr<IShellItem>  item;
    ComPtr<IShellItem>  parent;
    PWSTR               text   = nullptr;



    outParent = IShellItemVerbs::ShellFolderItem();

    hr = CreateItem (id, &item);
    CHR (hr);

    hr = item->GetParent (&parent);
    BAIL_OUT_IF (hr == MK_E_NOOBJECT, S_FALSE);   // EHM-ALLOW-SFALSE: the top of the namespace has no parent
    CHR (hr);

    hr = parent->GetDisplayName (SIGDN_DESKTOPABSOLUTEPARSING, &text);
    CHR (hr);

    outParent.id = text;
    CoTaskMemFree (text);
    text = nullptr;

    hr = parent->GetDisplayName (SIGDN_NORMALDISPLAY, &text);
    CHR (hr);

    outParent.name     = text;
    outParent.path     = Win32ShellTransfers::GetItemPath (parent.Get());
    outParent.isFolder = true;

Error:
    CoTaskMemFree (text);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellNamespace::OpenShellItem
//
//  The item's default action by its place in the shell, which reaches items
//  that have no path: a network computer, a library, a control panel.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellNamespace::OpenShellItem (HWND owner, const std::wstring & id)
{
    HRESULT            hr     = S_OK;
    PIDLIST_ABSOLUTE   pidl   = nullptr;
    SHELLEXECUTEINFOW  info   = {};
    BOOL               opened = FALSE;



    hr = SHParseDisplayName (id.c_str(), nullptr, &pidl, 0, nullptr);
    CHR (hr);

    info.cbSize   = sizeof (info);
    info.fMask    = SEE_MASK_IDLIST;
    info.hwnd     = owner;
    info.lpIDList = pidl;
    info.nShow    = SW_SHOWNORMAL;

    opened = ShellExecuteExW (&info);
    CWR (opened);

Error:
    CoTaskMemFree (pidl);
    return hr;
}
