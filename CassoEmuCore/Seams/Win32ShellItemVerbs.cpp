#include "Pch.h"

#include "Seams/Win32ShellItemVerbs.h"





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
    HRESULT                hr       = S_OK;
    ComPtr<IContextMenu>   menu;
    ComPtr<IContextMenu2>  menu2;
    HMENU                  popup    = nullptr;
    UINT                   picked   = 0;
    CMINVOKECOMMANDINFOEX  invoke   = { sizeof (invoke) };
    HRESULT                hasMenu2 = S_OK;



    hr = CreateMenuHost (owner);
    CHR (hr);

    hr = GetItemsMenu (m_menuHost, paths, &menu);
    CHR (hr);

    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, s_kFirstCommandId, s_kLastCommandId, CMF_NORMAL);
    CHR (hr);

    hasMenu2 = menu.As (&menu2);

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
//  Win32ShellItemVerbs::RenameItem
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::RenameItem (HWND owner, const std::wstring & path, const std::wstring & newName)
{
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
//  Win32ShellItemVerbs::PasteInto
//
//  A cut moves and a copy copies, as whoever put the files there asked.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::PasteInto (HWND owner, const std::wstring & folder)
{
    HRESULT                 hr      = S_OK;
    ComPtr<IDataObject>     data;
    ComPtr<IShellItem>      target;
    ComPtr<IFileOperation>  operation;
    FORMATETC               format  = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_PREFERREDDROPEFFECT), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM               medium  = {};
    HRESULT                 hasMark = S_OK;
    DWORD                   effect  = DROPEFFECT_COPY;



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
        hr = operation->CopyItems (data.Get(), target.Get());
    }

    CHR (hr);

    hr = operation->PerformOperations();
    CHR (hr);

    //  A cut is used up by its paste.
    if ((effect & DROPEFFECT_MOVE) != 0)
    {
        hr = OleSetClipboard (nullptr);
        CHR (hr);
    }

Error:
    return hr;
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
