#include "Pch.h"
#include "Seams/ShellItemId.h"

#include "Seams/ShellVerbs/Win32ShellMenus.h"
#include "Seams/Win32ExplorerCommandEntries.h"
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

    for (size_t i = 0; i < handlers.size(); i++)
    {
        IAssocHandler             * handler    = handlers[i];
        wchar_t                   * name       = nullptr;
        wchar_t                   * iconFile   = nullptr;
        int                         iconIndex  = 0;
        HRESULT                     named      = handler->GetUIName (&name);
        HRESULT                     iconed     = handler->GetIconLocation (&iconFile, &iconIndex);
        wchar_t                   * program    = nullptr;
        HRESULT                     programmed = handler->GetName (&program);
        IShellItemVerbs::Handler    entry;



        //  Casso Explorer opens what it browses itself, so it is left out.
        if (SUCCEEDED (programmed) && program != nullptr && IsThisProgram (program))
        {
            CoTaskMemFree (program);
            CoTaskMemFree (name);
            CoTaskMemFree (iconFile);
            continue;
        }

        CoTaskMemFree (program);
        program = nullptr;

        entry.name       = (SUCCEEDED (named) && name != nullptr) ? std::wstring (name) : std::wstring (L"?");
        entry.assocIndex = i;

        if (SUCCEEDED (iconed) && iconFile != nullptr)
        {
            entry.iconFile  = iconFile;
            entry.iconIndex = iconIndex;
        }

        //  A packaged app's icon is an indirect string for a picture file.
        if (entry.iconFile.starts_with (L"@"))
        {
            wchar_t  resolved[MAX_PATH] = {};
            HRESULT  loaded             = SHLoadIndirectString (entry.iconFile.c_str(), resolved, (UINT) std::size (resolved), nullptr);

            entry.iconFile  = SUCCEEDED (loaded) ? std::wstring (resolved) : std::wstring();
            entry.iconIndex = 0;
        }

        outHandlers.push_back (std::move (entry));
        CoTaskMemFree (name);
        CoTaskMemFree (iconFile);
    }

    //  The programs recently used for the type follow, and the list is in
    //  name order, as File Explorer has it.
    AddRecentApplications (path, outHandlers);

    std::stable_sort (outHandlers.begin(), outHandlers.end(), [] (const IShellItemVerbs::Handler & a, const IShellItemVerbs::Handler & b)
    {
        return _wcsicmp (a.name.c_str(), b.name.c_str()) < 0;
    });

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

