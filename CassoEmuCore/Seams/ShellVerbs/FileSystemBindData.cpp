#include "Pch.h"

#include "Seams/ShellVerbs/FileSystemBindData.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindData::FileSystemBindData
//
////////////////////////////////////////////////////////////////////////////////

FileSystemBindData::FileSystemBindData (const WIN32_FIND_DATAW & found)
    : m_found (found)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindData::CreateBindContext
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileSystemBindData::CreateBindContext (const WIN32_FIND_DATAW & found, IBindCtx ** outContext)
{
    HRESULT                       hr      = S_OK;
    ComPtr<IBindCtx>              context;
    ComPtr<FileSystemBindData>    data;
    FileSystemBindData          * created = nullptr;



    CBRAEx (outContext != nullptr, E_INVALIDARG);

    *outContext = nullptr;

    hr = CreateBindCtx (0, &context);
    CHRA (hr);

    created = new (std::nothrow) FileSystemBindData (found);
    CPRA (created);

    data.Attach (created);

    hr = context->RegisterObjectParam (const_cast<LPOLESTR> (STR_FILE_SYS_BIND_DATA), data.Get());
    CHRA (hr);

    *outContext = context.Detach();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindData::QueryInterface
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE FileSystemBindData::QueryInterface (REFIID riid, void ** ppvObject)
{
    HRESULT  hr      = S_OK;
    bool     isKnown = false;



    CBREx (ppvObject != nullptr, E_POINTER);

    *ppvObject = nullptr;

    isKnown = (riid == __uuidof (IUnknown) || riid == __uuidof (IFileSystemBindData));
    CBREx (isKnown, E_NOINTERFACE);

    *ppvObject = static_cast<IFileSystemBindData *> (this);

    AddRef();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindData::AddRef
//
////////////////////////////////////////////////////////////////////////////////

ULONG STDMETHODCALLTYPE FileSystemBindData::AddRef()
{
    return m_refCount.fetch_add (1, std::memory_order_relaxed) + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindData::Release
//
////////////////////////////////////////////////////////////////////////////////

ULONG STDMETHODCALLTYPE FileSystemBindData::Release()
{
    ULONG  count = m_refCount.fetch_sub (1, std::memory_order_acq_rel) - 1;



    if (count == 0)
    {
        delete this;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindData::SetFindData
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE FileSystemBindData::SetFindData (const WIN32_FIND_DATAW * pfd)
{
    HRESULT  hr = S_OK;



    CBREx (pfd != nullptr, E_POINTER);

    m_found = *pfd;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindData::GetFindData
//
////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE FileSystemBindData::GetFindData (WIN32_FIND_DATAW * pfd)
{
    HRESULT  hr = S_OK;



    CBREx (pfd != nullptr, E_POINTER);

    *pfd = m_found;

Error:
    return hr;
}





