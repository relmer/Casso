#include "Pch.h"

#include "MockDxuiControl.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace
{
    //
    //  Capture a sequence of EnableWindow hook invocations so tests
    //  can assert the prev-top disable / enable order without
    //  depending on actual HWND state.
    //
    struct EnableEvent
    {
        HWND  hwnd   = nullptr;
        bool  enable = false;
    };


    HWND  AsHwnd (uintptr_t  n)
    {
        return reinterpret_cast<HWND> (n);
    }
}





TEST_CLASS (DxuiDialogManagerTests)
{
public:

    TEST_METHOD_INITIALIZE (Setup)
    {
        DxuiResetUiThreadIdForTest();
    }


    TEST_METHOD (StackStartsEmpty)
    {
        DxuiDialogManager  mgr;


        Assert::AreEqual ((size_t) 0,        mgr.StackSize());
        Assert::IsNull ((const void *) mgr.TopStandIn());
        Assert::IsFalse (mgr.TopModalScrim());
        Assert::IsNull ((const void *) mgr.TopOwnerHwnd());
    }


    TEST_METHOD (PushForTest_GrowsStackAndExposesTop)
    {
        DxuiDialogManager      mgr;
        MockDxuiControl        standIn;
        DxuiDialogShowParams   params;

        params.modalScrim = true;
        params.ownerHwnd  = AsHwnd (0x1000);

        int  id = mgr.PushForTest (&standIn, params);


        Assert::AreNotEqual (0,                                 id);
        Assert::AreEqual ((size_t) 1,                          mgr.StackSize());
        Assert::AreEqual ((const void *) &standIn,             (const void *) mgr.TopStandIn());
        Assert::IsTrue   (mgr.TopModalScrim());
        Assert::AreEqual ((const void *) AsHwnd (0x1000),      (const void *) mgr.TopOwnerHwnd());
    }


    TEST_METHOD (PushForTest_ModalScrim_DefaultsFalse)
    {
        DxuiDialogManager      mgr;
        MockDxuiControl        standIn;
        DxuiDialogShowParams   params;

        int  id = mgr.PushForTest (&standIn, params);

        UNREFERENCED_PARAMETER (id);
        Assert::IsFalse (mgr.TopModalScrim());
    }


    TEST_METHOD (PopForTest_RemovesNamedFrame)
    {
        DxuiDialogManager      mgr;
        MockDxuiControl        standIn;
        DxuiDialogShowParams   params;

        int  id = mgr.PushForTest (&standIn, params);


        bool  popped = mgr.PopForTest (id, 42);

        Assert::IsTrue   (popped);
        Assert::AreEqual ((size_t) 0, mgr.StackSize());
        Assert::IsNull ((const void *) mgr.TopStandIn());
    }


    TEST_METHOD (PopForTest_UnknownFrameId_ReturnsFalse)
    {
        DxuiDialogManager      mgr;
        MockDxuiControl        standIn;
        DxuiDialogShowParams   params;

        int  pushedId = mgr.PushForTest (&standIn, params);

        IGNORE_RETURN_VALUE (pushedId, 0);


        bool  popped = mgr.PopForTest (9999, 0);

        Assert::IsFalse (popped);
        Assert::AreEqual ((size_t) 1, mgr.StackSize());
    }


    TEST_METHOD (PushTwoFrames_PopInner_OuterRegainsTop)
    {
        DxuiDialogManager      mgr;
        MockDxuiControl        outer;
        MockDxuiControl        inner;
        DxuiDialogShowParams   outerParams;
        DxuiDialogShowParams   innerParams;

        outerParams.ownerHwnd = AsHwnd (0x4000);
        innerParams.ownerHwnd = AsHwnd (0x5000);

        int  outerId = mgr.PushForTest (&outer, outerParams);
        int  innerId = mgr.PushForTest (&inner, innerParams);


        Assert::AreEqual ((size_t) 2,                       mgr.StackSize());
        Assert::AreEqual ((const void *) &inner,            (const void *) mgr.TopStandIn());

        bool  poppedInner = mgr.PopForTest (innerId, 1);

        Assert::IsTrue   (poppedInner);
        Assert::AreEqual ((size_t) 1,                       mgr.StackSize());
        Assert::AreEqual ((const void *) &outer,            (const void *) mgr.TopStandIn());

        bool  poppedOuter = mgr.PopForTest (outerId, 2);

        Assert::IsTrue   (poppedOuter);
        Assert::AreEqual ((size_t) 0,                       mgr.StackSize());
    }


    TEST_METHOD (EnableWindowHook_FiresPrevTopDisableOnPushEnableOnPop)
    {
        DxuiDialogManager           mgr;
        MockDxuiControl             outer;
        MockDxuiControl             inner;
        DxuiDialogShowParams        outerParams;
        DxuiDialogShowParams        innerParams;
        std::vector<EnableEvent>    events;

        outerParams.ownerHwnd = AsHwnd (0x100);   // app's top HWND
        innerParams.ownerHwnd = AsHwnd (0x200);   // outer dialog's HWND

        mgr.SetEnableWindowHookForTest ([&events] (HWND h, bool e)
                                        {
                                            EnableEvent  ev;
                                            ev.hwnd   = h;
                                            ev.enable = e;
                                            events.push_back (ev);
                                        });

        int  outerId = mgr.PushForTest (&outer, outerParams);

        //
        //  First push: stack was empty, prev-top falls back to the
        //  new frame's ownerHwnd (app's main HWND).
        //
        Assert::AreEqual ((size_t) 1, events.size());
        Assert::AreEqual ((const void *) AsHwnd (0x100), (const void *) events[0].hwnd);
        Assert::IsFalse (events[0].enable);

        int  innerId = mgr.PushForTest (&inner, innerParams);

        //
        //  Second push: prev-top is the outer frame's ownerHwnd
        //  (which is the outer dialog's own HWND from the app's
        //  perspective).
        //
        Assert::AreEqual ((size_t) 2, events.size());
        Assert::AreEqual ((const void *) AsHwnd (0x200), (const void *) events[1].hwnd);
        Assert::IsFalse (events[1].enable);

        bool  poppedInnerHook = mgr.PopForTest (innerId, 0);

        IGNORE_RETURN_VALUE (poppedInnerHook, false);

        //
        //  Pop inner: re-enable the outer dialog HWND.
        //
        Assert::AreEqual ((size_t) 3, events.size());
        Assert::AreEqual ((const void *) AsHwnd (0x200), (const void *) events[2].hwnd);
        Assert::IsTrue (events[2].enable);

        bool  poppedOuterHook = mgr.PopForTest (outerId, 0);

        IGNORE_RETURN_VALUE (poppedOuterHook, false);

        //
        //  Pop outer: re-enable the app's main HWND.
        //
        Assert::AreEqual ((size_t) 4, events.size());
        Assert::AreEqual ((const void *) AsHwnd (0x100), (const void *) events[3].hwnd);
        Assert::IsTrue (events[3].enable);
    }


    TEST_METHOD (Show_BuiltDialog_ReturnsFutureResolvedOnButtonActivation)
    {
        DxuiDialogManager                mgr;
        std::unique_ptr<DxuiDialog>      dlg = std::make_unique<DxuiDialog>();
        DxuiDialog *                     dlgRaw = dlg.get();
        DxuiDialogShowParams             params;

        dlg->SetTitle (L"T");
        dlg->AddButton (L"OK",     7, true);
        dlg->AddButton (L"Cancel", 8, false, true);
        dlg->Build();

        std::future<int>  fut = mgr.Show (std::move (dlg), params);

        Assert::AreEqual ((size_t) 1, mgr.StackSize());

        std::optional<int>  activated = dlgRaw->ActivateDefault();

        IGNORE_RETURN_VALUE (activated, std::optional<int>());

        Assert::IsTrue (fut.wait_for (std::chrono::seconds (0)) == std::future_status::ready);
        Assert::AreEqual (7,            fut.get());
        Assert::AreEqual ((size_t) 0,   mgr.StackSize());
    }


    TEST_METHOD (Show_BuiltDialog_EscapeResolvesFutureWithCancelCode)
    {
        DxuiDialogManager                mgr;
        std::unique_ptr<DxuiDialog>      dlg = std::make_unique<DxuiDialog>();
        DxuiDialog *                     dlgRaw = dlg.get();
        DxuiDialogShowParams             params;

        dlg->AddButton (L"OK",     1, true);
        dlg->AddButton (L"Cancel", 2, false, true);
        dlg->Build();

        std::future<int>  fut = mgr.Show (std::move (dlg), params);

        DxuiKeyEvent  esc = {};
        esc.kind = DxuiKeyEventKind::Down;
        esc.vk   = VK_ESCAPE;

        bool  consumed = dlgRaw->OnKey (esc);

        IGNORE_RETURN_VALUE (consumed, false);

        Assert::IsTrue   (fut.wait_for (std::chrono::seconds (0)) == std::future_status::ready);
        Assert::AreEqual (2, fut.get());
        Assert::AreEqual ((size_t) 0, mgr.StackSize());
    }
};
