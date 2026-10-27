#include "Pch.h"

#include "Seams/ShellVerbs/Win32RecycleBin.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/AppleFileName.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::Win32RecycleBin
//
////////////////////////////////////////////////////////////////////////////////

Win32RecycleBin::Win32RecycleBin (Win32ShellItemVerbs & verbs)
    : m_verbs (verbs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::GetItemArray
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::GetItemArray (const std::vector<std::wstring> & paths, IShellItemArray ** outItems)
{
    HRESULT                        hr       = S_OK;
    std::vector<PIDLIST_ABSOLUTE>  pidls;
    bool                           hasItems = !paths.empty();



    *outItems = nullptr;

    CBREx (hasItems, E_INVALIDARG);

    for (const std::wstring & path : paths)
    {
        PIDLIST_ABSOLUTE  pidl = nullptr;

        hr = SHParseDisplayName (path.c_str(), nullptr, &pidl, 0, nullptr);
        CHR (hr);

        pidls.push_back (pidl);
    }

    hr = SHCreateShellItemArrayFromIDLists ((UINT) pidls.size(), (PCIDLIST_ABSOLUTE_ARRAY) pidls.data(), outItems);
    CHR (hr);

Error:
    for (PIDLIST_ABSOLUTE pidl : pidls)
    {
        CoTaskMemFree (pidl);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::CreateOperation
//
//  Undoable, as Explorer's are, and asking before anything is overwritten.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::CreateOperation (HWND owner, IFileOperation ** outOperation)
{
    HRESULT  hr = S_OK;



    hr = CoCreateInstance (CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS (outOperation));
    CHR (hr);

    hr = (*outOperation)->SetOwnerWindow (owner);
    CHR (hr);

    hr = (*outOperation)->SetOperationFlags (FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR | FOFX_ADDUNDORECORD);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::Perform
//
//  PerformOperations succeeds when the user cancels at one of its prompts, so
//  a cancel is found by GetAnyOperationsAborted and returned as ERROR_CANCELLED.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::Perform (IFileOperation * operation)
{
    HRESULT  hr      = S_OK;
    BOOL     aborted = FALSE;



    hr = operation->PerformOperations();
    CHR (hr);

    hr = operation->GetAnyOperationsAborted (&aborted);
    CHR (hr);

    BAIL_OUT_IF (aborted, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::Recycle
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::Recycle (HWND owner, const std::vector<std::wstring> & paths)
{
    Win32ShellVerbsTypes::CenterDialogsOver  centered (owner);
    HRESULT                  hr = S_OK;
    ComPtr<IFileOperation>   operation;
    ComPtr<IShellItemArray>  items;



    hr = GetItemArray (paths, &items);
    CHR (hr);

    hr = CreateOperation (owner, &operation);
    CHR (hr);

    hr = operation->SetOperationFlags (FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE | FOFX_ADDUNDORECORD);
    CHR (hr);

    hr = operation->DeleteItems (items.Get());
    CHR (hr);

    hr = Perform (operation.Get());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::DeletePermanently
//
//  No undo and no recycling, so Windows asks before deleting, as it does for
//  File Explorer's Shift+Delete.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::DeletePermanently (HWND owner, const std::vector<std::wstring> & ids)
{
    Win32ShellVerbsTypes::CenterDialogsOver        centered (owner);
    HRESULT                  hr = S_OK;
    ComPtr<IFileOperation>   operation;
    ComPtr<IShellItemArray>  items;



    hr = GetItemArray (ids, &items);
    CHR (hr);

    hr = CreateOperation (owner, &operation);
    CHR (hr);

    hr = operation->SetOperationFlags (FOF_NOCONFIRMMKDIR);
    CHR (hr);

    hr = operation->DeleteItems (items.Get());
    CHR (hr);

    hr = Perform (operation.Get());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::RestoreRecycled
//
//  Each path's most recently deleted copy in the Recycle Bin, found by where
//  it was deleted from and its name, restored by the Recycle Bin's own
//  Restore verb, as Explorer's undo puts a deletion back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::RestoreRecycled (HWND owner, const std::vector<std::wstring> & paths)
{
    Win32ShellVerbsTypes::CenterDialogsOver  centered (owner);
    HRESULT                          hr             = S_OK;
    ComPtr<IShellItem>               bin;
    ComPtr<IShellFolder>             folder;
    ComPtr<IEnumShellItems>          items;
    ComPtr<IContextMenu>             menu;
    std::vector<ComPtr<IShellItem>>  best           (paths.size());
    std::vector<ULONGLONG>           bestWhen       (paths.size(), 0);
    std::vector<PITEMID_CHILD>       children;
    std::vector<PIDLIST_ABSOLUTE>    owned;
    CMINVOKECOMMANDINFO              invoke         = { sizeof (invoke) };
    bool                             found          = false;



    hr = SHGetKnownFolderItem (FOLDERID_RecycleBinFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS (&bin));
    CHR (hr);

    hr = bin->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    for (;;)
    {
        ComPtr<IShellItem>   item;
        ComPtr<IShellItem2>  item2;
        PWSTR                from   = nullptr;
        PWSTR                name   = nullptr;
        FILETIME             when   = {};
        ULONGLONG            stamp  = 0;
        HRESULT              hrNext = items->Next (1, &item, nullptr);
        HRESULT              hrFrom = S_OK;
        HRESULT              hrName = S_OK;
        HRESULT              hrWhen = S_OK;

        if (hrNext != S_OK)
        {
            break;
        }

        hrFrom = item.As (&item2);
        hrFrom = SUCCEEDED (hrFrom) ? item2->GetString (s_kDeletedFrom, &from) : hrFrom;
        hrName = item->GetDisplayName (SIGDN_PARENTRELATIVEEDITING, &name);

        if (FAILED (hrFrom) || FAILED (hrName))
        {
            CoTaskMemFree (from);
            CoTaskMemFree (name);
            continue;
        }

        hrWhen = item2->GetFileTime (s_kDateDeleted, &when);

        if (SUCCEEDED (hrWhen))
        {
            stamp = ((ULONGLONG) when.dwHighDateTime << 32) | when.dwLowDateTime;
        }

        for (size_t p = 0; p < paths.size(); p++)
        {
            std::filesystem::path  original (paths[p]);

            if (_wcsicmp (original.parent_path().c_str(), from) == 0 && _wcsicmp (original.filename().c_str(), name) == 0 && stamp >= bestWhen[p])
            {
                best[p]     = item;
                bestWhen[p] = stamp;
            }
        }

        CoTaskMemFree (from);
        CoTaskMemFree (name);
    }

    for (const ComPtr<IShellItem> & item : best)
    {
        PIDLIST_ABSOLUTE  pidl   = nullptr;
        HRESULT           hrList = (item != nullptr) ? SHGetIDListFromObject (item.Get(), &pidl) : E_POINTER;

        if (SUCCEEDED (hrList))
        {
            owned.push_back (pidl);
            children.push_back (ILFindLastID (pidl));
        }
    }

    found = !children.empty();
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = bin->BindToHandler (nullptr, BHID_SFObject, IID_PPV_ARGS (&folder));
    CHR (hr);

    hr = folder->GetUIObjectOf (owner, (UINT) children.size(), (PCUITEMID_CHILD_ARRAY) children.data(), IID_IContextMenu, nullptr, (void **) menu.GetAddressOf());
    CHR (hr);

    invoke.hwnd   = owner;
    invoke.lpVerb = "undelete";
    invoke.nShow  = SW_SHOWNORMAL;

    hr = menu->InvokeCommand (&invoke);
    CHR (hr);

Error:
    for (PIDLIST_ABSOLUTE pidl : owned)
    {
        CoTaskMemFree (pidl);
    }

    return hr;
}





thread_local HWND  Win32ShellVerbsTypes::CenterDialogsOver::s_owner = nullptr;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::CenterDialogsOver::CenterDialogsOver
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellVerbsTypes::CenterDialogsOver::CenterDialogsOver (HWND owner)
{
    m_prior = s_owner;
    s_owner = owner;
    m_hook  = SetWindowsHookExW (WH_CBT, &Win32ShellVerbsTypes::CenterDialogsOver::OnCbt, nullptr, GetCurrentThreadId());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::CenterDialogsOver::~CenterDialogsOver
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellVerbsTypes::CenterDialogsOver::~CenterDialogsOver()
{
    if (m_hook != nullptr)
    {
        UnhookWindowsHookEx (m_hook);
    }

    s_owner = m_prior;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::CenterDialogsOver::OnCbt
//
//  A dialog about to be shown for the first time is centered over the owner,
//  and kept on the owner's monitor.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Win32ShellVerbsTypes::CenterDialogsOver::OnCbt (int code, WPARAM wParam, LPARAM lParam)
{
    HWND         dialog            = (HWND) wParam;
    wchar_t      className[16]     = {};
    RECT         owner             = {};
    RECT         box               = {};
    MONITORINFO  monitor           = { sizeof (monitor) };
    int          width             = 0;
    int          height            = 0;
    int          left              = 0;
    int          top               = 0;
    bool         isDialog          = false;



    //  Created where the owner is, so it is laid out at that monitor's scale.
    //  Moved there only once shown, a shell dialog laid out at another scale
    //  rescales badly and cuts off its buttons.
    if (code == HCBT_CREATEWND && s_owner != nullptr && GetClassNameW (dialog, className, ARRAYSIZE (className)) > 0
        && wcscmp (className, L"#32770") == 0 && GetWindowRect (s_owner, &owner))
    {
        CREATESTRUCTW *  create = ((CBT_CREATEWNDW *) lParam)->lpcs;

        if (create != nullptr && create->hwndParent != nullptr)
        {
            create->x = (owner.left + owner.right  - (std::max) (create->cx, 0)) / 2;
            create->y = (owner.top  + owner.bottom - (std::max) (create->cy, 0)) / 2;
        }
    }

    if (code == HCBT_ACTIVATE && s_owner != nullptr && GetClassNameW (dialog, className, ARRAYSIZE (className)) > 0)
    {
        isDialog = wcscmp (className, L"#32770") == 0 && GetWindowRect (s_owner, &owner) && GetWindowRect (dialog, &box)
                   && GetMonitorInfoW (MonitorFromWindow (s_owner, MONITOR_DEFAULTTONEAREST), &monitor);
    }

    if (isDialog && GetPropW (dialog, Win32RecycleBin::s_kCenteredProp) == nullptr)
    {
        width  = box.right  - box.left;
        height = box.bottom - box.top;
        left   = std::clamp ((int) ((owner.left + owner.right - width) / 2),  (int) monitor.rcWork.left, (std::max) ((int) monitor.rcWork.left, (int) monitor.rcWork.right  - width));
        top    = std::clamp ((int) ((owner.top + owner.bottom - height) / 2), (int) monitor.rcWork.top,  (std::max) ((int) monitor.rcWork.top,  (int) monitor.rcWork.bottom - height));

        //  Once, so a dialog the user moves stays where it was put.
        SetPropW (dialog, Win32RecycleBin::s_kCenteredProp, (HANDLE) 1);
        SetWindowPos (dialog, nullptr, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    return CallNextHookEx (nullptr, code, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::ToUnixSeconds
//
////////////////////////////////////////////////////////////////////////////////

int64_t Win32RecycleBin::ToUnixSeconds (const FILETIME & when)
{
    constexpr int64_t  s_kUnixEpochTicks = 116444736000000000LL;   // 1970-01-01 in 100 ns ticks since 1601
    constexpr int64_t  s_kTicksPerSecond = 10000000LL;
    int64_t            ticks             = (int64_t) (((ULONGLONG) when.dwHighDateTime << 32) | when.dwLowDateTime);



    return (ticks - s_kUnixEpochTicks) / s_kTicksPerSecond;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::ListRecycled
//
//  Every item in the Recycle Bin, on every drive, with the properties its
//  columns show.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::ListRecycled (std::vector<IShellItemVerbs::RecycledItem> & outItems)
{
    HRESULT                  hr = S_OK;
    ComPtr<IShellItem>       bin;
    ComPtr<IEnumShellItems>  items;



    outItems.clear();

    hr = SHGetKnownFolderItem (FOLDERID_RecycleBinFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS (&bin));
    CHR (hr);

    hr = bin->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    for (;;)
    {
        ComPtr<IShellItem>             item;
        ComPtr<IShellItem2>            item2;
        IShellItemVerbs::RecycledItem  entry;
        PWSTR                          text       = nullptr;
        FILETIME                       when       = {};
        ULONGLONG                      size       = 0;
        SFGAOF                         attributes = 0;
        HRESULT                        hrNext     = items->Next (1, &item, nullptr);
        HRESULT                        hrItem     = S_OK;

        if (hrNext != S_OK)
        {
            break;
        }

        hrItem = item.As (&item2);

        if (FAILED (hrItem))
        {
            continue;
        }

        hrItem = item->GetDisplayName (SIGDN_DESKTOPABSOLUTEPARSING, &text);

        if (FAILED (hrItem))
        {
            continue;
        }

        entry.id = text;
        CoTaskMemFree (text);
        text = nullptr;

        hrItem = item->GetDisplayName (SIGDN_PARENTRELATIVEEDITING, &text);

        if (SUCCEEDED (hrItem))
        {
            entry.name = text;
            CoTaskMemFree (text);
            text = nullptr;
        }

        hrItem = item2->GetString (s_kDeletedFrom, &text);

        if (SUCCEEDED (hrItem))
        {
            entry.originalFolder = text;
            CoTaskMemFree (text);
            text = nullptr;
        }

        hrItem = item2->GetString (Win32ShellItemVerbs::s_kTypeText, &text);

        if (SUCCEEDED (hrItem))
        {
            entry.typeText = text;
            CoTaskMemFree (text);
            text = nullptr;
        }

        hrItem = item2->GetUInt64 (Win32ShellItemVerbs::s_kSize, &size);

        if (SUCCEEDED (hrItem))
        {
            entry.sizeBytes = size;
        }

        hrItem = item2->GetFileTime (s_kDateDeleted, &when);

        if (SUCCEEDED (hrItem))
        {
            entry.deletedUnix = ToUnixSeconds (when);
            entry.hasDeleted  = true;
        }

        hrItem = item2->GetFileTime (Win32ShellItemVerbs::s_kDateModified, &when);

        if (SUCCEEDED (hrItem))
        {
            entry.modifiedUnix = ToUnixSeconds (when);
            entry.hasModified  = true;
        }

        //  A zip file is a folder to the shell too; only a real folder counts.
        hrItem = item->GetAttributes (SFGAO_FOLDER | SFGAO_STREAM, &attributes);

        if (SUCCEEDED (hrItem))
        {
            entry.isFolder = (attributes & SFGAO_FOLDER) != 0 && (attributes & SFGAO_STREAM) == 0;
        }

        outItems.push_back (std::move (entry));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::GetRecycledMenu
//
//  The Recycle Bin's own menu for the items with these ids, as it gives
//  Explorer: Restore, Cut, Delete and Properties.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::GetRecycledMenu (HWND owner, const std::vector<std::wstring> & ids, IContextMenu ** outMenu)
{
    HRESULT                        hr       = S_OK;
    ComPtr<IShellItem>             bin;
    ComPtr<IShellFolder>           folder;
    ComPtr<IEnumShellItems>        items;
    std::vector<PITEMID_CHILD>     children;
    std::vector<PIDLIST_ABSOLUTE>  owned;
    bool                           found    = false;



    *outMenu = nullptr;

    hr = SHGetKnownFolderItem (FOLDERID_RecycleBinFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS (&bin));
    CHR (hr);

    hr = bin->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    for (;;)
    {
        ComPtr<IShellItem>  item;
        PWSTR               id     = nullptr;
        PIDLIST_ABSOLUTE    pidl   = nullptr;
        HRESULT             hrNext = items->Next (1, &item, nullptr);
        HRESULT             hrItem = S_OK;
        bool                wanted = false;

        if (hrNext != S_OK)
        {
            break;
        }

        hrItem = item->GetDisplayName (SIGDN_DESKTOPABSOLUTEPARSING, &id);

        if (FAILED (hrItem))
        {
            continue;
        }

        wanted = std::any_of (ids.begin(), ids.end(), [id] (const std::wstring & one) { return _wcsicmp (one.c_str(), id) == 0; });
        CoTaskMemFree (id);

        hrItem = wanted ? SHGetIDListFromObject (item.Get(), &pidl) : E_FAIL;

        if (SUCCEEDED (hrItem))
        {
            owned.push_back (pidl);
            children.push_back (ILFindLastID (pidl));
        }
    }

    found = !children.empty();
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = bin->BindToHandler (nullptr, BHID_SFObject, IID_PPV_ARGS (&folder));
    CHR (hr);

    hr = folder->GetUIObjectOf (owner, (UINT) children.size(), (PCUITEMID_CHILD_ARRAY) children.data(), IID_IContextMenu, nullptr, (void **) outMenu);
    CHR (hr);

Error:
    for (PIDLIST_ABSOLUTE pidl : owned)
    {
        CoTaskMemFree (pidl);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::RunRecycledVerb
//
//  The Recycle Bin's own Restore, Delete and Properties; its Delete asks
//  before removing the items for good.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::RunRecycledVerb (HWND owner, const std::vector<std::wstring> & ids, IShellItemVerbs::RecycledVerb verb)
{
    HRESULT               hr       = S_OK;
    ComPtr<IContextMenu>  menu;
    CMINVOKECOMMANDINFO   invoke   = { sizeof (invoke) };
    Win32ShellVerbsTypes::CenterDialogsOver     centered (owner);



    hr = GetRecycledMenu (owner, ids, &menu);
    CHR (hr);

    invoke.hwnd   = owner;
    invoke.lpVerb = (verb == IShellItemVerbs::RecycledVerb::Restore) ? "undelete" : (verb == IShellItemVerbs::RecycledVerb::Delete) ? "delete" : "properties";
    invoke.nShow  = SW_SHOWNORMAL;

    hr = menu->InvokeCommand (&invoke);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::ShowRecycledMenu
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::ShowRecycledMenu (HWND owner, const std::vector<std::wstring> & ids, POINT screenPx)
{
    HRESULT               hr = S_OK;
    ComPtr<IContextMenu>  menu;



    hr = m_verbs.m_menus.CreateMenuHost (owner);
    CHR (hr);

    hr = GetRecycledMenu (m_verbs.m_menuHost, ids, &menu);
    CHR (hr);

    hr = m_verbs.m_menus.TrackMenu (owner, menu.Get(), screenPx, {});
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::EmptyRecycleBin
//
//  Every drive's, after the shell's own confirmation, as Explorer's command
//  does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RecycleBin::EmptyRecycleBin (HWND owner)
{
    Win32ShellVerbsTypes::CenterDialogsOver  centered (owner);
    HRESULT            hr       = SHEmptyRecycleBinW (owner, nullptr, 0);



    //  An empty bin, or a confirmation answered No, is not a failure.
    if (hr == E_UNEXPECTED || hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
    {
        hr = S_OK;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::WatchRecycleBin
//
//  Two registrations: one on the bin itself, for what goes in and out of it,
//  and one on deletions anywhere in the shell's namespace, which is how a
//  file deleted in Explorer arrives in the bin.
//
////////////////////////////////////////////////////////////////////////////////

void Win32RecycleBin::WatchRecycleBin (HWND hwnd, UINT message)
{
    HRESULT              hr      = S_OK;
    PIDLIST_ABSOLUTE     bin     = nullptr;
    SHChangeNotifyEntry  binItem = {};
    SHChangeNotifyEntry  anyItem = {};



    UnwatchRecycleBin();

    hr = SHGetKnownFolderIDList (FOLDERID_RecycleBinFolder, 0, nullptr, &bin);
    CHR (hr);

    binItem.pidl       = bin;
    binItem.fRecursive = TRUE;
    m_binWatch         = SHChangeNotifyRegister (hwnd, SHCNRF_ShellLevel | SHCNRF_InterruptLevel, SHCNE_ALLEVENTS, message, 1, &binItem);

    anyItem.pidl       = nullptr;
    anyItem.fRecursive = TRUE;
    m_binDeletes       = SHChangeNotifyRegister (hwnd, SHCNRF_ShellLevel, SHCNE_DELETE | SHCNE_RMDIR, message, 1, &anyItem);

Error:
    CoTaskMemFree (bin);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RecycleBin::UnwatchRecycleBin
//
////////////////////////////////////////////////////////////////////////////////

void Win32RecycleBin::UnwatchRecycleBin()
{
    if (m_binWatch != 0)
    {
        SHChangeNotifyDeregister (m_binWatch);
        m_binWatch = 0;
    }

    if (m_binDeletes != 0)
    {
        SHChangeNotifyDeregister (m_binDeletes);
        m_binDeletes = 0;
    }
}
