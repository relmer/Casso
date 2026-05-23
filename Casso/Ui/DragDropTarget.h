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

private:
    int                  PickAtScreen          (POINTL pt) const;

    std::atomic<ULONG>   m_refCount             { 1 };
    HWND                 m_hwnd                 = nullptr;
    bool                 m_fRegistered          = false;
    HitTestFn            m_hitTest;
    HitTester          * m_hitTester            = nullptr;
    DropFn               m_drop;
    int                  m_lastHitTag           = -1;
    bool                 m_fDragHasSupportedFile = false;
    std::wstring         m_dragPath;
};
