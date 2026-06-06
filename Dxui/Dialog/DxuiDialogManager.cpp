#include "Pch.h"

#include "Dialog/DxuiDialogManager.h"
#include "Dialog/DxuiDialog.h"





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
