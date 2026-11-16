#include "Pch.h"

#include "CassoExplorer/RawCopyDropHandler.h"





std::atomic<LONG>  RawCopyDropHandler::s_objects = 0;





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::ClassFactory
//
////////////////////////////////////////////////////////////////////////////////

class RawCopyDropHandler::ClassFactory : public IClassFactory
{
public:
    ClassFactory()          { s_objects++; }
    virtual ~ClassFactory() { s_objects--; }

    STDMETHODIMP QueryInterface (REFIID riid, void ** ppv) override
    {
        HRESULT  hr = S_OK;



        CBREx (ppv != nullptr, E_POINTER);

        *ppv = nullptr;
        CBREx (riid == IID_IUnknown || riid == IID_IClassFactory, E_NOINTERFACE);

        *ppv = static_cast<IClassFactory *> (this);
        AddRef();

    Error:
        return hr;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return ++m_refCount;
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG  count = --m_refCount;



        if (count == 0)
        {
            delete this;
        }

        return count;
    }

    STDMETHODIMP CreateInstance (IUnknown * outer, REFIID riid, void ** ppv) override
    {
        HRESULT  hr = S_OK;



        CBREx (outer == nullptr, CLASS_E_NOAGGREGATION);

        hr = RawCopyDropHandler::Create (riid, ppv);
        CHR (hr);

    Error:
        return hr;
    }

