#include "Pch.h"

#include "DragDropTarget.h"

#include "DriveWidgetElement.h"
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
//  Registers as the HWND's drop target. Caller must already have
//  initialized OLE on this thread.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DragDropTarget::Initialize (HWND hwnd, HitTestFn hitTest)
{
    HRESULT  hr = S_OK;



    CPR (hwnd);

    m_hwnd    = hwnd;
    m_hitTest = std::move (hitTest);

    // Non-asserting: RegisterDragDrop fails legitimately if OLE wasn't
    // initialized on this thread yet. Callers handle the warning.
    hr = RegisterDragDrop (hwnd, this);

    if (SUCCEEDED (hr))
    {
        m_fRegistered = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
//  Revokes registration. Safe to call multiple times.
//
////////////////////////////////////////////////////////////////////////////////

void DragDropTarget::Shutdown()
{
    if (m_fRegistered && m_hwnd != nullptr)
    {
        HRESULT  hr = RevokeDragDrop (m_hwnd);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_fRegistered = false;
    }

    m_hwnd    = nullptr;
    m_hitTest = {};
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

    // We don't delete here -- this object lives as a member of
    // EmulatorShell. OLE's RegisterDragDrop should release its
    // reference when RevokeDragDrop runs in Shutdown.
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractFirstHDropPath
//
//  Extracts the first file path from a CF_HDROP data object. Returns S_OK
//  + the path on success, S_FALSE if the data object doesn't carry CF_HDROP.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DragDropTarget::ExtractFirstHDropPath (IDataObject * pData, std::wstring & outPath)
{
    HRESULT   hr        = S_OK;
    FORMATETC fmt       = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium    = { };
    HDROP     hDrop     = nullptr;
    UINT      cFiles    = 0;
    bool      fLocked   = false;
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
    HRESULT       hr  = S_OK;



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
    DriveWidgetElement *  pWidget = nullptr;



    if (pdwEffect == nullptr)
    {
        return E_POINTER;
    }

    if (!m_fDragHasSupportedFile)
    {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    if (m_hitTest)
    {
        pWidget = m_hitTest (pt.x, pt.y);
    }

    *pdwEffect = (pWidget != nullptr) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DragLeave
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DragDropTarget::DragLeave()
{
    m_fDragHasSupportedFile = false;
    m_dragPath.clear();
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Drop
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP DragDropTarget::Drop (
    IDataObject * pData,
    DWORD         /*grfKeyState*/,
    POINTL        pt,
    DWORD       * pdwEffect)
{
    std::wstring          path;
    DriveWidgetElement *  pWidget = nullptr;
    HRESULT               hr      = S_OK;



    if (pdwEffect != nullptr)
    {
        *pdwEffect = DROPEFFECT_NONE;
    }

    hr = ExtractFirstHDropPath (pData, path);

    if (hr != S_OK || !IsSupportedDiskImageExtension (path))
    {
        m_fDragHasSupportedFile = false;
        return S_OK;
    }

    if (m_hitTest)
    {
        pWidget = m_hitTest (pt.x, pt.y);
    }

    if (pWidget != nullptr)
    {
        HRESULT  hrMount = pWidget->HandleDroppedFile (path);
        IGNORE_RETURN_VALUE (hrMount, S_OK);

        if (pdwEffect != nullptr)
        {
            *pdwEffect = DROPEFFECT_COPY;
        }
    }

    m_fDragHasSupportedFile = false;
    m_dragPath.clear();

    return S_OK;
}