HRESULT Win32ShellMenus::OpenWith (HWND owner, const std::wstring & path, const IShellItemVerbs::Handler & handler)
{
    HRESULT                       hr           = S_OK;
    std::vector<IAssocHandler *>  handlers;
    ComPtr<IShellItem>            item;
    ComPtr<IDataObject>           data;
    size_t                        count        = 0;
    size_t                        handlerIndex = handler.assocIndex;
    std::wstring                  className    = L"Applications\\" + handler.application;
    SHELLEXECUTEINFOW             execute      = { sizeof (execute) };



    //  A recently used program runs by its Applications key.
    if (!handler.application.empty())
    {
        execute.fMask   = SEE_MASK_CLASSNAME;
        execute.hwnd    = owner;
        execute.lpVerb  = L"open";
        execute.lpFile  = path.c_str();
        execute.lpClass = className.c_str();
        execute.nShow   = SW_SHOWNORMAL;

        BOOL  executed = ShellExecuteExW (&execute);

        CWRA (executed);
        BAIL_OUT_IF (true, S_OK);
    }

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





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::ListSendToTargets
//
//  The SendTo folder's visible files, with the display name of each.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::ListSendToTargets (std::vector<IShellItemVerbs::SendToTarget> & outTargets)
{
    HRESULT          hr     = S_OK;
    PWSTR            folder = nullptr;
    std::error_code  error;



    outTargets.clear();

    hr = SHGetKnownFolderPath (FOLDERID_SendTo, KF_FLAG_DEFAULT, nullptr, &folder);
    CHR (hr);

    for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (folder, error))
    {
        SHFILEINFOW  info       = {};
        DWORD        attributes = GetFileAttributesW (entry.path().c_str());

        if (!entry.is_regular_file (error) || attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0)
        {
            continue;
        }

        if (SHGetFileInfoW (entry.path().c_str(), 0, &info, sizeof (info), SHGFI_DISPLAYNAME) == 0)
        {
            continue;
        }

        outTargets.push_back ({ entry.path().wstring(), info.szDisplayName });
    }

    std::sort (outTargets.begin(), outTargets.end(), [] (const IShellItemVerbs::SendToTarget & a, const IShellItemVerbs::SendToTarget & b)
    {
        return _wcsicmp (a.label.c_str(), b.label.c_str()) < 0;
    });

Error:
    CoTaskMemFree (folder);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::SendTo
//
//  Drop the items on the target, as Send to does: a zip folder compresses
//  them, a drive copies them, a shortcut opens them in its program.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::SendTo (const std::vector<std::wstring> & paths, const std::wstring & target)
{
    HRESULT                  hr     = S_OK;
    ComPtr<IShellItem>       item;
    ComPtr<IDropTarget>      drop;
    ComPtr<IShellItemArray>  items;
    ComPtr<IDataObject>      data;
    DWORD                    effect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    POINTL                   point  = {};



    hr = SHCreateItemFromParsingName (target.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->BindToHandler (nullptr, BHID_SFUIObject, IID_PPV_ARGS (&drop));
    CHR (hr);

    hr = Win32ExplorerCommandEntries::MakeItems (paths, &items);
    CHR (hr);

    hr = items->BindToHandler (nullptr, BHID_DataObject, IID_PPV_ARGS (&data));
    CHR (hr);

    hr = drop->DragEnter (data.Get(), MK_LBUTTON, point, &effect);
    CHR (hr);

    if (effect == DROPEFFECT_NONE)
    {
        hr = drop->DragLeave();
        CHR (hr);

        BAIL_OUT_IF (true, S_FALSE);   // EHM-ALLOW-SFALSE: the target takes no drop of these items
    }

    hr = drop->Drop (data.Get(), MK_LBUTTON, point, &effect);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::ListFileActions
//
//  Each verb that applies to every item, with the shell menu's label for it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellMenus::ListFileActions (const std::vector<std::wstring> & paths, const std::vector<std::string> & verbs, std::vector<IShellItemVerbs::ItemVerb> & outFound)
{
    static const std::map<std::string, std::wstring>  s_kLabels =
    {
        { "print",     L"&Print"           },
        { "link",      L"Create &shortcut" },
        { "rotate90",  L"Rotate righ&t"    },
        { "rotate270", L"Rotate &left"     },
    };



    outFound.clear();

    if (paths.empty())
    {
        return S_OK;
    }

    for (const std::string & verb : verbs)
    {
        auto  label = s_kLabels.find (verb);

        if (label == s_kLabels.end())
        {
            continue;
        }

        if (std::all_of (paths.begin(), paths.end(), [&verb] (const std::wstring & path) { return AppliesToFile (path, verb); }))
        {
            outFound.push_back ({ verb, label->second });
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::AppliesToFile
//
//  Create shortcut applies to any item, Print to a type registered with a
//  print verb, and Rotate to an image.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellMenus::AppliesToFile (const std::wstring & path, const std::string & verb)
{
    HRESULT                     hr             = S_OK;
    ComPtr<IShellItem>          item;
    ComPtr<IQueryAssociations>  assoc;
    wchar_t                     text[MAX_PATH] = {};
    DWORD                       length         = 0;
    std::wstring                extension      = std::filesystem::path (path).extension().wstring();
    PERCEIVED                   perceived      = PERCEIVED_TYPE_UNSPECIFIED;
    PERCEIVEDFLAG               flags          = 0;



    if (verb == "link")
    {
        return true;
    }

    if (verb == "rotate90" || verb == "rotate270")
    {
        HRESULT  typed = extension.empty() ? E_FAIL : AssocGetPerceivedType (extension.c_str(), &perceived, &flags, nullptr);

        return SUCCEEDED (typed) && perceived == PERCEIVED_TYPE_IMAGE;
    }

    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->BindToHandler (nullptr, BHID_SFUIObject, IID_PPV_ARGS (&assoc));
    CHR (hr);

    //  A print verb runs a command, a delegate or a drop target.
    for (ASSOCSTR kind : { ASSOCSTR_COMMAND, ASSOCSTR_DELEGATEEXECUTE, ASSOCSTR_DROPTARGET })
    {
        length = (DWORD) std::size (text);
        hr     = assoc->GetString (ASSOCF_NONE, kind, L"print", text, &length);

        if (SUCCEEDED (hr))
        {
            return true;
        }
    }

    hr = E_FAIL;

Error:
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::AddRecentApplications
//
//  The programs in the type's recently used list that the handler list
//  doesn't have, as File Explorer adds them: each registered under
//  Applications with an open command, and not marked NoOpenWith.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellMenus::AddRecentApplications (const std::wstring & path, std::vector<IShellItemVerbs::Handler> & handlers)
{
    std::wstring  extension = std::filesystem::path (path).extension().wstring();
    std::wstring  listKey   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + extension + L"\\OpenWithList";
    wchar_t       order[64] = {};
    DWORD         size      = sizeof (order);



    if (extension.empty() || RegGetValueW (HKEY_CURRENT_USER, listKey.c_str(), L"MRUList", RRF_RT_REG_SZ, nullptr, order, &size) != ERROR_SUCCESS)
    {
        return;
    }

    for (const wchar_t * letter = order; *letter != L'\0'; letter++)
    {
        wchar_t                   exe[MAX_PATH]     = {};
        wchar_t                   friendly[256]     = {};
        wchar_t                   program[MAX_PATH] = {};
        wchar_t                   value[2]          = { *letter, L'\0' };
        DWORD                     length            = sizeof (exe);
        std::wstring              appKey;
        HKEY                      command           = nullptr;
        HRESULT                   queried           = S_OK;
        IShellItemVerbs::Handler  entry;

        if (RegGetValueW (HKEY_CURRENT_USER, listKey.c_str(), value, RRF_RT_REG_SZ, nullptr, exe, &length) != ERROR_SUCCESS || wcschr (exe, L'!') != nullptr || IsThisProgram (exe))
        {
            continue;
        }

        appKey = std::wstring (L"Applications\\") + exe;

        if (RegGetValueW (HKEY_CLASSES_ROOT, appKey.c_str(), L"NoOpenWith", RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
        {
            continue;
        }

        if (RegOpenKeyExW (HKEY_CLASSES_ROOT, (appKey + L"\\shell\\open\\command").c_str(), 0, KEY_READ, &command) != ERROR_SUCCESS)
        {
            continue;
        }

        RegCloseKey (command);

        length  = (DWORD) std::size (friendly);
        queried = AssocQueryStringW (ASSOCF_OPEN_BYEXENAME, ASSOCSTR_FRIENDLYAPPNAME, exe, nullptr, friendly, &length);

        if (FAILED (queried))
        {
            continue;
        }

        length  = (DWORD) std::size (program);
        queried = AssocQueryStringW (ASSOCF_OPEN_BYEXENAME, ASSOCSTR_EXECUTABLE, exe, nullptr, program, &length);

        if (FAILED (queried))
        {
            program[0] = L'\0';
        }

        if (std::any_of (handlers.begin(), handlers.end(), [&] (const IShellItemVerbs::Handler & each)
        {
            return _wcsicmp (each.name.c_str(), friendly) == 0 || _wcsicmp (std::filesystem::path (each.iconFile).filename().c_str(), exe) == 0;
        }))
        {
            continue;
        }

        entry.name        = friendly;
        entry.iconFile    = program;
        entry.application = exe;

        handlers.push_back (std::move (entry));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellMenus::IsThisProgram
//
//  Whether a program, by its path or file name, is the one running.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellMenus::IsThisProgram (const std::wstring & program)
{
    wchar_t  self[MAX_PATH] = {};



    if (GetModuleFileNameW (nullptr, self, (DWORD) std::size (self)) == 0)
    {
        return false;
    }

    return _wcsicmp (std::filesystem::path (program).filename().c_str(), std::filesystem::path (self).filename().c_str()) == 0;
}
