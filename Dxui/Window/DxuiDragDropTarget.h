#pragma once

#include "../Core/DxuiHitTester.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropTarget
//
//  IDropTarget implementation that consults a DxuiHitTester (or an
//  ad-hoc screen-coord callback) to route OLE drag/drop events to
//  hit-tested widgets. The optional FilterFn lets the host accept or
//  reject the dragged payload by file path (e.g. only ".dsk"/".po" disk
//  images); if no filter is supplied, any payload is accepted.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDragDropTarget : public IDropTarget
{
public:
    using HitTestFn = std::function<bool (int screenX, int screenY)>;
    using DropFn    = std::function<void (int tag, const std::wstring & path)>;
    using FilterFn  = std::function<bool (const std::wstring & path)>;

    DxuiDragDropTarget  ();
    virtual ~DxuiDragDropTarget();

    HRESULT              Initialize             (HWND hwnd, HitTestFn hitTest);
    HRESULT              Initialize             (HWND              hwnd,
                                                 DxuiHitTester  *  pHitTester,
                                                 DropFn            drop,
                                                 FilterFn          filter = {});
    HRESULT              AttachAdditionalWindow (HWND hwnd);
    void                 Shutdown               ();
    void                 SetFilter              (FilterFn filter) { m_filter = std::move (filter); }

    STDMETHODIMP         QueryInterface         (REFIID riid, void ** ppv) override;
    STDMETHODIMP_(ULONG) AddRef                 () override;
    STDMETHODIMP_(ULONG) Release                () override;

    STDMETHODIMP         DragEnter              (IDataObject * pData,
                                                 DWORD         grfKeyState,
                                                 POINTL        pt,
                                                 DWORD       * pdwEffect) override;
    STDMETHODIMP         DragOver               (DWORD   grfKeyState,
                                                 POINTL  pt,
                                                 DWORD * pdwEffect) override;
    STDMETHODIMP         DragLeave              () override;
    STDMETHODIMP         Drop                   (IDataObject * pData,
                                                 DWORD         grfKeyState,
                                                 POINTL        pt,
                                                 DWORD       * pdwEffect) override;

    static HRESULT       ExtractFirstHDropPath  (IDataObject   * pData,
                                                 std::wstring  & outPath);
    static int           PickAtClient           (const DxuiHitTester & hitTester,
                                                 int                   xClient,
                                                 int                   yClient);

    // -- Drag-state accessors for visual feedback overlay. ----------------
    // IsDragInProgress: true between DragEnter and DragLeave/Drop. Any file.
    // IsDragAcceptedType: true iff the dragged payload passed the host
    //    filter (or there is no filter). False when not dragging.
    // HoveredTag: index of the widget under the cursor (-1 if none).
    bool                 IsDragInProgress       () const { return m_fDragActive;            }
    bool                 IsDragAcceptedType     () const { return m_fDragHasSupportedFile;  }
    int                  HoveredTag             () const { return m_lastHitTag;             }

    // Drop completion sets a one-shot flag so the next WM_LBUTTONUP
    // posted by the OS (the synthetic release at the end of an OLE drag)
    // doesn't get treated as a real click. Consume returns the flag and
    // clears it.
    bool                 ConsumeSuppressedClick()
    {
        bool result = m_fSuppressNextClick;
        m_fSuppressNextClick = false;
        return result;
    }

private:
    int                  PickAtScreen           (POINTL pt) const;
    void                 RevokeAllRegistrations();

    std::atomic<ULONG>   m_refCount              { 1 };
    HWND                 m_hwnd                  = nullptr;
    std::vector<HWND>    m_registeredHwnds;
    HitTestFn            m_hitTest;
    DxuiHitTester     *  m_hitTester             = nullptr;
    DropFn               m_drop;
    FilterFn             m_filter;
    int                  m_lastHitTag            = -1;
    bool                 m_fDragActive           = false;
    bool                 m_fDragHasSupportedFile = false;
    bool                 m_fSuppressNextClick    = false;
    std::wstring         m_dragPath;
};
