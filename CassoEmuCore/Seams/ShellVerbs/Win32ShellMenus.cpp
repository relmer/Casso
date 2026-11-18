#include "Pch.h"
#include "Seams/ShellItemId.h"

#include "Seams/ShellVerbs/Win32ShellMenus.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::Win32ShellMenus
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellMenus::Win32ShellMenus (Win32ShellItemVerbs & verbs)
    : m_verbs (verbs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::Open (HWND owner, const std::wstring & path)
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
//  Win32ShellMenus::EnumHandlers
//
//  The caller releases every handler returned.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::EnumHandlers (const std::wstring & path, std::vector<IAssocHandler *> & outHandlers)
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
//  Win32ShellMenus::GetOpenWithHandlers
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::GetOpenWithHandlers (const std::wstring & path, std::vector<IShellItemVerbs::Handler> & outHandlers)
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
            outHandlers.push_back (IShellItemVerbs::Handler { name });
            CoTaskMemFree (name);
        }
        else
        {
            outHandlers.push_back (IShellItemVerbs::Handler { L"?" });
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
//  Win32ShellMenus::OpenWith
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::OpenWith (HWND owner, const std::wstring & path, size_t handlerIndex)
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
//  Win32ShellMenus::ChooseOtherApp
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::ChooseOtherApp (HWND owner, const std::wstring & path)
{
    OPENASINFO  info = {};



    info.pcszFile  = path.c_str();
    info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_EXEC;

    return SHOpenWithDialog (owner, &info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::GetItemsMenu
//
//  The menu Explorer builds for these items: from the folder they are in, for
//  its children, or from the desktop for their full ids when they are in
//  different folders, as a search's results can be.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::GetItemsMenu (HWND owner, const std::vector<std::wstring> & paths, IContextMenu ** outMenu)
{
    HRESULT                        hr       = S_OK;
    std::vector<PIDLIST_ABSOLUTE>  full;
    std::vector<PCUITEMID_CHILD>   children;
    ComPtr<IShellFolder>           parent;
    PIDLIST_ABSOLUTE               folder   = nullptr;
    bool                           shared   = true;
    bool                           hasItems = !paths.empty();



    *outMenu = nullptr;

    CBREx (hasItems, E_INVALIDARG);

    for (const std::wstring & path : paths)
    {
        PIDLIST_ABSOLUTE  pidl = nullptr;

        hr = ShellItemId::ToPidl (path, &pidl);
        CHR (hr);

        full.push_back (pidl);
    }

    folder = ILClone (full[0]);
    CBREx (folder != nullptr, E_OUTOFMEMORY);
    ILRemoveLastID (folder);

    for (size_t i = 1; i < full.size(); i++)
    {
        shared = shared && ILIsParent (folder, full[i], TRUE);
    }

    if (shared)
    {
        hr = SHBindToParent (full[0], IID_PPV_ARGS (&parent), nullptr);
        CHR (hr);
    }
    else
    {
        hr = SHGetDesktopFolder (&parent);
        CHR (hr);
    }

    for (PIDLIST_ABSOLUTE pidl : full)
    {
        children.push_back (shared ? ILFindLastID (pidl) : (PCUITEMID_CHILD) pidl);
    }

    hr = parent->GetUIObjectOf (owner, (UINT) children.size(), children.data(), IID_IContextMenu, nullptr, (void **) outMenu);
    CHR (hr);

Error:
    ILFree (folder);

    for (PIDLIST_ABSOLUTE pidl : full)
    {
        CoTaskMemFree (pidl);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::CreateMenuHost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::CreateMenuHost (HWND owner)
{
    HRESULT      hr         = S_OK;
    WNDCLASSEXW  wc         = { sizeof (wc) };
    HINSTANCE    inst       = GetModuleHandleW (nullptr);
    ATOM         registered = 0;



    BAIL_OUT_IF (m_verbs.m_menuHost != nullptr, S_OK);

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
    m_verbs.m_menuHost = CreateWindowExW (0, s_kClassName, L"", WS_POPUP, 0, 0, 0, 0, owner, nullptr, inst, &m_verbs);
    CWR (m_verbs.m_menuHost != nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::MenuHostProc
//
//  Hands the menu's own messages back to it while it is up.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Win32ShellMenus::MenuHostProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Win32ShellItemVerbs  * self   = nullptr;
    LRESULT                result = 0;



    if (message == WM_NCCREATE)
    {
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, (LONG_PTR) ((CREATESTRUCTW *) lParam)->lpCreateParams);
    }

    self = (Win32ShellItemVerbs *) GetWindowLongPtrW (hwnd, GWLP_USERDATA);

    if (self != nullptr && self->m_menus.m_activeMenu != nullptr)
    {
        switch (message)
        {
            case WM_INITMENUPOPUP:
            case WM_DRAWITEM:
            case WM_MEASUREITEM:
            case WM_MENUCHAR:
            {
                ComPtr<IContextMenu3>  menu3;
                HRESULT                hasMenu3 = self->m_menus.m_activeMenu->QueryInterface (IID_PPV_ARGS (&menu3));
                HRESULT                handled  = E_NOTIMPL;

                if (SUCCEEDED (hasMenu3))
                {
                    handled = menu3->HandleMenuMsg2 (message, wParam, lParam, &result);

                    if (SUCCEEDED (handled))
                    {
                        return result;
                    }
                }

                handled = self->m_menus.m_activeMenu->HandleMenuMsg (message, wParam, lParam);

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
//  Win32ShellMenus::ShowShellMenu
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::ShowShellMenu (HWND owner, const std::vector<std::wstring> & paths, POINT screenPx, const std::function<void()> & rename)
{
    HRESULT               hr = S_OK;
    ComPtr<IContextMenu>  menu;



    hr = CreateMenuHost (owner);
    CHR (hr);

    hr = GetItemsMenu (m_verbs.m_menuHost, paths, &menu);
    CHR (hr);

    hr = TrackMenu (owner, menu.Get(), screenPx, rename);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::ShowBackgroundMenu
//
//  The folder's own view object is its background menu. View, Sort by and
//  the like are commands of File Explorer's view rather than of the folder,
//  so they are not part of it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::ShowBackgroundMenu (HWND owner, const std::wstring & folder, POINT screenPx)
{
    HRESULT               hr = S_OK;
    ComPtr<IShellItem>    item;
    ComPtr<IShellFolder>  shellFolder;
    ComPtr<IContextMenu>  menu;



    hr = CreateMenuHost (owner);
    CHR (hr);

    hr = SHCreateItemFromParsingName (folder.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->BindToHandler (nullptr, BHID_SFObject, IID_PPV_ARGS (&shellFolder));
    CHR (hr);

    hr = shellFolder->CreateViewObject (m_verbs.m_menuHost, IID_PPV_ARGS (&menu));
    CHR (hr);

    hr = TrackMenu (owner, menu.Get(), screenPx, {});
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::ListItemVerbs
//
//  The menu is built as for showing it, and each command's canonical verb
//  read back, since a handler adds its commands only to the built menu.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::ListItemVerbs (const std::vector<std::wstring> & paths, const std::vector<std::string> & verbs, std::vector<IShellItemVerbs::ItemVerb> & outFound)
{
    HRESULT                                  hr     = S_OK;
    ComPtr<IContextMenu>                     menu;
    HMENU                                    popup  = nullptr;
    std::vector<IShellItemVerbs::ItemVerb>   found;



    outFound.clear();

    hr = CreateMenuHost (nullptr);
    CHR (hr);

    hr = GetItemsMenu (m_verbs.m_menuHost, paths, &menu);
    CHR (hr);

    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, Win32ShellItemVerbs::s_kFirstCommandId, Win32ShellItemVerbs::s_kLastCommandId, CMF_NORMAL);
    CHR (hr);

    //  QueryContextMenu's success is the number of commands it added.
    hr = S_OK;

    for (int i = 0; i < GetMenuItemCount (popup); i++)
    {
        wchar_t        label[256] = {};
        char           verb[256]  = {};
        MENUITEMINFOW  info       = { sizeof (info) };
        HRESULT        named      = E_FAIL;

        info.fMask      = MIIM_ID | MIIM_STRING;
        info.dwTypeData = label;
        info.cch        = (UINT) std::size (label) - 1;

        if (!GetMenuItemInfoW (popup, (UINT) i, TRUE, &info) || info.wID < Win32ShellItemVerbs::s_kFirstCommandId || info.wID > Win32ShellItemVerbs::s_kLastCommandId)
        {
            continue;
        }

        named = menu->GetCommandString (info.wID - Win32ShellItemVerbs::s_kFirstCommandId, GCS_VERBA, nullptr, verb, (UINT) std::size (verb) - 1);

        if (SUCCEEDED (named) && verb[0] != '\0')
        {
            found.push_back ({ verb, label });
        }
    }

    for (const std::string & wanted : verbs)
    {
        for (const IShellItemVerbs::ItemVerb & each : found)
        {
            if (_stricmp (each.verb.c_str(), wanted.c_str()) == 0)
            {
                outFound.push_back (each);
                break;
            }
        }
    }

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::GetItemVerbLabels
//
//  The labels of the items' menu, without access keys. The labels are the only
//  name some cloud commands have.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::GetItemVerbLabels (HWND owner, const std::vector<std::wstring> & paths, std::vector<std::wstring> & outLabels)
{
    return WalkItemVerbs (owner, paths, nullptr, &outLabels);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::RunItemVerbByLabel
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::RunItemVerbByLabel (HWND owner, const std::vector<std::wstring> & paths, const std::wstring & label)
{
    return WalkItemVerbs (owner, paths, &label, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::WalkItemVerbs
//
//  Builds the items' menu, then lists its labels or invokes the one matching.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::WalkItemVerbs (HWND owner, const std::vector<std::wstring> & paths, const std::wstring * run, std::vector<std::wstring> * outLabels)
{
    Win32ShellVerbsTypes::CenterDialogsOver  centered (owner);
    HRESULT                hr     = S_OK;
    ComPtr<IContextMenu>   menu;
    HMENU                  popup  = nullptr;
    int                    count  = 0;
    bool                   found  = false;



    hr = CreateMenuHost (owner);
    CHR (hr);

    hr = GetItemsMenu (m_verbs.m_menuHost, paths, &menu);
    CHR (hr);

    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, Win32ShellItemVerbs::s_kFirstCommandId, Win32ShellItemVerbs::s_kLastCommandId, CMF_NORMAL);
    CHR (hr);

    count = GetMenuItemCount (popup);

    for (int at = 0; at < count && !found; at++)
    {
        wchar_t        text[260] = {};
        MENUITEMINFOW  info      = { sizeof (info) };
        BOOL           read      = FALSE;
        std::wstring   label;

        info.fMask      = MIIM_STRING | MIIM_ID;
        info.dwTypeData = text;
        info.cch        = (UINT) std::size (text);
        read            = GetMenuItemInfoW (popup, (UINT) at, TRUE, &info);

        if (!read || text[0] == L'\0')
        {
            continue;
        }

        label = text;
        std::erase (label, L'&');

        if (outLabels != nullptr)
        {
            outLabels->push_back (label);
        }

        if (run != nullptr && label == *run)
        {
            CMINVOKECOMMANDINFOEX  invoke = { sizeof (invoke) };

            //  The shell can report an error for a removal that it did make, and the
            //  caller reads the list again after a failure.
            invoke.fMask  = CMIC_MASK_UNICODE | ((label == L"Remove from list") ? CMIC_MASK_FLAG_NO_UI : 0);
            invoke.hwnd   = owner;
            invoke.lpVerb  = MAKEINTRESOURCEA (info.wID - Win32ShellItemVerbs::s_kFirstCommandId);
            invoke.lpVerbW = MAKEINTRESOURCEW (info.wID - Win32ShellItemVerbs::s_kFirstCommandId);
            invoke.nShow   = SW_SHOWNORMAL;
            found          = true;

            hr = menu->InvokeCommand ((LPCMINVOKECOMMANDINFO) &invoke);
            CHR (hr);
        }
    }

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::RunItemVerb
//
//  The menu is built, as for showing it, so the handlers that own the verb
//  are loaded, and the verb is then invoked by name rather than by position,
//  from the items' folder, which an archive's Extract All offers as where
//  the files go.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::RunItemVerb (HWND owner, const std::vector<std::wstring> & paths, const char * verb)
{
    Win32ShellVerbsTypes::CenterDialogsOver      centered (owner);
    HRESULT                hr       = S_OK;
    ComPtr<IContextMenu>   menu;
    HMENU                  popup    = nullptr;
    CMINVOKECOMMANDINFOEX  invoke   = { sizeof (invoke) };
    std::wstring           wideVerb (verb, verb + strlen (verb));
    std::wstring           folder   = paths.empty() || ShellItemId::IsEncoded (paths[0]) ? std::wstring() : std::filesystem::path (paths[0]).parent_path().wstring();
    std::string            narrow   = TextEncoding::WideToNarrow (folder);



    hr = CreateMenuHost (owner);
    CHR (hr);

    hr = GetItemsMenu (m_verbs.m_menuHost, paths, &menu);
    CHR (hr);

    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, Win32ShellItemVerbs::s_kFirstCommandId, Win32ShellItemVerbs::s_kLastCommandId, CMF_NORMAL);
    CHR (hr);

    invoke.fMask        = CMIC_MASK_UNICODE;
    invoke.hwnd         = owner;
    invoke.lpVerb       = verb;
    invoke.lpVerbW      = wideVerb.c_str();
    invoke.lpDirectory  = narrow.c_str();
    invoke.lpDirectoryW = folder.c_str();
    invoke.nShow        = SW_SHOWNORMAL;

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
//  Win32ShellMenus::RemoveExtraSeparators
//
//  The shell's handlers each add their own separators, so two can meet, or
//  one can open or close the menu; Explorer shows none of those.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellMenus::RemoveExtraSeparators (HMENU menu)
{
    int   count         = GetMenuItemCount (menu);
    bool  lastSeparator = true;
    int   index         = 0;



    for (index = 0; index < count; )
    {
        MENUITEMINFOW  item = { sizeof (item) };

        item.fMask = MIIM_FTYPE;

        if (!GetMenuItemInfoW (menu, (UINT) index, TRUE, &item))
        {
            index++;
            continue;
        }

        if ((item.fType & MFT_SEPARATOR) != 0 && lastSeparator)
        {
            DeleteMenu (menu, (UINT) index, MF_BYPOSITION);
            count--;
            continue;
        }

        lastSeparator = (item.fType & MFT_SEPARATOR) != 0;
        index++;
    }

    if (count > 0 && lastSeparator)
    {
        DeleteMenu (menu, (UINT) (count - 1), MF_BYPOSITION);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::TrackMenu
//
//  Track a shell menu at a point from the menu host, and invoke the command
//  picked. Rename is in the menu only for a caller that renames in its
//  own view, as File Explorer's menu has it; the shell's own Rename does
//  nothing outside a shell view.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::TrackMenu (HWND owner, IContextMenu * menu, POINT screenPx, const std::function<void()> & rename)
{
    Win32ShellVerbsTypes::CenterDialogsOver  centered (owner);
    HRESULT                hr        = S_OK;
    ComPtr<IContextMenu2>  menu2;
    HMENU                  popup     = nullptr;
    UINT                   picked    = 0;
    CMINVOKECOMMANDINFOEX  invoke    = { sizeof (invoke) };
    HRESULT                hasMenu2  = S_OK;
    HRESULT                hasVerb   = S_OK;
    bool                   isRename  = false;
    UINT                   flags     = rename ? (CMF_NORMAL | CMF_CANRENAME) : CMF_NORMAL;
    char                   verb[64]  = {};



    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, Win32ShellItemVerbs::s_kFirstCommandId, Win32ShellItemVerbs::s_kLastCommandId, flags);
    CHR (hr);

    RemoveExtraSeparators (popup);

    hasMenu2 = menu->QueryInterface (IID_PPV_ARGS (&menu2));

    if (SUCCEEDED (hasMenu2))
    {
        m_activeMenu = menu2.Get();
    }

    picked = (UINT) TrackPopupMenuEx (popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPx.x, screenPx.y, m_verbs.m_menuHost, nullptr);

    m_activeMenu = nullptr;

    BAIL_OUT_IF (picked < Win32ShellItemVerbs::s_kFirstCommandId, S_OK);

    hasVerb  = menu->GetCommandString (picked - Win32ShellItemVerbs::s_kFirstCommandId, GCS_VERBA, nullptr, verb, (UINT) std::size (verb));
    isRename = rename && SUCCEEDED (hasVerb) && _stricmp (verb, "rename") == 0;

    if (isRename)
    {
        rename();
    }

    BAIL_OUT_IF (isRename, S_OK);

    invoke.fMask    = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
    invoke.hwnd     = owner;
    invoke.lpVerb   = MAKEINTRESOURCEA (picked - Win32ShellItemVerbs::s_kFirstCommandId);
    invoke.lpVerbW  = MAKEINTRESOURCEW (picked - Win32ShellItemVerbs::s_kFirstCommandId);
    invoke.nShow    = SW_SHOWNORMAL;
    invoke.ptInvoke = screenPx;

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
