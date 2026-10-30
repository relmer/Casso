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
                                   DWORD & resultEffect, DWORD * outLogicalEffect, const DxuiIconImage * dragImage)
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

    //  The drag image given, or the shell's, with room for a target's drop
    //  description. With no window given, the shell builds the image from the
    //  object's formats. A drag without one works as a bare pointer. A drag
    //  image that fails is not followed by the shell's, which would be drawn
    //  beside whatever part of it InitializeFromBitmap set on the object.
    if (dragImage != nullptr)
    {
        hrImage = SetDragImage (static_cast<IDataObject *> (source), *dragImage);
        IGNORE_RETURN_VALUE (hrImage, S_OK);
    }
    else
    {
        hrImage = CoCreateInstance (CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&helper));

        if (SUCCEEDED (hrImage))
        {
            hrImage = helper->SetFlags (DSH_ALLOWDROPDESCRIPTIONTEXT);
            IGNORE_RETURN_VALUE (hrImage, S_OK);

            hrImage = helper->InitializeFromWindow (nullptr, &startScreen, static_cast<IDataObject *> (source));
            IGNORE_RETURN_VALUE (hrImage, S_OK);
        }
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
//  DxuiDragDropSource::SetDragImage
//
//  The image goes to the helper as a premultiplied 32-bit DIB with no color
//  key, so its alpha edges blend. On success the helper frees the bitmap,
//  so it is not deleted here. The pointer sits under the image's bottom
//  center.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropSource::SetDragImage (IDataObject * data, const DxuiIconImage & image)
{
    HRESULT                      hr     = S_OK;
    ComPtr<IDragSourceHelper2>   helper;
    BITMAPINFO                   info   = {};
    void                       * bits   = nullptr;
    HBITMAP                      bitmap = nullptr;
    SHDRAGIMAGE                  drag   = {};
    size_t                       pixels = (size_t) image.width * (size_t) image.height;
    bool                         sized  = image.width > 0 && image.height > 0 && image.bgraPremul.size() == pixels;



    CBRAEx (data != nullptr, E_INVALIDARG);
    CBREx  (sized, E_INVALIDARG);

    hr = CoCreateInstance (CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&helper));
    CHR (hr);

    hr = helper->SetFlags (DSH_ALLOWDROPDESCRIPTIONTEXT);
    CHR (hr);

    //  Top-down, as the image's rows run.
    info.bmiHeader.biSize        = sizeof (info.bmiHeader);
    info.bmiHeader.biWidth       = image.width;
    info.bmiHeader.biHeight      = -image.height;
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = 32;
    info.bmiHeader.biCompression = BI_RGB;

    bitmap = CreateDIBSection (nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    CBREx (bitmap != nullptr && bits != nullptr, E_OUTOFMEMORY);

    memcpy (bits, image.bgraPremul.data(), pixels * sizeof (uint32_t));

    drag.sizeDragImage = SIZE  { image.width, image.height };
    drag.ptOffset      = POINT { image.width / 2, image.height };
    drag.hbmpDragImage = bitmap;
    drag.crColorKey    = CLR_NONE;

    hr = helper->InitializeFromBitmap (&drag, data);
    CHR (hr);

    bitmap = nullptr;

Error:
    if (bitmap != nullptr)
    {
        DeleteObject (bitmap);
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
    //  While the shell's drag window is up, it draws the drop's icon and words
    //  itself, as under File Explorer's drags; OLE's drop cursors would cover
    //  them, so the pointer is a plain arrow. Where the drop has no effect,
    //  OLE's no-drop cursor is used, as in File Explorer's drags.
    if (GetStoredDword (L"IsShowingLayered", 0) != 0)
    {
        DescribeEffect (effect);

        if ((effect & (DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK)) == 0)
        {
            return DRAGDROP_S_USEDEFAULTCURSORS;
        }

        SetCursor (LoadCursorW (nullptr, IDC_ARROW));
        return S_OK;
    }

    return DRAGDROP_S_USEDEFAULTCURSORS;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::DescribeEffect
//
//  The shell's drag window draws a description only when the source tells
//  it to reread one, and it rereads what is on the object when it gets to
//  the message, not what was there when the message was posted. File
//  Explorer clears its words and sets them again within a few milliseconds
//  as the effect settles, so a reread asked for on each change can land in
//  the gap and leave a bare "+" drawn. It is asked for on every feedback
//  instead, which OLE gives several times a second with the pointer still,
//  so the window always catches up with the description.
//
//  File Explorer also clears the description, to DROPIMAGE_INVALID with no
//  words, wherever it has no effect yet, and a target may set none at all.
//  Either way the type is filled in from the effect, so the window draws the
//  effect's icon. A type the target chose is not replaced. With no effect
//  the type is DROPIMAGE_INVALID, so the window doesn't draw a description,
//  as in File Explorer; DROPIMAGE_NONE would draw its icon in an empty label.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDragDropSource::DescribeEffect (DWORD effect)
{
    CLIPFORMAT              format      = (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_DROPDESCRIPTION);
    std::vector<uint8_t>  & bytes       = m_stored[format];
    DROPDESCRIPTION       * description = nullptr;
    DROPIMAGETYPE           type        = DROPIMAGE_INVALID;
    HWND                    window      = nullptr;



    //  A target that offers both is described as a copy, as File Explorer
    //  words it.
    if (effect & DROPEFFECT_COPY)
    {
        type = DROPIMAGE_COPY;
    }
    else if (effect & DROPEFFECT_MOVE)
    {
        type = DROPIMAGE_MOVE;
    }
    else if (effect & DROPEFFECT_LINK)
    {
        type = DROPIMAGE_LINK;
    }

    if (bytes.size() < sizeof (DROPDESCRIPTION))
    {
        bytes.assign (sizeof (DROPDESCRIPTION), 0);
        m_storedTymed[format] = TYMED_HGLOBAL;
        ((DROPDESCRIPTION *) bytes.data())->type = DROPIMAGE_INVALID;
    }

    description = (DROPDESCRIPTION *) bytes.data();

    if (description->type == DROPIMAGE_INVALID || m_describesEffect)
    {
        description->type = type;
        m_describesEffect = true;
    }

    window = (HWND) (ULONG_PTR) GetStoredDword (L"DragWindow", 0);

    if (window != nullptr)
    {
        PostMessageW (window, s_kUpdateDragWindowMessage, 0, 0);
    }
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
    HRESULT                       hr       = S_OK;
    const Format                * entry    = nullptr;
    const std::vector<uint8_t>  * bytes    = nullptr;
    HGLOBAL                       global   = nullptr;
    void                        * locked   = nullptr;
    int                           index    = 0;
    bool                          inRange  = false;
    bool                          wantsHg  = false;
    bool                          stored   = false;
    bool                          asStream = false;
    IStream                     * stream   = nullptr;
    ULARGE_INTEGER                size     = {};



    CBREx (format != nullptr && medium != nullptr, E_POINTER);

    ZeroMemory (medium, sizeof (*medium));

    stored   = m_stored.contains (format->cfFormat);
    asStream = stored && (m_storedTymed[format->cfFormat] & TYMED_ISTREAM) != 0 && (format->tymed & TYMED_ISTREAM) != 0;
    wantsHg  = (format->tymed & TYMED_HGLOBAL) != 0;
    CBREx (asStream || wantsHg, DV_E_TYMED);

    if (stored)
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

    medium->pUnkForRelease = nullptr;

    //  A format set as a stream is read back as one, over the same memory,
    //  which the stream frees when it is released.
    if (asStream)
    {
        hr = CreateStreamOnHGlobal (global, TRUE, &stream);
        CHR (hr);

        global = nullptr;

        size.QuadPart = bytes->size();

        hr = stream->SetSize (size);
        CHR (hr);

        medium->tymed = TYMED_ISTREAM;
        medium->pstm  = stream;
        stream        = nullptr;
    }
    else
    {
        medium->tymed   = TYMED_HGLOBAL;
        medium->hGlobal = global;
        global          = nullptr;
    }

Error:
    if (stream != nullptr)
    {
        stream->Release();
    }

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
    DWORD  media = TYMED_HGLOBAL;



    if (format == nullptr)
    {
        return E_POINTER;
    }

    if (FindFormat (format->cfFormat) == nullptr && !m_stored.contains (format->cfFormat))
    {
        return DV_E_FORMATETC;
    }

    //  A stored format is read in global memory, or in the medium it was set in.
    if (m_storedTymed.contains (format->cfFormat))
    {
        media |= m_storedTymed[format->cfFormat];
    }

    return (format->tymed & media) != 0 ? S_OK : DV_E_TYMED;
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
    bool                   global = false;
    bool                   stream = false;



    CBREx (format != nullptr && medium != nullptr, E_POINTER);

    global = (medium->tymed & TYMED_HGLOBAL) != 0 && (format->tymed & TYMED_HGLOBAL) != 0;
    stream = (medium->tymed & TYMED_ISTREAM) != 0 && (format->tymed & TYMED_ISTREAM) != 0 && medium->pstm != nullptr;
    CBREx (global || stream, DV_E_TYMED);

    if (global)
    {
        locked = (const uint8_t *) GlobalLock (medium->hGlobal);
        CWR (locked);

        bytes.assign (locked, locked + GlobalSize (medium->hGlobal));
        GlobalUnlock (medium->hGlobal);
    }
    else
    {
        hr = ReadStream (medium->pstm, bytes);
        CHR (hr);
    }

    m_stored[format->cfFormat]      = std::move (bytes);
    m_storedTymed[format->cfFormat] = medium->tymed;

    //  A description the target set is the target's, until GiveFeedback fills
    //  in a type it left invalid.
    if (format->cfFormat == (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_DROPDESCRIPTION))
    {
        m_describesEffect = false;
    }

    if (release)
    {
        ReleaseStgMedium (medium);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource::ReadStream
//
//  The whole stream, from its start.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropSource::ReadStream (IStream * stream, std::vector<uint8_t> & outBytes)
{
    HRESULT        hr    = S_OK;
    STATSTG        stat  = {};
    LARGE_INTEGER  start = {};
    ULONG          read  = 0;
    bool           whole = false;



    outBytes.clear();

    hr = stream->Stat (&stat, STATFLAG_NONAME);
    CHR (hr);

    hr = stream->Seek (start, STREAM_SEEK_SET, nullptr);
    CHR (hr);

    outBytes.resize ((size_t) stat.cbSize.QuadPart);

    if (!outBytes.empty())
    {
        hr = stream->Read (outBytes.data(), (ULONG) outBytes.size(), &read);
        CHR (hr);

        whole = read == outBytes.size();
        CBREx (whole, STG_E_READFAULT);
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
        FORMATETC  one = { stored, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | m_storedTymed[stored] };

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
