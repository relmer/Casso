#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiFocusManager.h"
#include "Window/DxuiHwndSource.h"
#include "Window/IDxuiHostClient.h"


class DxuiButton;





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
        bool                composited        = false;   // composited-transparent window (enables SetComposedOpacity)
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
    // activate=false shows the window without pulling foreground/focus (for
    // windows that pop up on their own and must not steal keystrokes).
    void     Show        (bool activate = true);
    void     Hide        ();
    void     Close        ();

    //
    //  Modal show. Displays the window, disables its owner, and runs a
    //  private message loop until EndDialog() is called (by a command
    //  button, Enter on the default button, or Escape / close-box on
    //  IDCANCEL), then re-enables the owner and returns the result code.
    //  Mirrors Win32 DialogBox / EndDialog.
    //
    //  defaultButtonId is the command id of the button Enter activates
    //  (the window emphasizes it); pass IDOK, IDYES, etc. Every command
    //  button without a custom SetOnClick auto-closes via EndDialog(its
    //  command id), so plain OK / Cancel / Yes / No need no wiring.
    //
    int      ShowModalDialog  (int defaultButtonId);

    //
    //  Modeless show. Applies the same dialog behaviors as
    //  ShowModalDialog -- focus-manager Tab traversal, default-button
    //  emphasis, auto-EndDialog button wiring, and the periodic tick --
    //  but does NOT disable the owner and does NOT run a private loop:
    //  it shows the window and returns immediately. The host's own
    //  message loop drives it; call ProcessDialogMessage() before
    //  TranslateMessage / DispatchMessage to mirror Win32
    //  CreateDialog + IsDialogMessage.
    //
    //  EndDialog() on a modeless window hides it and fires the callback
    //  installed via SetOnDialogEnd (there is no return value, since the
    //  call did not block). Use for a live overlay whose backdrop must
    //  keep animating (e.g. Settings over the running emulator).
    //
    void     ShowModelessDialog (int defaultButtonId);
    void     EndDialog   (int result);

    //
    //  Window-level translucency for a composited window (CreateParams::
    //  composited). Fades the whole window (0 = invisible .. 1 = opaque) so
    //  desktop content behind it shows through -- e.g. the Settings sheet
    //  revealing the live emulator while a Display control is dragged. No-op
    //  when not composited.
    //
    void     SetComposedOpacity (float opacity);

    //
    //  Dxui's IsDialogMessage equivalent for a modeless dialog. The host's
    //  shared message loop calls this BEFORE TranslateMessage /
    //  DispatchMessage; it consumes only the dialog-navigation keys
    //  (Tab/Shift+Tab, Enter, Escape) for this window, leaving normal text
    //  input and all other messages to the standard dispatch path.
    //
    bool     ProcessDialogMessage (const MSG & msg);

    //
    //  Modeless close callback: invoked by EndDialog() with the result
    //  code when a modeless window closes (a command button, Escape, or
    //  content code). The modal path returns the code from
    //  ShowModalDialog instead, so this fires only in the modeless case.
    //
    void     SetOnDialogEnd (std::function<void (int)> fn) { m_onDialogEnd = std::move (fn); }

    //
    //  Control to focus when the dialog is first shown (e.g. a picker's
    //  search box), so typing / Tab work immediately. Null = focus
    //  nothing, so Enter hits the default button. Call before
    //  ShowModalDialog / ShowModelessDialog.
    //
    void     SetInitialFocus (IDxuiControl * ctl) { m_initialFocus = ctl; }

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
    //  Dialog periodic hook. While a dialog (modal or modeless) is
    //  showing, the window drives a timer that repaints (so a focused
    //  caret blinks) and calls this each tick -- override for a poller
    //  (e.g. a download progress dialog). Default no-op.
    //
    virtual void  OnDialogTick    () {}

    //
    //  Ctrl+Tab / Ctrl+Shift+Tab while a dialog is showing. The dialog
    //  message pump detects the Ctrl chord and calls this (backward=true for
    //  Shift) so a tabbed window (e.g. DxuiPropertySheet) can cycle its pages;
    //  plain Tab still does focus traversal. Return true if consumed. Default
    //  no-op (a plain dialog has no page tabs).
    //
    virtual bool  OnDialogTabSwitch (bool backward) { UNREFERENCED_PARAMETER (backward); return false; }

    //
    //  Modal in-content overlay (e.g. the Settings color picker). While
    //  HasModalOverlay() returns true the window paints PaintModalOverlay on
    //  top of the whole page every frame and routes ALL mouse / char / key
    //  input to the OnOverlay* hooks first -- so the overlay reads and behaves
    //  as a modal dialog floating above the page without a separate HWND
    //  (FR-129 preserves the bespoke Settings look). OnOverlayMouse returns
    //  true when it consumed the event; OnOverlayKey / OnOverlayChar likewise,
    //  though keys are swallowed either way so they never reach the page while
    //  the overlay is up. Defaults leave a plain window overlay-free.
    //
    virtual bool  HasModalOverlay   () const { return false; }
    virtual void  PaintModalOverlay (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
                  { UNREFERENCED_PARAMETER (painter); UNREFERENCED_PARAMETER (text); UNREFERENCED_PARAMETER (theme); }
    virtual bool  OnOverlayMouse    (const DxuiMouseEvent & ev) { UNREFERENCED_PARAMETER (ev); return false; }
    virtual bool  OnOverlayChar     (wchar_t ch)                { UNREFERENCED_PARAMETER (ch); return false; }
    virtual bool  OnOverlayKey      (WPARAM vk)                 { UNREFERENCED_PARAMETER (vk); return false; }

    //
    //  Tune the dialog repaint / tick cadence (ms) before
    //  ShowModalDialog / ShowModelessDialog. The default suits caret
    //  blink; a poller (e.g. download progress) sets a faster interval.
    //
    void  SetDialogTickIntervalMs (UINT ms) { m_dialogTickMs = ms; }

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
    DxuiMessageResult  OnTimer       (UINT_PTR timerId) override;
    DxuiMessageResult  OnClose       () override;
    void               OnDestroy     () override;

    DxuiMessageResult  DispatchMouse (DxuiMouseEventKind kind,
                                      DxuiMouseButton    button,
                                      int                x,
                                      int                y,
                                      float              wheelDelta,
                                      bool               wheelHorizontal = false);
    DxuiMessageResult  DispatchKey   (DxuiKeyEventKind kind, WPARAM code);
    DxuiMessageResult  DispatchDialogKey (WPARAM vk);

    //
    //  Dialog support: enter dialog mode (wire buttons, attach/rebuild
    //  focus, seed initial focus), route a key to the focused control,
    //  and find / fire a button by command id. The button walks are
    //  recursive over the panel tree so buttons may live in a nested
    //  button-row panel.
    //
    void          BeginDialogMode  (int defaultButtonId, bool modal);
    void          WireDialogButtons ();
    bool          RouteKeyToFocused (WPARAM vk, bool shift);
    bool          TriggerButtonById (int commandId);
    static DxuiButton *  FindButtonById   (IDxuiControl * node, int commandId);
    static void          ForEachButton    (IDxuiControl * node, const std::function<void (DxuiButton *)> & fn);


    std::unique_ptr<DxuiHwndSource>  m_source;
    SIZE                             m_minSizeDip = { 0, 0 };
    HWND                             m_ownerHwnd  = nullptr;
    const IDxuiTheme *               m_theme      = nullptr;
    IDxuiControl *                   m_initialFocus = nullptr;
    DxuiFocusManager                 m_focus;
    bool                             m_dialogActive   = false;   // dialog behaviors on (modal or modeless)
    bool                             m_modal          = false;   // blocking-modal (owner disabled + private loop)
    bool                             m_modalDone      = false;
    int                              m_modalResult    = 0;
    int                              m_defaultButtonId = 0;
    UINT                             m_dialogTickMs   = 250;   // dialog repaint / tick cadence (caret-blink default)
    std::function<void (int)>        m_onDialogEnd;            // modeless close callback
};
