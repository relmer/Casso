#include "Pch.h"

#pragma comment(lib, "propsys.lib")

#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::~Win32ShellItemVerbs
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellItemVerbs::~Win32ShellItemVerbs()
{
    if (m_menuHost != nullptr)
    {
        DestroyWindow (m_menuHost);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::Open (HWND owner, const std::wstring & path)
{
    HRESULT           hr   = S_OK;
    SHELLEXECUTEINFOW info = { sizeof (info) };
    BOOL              ran  = FALSE;



    info.fMask  = SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_LOG_USAGE;
    info.hwnd   = owner;
    info.lpFile = path.c_str();
    info.nShow  = SW_SHOWNORMAL;

    ran = ShellExecuteExW (&info);
    CWR (ran);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::EnumHandlers
//
//  The caller releases every handler returned.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::EnumHandlers (const std::wstring & path, std::vector<IAssocHandler *> & outHandlers)
{
    HRESULT                     hr           = S_OK;
    ComPtr<IEnumAssocHandlers>  handlers;
    IAssocHandler             * handler      = nullptr;
    std::wstring                extension    = std::filesystem::path (path).extension().wstring();
    bool                        hasExtension = !extension.empty();
    ULONG                       fetched      = 0;



    outHandlers.clear();

    //  A file with no extension has no type to look programs up by.
    BAIL_OUT_IF (!hasExtension, S_OK);

    hr = SHAssocEnumHandlers (extension.c_str(), ASSOC_FILTER_RECOMMENDED, &handlers);
    CHR (hr);

    while (handlers->Next (1, &handler, &fetched) == S_OK && fetched == 1)
    {
        outHandlers.push_back (handler);
        handler = nullptr;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::GetOpenWithHandlers
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::GetOpenWithHandlers (const std::wstring & path, std::vector<Handler> & outHandlers)
{
    HRESULT                       hr = S_OK;
    std::vector<IAssocHandler *>  handlers;



    outHandlers.clear();

    hr = EnumHandlers (path, handlers);
    CHR (hr);

    for (IAssocHandler * handler : handlers)
    {
        wchar_t  * name  = nullptr;
        HRESULT    named = handler->GetUIName (&name);

        if (SUCCEEDED (named) && name != nullptr)
        {
            outHandlers.push_back (Handler { name });
            CoTaskMemFree (name);
        }
        else
        {
            outHandlers.push_back (Handler { L"?" });
        }
    }

Error:
    for (IAssocHandler * handler : handlers)
    {
        handler->Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::OpenWith
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::OpenWith (HWND owner, const std::wstring & path, size_t handlerIndex)
{
    HRESULT                       hr       = S_OK;
    std::vector<IAssocHandler *>  handlers;
    ComPtr<IShellItem>            item;
    ComPtr<IDataObject>           data;
    size_t                        count    = 0;



    (void) owner;

    hr = EnumHandlers (path, handlers);
    CHR (hr);

    count = handlers.size();
    CBREx (handlerIndex < count, E_INVALIDARG);

    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->BindToHandler (nullptr, BHID_DataObject, IID_PPV_ARGS (&data));
    CHR (hr);

    hr = handlers[handlerIndex]->Invoke (data.Get());
    CHR (hr);

Error:
    for (IAssocHandler * handler : handlers)
    {
        handler->Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ChooseOtherApp
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ChooseOtherApp (HWND owner, const std::wstring & path)
{
    OPENASINFO  info = {};



    info.pcszFile  = path.c_str();
    info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_EXEC;

    return SHOpenWithDialog (owner, &info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::GetItemsMenu
//
//  The menu Explorer builds for these items: asked of the folder that holds
//  them, for its children.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::GetItemsMenu (HWND owner, const std::vector<std::wstring> & paths, IContextMenu ** outMenu)
{
    HRESULT                        hr       = S_OK;
    std::vector<PIDLIST_ABSOLUTE>  full;
    std::vector<PCUITEMID_CHILD>   children;
    ComPtr<IShellFolder>           parent;
    bool                           hasItems = !paths.empty();



    *outMenu = nullptr;

    CBREx (hasItems, E_INVALIDARG);

    for (const std::wstring & path : paths)
    {
        PIDLIST_ABSOLUTE  pidl = nullptr;

        hr = SHParseDisplayName (path.c_str(), nullptr, &pidl, 0, nullptr);
        CHR (hr);

        full.push_back (pidl);
    }

    hr = SHBindToParent (full[0], IID_PPV_ARGS (&parent), nullptr);
    CHR (hr);

    for (PIDLIST_ABSOLUTE pidl : full)
    {
        children.push_back (ILFindLastID (pidl));
    }

    hr = parent->GetUIObjectOf (owner, (UINT) children.size(), children.data(), IID_IContextMenu, nullptr, (void **) outMenu);
    CHR (hr);

Error:
    for (PIDLIST_ABSOLUTE pidl : full)
    {
        CoTaskMemFree (pidl);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::CreateMenuHost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::CreateMenuHost (HWND owner)
{
    HRESULT      hr         = S_OK;
    WNDCLASSEXW  wc         = { sizeof (wc) };
    HINSTANCE    inst       = GetModuleHandleW (nullptr);
    ATOM         registered = 0;



    BAIL_OUT_IF (m_menuHost != nullptr, S_OK);

    if (!GetClassInfoExW (inst, s_kClassName, &wc))
    {
        wc.cbSize        = sizeof (wc);
        wc.lpfnWndProc   = MenuHostProc;
        wc.hInstance     = inst;
        wc.lpszClassName = s_kClassName;

        registered = RegisterClassExW (&wc);
        CWR (registered != 0);
    }

    //  Owned by the browser, so the shell's dialogs and menus sit over it.
    m_menuHost = CreateWindowExW (0, s_kClassName, L"", WS_POPUP, 0, 0, 0, 0, owner, nullptr, inst, this);
    CWR (m_menuHost != nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::MenuHostProc
//
//  Hands the menu's own messages back to it while it is up.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Win32ShellItemVerbs::MenuHostProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Win32ShellItemVerbs  * self   = nullptr;
    LRESULT                result = 0;



    if (message == WM_NCCREATE)
    {
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCTW *) lParam)->lpCreateParams);
    }

    self = (Win32ShellItemVerbs *) GetWindowLongPtrW (hwnd, GWLP_USERDATA);

    if (self != nullptr && self->m_activeMenu != nullptr)
    {
        switch (message)
        {
            case WM_INITMENUPOPUP:
            case WM_DRAWITEM:
            case WM_MEASUREITEM:
            case WM_MENUCHAR:
            {
                ComPtr<IContextMenu3>  menu3;
                HRESULT                hasMenu3 = self->m_activeMenu->QueryInterface (IID_PPV_ARGS (&menu3));
                HRESULT                handled  = E_NOTIMPL;

                if (SUCCEEDED (hasMenu3))
                {
                    handled = menu3->HandleMenuMsg2 (message, wParam, lParam, &result);

                    if (SUCCEEDED (handled))
                    {
                        return result;
                    }
                }

                handled = self->m_activeMenu->HandleMenuMsg (message, wParam, lParam);

                if (SUCCEEDED (handled))
                {
                    return (message == WM_INITMENUPOPUP) ? 0 : TRUE;
                }

                break;
            }

            default:
                break;
        }
    }

    return DefWindowProcW (hwnd, message, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ShowShellMenu
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ShowShellMenu (HWND owner, const std::vector<std::wstring> & paths, POINT screenPx)
{
    HRESULT               hr = S_OK;
    ComPtr<IContextMenu>  menu;



    hr = CreateMenuHost (owner);
    CHR (hr);

    hr = GetItemsMenu (m_menuHost, paths, &menu);
    CHR (hr);

    hr = TrackMenu (owner, menu.Get(), screenPx);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::TrackMenu
//
//  A shell menu shown at a point from the menu host, and whatever is picked
//  carried out.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::TrackMenu (HWND owner, IContextMenu * menu, POINT screenPx)
{
    CenterDialogsOver  centered (owner);
    HRESULT                hr       = S_OK;
    ComPtr<IContextMenu2>  menu2;
    HMENU                  popup    = nullptr;
    UINT                   picked   = 0;
    CMINVOKECOMMANDINFOEX  invoke   = { sizeof (invoke) };
    HRESULT                hasMenu2 = S_OK;



    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, s_kFirstCommandId, s_kLastCommandId, CMF_NORMAL);
    CHR (hr);

    hasMenu2 = menu->QueryInterface (IID_PPV_ARGS (&menu2));

    if (SUCCEEDED (hasMenu2))
    {
        m_activeMenu = menu2.Get();
    }

    picked = (UINT) TrackPopupMenuEx (popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPx.x, screenPx.y, m_menuHost, nullptr);

    m_activeMenu = nullptr;

    BAIL_OUT_IF (picked < s_kFirstCommandId, S_OK);

    invoke.fMask        = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
    invoke.hwnd         = owner;
    invoke.lpVerb       = MAKEINTRESOURCEA (picked - s_kFirstCommandId);
    invoke.lpVerbW      = MAKEINTRESOURCEW (picked - s_kFirstCommandId);
    invoke.nShow        = SW_SHOWNORMAL;
    invoke.ptInvoke     = screenPx;

    hr = menu->InvokeCommand ((LPCMINVOKECOMMANDINFO) &invoke);
    CHR (hr);

Error:
    m_activeMenu = nullptr;

    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::GetItemArray
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::GetItemArray (const std::vector<std::wstring> & paths, IShellItemArray ** outItems)
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
//  Win32ShellItemVerbs::CreateOperation
//
//  Undoable, as Explorer's are, and asking before anything is overwritten.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::CreateOperation (HWND owner, IFileOperation ** outOperation)
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
//  Win32ShellItemVerbs::Recycle
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::Recycle (HWND owner, const std::vector<std::wstring> & paths)
{
    CenterDialogsOver  centered (owner);
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

    hr = operation->PerformOperations();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::RestoreRecycled
//
//  Each path's most recently deleted copy in the Recycle Bin, found by where
//  it was deleted from and its name, restored by the Recycle Bin's own
//  Restore verb, as Explorer's undo puts a deletion back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::RestoreRecycled (HWND owner, const std::vector<std::wstring> & paths)
{
    CenterDialogsOver  centered (owner);
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





thread_local HWND  Win32ShellItemVerbs::CenterDialogsOver::s_owner = nullptr;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::CenterDialogsOver::CenterDialogsOver
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellItemVerbs::CenterDialogsOver::CenterDialogsOver (HWND owner)
{
    m_prior = s_owner;
    s_owner = owner;
    m_hook  = SetWindowsHookExW (WH_CBT, &CenterDialogsOver::OnCbt, nullptr, GetCurrentThreadId());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::CenterDialogsOver::~CenterDialogsOver
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellItemVerbs::CenterDialogsOver::~CenterDialogsOver()
{
    if (m_hook != nullptr)
    {
        UnhookWindowsHookEx (m_hook);
    }

    s_owner = m_prior;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::CenterDialogsOver::OnCbt
//
//  A dialog about to be shown for the first time is centered over the owner,
//  and kept on the owner's monitor.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Win32ShellItemVerbs::CenterDialogsOver::OnCbt (int code, WPARAM wParam, LPARAM lParam)
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



    if (code == HCBT_ACTIVATE && s_owner != nullptr && GetClassNameW (dialog, className, ARRAYSIZE (className)) > 0)
    {
        isDialog = wcscmp (className, L"#32770") == 0 && GetWindowRect (s_owner, &owner) && GetWindowRect (dialog, &box)
                   && GetMonitorInfoW (MonitorFromWindow (s_owner, MONITOR_DEFAULTTONEAREST), &monitor);
    }

    if (isDialog && GetPropW (dialog, s_kCenteredProp) == nullptr)
    {
        width  = box.right  - box.left;
        height = box.bottom - box.top;
        left   = std::clamp ((int) ((owner.left + owner.right - width) / 2),  (int) monitor.rcWork.left, (std::max) ((int) monitor.rcWork.left, (int) monitor.rcWork.right  - width));
        top    = std::clamp ((int) ((owner.top + owner.bottom - height) / 2), (int) monitor.rcWork.top,  (std::max) ((int) monitor.rcWork.top,  (int) monitor.rcWork.bottom - height));

        //  Once, so a dialog the user moves stays where it was put.
        SetPropW (dialog, s_kCenteredProp, (HANDLE) 1);
        SetWindowPos (dialog, nullptr, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    return CallNextHookEx (nullptr, code, wParam, lParam);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ToUnixSeconds
//
////////////////////////////////////////////////////////////////////////////////

int64_t Win32ShellItemVerbs::ToUnixSeconds (const FILETIME & when)
{
    constexpr int64_t  s_kUnixEpochTicks = 116444736000000000LL;   // 1970-01-01 in 100 ns ticks since 1601
    constexpr int64_t  s_kTicksPerSecond = 10000000LL;
    int64_t            ticks             = (int64_t) (((ULONGLONG) when.dwHighDateTime << 32) | when.dwLowDateTime);



    return (ticks - s_kUnixEpochTicks) / s_kTicksPerSecond;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ListRecycled
//
//  Every item in the Recycle Bin, on every drive, with the properties its
//  columns show.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ListRecycled (std::vector<RecycledItem> & outItems)
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
        ComPtr<IShellItem>   item;
        ComPtr<IShellItem2>  item2;
        RecycledItem         entry;
        PWSTR                text       = nullptr;
        FILETIME             when       = {};
        ULONGLONG            size       = 0;
        SFGAOF               attributes = 0;
        HRESULT              hrNext     = items->Next (1, &item, nullptr);
        HRESULT              hrItem     = S_OK;

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

        hrItem = item2->GetString (s_kTypeText, &text);

        if (SUCCEEDED (hrItem))
        {
            entry.typeText = text;
            CoTaskMemFree (text);
            text = nullptr;
        }

        hrItem = item2->GetUInt64 (s_kSize, &size);

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

        hrItem = item2->GetFileTime (s_kDateModified, &when);

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
//  Win32ShellItemVerbs::GetRecycledMenu
//
//  The Recycle Bin's own menu for the items with these ids, as it gives
//  Explorer: Restore, Cut, Delete and Properties.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::GetRecycledMenu (HWND owner, const std::vector<std::wstring> & ids, IContextMenu ** outMenu)
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
//  Win32ShellItemVerbs::RunRecycledVerb
//
//  The Recycle Bin's own Restore, Delete and Properties; its Delete asks
//  before removing the items for good.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::RunRecycledVerb (HWND owner, const std::vector<std::wstring> & ids, RecycledVerb verb)
{
    HRESULT               hr       = S_OK;
    ComPtr<IContextMenu>  menu;
    CMINVOKECOMMANDINFO   invoke   = { sizeof (invoke) };
    CenterDialogsOver     centered (owner);



    hr = GetRecycledMenu (owner, ids, &menu);
    CHR (hr);

    invoke.hwnd   = owner;
    invoke.lpVerb = (verb == RecycledVerb::Restore) ? "undelete" : (verb == RecycledVerb::Delete) ? "delete" : "properties";
    invoke.nShow  = SW_SHOWNORMAL;

    hr = menu->InvokeCommand (&invoke);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ShowRecycledMenu
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ShowRecycledMenu (HWND owner, const std::vector<std::wstring> & ids, POINT screenPx)
{
    HRESULT               hr = S_OK;
    ComPtr<IContextMenu>  menu;



    hr = CreateMenuHost (owner);
    CHR (hr);

    hr = GetRecycledMenu (m_menuHost, ids, &menu);
    CHR (hr);

    hr = TrackMenu (owner, menu.Get(), screenPx);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::EmptyRecycleBin
//
//  Every drive's, after the shell's own confirmation, as Explorer's command
//  does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::EmptyRecycleBin (HWND owner)
{
    CenterDialogsOver  centered (owner);
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
//  Win32ShellItemVerbs::ListShellFolder
//
//  What the shell lists in the folder, as Explorer lists it, with the
//  columns Explorer shows: type, size and date modified.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ListShellFolder (const std::wstring & id, std::vector<ShellFolderItem> & outItems)
{
    return ReadShellFolder (id, outItems);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ReadShellFolder
//
//  Belongs to no object, so it runs on a reader thread that may outlive the
//  window.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ReadShellFolder (const std::wstring & id, std::vector<ShellFolderItem> & outItems)
{
    HRESULT                  hr = S_OK;
    ComPtr<IShellItem>       folder;
    ComPtr<IEnumShellItems>  items;



    outItems.clear();

    //  A search's results are a folder of their own.
    if (SearchQuery::IsId (id))
    {
        return RunSearch (id, outItems);
    }

    hr = SHCreateItemFromParsingName (id.c_str(), nullptr, IID_PPV_ARGS (&folder));
    CHR (hr);

    hr = folder->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    for (;;)
    {
        ComPtr<IShellItem>  item;
        ShellFolderItem     entry;
        HRESULT             hrNext = items->Next (1, &item, nullptr);
        HRESULT             hrItem = S_OK;

        if (hrNext != S_OK)
        {
            break;
        }

        hrItem = DescribeShellItem (item.Get(), entry);

        if (SUCCEEDED (hrItem))
        {
            outItems.push_back (std::move (entry));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::DescribeShellItem
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::DescribeShellItem (IShellItem * item, ShellFolderItem & outEntry)
{
    HRESULT              hr         = S_OK;
    HRESULT              hrItem     = S_OK;
    ComPtr<IShellItem2>  item2;
    PWSTR                text       = nullptr;
    FILETIME             when       = {};
    ULONGLONG            size       = 0;
    SFGAOF               attributes = 0;



    outEntry = ShellFolderItem();

    hr = item->QueryInterface (IID_PPV_ARGS (&item2));
    CHR (hr);

    hr = item->GetDisplayName (SIGDN_DESKTOPABSOLUTEPARSING, &text);
    CHR (hr);

    outEntry.id = text;
    CoTaskMemFree (text);
    text = nullptr;

    hrItem = item->GetDisplayName (SIGDN_NORMALDISPLAY, &text);

    if (SUCCEEDED (hrItem))
    {
        outEntry.name = text;
        CoTaskMemFree (text);
        text = nullptr;
    }

    outEntry.path = GetItemPath (item);

    hrItem = item2->GetString (s_kTypeText, &text);

    if (SUCCEEDED (hrItem))
    {
        outEntry.typeText = text;
        CoTaskMemFree (text);
        text = nullptr;
    }

    hrItem = item2->GetUInt64 (s_kSize, &size);

    if (SUCCEEDED (hrItem))
    {
        outEntry.sizeBytes = size;
    }

    hrItem = item2->GetFileTime (s_kDateModified, &when);

    if (SUCCEEDED (hrItem))
    {
        outEntry.modifiedUnix = ToUnixSeconds (when);
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
//  Win32ShellItemVerbs::ListNavigationRoots
//
//  The desktop's items that ask for the navigation pane, as Explorer's pane
//  lists them, in its order. Home, Gallery and the user's own OneDrive lead,
//  above the pinned folders.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ListNavigationRoots (std::vector<ShellFolderItem> & outRoots)
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
        PITEMID_CHILD        child  = nullptr;
        ComPtr<IShellItem>   item;
        ComPtr<IShellItem2>  item2;
        ShellFolderItem      entry;
        BOOL                 pinned = FALSE;
        HRESULT              hrNext = children->Next (1, &child, nullptr);
        HRESULT              hrItem = S_OK;
        HRESULT              hrName = S_OK;
        STRRET               strret = {};
        PWSTR                name   = nullptr;
        std::wstring         iconId;

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
        ComPtr<IShellItem>  item;
        ShellFolderItem     entry;
        HRESULT             hrItem = (id != nullptr) ? SHCreateItemFromParsingName (id, nullptr, IID_PPV_ARGS (&item)) : E_ABORT;

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

    std::stable_sort (outRoots.begin(), outRoots.end(), [] (const ShellFolderItem & a, const ShellFolderItem & b)
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
//  Win32ShellItemVerbs::SetPinnedToQuickAccess
//
//  The verbs Explorer's own Pin to Quick access and Unpin from Quick access
//  run. Pin is on the folder's own menu; Unpin is on its entry in Quick
//  access alone. A menu has to be built before a verb is asked of it by name.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::SetPinnedToQuickAccess (HWND owner, const std::wstring & folder, bool pinned)
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
        hr = GetItemsMenu (owner, { folder }, &menu);
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

            if (_wcsicmp (GetItemPath (item.Get()).c_str(), folder.c_str()) == 0)
            {
                hr = item->BindToHandler (nullptr, BHID_SFUIObject, IID_PPV_ARGS (&menu));
                CHR (hr);
                break;
            }
        }

        CBREx (menu != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));
    }

    hr = menu->QueryContextMenu (popup, 0, s_kFirstCommandId, s_kLastCommandId, CMF_NORMAL);
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
//  Win32ShellItemVerbs::GetNavigationRank
//
//  Explorer's order: Home, Gallery, the user's OneDrive; the cloud drives on
//  the disk; This PC, Libraries, Network, Linux; anything else after.
//
////////////////////////////////////////////////////////////////////////////////

int Win32ShellItemVerbs::GetNavigationRank (const ShellFolderItem & root)
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
//  Win32ShellItemVerbs::RunSearch
//
//  From the Windows search index where the folder is indexed, in File
//  Explorer's query syntax; otherwise by walking the folder and matching
//  names. Each result is described as any shell item is, with its folder.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::RunSearch (const std::wstring & id, std::vector<ShellFolderItem> & outItems)
{
    HRESULT                    hr      = S_OK;
    std::wstring               scope;
    std::wstring               query;
    std::vector<std::wstring>  paths;
    bool                       indexed = false;
    bool                       parsed  = SearchQuery::TryParseId (id, scope, query);



    CBREx (parsed, E_INVALIDARG);

    indexed = IsIndexed (scope);

    if (indexed)
    {
        hr = QueryIndex (scope, query, paths);
    }

    //  A folder the index leaves out, or an index that cannot answer, is
    //  searched by walking it.
    if (!indexed || FAILED (hr))
    {
        paths.clear();
        WalkForNames (scope, SearchQuery::GetNameWords (query), paths);
        hr = S_OK;
    }

    for (const std::wstring & path : paths)
    {
        ComPtr<IShellItem>  item;
        ShellFolderItem     entry;
        HRESULT             hrItem = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));

        if (SUCCEEDED (hrItem))
        {
            hrItem = DescribeShellItem (item.Get(), entry);
        }

        if (SUCCEEDED (hrItem))
        {
            entry.folder = path.substr (0, path.find_last_of (L'\\'));
            outItems.push_back (std::move (entry));
        }
    }

    //  Files inside the disk images there match by name too.
    SearchImages (scope, SearchQuery::GetNameWords (query), outItems);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::SearchImages
//
//  Each disk image below the scope, read as Casso Explorer lists it; a file in
//  its catalog whose name holds every word is a result, its folder the image.
//  An image that cannot be read is passed over. A query of properties alone
//  matches no name, so no image is read for it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellItemVerbs::SearchImages (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<ShellFolderItem> & outItems)
{
    std::vector<std::wstring>  images;
    Win32DiskFileIo            fileIo;
    DiskOperations             operations (fileIo);



    if (words.empty())
    {
        return;
    }

    WalkForImages (scope, images);

    for (const std::wstring & image : images)
    {
        VolumeListing           listing;
        VolumeKind              kind   = VolumeKind::Dos33;
        DiskOperations::Result  result = operations.List (TextEncoding::WideToNarrow (image), listing, kind);

        if (!result.Succeeded())
        {
            continue;
        }

        for (const FileEntry & file : listing.entries)
        {
            ShellFolderItem  entry;

            entry.name = std::wstring (file.name.begin(), file.name.end());

            if (!SearchQuery::MatchesName (entry.name, words) || outItems.size() >= s_kMaxResults)
            {
                continue;
            }

            entry.id        = image + L"\\" + entry.name;
            entry.typeText  = CatalogModel::GetTypeText (file.type, kind);
            entry.sizeBytes = file.hasEofBytes ? file.eofBytes : (uint64_t) file.sizeUnits * ((kind == VolumeKind::Dos33) ? 256u : 512u);
            entry.isFolder  = file.isDirectory;
            entry.folder    = image;
            entry.imagePath = image;
            outItems.push_back (std::move (entry));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::WalkForImages
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellItemVerbs::WalkForImages (const std::wstring & scope, std::vector<std::wstring> & outImages)
{
    std::vector<std::wstring>  pending = { scope };



    while (!pending.empty() && outImages.size() < s_kMaxImages)
    {
        std::wstring      folder = pending.back();
        WIN32_FIND_DATAW  found  = {};
        HANDLE            find   = INVALID_HANDLE_VALUE;

        pending.pop_back();

        while (!folder.empty() && folder.back() == L'\\')
        {
            folder.pop_back();
        }

        find = FindFirstFileExW ((folder + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

        if (find == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        do
        {
            std::wstring  name     = found.cFileName;
            bool          isFolder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool          isLink   = (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

            if (name == L"." || name == L"..")
            {
                continue;
            }

            if (isFolder && !isLink)
            {
                pending.push_back (folder + L"\\" + name);
            }
            else if (!isFolder && TreeModel::IsSupportedImage (name))
            {
                outImages.push_back (folder + L"\\" + name);
            }
        }
        while (FindNextFileW (find, &found) && outImages.size() < s_kMaxImages);

        FindClose (find);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::IsIndexed
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellItemVerbs::IsIndexed (const std::wstring & scope)
{
    HRESULT                            hr       = S_OK;
    ComPtr<ISearchManager>             manager;
    ComPtr<ISearchCatalogManager>      catalog;
    ComPtr<ISearchCrawlScopeManager>   scopes;
    BOOL                               included = FALSE;



    hr = CoCreateInstance (__uuidof (CSearchManager), nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS (&manager));
    CHR (hr);

    hr = manager->GetCatalog (L"SystemIndex", &catalog);
    CHR (hr);

    hr = catalog->GetCrawlScopeManager (&scopes);
    CHR (hr);

    hr = scopes->IncludedInCrawlScope ((L"file:///" + scope).c_str(), &included);
    CHR (hr);

Error:
    return SUCCEEDED (hr) && included;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::QueryIndex
//
//  The index's own helper turns the query into its SQL, so the query means
//  what it means in File Explorer's box; the results are read through the
//  index's data source.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::QueryIndex (const std::wstring & scope, const std::wstring & query, std::vector<std::wstring> & outPaths)
{
    static constexpr CLSID  s_kCollatorDataSource = { 0x9E175B8B, 0xF52A, 0x11D8, { 0xB9, 0xA5, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30 } };
    static constexpr GUID   s_kDbGuidDefault      = { 0xC8B521FB, 0x5CF3, 0x11CE, { 0xAD, 0xE5, 0x00, 0xAA, 0x00, 0x44, 0x77, 0x3D } };



    struct Row
    {
        DBSTATUS   status = 0;
        DBLENGTH   length = 0;
        wchar_t    value[MAX_PATH * 2] = {};
    };

    HRESULT                          hr       = S_OK;
    ComPtr<ISearchManager>           manager;
    ComPtr<ISearchCatalogManager>    catalog;
    ComPtr<ISearchQueryHelper>       helper;
    ComPtr<IDBInitialize>            source;
    ComPtr<IDBCreateSession>         sessions;
    ComPtr<IDBCreateCommand>         session;
    ComPtr<ICommandText>             command;
    ComPtr<IRowset>                  rows;
    ComPtr<IAccessor>                accessor;
    PWSTR                            sql      = nullptr;
    HACCESSOR                        binder   = DB_NULL_HACCESSOR;
    DBBINDING                        binding  = {};
    Row                              row;



    outPaths.clear();

    hr = CoCreateInstance (__uuidof (CSearchManager), nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS (&manager));
    CHR (hr);

    hr = manager->GetCatalog (L"SystemIndex", &catalog);
    CHR (hr);

    hr = catalog->GetQueryHelper (&helper);
    CHR (hr);

    hr = helper->put_QuerySelectColumns (L"System.ItemPathDisplay");
    CHR (hr);

    hr = helper->put_QueryWhereRestrictions ((L"AND SCOPE='file:" + scope + L"'").c_str());
    CHR (hr);

    hr = helper->put_QueryMaxResults ((LONG) s_kMaxResults);
    CHR (hr);

    hr = helper->GenerateSQLFromUserQuery (query.c_str(), &sql);
    CHR (hr);

    hr = CoCreateInstance (s_kCollatorDataSource, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&source));
    CHR (hr);

    hr = source->Initialize();
    CHR (hr);

    hr = source.As (&sessions);
    CHR (hr);

    hr = sessions->CreateSession (nullptr, __uuidof (IDBCreateCommand), reinterpret_cast<IUnknown **> (session.GetAddressOf()));
    CHR (hr);

    hr = session->CreateCommand (nullptr, __uuidof (ICommandText), reinterpret_cast<IUnknown **> (command.GetAddressOf()));
    CHR (hr);

    hr = command->SetCommandText (s_kDbGuidDefault, sql);
    CHR (hr);

    hr = command->Execute (nullptr, __uuidof (IRowset), nullptr, nullptr, reinterpret_cast<IUnknown **> (rows.GetAddressOf()));
    CHR (hr);

    hr = rows.As (&accessor);
    CHR (hr);

    binding.iOrdinal   = 1;
    binding.obValue    = offsetof (Row, value);
    binding.obLength   = offsetof (Row, length);
    binding.obStatus   = offsetof (Row, status);
    binding.dwPart     = DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS;
    binding.dwMemOwner = DBMEMOWNER_CLIENTOWNED;
    binding.eParamIO   = DBPARAMIO_NOTPARAM;
    binding.cbMaxLen   = sizeof (row.value);
    binding.wType      = DBTYPE_WSTR;

    hr = accessor->CreateAccessor (DBACCESSOR_ROWDATA, 1, &binding, sizeof (Row), &binder, nullptr);
    CHR (hr);

    for (;;)
    {
        HROW          handles[64] = {};
        HROW        * fetched     = handles;
        DBCOUNTITEM   count       = 0;
        HRESULT       hrRows      = rows->GetNextRows (DB_NULL_HCHAPTER, 0, 64, &count, &fetched);

        for (DBCOUNTITEM i = 0; i < count; i++)
        {
            HRESULT  hrData = S_OK;

            row    = Row();
            hrData = rows->GetData (handles[i], binder, &row);

            if (SUCCEEDED (hrData) && row.status == DBSTATUS_S_OK)
            {
                outPaths.emplace_back (row.value);
            }
        }

        if (count > 0)
        {
            HRESULT  hrRelease = rows->ReleaseRows (count, handles, nullptr, nullptr, nullptr);

            IGNORE_RETURN_VALUE (hrRelease, S_OK);
        }

        if (FAILED (hrRows) || hrRows == DB_S_ENDOFROWSET || count == 0)
        {
            break;
        }
    }

Error:
    if (binder != DB_NULL_HACCESSOR)
    {
        accessor->ReleaseAccessor (binder, nullptr);
    }

    CoTaskMemFree (sql);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::WalkForNames
//
//  Every file and folder below the scope whose name holds each word, folder
//  by folder; a link to another folder is not followed, so a loop cannot.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellItemVerbs::WalkForNames (const std::wstring & scope, const std::vector<std::wstring> & words, std::vector<std::wstring> & outPaths)
{
    std::vector<std::wstring>  pending = { scope };



    while (!pending.empty() && outPaths.size() < s_kMaxResults)
    {
        std::wstring      folder = pending.back();
        WIN32_FIND_DATAW  found  = {};
        HANDLE            find   = INVALID_HANDLE_VALUE;

        pending.pop_back();

        while (!folder.empty() && folder.back() == L'\\')
        {
            folder.pop_back();
        }

        find = FindFirstFileExW ((folder + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

        if (find == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        do
        {
            std::wstring  name     = found.cFileName;
            bool          isFolder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool          isLink   = (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

            if (name == L"." || name == L"..")
            {
                continue;
            }

            if (SearchQuery::MatchesName (name, words) && !words.empty())
            {
                outPaths.push_back (folder + L"\\" + name);
            }

            if (isFolder && !isLink)
            {
                pending.push_back (folder + L"\\" + name);
            }
        }
        while (FindNextFileW (find, &found) && outPaths.size() < s_kMaxResults);

        FindClose (find);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ListDesktopFolders
//
//  The desktop as the shell's namespace has it, folders only: its roots in
//  the shell's own order, then the folders on the user's desktop by name. A
//  zip file is a folder to the shell, but not one Explorer lists here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ListDesktopFolders (std::vector<ShellFolderItem> & outFolders)
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
        ComPtr<IShellItem>  item;
        ShellFolderItem     entry;
        HRESULT             hrNext = items->Next (1, &item, nullptr);
        HRESULT             hrItem = S_OK;

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

    std::stable_sort (outFolders.begin(), outFolders.begin() + (ptrdiff_t) roots, [] (const ShellFolderItem & a, const ShellFolderItem & b)
    {
        return GetDesktopRank (a) < GetDesktopRank (b);
    });

    std::sort (outFolders.begin() + (ptrdiff_t) roots, outFolders.end(), [] (const ShellFolderItem & a, const ShellFolderItem & b)
    {
        return StrCmpLogicalW (a.name.c_str(), b.name.c_str()) < 0;
    });

Error:
    CoTaskMemFree (root);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::IsDesktopRoot
//
//  The desktop's own items: the shell's folders with no path, and the
//  user's folder and OneDrive, which the desktop holds as roots too.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellItemVerbs::IsDesktopRoot (const ShellFolderItem & item)
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
//  Win32ShellItemVerbs::IsOnDesktop
//
//  A real folder whose parent is the user's desktop or the public one; a
//  shortcut is not a folder there.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellItemVerbs::IsOnDesktop (const std::wstring & path)
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
//  Win32ShellItemVerbs::GetDesktopRank
//
//  Explorer's order for the desktop's roots: Home, Gallery, OneDrive, the
//  user's folder, This PC, Libraries, Network, Control Panel, Linux and the
//  Recycle Bin; anything else among the shell's with Network.
//
////////////////////////////////////////////////////////////////////////////////

int Win32ShellItemVerbs::GetDesktopRank (const ShellFolderItem & root)
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
//  Win32ShellItemVerbs::ListPinnedFolders
//
//  Quick access holds the pinned folders and the frequent ones; Explorer's
//  pane shows the pinned alone.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ListPinnedFolders (std::vector<ShellFolderItem> & outFolders)
{
    HRESULT                  hr     = S_OK;
    ComPtr<IShellItem>       quick;
    ComPtr<IEnumShellItems>  items;
    PROPERTYKEY              pinned = {};



    outFolders.clear();

    hr = PSGetPropertyKeyFromName (L"System.Home.IsPinned", &pinned);
    CHR (hr);

    hr = SHCreateItemFromParsingName (s_kQuickAccessId, nullptr, IID_PPV_ARGS (&quick));
    CHR (hr);

    hr = quick->BindToHandler (nullptr, BHID_EnumItems, IID_PPV_ARGS (&items));
    CHR (hr);

    for (;;)
    {
        ComPtr<IShellItem>   item;
        ComPtr<IShellItem2>  item2;
        ShellFolderItem      entry;
        BOOL                 isPinned = FALSE;
        HRESULT              hrNext   = items->Next (1, &item, nullptr);
        HRESULT              hrItem   = S_OK;

        if (hrNext != S_OK)
        {
            break;
        }

        hrItem = item.As (&item2);

        if (SUCCEEDED (hrItem))
        {
            hrItem = item2->GetBool (pinned, &isPinned);
        }

        if (FAILED (hrItem) || !isPinned)
        {
            continue;
        }

        hrItem = DescribeShellItem (item.Get(), entry);

        if (SUCCEEDED (hrItem) && entry.isFolder)
        {
            outFolders.push_back (std::move (entry));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::GetShellItemName
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::GetShellItemName (const std::wstring & id, std::wstring & outName)
{
    HRESULT             hr   = S_OK;
    ComPtr<IShellItem>  item;
    PWSTR               text = nullptr;



    hr = SHCreateItemFromParsingName (id.c_str(), nullptr, IID_PPV_ARGS (&item));
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
//  Win32ShellItemVerbs::GetShellParent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::GetShellParent (const std::wstring & id, ShellFolderItem & outParent)
{
    HRESULT             hr     = S_OK;
    ComPtr<IShellItem>  item;
    ComPtr<IShellItem>  parent;
    PWSTR               text   = nullptr;



    outParent = ShellFolderItem();

    hr = SHCreateItemFromParsingName (id.c_str(), nullptr, IID_PPV_ARGS (&item));
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
    outParent.path     = GetItemPath (parent.Get());
    outParent.isFolder = true;

Error:
    CoTaskMemFree (text);
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::OpenShellItem
//
//  The item's default action by its place in the shell, which reaches items
//  that have no path: a network computer, a library, a control panel.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::OpenShellItem (HWND owner, const std::wstring & id)
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





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::WatchRecycleBin
//
//  Two registrations: one on the bin itself, for what goes in and out of it,
//  and one on deletions anywhere in the shell's namespace, which is how a
//  file deleted in Explorer arrives in the bin.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellItemVerbs::WatchRecycleBin (HWND hwnd, UINT message)
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
//  Win32ShellItemVerbs::UnwatchRecycleBin
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellItemVerbs::UnwatchRecycleBin()
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





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::RenameItem
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::RenameItem (HWND owner, const std::wstring & path, const std::wstring & newName)
{
    CenterDialogsOver  centered (owner);
    HRESULT                 hr = S_OK;
    ComPtr<IFileOperation>  operation;
    ComPtr<IShellItem>      item;



    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = CreateOperation (owner, &operation);
    CHR (hr);

    hr = operation->RenameItem (item.Get(), newName.c_str(), nullptr);
    CHR (hr);

    hr = operation->PerformOperations();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::PlaceOnClipboard
//
//  The shell's own data object for the items, marked as a cut or a copy the
//  way Explorer marks it, so a paste in Explorer or here moves or copies.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::PlaceOnClipboard (HWND owner, const std::vector<std::wstring> & paths, bool cut)
{
    HRESULT                  hr     = S_OK;
    ComPtr<IShellItemArray>  items;
    ComPtr<IDataObject>      data;
    FORMATETC                format = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_PREFERREDDROPEFFECT), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM                medium = { TYMED_HGLOBAL };
    DWORD                  * effect = nullptr;



    (void) owner;

    hr = GetItemArray (paths, &items);
    CHR (hr);

    hr = items->BindToHandler (nullptr, BHID_DataObject, IID_PPV_ARGS (&data));
    CHR (hr);

    medium.hGlobal = GlobalAlloc (GMEM_MOVEABLE, sizeof (DWORD));
    CPR (medium.hGlobal);

    effect  = (DWORD *) GlobalLock (medium.hGlobal);
    CPRAF (effect, GlobalFree (medium.hGlobal));

    *effect = cut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
    GlobalUnlock (medium.hGlobal);

    //  The data object takes the memory.
    hr = data->SetData (&format, &medium, TRUE);
    CHRAF (hr, ReleaseStgMedium (&medium));

    hr = OleSetClipboard (data.Get());
    CHR (hr);

    //  The data stays on the clipboard after this window closes.
    hr = OleFlushClipboard();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ClipboardHasFiles
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellItemVerbs::ClipboardHasFiles() const
{
    return IsClipboardFormatAvailable (CF_HDROP) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::IsFromFolder
//
//  Whether any file the object holds is in the folder itself.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellItemVerbs::IsFromFolder (IDataObject * data, const std::wstring & folder)
{
    FORMATETC     format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM     medium = {};
    HDROP         drop   = nullptr;
    UINT          count  = 0;
    bool          found  = false;
    std::wstring  inside = folder;
    HRESULT       hrData = (data != nullptr) ? data->GetData (&format, &medium) : E_POINTER;



    if (FAILED (hrData))
    {
        return false;
    }

    while (!inside.empty() && inside.back() == L'\\')
    {
        inside.pop_back();
    }

    drop  = (HDROP) medium.hGlobal;
    count = DragQueryFileW (drop, 0xFFFFFFFF, nullptr, 0);

    for (UINT i = 0; i < count && !found; i++)
    {
        wchar_t       path[MAX_PATH * 4] = {};
        std::wstring  parent;

        DragQueryFileW (drop, i, path, (UINT) std::size (path));
        parent = std::filesystem::path (path).parent_path().wstring();
        found  = _wcsicmp (parent.c_str(), inside.c_str()) == 0;
    }

    ReleaseStgMedium (&medium);

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::PasteInto
//
//  A cut moves and a copy copies, as whoever put the files there asked.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::PasteInto (HWND owner, const std::wstring & folder, PasteResult & outResult)
{
    CenterDialogsOver  centered (owner);
    HRESULT                 hr        = S_OK;
    ComPtr<IDataObject>     data;
    ComPtr<IShellItem>      target;
    ComPtr<IFileOperation>  operation;
    FORMATETC               format    = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_PREFERREDDROPEFFECT), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM               medium    = {};
    HRESULT                 hasMark   = S_OK;
    DWORD                   effect    = DROPEFFECT_COPY;
    OperationRecorder       recorder;
    DWORD                   cookie    = 0;
    HRESULT                 unadvised = S_OK;
    std::wstring            parent;



    outResult = PasteResult();

    hr = OleGetClipboard (&data);
    CHR (hr);

    hasMark = data->GetData (&format, &medium);

    if (SUCCEEDED (hasMark))
    {
        DWORD  * marked = (DWORD *) GlobalLock (medium.hGlobal);

        effect = (marked != nullptr) ? *marked : DROPEFFECT_COPY;
        GlobalUnlock (medium.hGlobal);
        ReleaseStgMedium (&medium);
    }

    hr = SHCreateItemFromParsingName (folder.c_str(), nullptr, IID_PPV_ARGS (&target));
    CHR (hr);

    hr = CreateOperation (owner, &operation);
    CHR (hr);

    if ((effect & DROPEFFECT_MOVE) != 0)
    {
        hr = operation->MoveItems (data.Get(), target.Get());
    }
    else
    {
        //  A copy into the folder it came from is named "name - Copy", as
        //  Explorer names it, rather than stopping on the names being the same.
        if (IsFromFolder (data.Get(), folder))
        {
            hr = operation->SetOperationFlags (FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR | FOFX_ADDUNDORECORD | FOF_RENAMEONCOLLISION);
            CHR (hr);
        }

        hr = operation->CopyItems (data.Get(), target.Get());
    }

    CHR (hr);

    hr = operation->Advise (&recorder, &cookie);
    CHR (hr);

    hr        = operation->PerformOperations();
    unadvised = operation->Unadvise (cookie);
    IGNORE_RETURN_VALUE (unadvised, S_OK);
    CHR (hr);

    //  What went into the folder itself; the contents of a folder copied
    //  come back with it on an undo.
    outResult.moved = (effect & DROPEFFECT_MOVE) != 0;

    for (size_t i = 0; i < recorder.created.size(); i++)
    {
        parent = std::filesystem::path (recorder.created[i]).parent_path().wstring();

        if (_wcsicmp (parent.c_str(), std::filesystem::path (folder).wstring().c_str()) == 0)
        {
            outResult.sources.push_back (recorder.sources[i]);
            outResult.created.push_back (recorder.created[i]);
        }
    }

    //  A cut is used up by its paste.
    if (outResult.moved)
    {
        hr = OleSetClipboard (nullptr);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::MoveItemsTo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::MoveItemsTo (HWND owner, const std::vector<std::wstring> & paths, const std::vector<std::wstring> & targets)
{
    CenterDialogsOver       centered (owner);
    HRESULT                 hr        = S_OK;
    ComPtr<IFileOperation>  operation;
    bool                    paired    = paths.size() == targets.size();



    CBRAEx (paired, E_INVALIDARG);

    hr = CreateOperation (owner, &operation);
    CHR (hr);

    for (size_t i = 0; i < paths.size(); i++)
    {
        ComPtr<IShellItem>     item;
        ComPtr<IShellItem>     folder;
        std::filesystem::path  target (targets[i]);

        hr = SHCreateItemFromParsingName (paths[i].c_str(), nullptr, IID_PPV_ARGS (&item));
        CHR (hr);

        hr = SHCreateItemFromParsingName (target.parent_path().c_str(), nullptr, IID_PPV_ARGS (&folder));
        CHR (hr);

        hr = operation->MoveItem (item.Get(), folder.Get(), target.filename().c_str(), nullptr);
        CHR (hr);
    }

    hr = operation->PerformOperations();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::GetItemPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShellItemVerbs::GetItemPath (IShellItem * item)
{
    HRESULT       hr   = S_OK;
    PWSTR         name = nullptr;
    std::wstring  path;



    CBR (item != nullptr);

    hr = item->GetDisplayName (SIGDN_FILESYSPATH, &name);
    CHR (hr);

    path = name;

Error:
    CoTaskMemFree (name);
    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::OperationRecorder::QueryInterface
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShellItemVerbs::OperationRecorder::QueryInterface (REFIID riid, void ** ppv)
{
    HRESULT  hr    = S_OK;
    bool     known = riid == IID_IUnknown || riid == __uuidof (IFileOperationProgressSink);



    CBRAEx (ppv != nullptr, E_POINTER);

    *ppv = nullptr;

    CBREx (known, E_NOINTERFACE);

    *ppv = static_cast<IFileOperationProgressSink *> (this);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::OperationRecorder::PostMoveItem
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShellItemVerbs::OperationRecorder::PostMoveItem (DWORD, IShellItem * item, IShellItem *, LPCWSTR, HRESULT hr, IShellItem * made)
{
    Record (item, hr, made);
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::OperationRecorder::PostCopyItem
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShellItemVerbs::OperationRecorder::PostCopyItem (DWORD, IShellItem * item, IShellItem *, LPCWSTR, HRESULT hr, IShellItem * made)
{
    Record (item, hr, made);
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::OperationRecorder::Record
//
//  Only what was made: an item skipped or failed has nothing to take back.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellItemVerbs::OperationRecorder::Record (IShellItem * item, HRESULT hr, IShellItem * made)
{
    std::wstring  from = GetItemPath (item);
    std::wstring  to   = GetItemPath (made);



    if (SUCCEEDED (hr) && hr != COPYENGINE_S_DONT_PROCESS_CHILDREN && !from.empty() && !to.empty())
    {
        sources.push_back (from);
        created.push_back (to);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::Share
//
//  Invoked by its verb on the items' own menu, so no WinRT is needed here:
//  the shell's handler talks to the share sheet itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::Share (HWND owner, const std::vector<std::wstring> & paths)
{
    HRESULT                 hr     = S_OK;
    ComPtr<IContextMenu>    menu;
    HMENU                   popup  = nullptr;
    CMINVOKECOMMANDINFOEX   invoke = { sizeof (invoke) };



    hr = GetItemsMenu (owner, paths, &menu);
    CHR (hr);

    //  Some handlers add their verbs only once asked to fill a menu.
    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, s_kFirstCommandId, s_kLastCommandId, CMF_NORMAL);
    CHR (hr);

    invoke.fMask   = CMIC_MASK_UNICODE;
    invoke.hwnd    = owner;
    invoke.lpVerb  = s_kShareVerb;
    invoke.lpVerbW = s_kShareVerbW;
    invoke.nShow   = SW_SHOWNORMAL;

    hr = menu->InvokeCommand ((LPCMINVOKECOMMANDINFO) &invoke);
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
//  Win32ShellItemVerbs::CreateFolder
//
//  Through the file operation, so Explorer's undo takes it back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::CreateFolder (HWND owner, const std::wstring & parent, const std::wstring & name)
{
    CenterDialogsOver  centered (owner);
    HRESULT                 hr = S_OK;
    ComPtr<IFileOperation>  operation;
    ComPtr<IShellItem>      folder;



    hr = SHCreateItemFromParsingName (parent.c_str(), nullptr, IID_PPV_ARGS (&folder));
    CHR (hr);

    hr = CreateOperation (owner, &operation);
    CHR (hr);

    hr = operation->NewItem (folder.Get(), FILE_ATTRIBUTE_DIRECTORY, name.c_str(), nullptr, nullptr);
    CHR (hr);

    hr = operation->PerformOperations();
    CHR (hr);

Error:
    return hr;
}
