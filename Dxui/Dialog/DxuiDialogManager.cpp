#include "Pch.h"

#include "Dialog/DxuiDialogManager.h"
#include "Dialog/DxuiDialog.h"
#include "Window/DxuiHostWindow.h"
#include "Window/IDxuiHostClient.h"
#include "Core/DxuiFocusManager.h"
#include "Core/DxuiEvents.h"
#include "Core/IDxuiControl.h"





namespace
{
    //
    //  Internal IDxuiHostClient for the blocking ShowModal pump. Owns a
    //  focus manager over the dialog so Tab / typing / Enter reach the
    //  content controls; routes Escape and a window-close gesture to the
    //  cancel result, recording the chosen return code so the pump exits.
    //
    class DxuiModalDialogClient : public IDxuiHostClient
    {
    public:
        void  Bind (DxuiDialog * dialog, int cancelResult)
        {
            m_dialog       = dialog;
            m_cancelResult = cancelResult;
            m_result       = cancelResult;
        }

        void  SetHwnd (HWND hwnd) { m_hwnd = hwnd; }

        void  SetMinClientSizeDip (SIZE sizeDip) { m_minClientSizeDip = sizeDip; }

        DxuiMessageResult  OnGetMinMax (MINMAXINFO * info) override
        {
            RECT  rcClient = {};
            RECT  rcWindow = {};
            UINT  dpi      = (m_hwnd != nullptr) ? GetDpiForWindow (m_hwnd) : 96;
            int   minCW    = MulDiv (m_minClientSizeDip.cx, (int) dpi, 96);
            int   minCH    = MulDiv (m_minClientSizeDip.cy, (int) dpi, 96);
            int   ncW      = 0;
            int   ncH      = 0;
            bool  handled  = false;


            if (info != nullptr && m_hwnd != nullptr &&
                (m_minClientSizeDip.cx > 0 || m_minClientSizeDip.cy > 0))
            {
                if (GetClientRect (m_hwnd, &rcClient) && GetWindowRect (m_hwnd, &rcWindow))
                {
                    ncW = (rcWindow.right  - rcWindow.left) - (rcClient.right  - rcClient.left);
                    ncH = (rcWindow.bottom - rcWindow.top)  - (rcClient.bottom - rcClient.top);
                }

                info->ptMinTrackSize.x = minCW + ncW;
                info->ptMinTrackSize.y = minCH + ncH;
                handled                = true;
            }

            return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
        }

        //  Attach the focus manager to the (built, hosted) dialog and put
        //  initial focus on the first tab stop so typing / Tab work.
        void  SetupFocus (DxuiDialog * dialog, const IDxuiTheme * theme)
        {
            m_focus.SetTheme (theme);
            m_focus.Attach   (dialog);
            m_focus.Rebuild  ();

            if (dialog != nullptr && dialog->InitialFocus() != nullptr)
            {
                m_focus.SetFocused (dialog->InitialFocus());
            }
        }

        void  Resolve (int returnCode)
        {
            if (!m_done)
            {
                m_done   = true;
                m_result = returnCode;
            }
        }

        bool  Done   () const { return m_done;   }
        int   Result () const { return m_result; }

