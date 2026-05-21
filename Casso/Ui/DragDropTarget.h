#pragma once

#include "Pch.h"

#include "IDriveCommandSink.h"







////////////////////////////////////////////////////////////////////////////////
//
//  DragDropTarget
//
//  Single main-window IDropTarget (Open Question 10 -- one
//  RegisterDragDrop call for the whole HWND, hit-testing the dropped
//  point against RmlUi to find a drive widget under cursor).
//
//  Ownership
//  ---------
//  Allocated by `EmulatorShell` as a stack member; `Initialize(hwnd)` is
//  called once after OleInitialize succeeds, `Shutdown()` is called
//  before window destruction. Implements COM IUnknown by hand (no ATL,
//  no WRL ComPtr<> wrapping); ref count is initialized to 1 by
//  construction and incremented from RegisterDragDrop.
//
//  Hit-test
//  --------
//  On DragOver, calls back through a host-supplied lambda that takes
//  the (screenX, screenY) pair and returns the topmost drive widget
//  pointer (or nullptr). The host owns the RmlUi context and the
//  drive-widget catalog; this class only needs the answer.
//
//  Drop
//  ----
//  Extracts the first CF_HDROP file path. If the extension is not one
//  of the four supported disk image types (FR-022b), the drop is
//  rejected with DROPEFFECT_NONE.
//
////////////////////////////////////////////////////////////////////////////////

class DriveWidgetElement;


class DragDropTarget : public IDropTarget
{
public:
    using HitTestFn = std::function<DriveWidgetElement * (int screenX, int screenY)>;


    DragDropTarget();
    virtual ~DragDropTarget();

    // Registers as the HWND's drop target. Caller must have already
    // initialized OLE (OleInitialize) on this thread.
    HRESULT  Initialize (HWND hwnd, HitTestFn hitTest);

    // Revokes registration. Safe to call multiple times.
    void     Shutdown();

    // --- IUnknown ---
    STDMETHODIMP         QueryInterface (REFIID riid, void ** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // --- IDropTarget ---
    STDMETHODIMP DragEnter (IDataObject * pData, DWORD grfKeyState,
                            POINTL pt, DWORD * pdwEffect) override;
    STDMETHODIMP DragOver  (DWORD grfKeyState, POINTL pt,
                            DWORD * pdwEffect) override;
    STDMETHODIMP DragLeave() override;
    STDMETHODIMP Drop      (IDataObject * pData, DWORD grfKeyState,
                            POINTL pt, DWORD * pdwEffect) override;

    // ---- Pure-logic helpers (exposed for tests) ----

    // Extracts the first file path from a CF_HDROP data object. Returns
    // S_OK + the path on success, S_FALSE if the data object doesn't
    // carry CF_HDROP.
    static HRESULT ExtractFirstHDropPath (IDataObject * pData, std::wstring & outPath);

private:
    std::atomic<ULONG>  m_refCount    { 1 };
    HWND                m_hwnd        = nullptr;
    bool                m_fRegistered = false;
    HitTestFn           m_hitTest;

    // Cached during DragEnter so DragOver can answer accept/reject
    // without re-querying the data object every mouse-move.
    bool                m_fDragHasSupportedFile = false;
    std::wstring        m_dragPath;
};
