#include "Pch.h"

#include "DragDropTarget.h"

#include "DriveWidgetState.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DragDropTarget
//
////////////////////////////////////////////////////////////////////////////////

DragDropTarget::DragDropTarget()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  ~DragDropTarget
//
////////////////////////////////////////////////////////////////////////////////

DragDropTarget::~DragDropTarget()
{
    Shutdown();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DragDropTarget::Initialize (HWND hwnd, HitTestFn hitTest)
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

HRESULT DragDropTarget::Initialize (HWND hwnd, HitTester * pHitTester, DropFn drop)
{
    HRESULT  hr = S_OK;



    CBRAEx (hwnd,       E_INVALIDARG);
    CBRAEx (pHitTester, E_INVALIDARG);

    m_hwnd      = hwnd;
    m_hitTester = pHitTester;
    m_drop      = std::move (drop);
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

HRESULT DragDropTarget::AttachAdditionalWindow (HWND hwnd)
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

void DragDropTarget::RevokeAllRegistrations ()
{
    size_t   i  = 0;
    HRESULT  hr = S_OK;



    for (i = 0; i < m_registeredHwnds.size(); i++)
    {
        if (m_registeredHwnds[i] != nullptr)
        {
            hr = RevokeDragDrop (m_registeredHwnds[i]);
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

void DragDropTarget::Shutdown()
{
    RevokeAllRegistrations();

    m_hwnd                  = nullptr;
    m_hitTest               = {};
    m_hitTester             = nullptr;
    m_drop                  = {};
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

STDMETHODIMP DragDropTarget::QueryInterface (REFIID riid, void ** ppv)
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


STDMETHODIMP_(ULONG) DragDropTarget::AddRef()
{
    return m_refCount.fetch_add (1, std::memory_order_acq_rel) + 1;
}


STDMETHODIMP_(ULONG) DragDropTarget::Release()
{
    ULONG  result = m_refCount.fetch_sub (1, std::memory_order_acq_rel) - 1;



    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ExtractFirstHDropPath
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DragDropTarget::ExtractFirstHDropPath (IDataObject * pData, std::wstring & outPath)
{
    HRESULT   hr         = S_OK;
    FORMATETC fmt        = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium     = { };
    HDROP     hDrop      = nullptr;
    UINT      cFiles     = 0;
    bool      fLocked    = false;
    bool      fGotMedium = false;
    wchar_t   buffer[MAX_PATH] = { };



    outPath.clear();

    if (pData == nullptr)
    {
        hr = S_FALSE;
        goto Cleanup;
    }

    hr = pData->GetData (&fmt, &medium);
    if (FAILED (hr))
    {
        hr = S_FALSE;
        goto Cleanup;
    }

    fGotMedium = true;
    hDrop      = static_cast<HDROP> (GlobalLock (medium.hGlobal));
    if (hDrop == nullptr)
    {
        hr = S_FALSE;
        goto Cleanup;
    }

    fLocked = true;
    cFiles  = DragQueryFileW (hDrop, 0xFFFFFFFF, nullptr, 0);
    if (cFiles == 0)
    {
        hr = S_FALSE;
        goto Cleanup;
    }

    if (DragQueryFileW (hDrop, 0, buffer, MAX_PATH) == 0)
    {
        hr = S_FALSE;
        goto Cleanup;
    }

    outPath = buffer;
    hr      = S_OK;

Cleanup:
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

STDMETHODIMP DragDropTarget::DragEnter (
    IDataObject * pData,
    DWORD         /*grfKeyState*/,
    POINTL        pt,
    DWORD       * pdwEffect)
{
    std::wstring  path;
    HRESULT       hr = S_OK;



    m_fDragActive           = true;
    m_fDragHasSupportedFile = false;
    m_dragPath.clear();

    hr = ExtractFirstHDropPath (pData, path);
    if (hr == S_OK && IsSupportedDiskImageExtension (path))
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

STDMETHODIMP DragDropTarget::DragOver (
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

STDMETHODIMP DragDropTarget::DragLeave()
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

STDMETHODIMP DragDropTarget::Drop (
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

int DragDropTarget::PickAtClient (const HitTester & hitTester, int xClient, int yClient)
{
    const HitRect * hit = hitTester.Pick (xClient, yClient);



    if (hit == nullptr || hit->slot != HitSlot::Custom)
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

int DragDropTarget::PickAtScreen (POINTL pt) const
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
