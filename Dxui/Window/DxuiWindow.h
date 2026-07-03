#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "Window/DxuiHwndSource.h"
#include "Window/IDxuiHostClient.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindow
//
//  Top-level window element. Mirrors WPF's `Window : ContentControl`:
//  a DxuiWindow IS a DxuiPanel (so it is its own content root -- child
//  controls are added to it directly via Create<T> / Add<T>) AND it
//  owns the single OS window (HWND + swap chain + caption + paint pump)
//  through an internal DxuiHwndSource backend, which stays entirely
//  hidden from the consumer.
//
//  The backend paints and lays out this panel automatically (it is
//  installed as the host's non-owning content root). DxuiWindow's only
//  job on top of DxuiPanel is to translate the Win32 messages the host
//  does not own end-to-end (mouse / keyboard / cursor / close / min-max)
//  into the framework's DxuiMouseEvent / DxuiKeyEvent dispatch and a few
//  virtual hooks -- so a subclass never touches an HWND, a WPARAM, or
//  IDxuiHostClient.
//
//  Typical subclass:
//      class MyWindow : public DxuiWindow { ... };
//      void MyWindow::OnCreate () override
//      {
//          m_ok = Create<DxuiButton> (L"OK");     // parented to *this
//          m_ok->SetOnClick ([this]{ ... });
//      }
//
//  All public methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////


class DxuiWindow : public DxuiPanel, private IDxuiHostClient
{
public:
    struct CreateParams
    {
        std::wstring        title;
        HINSTANCE           hInstance         = nullptr;
        HWND                ownerHwnd         = nullptr;
        SIZE                initialSizeDip    = { 1024, 768 };
        SIZE                minSizeDip        = { 0, 0 };
        bool                resizable         = true;
        bool                insetContentBelowCaption = false;
        DxuiCaptionStyle    captionStyle      = DxuiCaptionStyle::Standard;
        LPCWSTR             classNameOverride = nullptr;
        HICON               appIconBig        = nullptr;
        HICON               appIconSmall      = nullptr;
    };


    DxuiWindow  () = default;
    ~DxuiWindow () override;

    //
    //  Conjure the OS window (hidden) and its backend, install this
    //  panel as the content root, then invoke the OnCreate() hook so
    //  the subclass can populate its children. Call Show() to display.
    //
    HRESULT  Create      (const CreateParams & params);
    void     Show        ();
    void     Hide        ();
    void     Close        ();

    bool     IsCreated   () const { return m_source != nullptr; }
    HWND     Hwnd        () const { return m_source != nullptr ? m_source->Hwnd() : nullptr; }

    //
    //  Request a repaint of the window (the backend's WM_PAINT pump
    //  walks this panel tree). Consumers with a per-frame data model
    //  call this after mutating state.
    //
    void     Invalidate  ();

    void     SetTheme    (const IDxuiTheme * theme);
    void     SetTitle    (const std::wstring & title);
    int      CaptionHeightPx () const { return m_source != nullptr ? m_source->CaptionHeightPx() : 0; }
    UINT     Dpi         () const { return m_source != nullptr ? m_source->Scaler().Dpi() : USER_DEFAULT_SCREEN_DPI; }

    //
    //  Popup backend for DxuiPopupMenu / DxuiTooltip owned by content
    //  in this window (they render through the host's pooled popup
    //  surfaces) plus the shared text renderer used to size popups.
    //  Null before Create() / after Close().
    //
    DxuiHwndSource    *  PopupHost    () const { return m_source.get(); }
    IDxuiTextRenderer *  TextRenderer () const { return m_source != nullptr ? m_source->GetTextRenderer() : nullptr; }


protected:
    //
    //  Subclass hooks. OnCreate fires once the backend + HWND exist
    //  (populate children here). OnWindowClose fires on the caption
    //  close box / WM_CLOSE (default hides -- non-modal); override to
    //  destroy or prompt. OnWindowDestroy fires on WM_DESTROY.
    //
    virtual void  OnCreate        () {}
    virtual void  OnWindowClose   () { Hide(); }
    virtual void  OnWindowDestroy () {}

    //
    //  Tear down the backend (HWND + swap chain). Safe to call from a
    //  subclass destructor when the subclass owns members that the
    //  backend teardown could otherwise reach; the base destructor
    //  calls it too (idempotent).
    //
    void  DestroyBackend ();


private:
    DxuiMessageResult  OnLButtonDown (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnLButtonUp   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnRButtonDown (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnMouseMove   (WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnMouseWheel  (WPARAM wParam, LPARAM lParam, bool horizontal) override;
    DxuiMessageResult  OnKeyDown     (WPARAM vk, LPARAM lParam) override;
    DxuiMessageResult  OnChar        (WPARAM ch, LPARAM lParam) override;
    DxuiMessageResult  OnSetCursor   (WORD hitTest) override;
    DxuiMessageResult  OnGetMinMax   (MINMAXINFO * info) override;
    DxuiMessageResult  OnClose       () override;
    void               OnDestroy     () override;

    DxuiMessageResult  DispatchMouse (DxuiMouseEventKind kind,
                                      DxuiMouseButton    button,
                                      int                x,
                                      int                y,
                                      float              wheelDelta);
    DxuiMessageResult  DispatchKey   (DxuiKeyEventKind kind, WPARAM code);


    std::unique_ptr<DxuiHwndSource>  m_source;
    SIZE                             m_minSizeDip = { 0, 0 };
};
