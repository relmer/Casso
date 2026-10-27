#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShellTransfers.h"
#include "Seams/Win32ShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::Win32ShellTransfers
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellTransfers::Win32ShellTransfers (Win32ShellItemVerbs & verbs)
    : m_verbs (verbs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::DragItems
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::DragItems (const std::vector<std::wstring> & ids, DWORD allowedEffects, const DxuiIconImage * dragImage)
{
    HRESULT                  hr      = S_OK;
    HRESULT                  hrImage = S_OK;
    ComPtr<IShellItemArray>  items;
    ComPtr<IDataObject>      data;
    DWORD                    effect  = DROPEFFECT_NONE;



    hr = m_verbs.m_recycleBin.GetItemArray (ids, &items);
    CHR (hr);

    hr = items->BindToHandler (nullptr, BHID_DataObject, IID_PPV_ARGS (&data));
    CHR (hr);

    //  An image set on the object first is drawn for the drag; without one,
    //  SHDoDragDrop builds the shell's.
    if (dragImage != nullptr)
    {
        hrImage = DxuiDragDropSource::SetDragImage (data.Get(), *dragImage);
        IGNORE_RETURN_VALUE (hrImage, S_OK);
    }

    hr = SHDoDragDrop (nullptr, data.Get(), nullptr, allowedEffects, &effect);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::ReadItemBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::ReadItemBytes (const std::wstring & id, size_t maxBytes, std::vector<Byte> & outBytes)
{
    HRESULT             hr     = S_OK;
    ComPtr<IShellItem>  item;
    ComPtr<IStream>     stream;
    ULONG               read   = 0;
    size_t              chunk  = 0;



    outBytes.clear();

    hr = Win32ShellNamespace::CreateItem (id, &item);
    CHR (hr);

    hr = item->BindToHandler (nullptr, BHID_Stream, IID_PPV_ARGS (&stream));
    CHR (hr);

    while (outBytes.size() < maxBytes)
    {
        chunk = (std::min) (maxBytes - outBytes.size(), (size_t) 65536);
        outBytes.resize (outBytes.size() + chunk);

        hr = stream->Read (outBytes.data() + outBytes.size() - chunk, (ULONG) chunk, &read);
        outBytes.resize (outBytes.size() - chunk + read);
        CHR (hr);

        if (read == 0)
        {
            break;
        }
    }

    hr = S_OK;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::RenameItem
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::RenameItem (HWND owner, const std::wstring & path, const std::wstring & newName)
{
    Win32ShellVerbsTypes::CenterDialogsOver  centered (owner);
    HRESULT                 hr = S_OK;
    ComPtr<IFileOperation>  operation;
    ComPtr<IShellItem>      item;



    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = m_verbs.m_recycleBin.CreateOperation (owner, &operation);
    CHR (hr);

    hr = operation->RenameItem (item.Get(), newName.c_str(), nullptr);
    CHR (hr);

    hr = Win32RecycleBin::Perform (operation.Get());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::PlaceOnClipboard
//
//  The shell's own Cut or Copy verb on the items' menu, as File Explorer runs
//  it, so the shell marks the cut its own way and a paste in Explorer or
//  here moves or copies, from inside an archive too.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::PlaceOnClipboard (HWND owner, const std::vector<std::wstring> & paths, bool cut)
{
    HRESULT                 hr     = S_OK;
    ComPtr<IContextMenu>    menu;
    HMENU                   popup  = nullptr;
    CMINVOKECOMMANDINFOEX   invoke = { sizeof (invoke) };



    hr = m_verbs.m_menus.GetItemsMenu (owner, paths, &menu);
    CHR (hr);

    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, Win32ShellItemVerbs::s_kFirstCommandId, Win32ShellItemVerbs::s_kLastCommandId, CMF_NORMAL);
    CHR (hr);

    invoke.fMask   = CMIC_MASK_UNICODE;
    invoke.hwnd    = owner;
    invoke.lpVerb  = cut ? "cut" : "copy";
    invoke.lpVerbW = cut ? L"cut" : L"copy";
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
//  Win32ShellTransfers::ClipboardHasFiles
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellTransfers::ClipboardHasFiles() const
{
    //  Files copied from inside a zip come as the shell's item list alone,
    //  with no file list; the shell's paste takes either.
    return IsClipboardFormatAvailable (CF_HDROP) != FALSE
        || IsClipboardFormatAvailable (RegisterClipboardFormatW (CFSTR_SHELLIDLIST)) != FALSE
        || IsClipboardFormatAvailable (RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW)) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::HasOnlyVirtualFiles
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellTransfers::HasOnlyVirtualFiles (IDataObject * data)
{
    FORMATETC  drop        = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    FORMATETC  items       = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_SHELLIDLIST), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    FORMATETC  descriptor  = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };



    return data->QueryGetData (&drop) != S_OK && data->QueryGetData (&items) != S_OK && data->QueryGetData (&descriptor) == S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::DropVirtualFiles
//
//  The names made are the descriptor's, in the folder, for the paste's
//  undo; one the folder had already is renamed by the drop target, and its
//  new name is not known here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::DropVirtualFiles (IDataObject * data, IShellItem * target, const std::wstring & folder, IShellItemVerbs::PasteResult & outResult)
{
    HRESULT                      hr      = S_OK;
    ComPtr<IDropTarget>          drop;
    FORMATETC                    format  = { (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM                    medium  = {};
    FILEGROUPDESCRIPTORW       * group   = nullptr;
    std::vector<bool>            existed;
    std::vector<std::wstring>    names;
    POINTL                       point   = {};
    DWORD                        effect  = DROPEFFECT_COPY;
    std::error_code              ignore;



    hr = data->GetData (&format, &medium);
    CHR (hr);

    group = (FILEGROUPDESCRIPTORW *) GlobalLock (medium.hGlobal);

    for (UINT i = 0; group != nullptr && i < group->cItems; i++)
    {
        names.push_back ((std::filesystem::path (folder) / group->fgd[i].cFileName).wstring());
        existed.push_back (std::filesystem::exists (names.back(), ignore));
    }

    GlobalUnlock (medium.hGlobal);
    ReleaseStgMedium (&medium);

    hr = target->BindToHandler (nullptr, BHID_SFUIObject, IID_PPV_ARGS (&drop));
    CHR (hr);

    hr = drop->DragEnter (data, MK_LBUTTON, point, &effect);
    CHR (hr);

    effect = DROPEFFECT_COPY;

    hr = drop->Drop (data, MK_LBUTTON, point, &effect);
    CHR (hr);

    for (size_t i = 0; i < names.size(); i++)
    {
        if (!existed[i] && std::filesystem::exists (names[i], ignore))
        {
            outResult.created.push_back (names[i]);
            outResult.sources.push_back (std::wstring());
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::IsFromFolder
//
//  Whether any file the object holds is in the folder itself.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ShellTransfers::IsFromFolder (IDataObject * data, const std::wstring & folder)
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
//  Win32ShellTransfers::PasteInto
//
//  A cut moves and a copy copies, as whoever put the files there asked.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::PasteInto (HWND owner, const std::wstring & folder, IShellItemVerbs::PasteResult & outResult)
{
    Win32ShellVerbsTypes::CenterDialogsOver  centered (owner);
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



    outResult = IShellItemVerbs::PasteResult();

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

    if (HasOnlyVirtualFiles (data.Get()))
    {
        hr = DropVirtualFiles (data.Get(), target.Get(), folder, outResult);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_verbs.m_recycleBin.CreateOperation (owner, &operation);
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

    hr        = Win32RecycleBin::Perform (operation.Get());
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
//  Win32ShellTransfers::MoveItemsTo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::MoveItemsTo (HWND owner, const std::vector<std::wstring> & paths, const std::vector<std::wstring> & targets)
{
    Win32ShellVerbsTypes::CenterDialogsOver       centered (owner);
    HRESULT                 hr        = S_OK;
    ComPtr<IFileOperation>  operation;
    bool                    paired    = paths.size() == targets.size();



    CBRAEx (paired, E_INVALIDARG);

    hr = m_verbs.m_recycleBin.CreateOperation (owner, &operation);
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

    hr = Win32RecycleBin::Perform (operation.Get());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::GetItemPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShellTransfers::GetItemPath (IShellItem * item)
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
//  Win32ShellTransfers::OperationRecorder::QueryInterface
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShellTransfers::OperationRecorder::QueryInterface (REFIID riid, void ** ppv)
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
//  Win32ShellTransfers::OperationRecorder::PostMoveItem
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShellTransfers::OperationRecorder::PostMoveItem (DWORD, IShellItem * item, IShellItem *, LPCWSTR, HRESULT hr, IShellItem * made)
{
    Record (item, hr, made);
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::OperationRecorder::PostCopyItem
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShellTransfers::OperationRecorder::PostCopyItem (DWORD, IShellItem * item, IShellItem *, LPCWSTR, HRESULT hr, IShellItem * made)
{
    Record (item, hr, made);
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellTransfers::OperationRecorder::Record
//
//  Only what was made: an item skipped or failed has nothing to take back.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShellTransfers::OperationRecorder::Record (IShellItem * item, HRESULT hr, IShellItem * made)
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
//  Win32ShellTransfers::Share
//
//  Invoked by its verb on the items' own menu, so no WinRT is needed here:
//  the shell's handler talks to the share sheet itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::Share (HWND owner, const std::vector<std::wstring> & paths)
{
    HRESULT                 hr     = S_OK;
    ComPtr<IContextMenu>    menu;
    HMENU                   popup  = nullptr;
    CMINVOKECOMMANDINFOEX   invoke = { sizeof (invoke) };



    hr = m_verbs.m_menus.GetItemsMenu (owner, paths, &menu);
    CHR (hr);

    //  Some handlers add their verbs only once asked to fill a menu.
    popup = CreatePopupMenu();
    CWR (popup != nullptr);

    hr = menu->QueryContextMenu (popup, 0, Win32ShellItemVerbs::s_kFirstCommandId, Win32ShellItemVerbs::s_kLastCommandId, CMF_NORMAL);
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
//  Win32ShellTransfers::CreateFolder
//
//  Through the file operation, so Explorer's undo takes it back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellTransfers::CreateFolder (HWND owner, const std::wstring & parent, const std::wstring & name)
{
    Win32ShellVerbsTypes::CenterDialogsOver  centered (owner);
    HRESULT                 hr = S_OK;
    ComPtr<IFileOperation>  operation;
    ComPtr<IShellItem>      folder;



    hr = SHCreateItemFromParsingName (parent.c_str(), nullptr, IID_PPV_ARGS (&folder));
    CHR (hr);

    hr = m_verbs.m_recycleBin.CreateOperation (owner, &operation);
    CHR (hr);

    hr = operation->NewItem (folder.Get(), FILE_ATTRIBUTE_DIRECTORY, name.c_str(), nullptr, nullptr);
    CHR (hr);

    hr = Win32RecycleBin::Perform (operation.Get());
    CHR (hr);

Error:
    return hr;
}