        DxuiMessageResult  OnKeyDown (WPARAM vk, LPARAM lParam) override
        {
            bool  shift   = (GetKeyState (VK_SHIFT) & 0x8000) != 0;
            bool  handled = false;


            UNREFERENCED_PARAMETER (lParam);

            if (m_dialog != nullptr)
            {
                if (vk == VK_TAB)
                {
                    handled = m_focus.HandleKey (shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
                }
                else if (vk == VK_ESCAPE)
                {
                    std::optional<int>  rc = m_dialog->ActivateCancel();

                    handled = rc.has_value();

                    if (!handled)
                    {
                        Resolve (m_cancelResult);
                        handled = true;
                    }
                }
                else
                {
                    handled = RouteKeyToFocused (vk, shift);

                    if (!handled && vk == VK_RETURN)
                    {
                        std::optional<int>  rc = m_dialog->ActivateDefault();

                        handled = rc.has_value();
                    }
                }
            }

            return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
        }

        DxuiMessageResult  OnChar (WPARAM ch, LPARAM lParam) override
        {
            IDxuiControl *  focused = m_focus.Focused();
            bool            handled = false;


            UNREFERENCED_PARAMETER (lParam);

            if (focused != nullptr)
            {
                handled = focused->OnChar ((wchar_t) ch);
            }

            return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
        }

        DxuiMessageResult  OnClose () override
        {
            std::optional<int>  rc = std::nullopt;


            if (m_dialog != nullptr)
            {
                rc = m_dialog->ActivateCancel();
            }

            if (!rc.has_value())
            {
                Resolve (m_cancelResult);
            }

            return DxuiMessageResult::Handled;
        }

        DxuiMessageResult  OnMouseMove (WPARAM wParam, LPARAM lParam) override
        {
            DxuiMouseButton  button = ((wParam & MK_LBUTTON) != 0) ? DxuiMouseButton::Left
                                                                   : DxuiMouseButton::None;

            return RouteMouse (DxuiMouseEventKind::Move, button, lParam);
        }

        DxuiMessageResult  OnLButtonDown (WPARAM wParam, LPARAM lParam) override
        {
            DxuiMessageResult  routed = RouteMouse (DxuiMouseEventKind::Down, DxuiMouseButton::Left, lParam);


            UNREFERENCED_PARAMETER (wParam);

            if (routed == DxuiMessageResult::Handled && m_hwnd != nullptr)
            {
                SetCapture (m_hwnd);
            }

            return routed;
        }

        DxuiMessageResult  OnLButtonUp (WPARAM wParam, LPARAM lParam) override
        {
            DxuiMessageResult  routed = RouteMouse (DxuiMouseEventKind::Up, DxuiMouseButton::Left, lParam);


            UNREFERENCED_PARAMETER (wParam);

            if (m_hwnd != nullptr && GetCapture() == m_hwnd)
            {
                ReleaseCapture();
            }

            return routed;
        }

        DxuiMessageResult  OnMouseWheel (WPARAM wParam, LPARAM lParam, bool horizontal) override
        {
            DxuiMouseEvent  ev;
            POINT           pt      = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
            float           delta   = (float) GET_WHEEL_DELTA_WPARAM (wParam) / (float) WHEEL_DELTA;
            bool            handled = false;


            if (m_dialog == nullptr)
            {
                return DxuiMessageResult::NotHandled;
            }

            if (m_hwnd != nullptr)
            {
                ScreenToClient (m_hwnd, &pt);
            }

            // The list scrolls horizontally on Shift+wheel; a horizontal
            // wheel / touchpad swipe maps to the same path with the sign
            // flipped (Win32 +right vs the list's +delta = scroll-left).
            ev.kind        = DxuiMouseEventKind::Wheel;
            ev.positionDip = pt;
            ev.wheelDelta  = horizontal ? -delta : delta;
            ev.shift       = horizontal || ((GetKeyState (VK_SHIFT) & 0x8000) != 0);
            handled        = m_dialog->OnMouse (ev);

            if (handled && m_hwnd != nullptr)
            {
                InvalidateRect (m_hwnd, nullptr, FALSE);
            }

            return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
        }

        DxuiMessageResult  OnTimer (UINT_PTR timerId) override
        {
            UNREFERENCED_PARAMETER (timerId);

            if (m_dialog != nullptr)
            {
                m_dialog->Tick();
            }

            return DxuiMessageResult::Handled;
        }


    private:
        bool  RouteKeyToFocused (WPARAM vk, bool shift)
        {
            IDxuiControl *  focused = m_focus.Focused();
            DxuiKeyEvent    ke;


            if (focused == nullptr)
            {
                return false;
            }

            ke.kind  = DxuiKeyEventKind::Down;
            ke.vk    = vk;
            ke.shift = shift;

            return focused->OnKey (ke);
        }


        DxuiMessageResult  RouteMouse (DxuiMouseEventKind kind, DxuiMouseButton button, LPARAM lParam)
        {
            DxuiMouseEvent  ev;
            bool            handled = false;


            if (m_dialog != nullptr)
            {
                ev.kind        = kind;
                ev.button      = button;
                ev.positionDip = { GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam) };
                handled        = m_dialog->OnMouse (ev);
            }

            // Interactive content (list selection / hover, column-resize +
            // scrollbar drags, search-box caret) mutates on mouse input but
            // the host only repaints after keyboard / timer -- invalidate
            // here so those changes actually show.
            if (handled && m_hwnd != nullptr)
            {
                InvalidateRect (m_hwnd, nullptr, FALSE);
            }

            return handled ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
        }


