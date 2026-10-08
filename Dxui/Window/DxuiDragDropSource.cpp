#include "Pch.h"

#include "DxuiDragDropSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::DxuiDragDropSource
//
////////////////////////////////////////////////////////////////////////////////

DxuiDragDropSource::DxuiDragDropSource (std::vector<Format> formats)
    : m_formats (std::move (formats))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropSource::Create (std::vector<Format> formats, DxuiDragDropSource ** outSource)
{
    HRESULT               hr     = S_OK;
    DxuiDragDropSource *  source = nullptr;



    CBRAEx (outSource != nullptr, E_INVALIDARG);

    *outSource = nullptr;

    source = new (std::nothrow) DxuiDragDropSource (std::move (formats));
    CPRA (source);

    *outSource = source;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::Begin
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropSource::Begin (std::vector<Format> formats, DWORD allowedEffects, POINT startScreen,
                                   DWORD & resultEffect, DWORD * outLogicalEffect)
{
    HRESULT                      hr      = S_OK;
    HRESULT                      hrImage = S_OK;
    DxuiDragDropSource         * source  = nullptr;
    ComPtr<IDragSourceHelper2>   helper;



    resultEffect = DROPEFFECT_NONE;

    if (outLogicalEffect != nullptr)
    {
        *outLogicalEffect = DROPEFFECT_NONE;
    }

    hr = Create (std::move (formats), &source);
    CHR (hr);

    //  The shell's drag image, with room for a target's drop description. With
    //  no window given, the shell builds the image from the object's own
    //  formats. A drag without one still works, as a bare pointer.
    hrImage = CoCreateInstance (CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&helper));

    if (SUCCEEDED (hrImage))
    {
        hrImage = helper->SetFlags (DSH_ALLOWDROPDESCRIPTIONTEXT);
        IGNORE_RETURN_VALUE (hrImage, S_OK);

        hrImage = helper->InitializeFromWindow (nullptr, &startScreen, static_cast<IDataObject *> (source));
        IGNORE_RETURN_VALUE (hrImage, S_OK);
    }

    hr = DoDragDrop (static_cast<IDataObject *> (source), static_cast<IDropSource *> (source),
                     allowedEffects, &resultEffect);

    //  A cancel and a drop both end the drag normally; only a failure is
    //  reported as one.
    if (hr == DRAGDROP_S_DROP || hr == DRAGDROP_S_CANCEL)
    {
        hr = S_OK;
    }

    CHR (hr);

    if (outLogicalEffect != nullptr)
    {
        *outLogicalEffect = source->GetStoredDword (CFSTR_LOGICALPERFORMEDDROPEFFECT, resultEffect);
    }

Error:
    if (source != nullptr)
    {
        source->Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::QueryInterface
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::QueryInterface (REFIID riid, void ** ppv)
{
    if (ppv == nullptr)
    {
        return E_POINTER;
    }

    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == IID_IDataObject)
    {
        *ppv = static_cast<IDataObject *> (this);
    }
    else if (riid == IID_IDropSource)
    {
        *ppv = static_cast<IDropSource *> (this);
    }
    else
    {
        return E_NOINTERFACE;
    }

    AddRef();

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::AddRef
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) DxuiDragDropSource::AddRef()
{
    return ++m_refCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::Release
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) DxuiDragDropSource::Release()
{
    ULONG  remaining = --m_refCount;



    if (remaining == 0)
    {
        delete this;
    }

    return remaining;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::QueryContinueDrag
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::QueryContinueDrag (BOOL escapePressed, DWORD keyState)
{
    if (escapePressed)
    {
        return DRAGDROP_S_CANCEL;
    }

    if ((keyState & (MK_LBUTTON | MK_RBUTTON)) == 0)
    {
        return DRAGDROP_S_DROP;
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::GiveFeedback
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::GiveFeedback (DWORD effect)
{
    UNREFERENCED_PARAMETER (effect);

    return DRAGDROP_S_USEDEFAULTCURSORS;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::FindFormat
//
////////////////////////////////////////////////////////////////////////////////

const DxuiDragDropSource::Format * DxuiDragDropSource::FindFormat (CLIPFORMAT format) const
{
    for (const Format & entry : m_formats)
    {
        if (entry.format == format)
        {
            return &entry;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::GetRendered
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropSource::GetRendered (const Format & format, int index, const std::vector<uint8_t> *& outBytes)
{
    HRESULT                     hr        = S_OK;
    std::pair<CLIPFORMAT, int>  key       = { format.format, index };
    auto                        it        = m_rendered.find (key);
    std::vector<uint8_t>        bytes;
    bool                        canRender = (bool) format.render;



    outBytes = nullptr;

    if (it != m_rendered.end())
    {
        outBytes = &it->second;
        BAIL_OUT_IF (true, S_OK);
    }

    CBREx (canRender, DV_E_FORMATETC);

    m_renderCounts[key]++;

    hr = format.render (index, bytes);
    CHR (hr);

    outBytes = &(m_rendered[key] = std::move (bytes));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::GetRenderCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDragDropSource::GetRenderCount (CLIPFORMAT format, int index) const
{
    auto  it = m_renderCounts.find (std::pair<CLIPFORMAT, int> { format, index });



    return (it != m_renderCounts.end()) ? it->second : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::GetData
//
//  Global memory is the one medium every target of these formats reads, so it
//  is the one offered. The item index is the request's lindex for a format
//  with several items, and zero otherwise.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::GetData (FORMATETC * format, STGMEDIUM * medium)
{
    HRESULT                       hr      = S_OK;
    const Format                * entry   = nullptr;
    const std::vector<uint8_t>  * bytes   = nullptr;
    HGLOBAL                       global  = nullptr;
    void                        * locked  = nullptr;
    int                           index   = 0;
    bool                          inRange = false;
    bool                          wantsHg = false;



    CBREx (format != nullptr && medium != nullptr, E_POINTER);

    ZeroMemory (medium, sizeof (*medium));

    wantsHg = (format->tymed & TYMED_HGLOBAL) != 0;
    CBREx (wantsHg, DV_E_TYMED);

    if (m_stored.contains (format->cfFormat))
    {
        bytes = &m_stored[format->cfFormat];
    }
    else
    {
        entry = FindFormat (format->cfFormat);
        CBREx (entry != nullptr, DV_E_FORMATETC);

        index   = (entry->count > 1) ? (int) format->lindex : 0;
        inRange = index >= 0 && index < entry->count;
        CBREx (inRange, DV_E_LINDEX);

        hr = GetRendered (*entry, index, bytes);
        CHR (hr);
    }

    global = GlobalAlloc (GMEM_MOVEABLE, (std::max) (bytes->size(), (size_t) 1));
    CWR (global);

    locked = GlobalLock (global);
    CWR (locked);

    if (!bytes->empty())
    {
        memcpy (locked, bytes->data(), bytes->size());
    }

    GlobalUnlock (global);

    medium->tymed          = TYMED_HGLOBAL;
    medium->hGlobal        = global;
    medium->pUnkForRelease = nullptr;
    global                 = nullptr;

Error:
    if (global != nullptr)
    {
        GlobalFree (global);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::GetDataHere
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::GetDataHere (FORMATETC * format, STGMEDIUM * medium)
{
    UNREFERENCED_PARAMETER (format);
    UNREFERENCED_PARAMETER (medium);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::QueryGetData
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::QueryGetData (FORMATETC * format)
{
    if (format == nullptr)
    {
        return E_POINTER;
    }

    if (FindFormat (format->cfFormat) == nullptr && !m_stored.contains (format->cfFormat))
    {
        return DV_E_FORMATETC;
    }

    return (format->tymed & TYMED_HGLOBAL) != 0 ? S_OK : DV_E_TYMED;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::GetCanonicalFormatEtc
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::GetCanonicalFormatEtc (FORMATETC * formatIn, FORMATETC * formatOut)
{
    UNREFERENCED_PARAMETER (formatIn);

    if (formatOut != nullptr)
    {
        formatOut->ptd = nullptr;
    }

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::SetData
//
//  Targets and the shell's drag helpers set formats of their own on the
//  object -- the drag image, a drop description, a performed effect -- and
//  read them back later in the drag. Each is kept as bytes, in global memory,
//  the one medium they use.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::SetData (FORMATETC * format, STGMEDIUM * medium, BOOL release)
{
    HRESULT                hr     = S_OK;
    const uint8_t        * locked = nullptr;
    std::vector<uint8_t>   bytes;



    CBREx (format != nullptr && medium != nullptr, E_POINTER);
    CBREx ((medium->tymed & TYMED_HGLOBAL) != 0 && (format->tymed & TYMED_HGLOBAL) != 0, DV_E_TYMED);

    locked = (const uint8_t *) GlobalLock (medium->hGlobal);
    CWR (locked);

    bytes.assign (locked, locked + GlobalSize (medium->hGlobal));
    GlobalUnlock (medium->hGlobal);

    m_stored[format->cfFormat] = std::move (bytes);

    if (release)
    {
        ReleaseStgMedium (medium);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::GetStoredDword
//
////////////////////////////////////////////////////////////////////////////////

DWORD DxuiDragDropSource::GetStoredDword (const wchar_t * formatName, DWORD fallback) const
{
    auto   found = m_stored.find ((CLIPFORMAT) RegisterClipboardFormatW (formatName));
    DWORD  value = fallback;



    if (found != m_stored.end() && found->second.size() >= sizeof (DWORD))
    {
        memcpy (&value, found->second.data(), sizeof (DWORD));
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::EnumFormatEtc
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::EnumFormatEtc (DWORD direction, IEnumFORMATETC ** outEnum)
{
    std::vector<FORMATETC>  formats;



    if (outEnum == nullptr)
    {
        return E_POINTER;
    }

    *outEnum = nullptr;

    if (direction != DATADIR_GET)
    {
        return E_NOTIMPL;
    }

    for (const Format & entry : m_formats)
    {
        FORMATETC  one = { entry.format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

        formats.push_back (one);
    }

    for (const auto & [stored, bytes] : m_stored)
    {
        FORMATETC  one = { stored, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

        if (FindFormat (stored) == nullptr)
        {
            formats.push_back (one);
        }
    }

    return SHCreateStdEnumFmtEtc ((UINT) formats.size(), formats.data(), outEnum);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::DAdvise
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::DAdvise (FORMATETC * format, DWORD advf, IAdviseSink * sink, DWORD * connection)
{
    UNREFERENCED_PARAMETER (format);
    UNREFERENCED_PARAMETER (advf);
    UNREFERENCED_PARAMETER (sink);
    UNREFERENCED_PARAMETER (connection);

    return OLE_E_ADVISENOTSUPPORTED;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::DUnadvise
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::DUnadvise (DWORD connection)
{
    UNREFERENCED_PARAMETER (connection);

    return OLE_E_ADVISENOTSUPPORTED;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::EnumDAdvise
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropSource::EnumDAdvise (IEnumSTATDATA ** outEnum)
{
    if (outEnum != nullptr)
    {
        *outEnum = nullptr;
    }

    return OLE_E_ADVISENOTSUPPORTED;
}