    STDMETHODIMP LockServer (BOOL lock) override
    {
        if (lock)
        {
            s_objects++;
        }
        else
        {
            s_objects--;
        }

        return S_OK;
    }

private:
    std::atomic<ULONG>  m_refCount = 1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::RawCopyDropHandler
//
////////////////////////////////////////////////////////////////////////////////

RawCopyDropHandler::RawCopyDropHandler()
{
    s_objects++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::~RawCopyDropHandler
//
////////////////////////////////////////////////////////////////////////////////

RawCopyDropHandler::~RawCopyDropHandler()
{
    s_objects--;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawCopyDropHandler::Create (REFIID riid, void ** ppv)
{
    HRESULT               hr      = S_OK;
    RawCopyDropHandler  * handler = new (std::nothrow) RawCopyDropHandler();



    CBREx (handler != nullptr, E_OUTOFMEMORY);

    hr = handler->QueryInterface (riid, ppv);
    CHR (hr);

Error:
    if (handler != nullptr)
    {
        handler->Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::CreateClassFactory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawCopyDropHandler::CreateClassFactory (REFIID riid, void ** ppv)
{
    HRESULT          hr      = S_OK;
    ClassFactory   * factory = new (std::nothrow) ClassFactory();



    CBREx (factory != nullptr, E_OUTOFMEMORY);

    hr = factory->QueryInterface (riid, ppv);
    CHR (hr);

Error:
    if (factory != nullptr)
    {
        factory->Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::GetFolder
//
//  The path up to its terminating null, or the whole of the bytes when a
//  caller left the null out.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring RawCopyDropHandler::GetFolder (const std::vector<uint8_t> & bytes)
{
    std::wstring  folder (bytes.size() / sizeof (wchar_t), L'\0');



    if (!folder.empty())
    {
        memcpy (folder.data(), bytes.data(), folder.size() * sizeof (wchar_t));
        folder.resize (wcsnlen (folder.c_str(), folder.size()));
    }

    return folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::QueryInterface
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RawCopyDropHandler::QueryInterface (REFIID riid, void ** ppv)
{
    HRESULT  hr = S_OK;



    CBREx (ppv != nullptr, E_POINTER);

    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == IID_IShellExtInit)
    {
        *ppv = static_cast<IShellExtInit *> (this);
    }
    else if (riid == IID_IContextMenu)
    {
        *ppv = static_cast<IContextMenu *> (this);
    }

    CBREx (*ppv != nullptr, E_NOINTERFACE);
    AddRef();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::AddRef
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) RawCopyDropHandler::AddRef()
{
    return ++m_refCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::Release
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) RawCopyDropHandler::Release()
{
    ULONG  count = --m_refCount;



    if (count == 0)
    {
        delete this;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::Initialize
//
//  The shell may initialize a handler more than once, so each call starts
//  over. After a drag without the mark, or with a target that isn't a file
//  system folder, m_folder is empty, and QueryContextMenu doesn't add the
//  item.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RawCopyDropHandler::Initialize (PCIDLIST_ABSOLUTE folder, IDataObject * data, HKEY progId)
{
    wchar_t  path[MAX_PATH] = {};



    UNREFERENCED_PARAMETER (progId);

    m_data.Reset();
    m_folder.clear();

    if (folder != nullptr && IsOffered (data) && SHGetPathFromIDListW (folder, path))
    {
        m_data   = data;
        m_folder = path;
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::QueryContextMenu
//
//  The return value is the count of command ids used, as a success code.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RawCopyDropHandler::QueryContextMenu (HMENU menu, UINT index, UINT firstCommand, UINT lastCommand, UINT flags)
{
    HRESULT  hr    = S_OK;
    BOOL     added = FALSE;



    BAIL_OUT_IF (m_folder.empty() || (flags & CMF_DEFAULTONLY) != 0 || firstCommand > lastCommand, MAKE_HRESULT (SEVERITY_SUCCESS, FACILITY_NULL, 0));

    added = InsertMenuW (menu, index, MF_BYPOSITION | MF_STRING, firstCommand, kMenuText);
    CWR (added);

    hr = MAKE_HRESULT (SEVERITY_SUCCESS, FACILITY_NULL, 1);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::InvokeCommand
//
//  The folder's path, with its terminating null, in global memory. It is
//  stored on the data object, and Casso Explorer reads it once the drag ends.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RawCopyDropHandler::InvokeCommand (CMINVOKECOMMANDINFO * info)
{
    HRESULT     hr     = S_OK;
    FORMATETC   format = { (CLIPFORMAT) RegisterClipboardFormatW (kFolderFormat), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM   medium = { TYMED_HGLOBAL };
    size_t      bytes  = (m_folder.size() + 1) * sizeof (wchar_t);
    void      * locked = nullptr;
    bool        picked = false;



    CBREx (info != nullptr, E_POINTER);

    picked = IsHandlerCommand (info) && m_data != nullptr && !m_folder.empty();
    CBR (picked);

    medium.hGlobal = GlobalAlloc (GMEM_MOVEABLE, bytes);
    CWR (medium.hGlobal);

    locked = GlobalLock (medium.hGlobal);
    CWR (locked);

    memcpy (locked, m_folder.c_str(), bytes);
    GlobalUnlock (medium.hGlobal);

    hr = m_data->SetData (&format, &medium, TRUE);
    CHR (hr);

    medium.hGlobal = nullptr;

Error:
    if (medium.hGlobal != nullptr)
    {
        GlobalFree (medium.hGlobal);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::GetCommandString
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP RawCopyDropHandler::GetCommandString (UINT_PTR command, UINT type, UINT * reserved, CHAR * name, UINT nameChars)
{
    HRESULT  hr = S_OK;



    UNREFERENCED_PARAMETER (reserved);

    CBREx (command == 0 && name != nullptr && nameChars > 0, E_NOTIMPL);

    switch (type)
    {
        case GCS_VERBW:
            hr = (wcscpy_s ((wchar_t *) name, nameChars, kVerb) == 0) ? S_OK : E_NOT_SUFFICIENT_BUFFER;
            break;

        case GCS_VALIDATEW:
        case GCS_VALIDATEA:
            hr = S_OK;
            break;

        default:
            hr = E_NOTIMPL;
            break;
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::IsOffered
//
////////////////////////////////////////////////////////////////////////////////

bool RawCopyDropHandler::IsOffered (IDataObject * data)
{
    FORMATETC  format = { (CLIPFORMAT) RegisterClipboardFormatW (kOfferedFormat), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };



    return data != nullptr && data->QueryGetData (&format) == S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler::IsHandlerCommand
//
//  A command is picked by its offset from the first id, in the low word of
//  lpVerb with the high word zero, or by its verb, as a string. A caller with
//  CMIC_MASK_UNICODE may pass the verb in the extended structure.
//
////////////////////////////////////////////////////////////////////////////////

bool RawCopyDropHandler::IsHandlerCommand (const CMINVOKECOMMANDINFO * info)
{
    const CMINVOKECOMMANDINFOEX  * extended = (const CMINVOKECOMMANDINFOEX *) info;
    bool                           unicode  = info->cbSize >= sizeof (CMINVOKECOMMANDINFOEX) && (info->fMask & CMIC_MASK_UNICODE) != 0;
    bool                           picked   = false;



    if (unicode && !IS_INTRESOURCE (extended->lpVerbW))
    {
        picked = wcscmp (extended->lpVerbW, kVerb) == 0;
    }
    else if (!IS_INTRESOURCE (info->lpVerb))
    {
        picked = strcmp (info->lpVerb, kVerbA) == 0;
    }
    else
    {
        picked = LOWORD (info->lpVerb) == 0;
    }

    return picked;
}
