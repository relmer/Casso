#pragma once

#include "Pch.h"

#include "IDriveCommandSink.h"







////////////////////////////////////////////////////////////////////////////////
//
//  DragDropTarget
//
//  Single main-window IDropTarget — one
//  RegisterDragDrop call for the whole HWND, hit-testing the dropped
//  point against the native chrome to find a drive widget under cursor.
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
//  the (screenX, screenY) pair and returns true when the point lands
//  on an accepting drive widget. The host owns the chrome tree; this
//  class only needs the yes/no answer.
//
//  Drop
//  ----
//  Extracts the first CF_HDROP file path. If the extension is not one
//  of the four supported disk image types (FR-022b), the drop is
//  rejected with DROPEFFECT_NONE.
//
////////////////////////////////////////////////////////////////////////////////


class DragDropTarget : public IDropTarget
{
public:
    using HitTestFn = std::function<bool (int screenX, int screenY)>;


    DragDropTarget  ();
    virtual ~DragDropTarget ();

    HRESULT              Initialize           (HWND hwnd, HitTestFn hitTest);
    void                 Shutdown             ();

    // --- IUnknown ---
    STDMETHODIMP         QueryInterface       (REFIID riid, void ** ppv) override;
    STDMETHODIMP_(ULONG) AddRef               () override;
    STDMETHODIMP_(ULONG) Release              () override;

    // --- IDropTarget ---
    STDMETHODIMP         DragEnter            (IDataObject * pData,
                                               DWORD         grfKeyState,
                                               POINTL        pt,
                                               DWORD       * pdwEffect) override;
    STDMETHODIMP         DragOver             (DWORD   grfKeyState,
                                               POINTL  pt,
                                               DWORD * pdwEffect) override;
    STDMETHODIMP         DragLeave            () override;
    STDMETHODIMP         Drop                 (IDataObject * pData,
                                               DWORD         grfKeyState,
                                               POINTL        pt,
                                               DWORD       * pdwEffect) override;

    // ---- Pure-logic helpers (exposed for tests) ----

    static HRESULT       ExtractFirstHDropPath (IDataObject   * pData,
                                                std::wstring  & outPath);

private:
    std::atomic<ULONG>  m_refCount             { 1 };
    HWND                m_hwnd                 = nullptr;
    bool                m_fRegistered          = false;
    HitTestFn           m_hitTest;

    // Cached during DragEnter so DragOver can answer accept/reject cheaply.
    bool                m_fDragHasSupportedFile = false;
    std::wstring        m_dragPath;
};