        DxuiFocusManager  m_focus;
        DxuiDialog *      m_dialog       = nullptr;
        HWND              m_hwnd         = nullptr;
        SIZE              m_minClientSizeDip = { 0, 0 };
        int               m_result       = -1;
        int               m_cancelResult = -1;
        bool              m_done         = false;
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::DxuiDialogManager
//
////////////////////////////////////////////////////////////////////////////////

DxuiDialogManager::DxuiDialogManager()
{
    DXUI_ASSERT_UI_THREAD();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::~DxuiDialogManager
//
////////////////////////////////////////////////////////////////////////////////

DxuiDialogManager::~DxuiDialogManager()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::Show
//
//  Takes ownership of a fully-built DxuiDialog, installs its close
//  handler so the returned future resolves when the dialog closes,
//  and pushes a stack frame whose stand-in pointer is the dialog
//  itself. Production HWND hosting + modal-scrim painting is wired
//  in the follow-up step that migrates Casso's two dialogs onto this
//  framework.
//
////////////////////////////////////////////////////////////////////////////////

std::future<int> DxuiDialogManager::Show (std::unique_ptr<DxuiDialog>  dialog,
                                          DxuiDialogShowParams         params)
{
    std::shared_ptr<std::promise<int>>  promise   = std::make_shared<std::promise<int>>();
    std::future<int>                    future    = promise->get_future();
    DxuiDialog *                        dialogRaw = dialog.get();
    int                                 frameId   = 0;



    DXUI_ASSERT_UI_THREAD();
    assert (dialogRaw != nullptr && "DxuiDialogManager::Show requires a dialog");
    assert (dialogRaw->IsBuilt() && "DxuiDialog must be Build()-ed before Show");

    if (dialogRaw == nullptr)
    {
        promise->set_value (-1);
        return future;
    }

    frameId = PushInternal (std::move (dialog), dialogRaw, params, promise);

    //
    //  Install the close handler AFTER PushInternal so the frame is
    //  on the stack when the handler fires (defensive against a
    //  consumer that constructs a dialog with a default value that
    //  fires immediately).
    //
    dialogRaw->SetCloseHandler ([this, frameId] (int returnCode)
                                {
                                    this->PopInternal (frameId, returnCode);
                                });

    return future;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::ShowModal
//
//  Blocking production show. Stands up a borderless full-ownership
//  DxuiHostWindow with the standard host-owned caption (title + close),
//  disables the owner, and runs a private message pump until the dialog
//  resolves a return code (button click, Enter on the default button,
//  Escape or caption-close on cancel). The dialog is built caption-less
//  here so the host owns the caption. Returns params.cancelResult when
//  the window is closed with no cancel button present.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDialogManager::ShowModal (std::unique_ptr<DxuiDialog>    dialog,
                                  const DxuiDialogModalParams  & params)
{
    constexpr UINT                 s_kCaretRepaintMs = 250;   // caret-blink repaint cadence

    HRESULT                        hr            = S_OK;
    DxuiHostWindow                 host;
    DxuiModalDialogClient          client;
    DxuiHostWindow::CreateParams   hostParams;
    DxuiDialog                   * dialogRaw     = dialog.get();
    MSG                            msg           = {};
    int                            result        = params.cancelResult;
    BOOL                           gotMessage    = FALSE;
    bool                           ownerDisabled = false;



    DXUI_ASSERT_UI_THREAD();
    CBRA (dialogRaw);
    assert (!dialogRaw->IsBuilt() && "ShowModal builds the dialog itself; pass it configured but not Build()-ed");

    //
    //  The host supplies the standard caption (title + close), so the
    //  dialog itself is built caption-less (content + button row only).
    //
    dialogRaw->SetOwnCaption (false);
    dialogRaw->Build();

    client.Bind (dialogRaw, params.cancelResult);
    dialogRaw->SetCloseHandler ([&client] (int returnCode) { client.Resolve (returnCode); });

    hostParams.title                = dialogRaw->Title();
    hostParams.hInstance            = params.hInstance;
    hostParams.ownerHwnd            = params.ownerHwnd;
    hostParams.borderless           = true;
    hostParams.resizable            = params.resizable;
    hostParams.roundedCorners       = true;
    hostParams.backdrop             = DxuiHostWindowBackdrop::None;
    hostParams.captionStyle         = DxuiCaptionStyle::CloseOnly;
    hostParams.insetRootBelowCaption = true;
    hostParams.initialSizeDip       = params.clientSizeDip;
    hostParams.createSwapChain      = true;

    host.SetClient (&client);

    hr = host.Create (hostParams);
    CHRA (hr);

    client.SetHwnd (host.Hwnd());
    client.SetMinClientSizeDip (params.minClientSizeDip);
    host.SetTheme        (params.theme);
    host.SetContentPanel (std::move (dialog));
    client.SetupFocus    (dialogRaw, params.theme);

    //
    //  Always run a timer. When the dialog set an app tick (e.g. the
    //  download poller) honour that interval; otherwise fall back to a
    //  caret-blink cadence so a focused search box / text input in the
    //  content actually blinks (its caret is clock-driven in Paint, so it
    //  only animates while the host repaints).
    //
    {
        UINT  appTickMs = dialogRaw->TickIntervalMs();
        UINT  timerMs   = (appTickMs > 0) ? appTickMs : s_kCaretRepaintMs;

        host.SetTimer (1, timerMs);
    }

    if (params.ownerHwnd != nullptr)
    {
        EnableWindow (params.ownerHwnd, FALSE);
        ownerDisabled = true;
    }

    ShowWindow          (host.Hwnd(), SW_SHOWNORMAL);
    UpdateWindow        (host.Hwnd());
    SetForegroundWindow (host.Hwnd());

    while (!client.Done())
    {
        gotMessage = GetMessageW (&msg, nullptr, 0, 0);

        if (gotMessage == 0)
        {
            PostQuitMessage ((int) msg.wParam);
            break;
        }

        if (gotMessage == -1)
        {
            break;
        }

        TranslateMessage (&msg);
        DispatchMessageW (&msg);
    }

    result = client.Result();

Error:
    if (ownerDisabled)
    {
        EnableWindow (params.ownerHwnd, TRUE);
    }

    host.Destroy();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::PushForTest
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDialogManager::PushForTest (IDxuiControl *        fakeHwndStandIn,
                                    DxuiDialogShowParams  params)
{
    DXUI_ASSERT_UI_THREAD();

    return PushInternal (nullptr, fakeHwndStandIn, params, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::PopForTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogManager::PopForTest (int frameId, int result)
{
    DXUI_ASSERT_UI_THREAD();

    return PopInternal (frameId, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::PushInternal
//
//  Captures the prev-top HWND (top-of-stack ownerHwnd, or the new
//  frame's ownerHwnd when the stack is empty), disables it via the
//  install hook, then pushes the frame and returns its id.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiDialogManager::PushInternal (std::unique_ptr<DxuiDialog>          dialog,
                                     IDxuiControl *                       standIn,
                                     DxuiDialogShowParams                 params,
                                     std::shared_ptr<std::promise<int>>   promise)
{
    Frame  frame;
    HWND   prevTop = params.ownerHwnd;



    //
    //  The caller threads the HWND-to-disable through
    //  `params.ownerHwnd` -- typically the top-of-stack dialog's
    //  HWND for nested cases, or the application's main HWND when
    //  the stack is empty. The manager simply disables it on push
    //  and re-enables it on pop; stack-traversal logic is the
    //  caller's responsibility.
    //
    ApplyEnable (prevTop, false);

    frame.id          = m_nextFrameId++;
    frame.dialog      = std::move (dialog);
    frame.standIn     = standIn;
    frame.params      = params;
    frame.prevTopHwnd = prevTop;
    frame.promise     = std::move (promise);

    m_stack.push_back (std::move (frame));

    return m_stack.back().id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::PopInternal
//
//  Locates the named frame, resolves its promise (if any), re-enables
//  its prev-top HWND, and removes it from the stack. Returns true iff
//  a matching frame was found.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogManager::PopInternal (int frameId, int result)
{
    size_t  idx = SIZE_MAX;



    for (size_t i = 0; i < m_stack.size(); i++)
    {
        if (m_stack[i].id == frameId)
        {
            idx = i;
            break;
        }
    }

    if (idx == SIZE_MAX)
    {
        return false;
    }

    //
    //  Capture the bits we need before erasing.
    //
    {
        Frame &                             frame    = m_stack[idx];
        HWND                                prevTop  = frame.prevTopHwnd;
        std::shared_ptr<std::promise<int>>  promise  = frame.promise;

        if (promise)
        {
            promise->set_value (result);
        }

        ApplyEnable (prevTop, true);
    }

    m_stack.erase (m_stack.begin() + (std::ptrdiff_t) idx);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::ApplyEnable
//
//  Routes owner-window EnableWindow calls through the install hook
//  when present, otherwise directly through `::EnableWindow`. The hook
//  is the test seam that lets stack tests verify the disable / enable
//  sequence without standing up real HWNDs.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogManager::ApplyEnable (HWND hwnd, bool enable)
{
    if (m_enableHook)
    {
        m_enableHook (hwnd, enable);
        return;
    }

    if (hwnd != nullptr)
    {
        BOOL  prev = ::EnableWindow (hwnd, enable ? TRUE : FALSE);

        IGNORE_RETURN_VALUE (prev, FALSE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::StackSize
//
////////////////////////////////////////////////////////////////////////////////

size_t DxuiDialogManager::StackSize() const
{
    return m_stack.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::TopStandIn
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DxuiDialogManager::TopStandIn() const
{
    if (m_stack.empty())
    {
        return nullptr;
    }

    return m_stack.back().standIn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::TopModalScrim
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDialogManager::TopModalScrim() const
{
    if (m_stack.empty())
    {
        return false;
    }

    return m_stack.back().params.modalScrim;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::TopOwnerHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND DxuiDialogManager::TopOwnerHwnd() const
{
    if (m_stack.empty())
    {
        return nullptr;
    }

    return m_stack.back().params.ownerHwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager::SetEnableWindowHookForTest
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDialogManager::SetEnableWindowHookForTest (EnableWindowHook hook)
{
    DXUI_ASSERT_UI_THREAD();
    m_enableHook = std::move (hook);
}
