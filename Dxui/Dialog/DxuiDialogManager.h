#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"


class DxuiDialog;
class IDxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDialogManager
//
//  Modal-with-nesting dialog stack. Each `Show` call pushes a frame
//  onto an internal stack; closing the dialog pops the frame. Frames
//  capture the prev-top HWND (or the consumer-supplied `ownerHwnd`
//  when the stack is empty), disable it on push, and re-enable it on
//  pop. Win32 then handles z-order / activation restore automatically
//  via the standard owner-window mechanism.
//
//  Step 8 scope (this commit):
//      * Class scaffolding + stack semantics.
//      * `Show` accepts a fully-built `DxuiDialog`; the manager
//        installs the dialog's close handler so the returned future
//        resolves with the dialog's chosen return code. A consumer-
//        driven test pump can fire button clicks / Enter / Escape to
//        drive the dialog to completion.
//      * `DxuiHwndSource` hosting (real HWND creation + modal scrim
//        rendering) is wired in a follow-up step; the framework is
//        complete enough for stack-management tests to ride against
//        the test seams below.
//
//  Test seams:
//      * `PushForTest (fakeHwndStandIn, params)` pushes a frame keyed
//        on an arbitrary IDxuiControl pointer (no real DxuiDialog
//        required). Returns the frame ID.
//      * `PopForTest  (frameId, result)` pops the named frame and
//        resolves the recorded promise if any.
//      * `SetEnableWindowHookForTest` injects a recording hook for
//        owner-HWND EnableWindow calls so tests can assert the
//        prev-top disable/enable sequence without touching real
//        windows.
//
//  All public methods are called on the UI thread (FR-083); each
//  entry point asserts this in debug builds.
//
////////////////////////////////////////////////////////////////////////////////



struct DxuiDialogShowParams
{
    bool  modalScrim = false;
    HWND  ownerHwnd  = nullptr;
};



//
//  Blocking-modal show parameters. Unlike the future-based Show, these
//  drive ShowModal, which stands up its own DxuiHwndSource, runs a
//  private modal message pump, and returns the chosen return code
//  synchronously -- the production replacement for the legacy
//  DialogPrimitive::Show contract.
//
struct DxuiDialogModalParams
{
    HINSTANCE          hInstance      = nullptr;
    HWND               ownerHwnd      = nullptr;
    const IDxuiTheme * theme          = nullptr;
    SIZE               clientSizeDip  = { 420, 200 };
    int                cancelResult   = -1;

    // When true the modal window gets resize borders + maximize; the
    // content re-docks on every WM_SIZE. minClientSizeDip clamps the
    // minimum client area (DIP). Defaults keep dialogs fixed-size.
    bool               resizable         = false;
    SIZE               minClientSizeDip  = { 0, 0 };
};



class DxuiDialogManager
{
public:
    using EnableWindowHook = std::function<void (HWND hwnd, bool enable)>;


    DxuiDialogManager  ();
    ~DxuiDialogManager();

    //
    //  Production entry point. Takes ownership of a fully-built
    //  `DxuiDialog`; returns a `std::future<int>` that resolves with
    //  the chosen return code when the dialog closes (button click,
    //  Enter on the default button, or Escape on the cancel button).
    //  The dialog MUST have been `Build()`-ed by the caller.
    //
    std::future<int>  Show  (std::unique_ptr<DxuiDialog>  dialog,
                             DxuiDialogShowParams         params);

    //
    //  Blocking production entry point. Stands up a full-ownership
    //  DxuiHwndSource with the standard host-owned caption (title +
    //  close), installs an internal modal host-client, disables the
    //  owner, runs a private message pump until the dialog closes, then
    //  tears the host down and returns the chosen return code. The
    //  dialog is passed configured (title / content / buttons) but NOT
    //  Build()-ed -- ShowModal builds it caption-less so the host owns
    //  the caption. Returns params.cancelResult on a window-close
    //  gesture when no cancel button is present.
    //
    static int  ShowModal  (std::unique_ptr<DxuiDialog>    dialog,
                            const DxuiDialogModalParams  & params);

    //
    //  Test seams -- drive the stack without a real DxuiDialog /
    //  DxuiHwndSource. The `fakeHwndStandIn` pointer is recorded
    //  verbatim in the frame and is what `TopStandIn()` returns when
    //  the frame is on top. Returns the frame ID used to pop.
    //
    int   PushForTest  (IDxuiControl *           fakeHwndStandIn,
                        DxuiDialogShowParams     params);
    bool  PopForTest   (int                      frameId,
                        int                      result);

    //
    //  Stack inspection (UI thread).
    //
    size_t           StackSize     () const;
    IDxuiControl *   TopStandIn    () const;
    bool             TopModalScrim() const;
    HWND             TopOwnerHwnd  () const;

    //
    //  Install a hook that intercepts owner-window EnableWindow
    //  calls. Production wires this to `::EnableWindow`; tests
    //  install a recorder so they can assert the prev-top
    //  disable / enable sequence.
    //
    void  SetEnableWindowHookForTest  (EnableWindowHook hook);


private:
    struct Frame
    {
        int                                 id          = 0;
        std::unique_ptr<DxuiDialog>         dialog;            // null for test frames
        IDxuiControl *                      standIn     = nullptr;
        DxuiDialogShowParams                params;
        HWND                                prevTopHwnd = nullptr;
        std::shared_ptr<std::promise<int>>  promise;           // null for test frames
    };


    int   PushInternal  (std::unique_ptr<DxuiDialog>          dialog,
                         IDxuiControl *                       standIn,
                         DxuiDialogShowParams                 params,
                         std::shared_ptr<std::promise<int>>   promise);
    bool  PopInternal   (int frameId, int result);

    void  ApplyEnable   (HWND hwnd, bool enable);


    std::vector<Frame>  m_stack;
    int                 m_nextFrameId  = 1;
    EnableWindowHook    m_enableHook;
};
