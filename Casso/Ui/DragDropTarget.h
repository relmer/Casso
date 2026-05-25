#pragma once

#include "Pch.h"

#include "HitTester.h"
#include "IDriveCommandSink.h"





class DragDropTarget : public IDropTarget
{
public:
    using HitTestFn = std::function<bool (int screenX, int screenY)>;
    using DropFn    = std::function<void (int tag, const std::wstring & path)>;

    DragDropTarget  ();
    virtual ~DragDropTarget ();

    HRESULT              Initialize            (HWND hwnd, HitTestFn hitTest);
    HRESULT              Initialize            (HWND hwnd, HitTester * pHitTester, DropFn drop);
    HRESULT              AttachAdditionalWindow (HWND hwnd);
    void                 Shutdown              ();

    STDMETHODIMP         QueryInterface        (REFIID riid, void ** ppv) override;
    STDMETHODIMP_(ULONG) AddRef                () override;
    STDMETHODIMP_(ULONG) Release               () override;

    STDMETHODIMP         DragEnter             (IDataObject * pData,
                                                DWORD         grfKeyState,
                                                POINTL        pt,
                                                DWORD       * pdwEffect) override;
    STDMETHODIMP         DragOver              (DWORD   grfKeyState,
                                                POINTL  pt,
                                                DWORD * pdwEffect) override;
    STDMETHODIMP         DragLeave             () override;
    STDMETHODIMP         Drop                  (IDataObject * pData,
                                                DWORD         grfKeyState,
                                                POINTL        pt,
                                                DWORD       * pdwEffect) override;

    static HRESULT       ExtractFirstHDropPath (IDataObject   * pData,
                                                 std::wstring  & outPath);
    static int           PickAtClient          (const HitTester & hitTester,
                                                 int               xClient,
                                                 int               yClient);

    // -- Drag-state accessors for visual feedback overlay. ----------------
    // IsDragInProgress: true between DragEnter and DragLeave/Drop. Any file.
    // IsDragAcceptedType: true iff the dragged payload is a supported disk
    //    image extension. False for unsupported types and when not dragging.
    // HoveredTag: index of the drive widget under the cursor (-1 if none).
    bool                 IsDragInProgress      () const { return m_fDragActive;            }
    bool                 IsDragAcceptedType    () const { return m_fDragHasSupportedFile;  }
    int                  HoveredTag            () const { return m_lastHitTag;             }

    // Drop completion sets a one-shot flag so the next WM_LBUTTONUP
    // posted by the OS (the synthetic release at the end of an OLE drag)
    // doesn't get treated as a real click. Consume returns the flag and
    // clears it.
    bool                 ConsumeSuppressedClick ()
    {
        bool result = m_fSuppressNextClick;
        m_fSuppressNextClick = false;
        return result;
    }

private:
    int                  PickAtScreen          (POINTL pt) const;
    void                 RevokeAllRegistrations ();

    std::atomic<ULONG>   m_refCount             { 1 };
    HWND                 m_hwnd                 = nullptr;
    std::vector<HWND>    m_registeredHwnds;
    HitTestFn            m_hitTest;
    HitTester          * m_hitTester            = nullptr;
    DropFn               m_drop;
    int                  m_lastHitTag           = -1;
    bool                 m_fDragActive           = false;
    bool                 m_fDragHasSupportedFile = false;
    bool                 m_fSuppressNextClick    = false;
    std::wstring         m_dragPath;
};
