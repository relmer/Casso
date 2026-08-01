#include "Pch.h"

#include "DxuiDragDropTarget.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropTarget
//
////////////////////////////////////////////////////////////////////////////////

DxuiDragDropTarget::DxuiDragDropTarget()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiDragDropTarget
//
////////////////////////////////////////////////////////////////////////////////

DxuiDragDropTarget::~DxuiDragDropTarget()
{
    Shutdown();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropTarget::Initialize (HWND hwnd, HitTestFn hitTest)
{
    HRESULT  hr = S_OK;



    CBRAEx (hwnd, E_INVALIDARG);

    m_hwnd      = hwnd;
    m_hitTest   = std::move (hitTest);
    m_hitTester = nullptr;
    m_drop      = {};

    hr = RegisterDragDrop (hwnd, this);
    if (SUCCEEDED (hr))
    {
        m_registeredHwnds.push_back (hwnd);
    }

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropTarget::Initialize (HWND hwnd, DxuiHitTester * pHitTester, DropFn drop, FilterFn filter)
{
    HRESULT  hr = S_OK;



    CBRAEx (hwnd,       E_INVALIDARG);
    CBRAEx (pHitTester, E_INVALIDARG);

    m_hwnd      = hwnd;
    m_hitTester = pHitTester;
    m_drop      = std::move (drop);
    m_filter    = std::move (filter);
    m_hitTest   = {};

    hr = RegisterDragDrop (hwnd, this);
    if (SUCCEEDED (hr))
    {
        m_registeredHwnds.push_back (hwnd);
    }

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  AttachAdditionalWindow
//
//  Registers the same IDropTarget instance with a second HWND so that
//  drag-overs on a child window (e.g. the CassoRenderSurface child that
//  occludes the parent's client area) get routed through the same
//  hit-test + dispatch path. Without this, OLE shows the not-allowed
//  cursor over the child because the child has no IDropTarget of its
//  own.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropTarget::AttachAdditionalWindow (HWND hwnd)
{
    HRESULT  hr = S_OK;



    CBRAEx (hwnd, E_INVALIDARG);

    hr = RegisterDragDrop (hwnd, this);
    if (SUCCEEDED (hr))
    {
        m_registeredHwnds.push_back (hwnd);
    }

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  RevokeAllRegistrations
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDragDropTarget::RevokeAllRegistrations()
{
    size_t   i  = 0;
    HRESULT  hr = S_OK;



    for (auto & registeredHwnd : m_registeredHwnds)
    {
        if (registeredHwnd != nullptr)
        {
            hr = RevokeDragDrop (registeredHwnd);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    m_registeredHwnds.clear();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDragDropTarget::Shutdown()
{
    RevokeAllRegistrations();

    m_hwnd                  = nullptr;
    m_hitTest               = {};
    m_hitTester             = nullptr;
    m_drop                  = {};
    m_filter                = {};
    m_lastHitTag            = -1;
    m_fDragActive           = false;
    m_fDragHasSupportedFile = false;
    m_dragPath.clear();
}




////////////////////////////////////////////////////////////////////////////////
//
//  IUnknown
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropTarget::QueryInterface (REFIID riid, void ** ppv)
{
    if (ppv == nullptr)
    {
        return E_POINTER;
    }

    if (riid == IID_IUnknown || riid == IID_IDropTarget)
    {
        *ppv = static_cast<IDropTarget *> (this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}


STDMETHODIMP_(ULONG) DxuiDragDropTarget::AddRef()
{
    return m_refCount.fetch_add (1, std::memory_order_acq_rel) + 1;
}


STDMETHODIMP_(ULONG) DxuiDragDropTarget::Release()
{
    ULONG  result = m_refCount.fetch_sub (1, std::memory_order_acq_rel) - 1;



    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ExtractFirstHDropPath
//
//  Pulls the first file path out of a CF_HDROP data object. outPath is
//  cleared up front and only assigned on success, so a caller that ignores
//  the HRESULT still sees an empty string on every failure path.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDragDropTarget::ExtractFirstHDropPath (IDataObject * pData, std::wstring & outPath)
{
    HRESULT    hr         = S_OK;
    FORMATETC  fmt        = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM  medium     = { };
    HDROP      hDrop      = nullptr;
    UINT       cFiles     = 0;
    UINT       cchCopied  = 0;
    bool       fLocked    = false;
    bool       fGotMedium = false;
    wchar_t    buffer[MAX_PATH] = { };



    outPath.clear();

    // A null data object from OLE is a caller bug, not a drag without files.
    CBRAEx (pData != nullptr, E_INVALIDARG);

    // Not a CF_HDROP drag (text, a URL, anything else) -- an expected miss,
    // so this does not assert.
    hr = pData->GetData (&fmt, &medium);
    CHR (hr);

    fGotMedium = true;

    // GlobalLock documents GetLastError, so CWRA keeps the real code rather
    // than flattening it.
    hDrop = static_cast<HDROP> (GlobalLock (medium.hGlobal));
    CWRA (hDrop);

    fLocked = true;

    cFiles = DragQueryFileW (hDrop, 0xFFFFFFFF, nullptr, 0);
    CBR (cFiles != 0);

    cchCopied = DragQueryFileW (hDrop, 0, buffer, MAX_PATH);
    CBR (cchCopied != 0);

    outPath = buffer;

Error:
    if (fLocked)
    {
        GlobalUnlock (medium.hGlobal);
    }

    if (fGotMedium)
    {
        ReleaseStgMedium (&medium);
    }

    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DragEnter
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropTarget::DragEnter (
    IDataObject * pData,
    DWORD         /*grfKeyState*/,
    POINTL        pt,
    DWORD       * pdwEffect)
{
    std::wstring  path;
    HRESULT       hrExtract = S_OK;



    m_fDragActive           = true;
    m_fDragHasSupportedFile = false;
    m_dragPath.clear();

    // A drag carrying something other than files is routine, so a failed
    // extract is not propagated -- it just means there is nothing to accept.
    hrExtract = ExtractFirstHDropPath (pData, path);
    if (SUCCEEDED (hrExtract) && (!m_filter || m_filter (path)))
    {
        m_fDragHasSupportedFile = true;
        m_dragPath              = path;
    }

    return DragOver (0, pt, pdwEffect);
}




////////////////////////////////////////////////////////////////////////////////
//
//  DragOver
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropTarget::DragOver (
    DWORD     /*grfKeyState*/,
    POINTL    pt,
    DWORD   * pdwEffect)
{
    int  tag = -1;



    if (pdwEffect == nullptr)
    {
        return E_POINTER;
    }

    tag = PickAtScreen (pt);
    m_lastHitTag = tag;
    *pdwEffect   = (m_fDragHasSupportedFile && tag >= 0) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DragLeave
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropTarget::DragLeave()
{
    m_fDragActive           = false;
    m_fDragHasSupportedFile = false;
    m_dragPath.clear();
    m_lastHitTag            = -1;
    return S_OK;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Drop
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DxuiDragDropTarget::Drop (
    IDataObject * /*pData*/,
    DWORD         /*grfKeyState*/,
    POINTL        pt,
    DWORD       * pdwEffect)
{
    int  tag = PickAtScreen (pt);



    if (pdwEffect != nullptr)
    {
        *pdwEffect = (m_fDragHasSupportedFile && tag >= 0) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    }

    if (m_fDragHasSupportedFile && tag >= 0 && m_drop)
    {
        m_drop (tag, m_dragPath);
        m_fSuppressNextClick = true;   // swallow the post-drop WM_LBUTTONUP
    }

    m_fDragActive           = false;
    m_fDragHasSupportedFile = false;
    m_dragPath.clear();
    m_lastHitTag            = -1;

    return S_OK;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PickAtClient
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDragDropTarget::PickAtClient (const DxuiHitTester & hitTester, int xClient, int yClient)
{
    const DxuiHitRect * hit = hitTester.Pick (xClient, yClient);



    if (hit == nullptr || hit->slot != DxuiHitSlot::Custom)
    {
        return -1;
    }

    return hit->tag;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PickAtScreen
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDragDropTarget::PickAtScreen (POINTL pt) const
{
    POINT  client = { pt.x, pt.y };
    int    tag    = -1;



    if (m_hitTester != nullptr && m_hwnd != nullptr)
    {
        if (!ScreenToClient (m_hwnd, &client))
        {
            return -1;
        }

        tag = PickAtClient (*m_hitTester, client.x, client.y);
        return tag;
    }

    if (m_hitTest)
    {
        return m_hitTest (pt.x, pt.y) ? 0 : -1;
    }

    return -1;
}
